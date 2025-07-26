#include "redis_parser.hpp"
#include "cli_helper/cli_helper.hpp"

#include <iostream>
#include <string>

// sending from sockets & closing it
#include <sys/socket.h>
#include <unistd.h>

// to convert to lowercase / check if string is a number
#include <cctype>
#include <algorithm>
#include <vector>

// for SET, GET, and expiry
#include <unordered_map>
#include <chrono>

// for BLPOP
#include <mutex>
#include <condition_variable>
#include <queue>

// for pair
#include <utility>

using namespace std;
using namespace std::chrono;

unordered_map<int, bool> isMultiQueued;
unordered_map<int, queue<string>> multiQueue;

unordered_map<string, string> redisMap;
unordered_map<string, steady_clock::time_point> expiryMap;

unordered_map<string, vector<string>> listMap;
unordered_map<string, condition_variable> cvMap;
unordered_map<string, mutex> mtxMap;
unordered_map<string, queue<int>> waitingClients;

unordered_map<string, vector<unordered_map<string, string>>> streamMap;
unordered_map<int, string> sendToBlocked;
queue<pair<int, string>> streamQueue;

void buildEntry(string& response, const vector<string>& tokens, string stream_key, long long cur_ms, int cur_ver) {
    // build entry

    unordered_map<string, string> new_entry;
    string new_stream_key = to_string(cur_ms) + "-" + to_string(cur_ver);

    new_entry["id"] = new_stream_key;
    for (int i = 3;i + 1 < tokens.size();i += 2) {
        new_entry[tokens[i]] = tokens[i + 1];
    }

    // append to stream
    streamMap[stream_key].push_back(move(new_entry));

    response = resp_bulk_string(new_stream_key);
}

void redisCommands(const vector<string>& tokens, int client_fd, string& response) {

    if (tokens[0] == "PING") {
        response = "+PONG\r\n";
    }
    else if (tokens.size() == 2 && to_lower(tokens[0]) == "echo") {
        response = resp_bulk_string(tokens[1]);

    }
    else if (tokens[0] == "SET") {
        string list_key = tokens[1], value = tokens[2];

        redisMap[list_key] = value;
        response = "+OK\r\n";

        // check if there's an expiry time
        if ((tokens.size() == 5) && (to_lower(tokens[3]) == "px")) {
            int time = stoi(tokens[4]);
            expiryMap[list_key] = steady_clock::now() + milliseconds(time);
        }

    }
    else if (tokens[0] == "GET") {
        string list_key = tokens[1];

        if (redisMap.find(list_key) != redisMap.end()) {
            string data = redisMap[list_key];

            // check if time now is > expiry time (is expired)
            if (expiryMap.find(list_key) != expiryMap.end() && steady_clock::now() > expiryMap[list_key]) {
                redisMap.erase(list_key);
                expiryMap.erase(list_key);
            }

            if (redisMap.find(list_key) != redisMap.end())
                response = resp_bulk_string(redisMap[list_key]);
            else
                response = "$-1\r\n";
        }
        else {
            response = "$-1\r\n";
        }

    }
    else if (tokens[0] == "RPUSH" || tokens[0] == "LPUSH" || tokens[0] == "LLEN") {
        string list_key = tokens[1];
        unique_lock<mutex> lock(mtxMap[list_key]);

        for (int i = 2;i < tokens.size();i++) {
            if (tokens[0] == "RPUSH")
                listMap[list_key].push_back(tokens[i]);
            else
                listMap[list_key].insert(listMap[list_key].begin(), tokens[i]);
        }

        if (tokens[0] == "RPUSH" && !waitingClients[list_key].empty()) {
            int cli_fd = waitingClients[list_key].front();
            waitingClients[list_key].pop();

            cvMap[list_key].notify_one();
        }

        // return number of elements in RESP Integer format
        //int size = listMap[list_key].size();
        response = ":" + to_string(listMap[list_key].size()) + "\r\n";
    }
    else if (tokens[0] == "LRANGE") {
        string list_key = tokens[1];
        int start = stoi(tokens[2]), end = stoi(tokens[3]);

        if (start < 0) start = listMap[list_key].size() + start;
        if (end < 0) end = listMap[list_key].size() + end;

        if (start < 0) start = 0;

        vector<string> res;

        for (int i = start;i <= end && i < listMap[list_key].size();i++) {
            res.push_back(listMap[list_key][i]);
        }

        response = lrange_bulk_string(res);

    }
    else if (tokens[0] == "LPOP") {
        string list_key = tokens[1];

        if (listMap[list_key].size() == 0)
            response = "$-1\r\n";
        else if (tokens.size() == 2) {
            response = resp_bulk_string(listMap[list_key][0]);
            listMap[list_key].erase(listMap[list_key].begin());
        }
        else {
            vector<string> res;
            int count = stoi(tokens[2]);
            int n = min(count, (int)listMap[list_key].size());

            for (int i = 0; i < n; i++) {
                res.push_back(listMap[list_key][0]);
                listMap[list_key].erase(listMap[list_key].begin());
            }

            response = lrange_bulk_string(res);
        }
    }
    else if (tokens[0] == "BLPOP") {
        string list_key = tokens[1];
        double wait_time = stod(tokens[2]);

        unique_lock<mutex> lock(mtxMap[list_key]);
        waitingClients[list_key].push(client_fd);

        // if wait_time = 0, then it should be infinite
        if (wait_time == 0) {
            cvMap[list_key].wait(lock, [&]() {
                return !listMap[list_key].empty();
                });

            vector<string> res = { list_key, listMap[list_key][0] };
            listMap[list_key].erase(listMap[list_key].begin());
            response = lrange_bulk_string(res);
        }
        else {
            bool timed_out = !cvMap[list_key].wait_for(lock, milliseconds((int)(wait_time * 1000)), [&]() {
                return !listMap[list_key].empty();
                });

            if (timed_out) {
                response = "$-1\r\n";
            }
            else {
                vector<string> res = { list_key, listMap[list_key][0] };
                listMap[list_key].erase(listMap[list_key].begin());
                response = lrange_bulk_string(res);
            }
        }
    }
    else if (tokens[0] == "INCR") {
        bool valid = false;
        string list_key = tokens[1];
        string value = redisMap[list_key];

        if (value.length() == 0) {
            valid = true;
            redisMap[list_key] = "1";
        }
        else if (isAllDigits(value)) {
            valid = true;
            redisMap[list_key] = to_string(stoi(redisMap[list_key]) + 1);
        }

        if (valid)
            response = ":" + redisMap[list_key] + "\r\n";
        else
            response = "-ERR value is not an integer or out of range\r\n";
    }
    else if (!isMultiQueued[client_fd] && tokens[0] == "MULTI") {
        isMultiQueued[client_fd] = true;
        response = "+OK\r\n";
    }
    else if (tokens[0] == "TYPE") {
        string list_key = tokens[1];
        if (streamMap.find(list_key) != streamMap.end()) {
            response = "+stream\r\n";
        }
        else if (redisMap[list_key] == "") {
            response = "+none\r\n";
        }
        else {
            response = "+string\r\n";
        }
    }
    else if (tokens[0] == "XADD") {
        string stream_key = tokens[1], stream_id = tokens[2];

        if (stream_id == "*") {
            //auto generate the ms + sequence number
            long long msTime = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

            int latest_version = -1;
            if (!streamMap[stream_key].empty()) {
                const auto& last_entry = streamMap[stream_key].back();
                auto [ms_str, ver_str] = parse_entry_id(last_entry.at("id"));

                latest_version = stoi(ver_str);
            }

            buildEntry(response, tokens, stream_key, msTime, latest_version + 1);
            return;
        }

        auto [cur_ms_str, cur_ver_str] = parse_entry_id(stream_id);

        // -1 == empty, if 0 then that means there was a past entry
        int max_ms = 0, max_ver = -1;
        if (!streamMap[stream_key].empty()) {
            const auto& last_entry = streamMap[stream_key].back();
            auto [ms_str, ver_str] = parse_entry_id(last_entry.at("id"));

            max_ms = stoi(ms_str);
            if (cur_ms_str == ms_str)
                max_ver = max(max_ver, stoi(ver_str));
        }

        int cur_ms = stoi(cur_ms_str);
        int cur_ver;
        if (cur_ver_str == "*") {
            // if 0-blank, then we have to set to 1 by default (0-0 isn't allowed, else ms-max_ver + 1)
            cur_ver = (cur_ms_str == "0" && max_ver == -1) ? 1 : max_ver + 1;
        }
        else {
            cur_ver = stoi(cur_ver_str);
        }

        if (cur_ms_str == "0" && cur_ver_str == "0") {
            response = "-ERR The ID specified in XADD must be greater than 0-0\r\n";
            return;
        }

        // Validate new ID is strictly greater
        if (cur_ms < max_ms || (cur_ms == max_ms && cur_ver <= max_ver)) {
            response = "-ERR The ID specified in XADD is equal or smaller than the target stream top item\r\n";
            return;
        }

        buildEntry(response, tokens, stream_key, cur_ms, cur_ver);

        // now send to the queued clients if there's any
        while (!streamQueue.empty()) {
            auto [send_client, stream_key] = streamQueue.front();

            sendToBlocked[send_client] = stream_key;
            streamQueue.pop();
            cvMap[stream_key].notify_one();
        }

    }
    else if (tokens[0] == "XRANGE") {
        string stream_key = tokens[1];
        string start_id = tokens[2];
        string end_id = tokens[3];

        vector<unordered_map<string, string>> matching_entries;

        for (auto& entry : streamMap[stream_key]) {
            string id = entry["id"];

            if ((end_id == "+" && start_id <= id) || (start_id <= id && id <= end_id)) {
                matching_entries.push_back(entry);
            }
        }

        response = "*" + to_string(matching_entries.size()) + "\r\n";
        for (auto& entry : matching_entries) {
            response += parse_entry(entry);
        }
    }
    else if (tokens[0] == "XREAD") {
        if (tokens[1] == "block") {
            int wait_time = stoi(tokens[2]);
            string stream_key = tokens[4], id = tokens[5];

            unique_lock<mutex> lock(mtxMap[stream_key]);
            streamQueue.push({ client_fd, stream_key });

            bool timed_out = false;
            if (wait_time == 0) {
                timed_out = !cvMap[stream_key].wait_for(lock, std::chrono::milliseconds(wait_time), [&]() {
                    return streamQueue.empty();
                    });
            }

            if (timed_out)
                response = "$-1\r\n";

            if (wait_time == 0) {
                cvMap[stream_key].wait(lock, [&]() {
                    return streamQueue.empty();
                    });
            }

            if (!streamMap[stream_key].empty()) {
                string recent_key = sendToBlocked[client_fd];
                const auto& last_entry = streamMap[recent_key].back();
                response = "*1\r\n*2\r\n" + resp_bulk_string(stream_key) + "*1\r\n" + parse_entry(last_entry);
            }

            cout << "response: " << response << "\n";
            return;
        }

        int count = 0;
        int keys = (tokens.size() - 2) / 2;

        vector<pair<string, string>> streams;
        queue<string> list_keys;
        for (int i = 2;i < tokens.size();i++) {
            if (count < keys) {
                list_keys.push(tokens[i]);
                count++;
            }
            else {
                string key = list_keys.front(); list_keys.pop();
                streams.push_back({ key, tokens[i] });
            }
        }

        // build the response
        response = parse_streams(streams, streamMap);
    }
    else {
        cerr << "Unknown command: " << tokens[0] << "\n";
        close(client_fd);
    }
}


// Command line
void parse_redis_command(char* buffer, int client_fd) {
    string request(buffer);
    string response = "";

    vector<string> tokens = parse_resp_array(request);
    if (tokens.empty()) {
        cerr << "Invalid request format\n";
        close(client_fd);
        return;
    }
    if (tokens[0] == "DISCARD") {

        if (isMultiQueued[client_fd]) {
            // clear the queue
            while (!multiQueue[client_fd].empty()) {
                multiQueue[client_fd].pop();
            }

            response = "+OK\r\n";
            isMultiQueued[client_fd] = false;
        }
        else {
            response = "-ERR DISCARD without MULTI\r\n";
        }

    }
    else if (tokens[0] == "EXEC") {
        if (!isMultiQueued[client_fd]) {
            response = "-ERR EXEC without MULTI\r\n";
        }
        else if (multiQueue[client_fd].empty()) { // called MULTI but too soon
            response = "*0\r\n";
            isMultiQueued[client_fd] = false;
        }
        else { // clear the queue
            vector<string> results;

            while (!multiQueue[client_fd].empty()) {
                string nestedResponse = "";

                string cmd = multiQueue[client_fd].front();
                multiQueue[client_fd].pop();

                vector<string> nestedTokens = parse_resp_array(cmd);

                // call function
                redisCommands(nestedTokens, client_fd, nestedResponse);
                results.push_back(nestedResponse);

                // send to server
                //send(client_fd, nestedResponse.c_str(), nestedResponse.size(), 0);
            }
            isMultiQueued[client_fd] = false;

            response = "*" + to_string(results.size()) + "\r\n";
            for (auto& r : results)
                response += r;
        }
    }
    else if (isMultiQueued[client_fd]) {
        multiQueue[client_fd].push(request);
        response = "+QUEUED\r\n";
    }
    else {
        redisCommands(tokens, client_fd, response);
    }

    //cout << "response: " << response << "\n";

    send(client_fd, response.c_str(), response.size(), 0);
    return;
}
