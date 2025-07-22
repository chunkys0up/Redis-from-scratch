#include "redis_parser.hpp"
#include <iostream>
#include <string>

// sending from sockets & closing it
#include <sys/socket.h>
#include <unistd.h>

// to convert to lowercase
#include <cctype>
#include <algorithm>
#include <vector>

// for SET, GET, and expiry
#include <unordered_map>
#include <chrono>


using namespace std;
using namespace std::chrono;

unordered_map<string, string> redisMap;
unordered_map<string, steady_clock::time_point> expiryMap;

unordered_map<string, vector<string>> listMap;

unordered_map<string, vector<bool>> waitMap;
unordered_map<string, vector<string>> queueMap;
queue<int> clientQueue;


string resp_bulk_string(const string& data) {
    return "$" + to_string(data.size()) + "\r\n" + data + "\r\n";
}

string lrange_bulk_string(const vector<string>& data) {
    string response = "*" + to_string(data.size()) + "\r\n";

    for (auto& element : data)
        response += "$" + to_string(element.length()) + "\r\n" + element + "\r\n";

    return response;
}

string to_lower(const string& token) {
    string res = token;
    transform(res.begin(), res.end(), res.begin(), ::tolower);
    return res;
}

vector<string> parse_resp_array(const string& input) {
    vector<string> result;
    size_t i = 0;

    if (input[i] != '*') return result;

    // test with this command *2\r\n$4\r\nECHO\r\n$3\r\nhey\r\n

    // Expect '$4\r\nECHO\r\n'
    while (input[i] != '\n') i++;
    i++;

    // get multiple elements if needed
    while (i < input.size()) {
        if (input[i] != '$') break;
        i++;

        // parse length
        int len = 0;
        while (isdigit(input[i])) {
            len = len * 10 + (input[i] - '0');
            i++;
        }

        // skip \r\n
        i += 2;

        //extract the string of length len
        string arg = input.substr(i, len);
        result.push_back(arg);

        // skip string + \r\n
        i += len + 2;
    }

    return result;
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

    }
    else if (tokens[0] == "RPUSH" || tokens[0] == "LPUSH" || tokens[0] == "LLEN") {

        string list_key = tokens[1];

        for (int i = 2;i < tokens.size();i++) {
            if (tokens[0] == "RPUSH") {
                if (!waitMap[list_key].empty()) {
                    waitMap[list_key].erase(waitMap[list_key].begin());
                    queueMap[list_key].push_back(tokens[i]);
                }
                listMap[list_key].push_back(tokens[i]);

            }
            else
                listMap[list_key].insert(listMap[list_key].begin(), tokens[i]);
        }

        // return number of elements in RESP Integer format
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

            for (int i = 1;i <= stoi(tokens[2]) && i < listMap[list_key].size();i++) {
                res.push_back(listMap[list_key][0]);
                listMap[list_key].erase(listMap[list_key].begin());
            }
            response = lrange_bulk_string(res);
        }
    }
    else if (tokens[0] == "BLPOP") {
        string list_key = tokens[1];

        double wait_time = stod(tokens[2]);
        duration<double> wait_duration(wait_time);

        steady_clock::time_point end_time = steady_clock::now() + duration_cast<milliseconds>(wait_duration);
        bool indefiniteTime = (wait_time == 0) ? true : false;


        if (!listMap[list_key].empty()) {
            vector<string> res = { list_key, listMap[list_key][0] };
            listMap[list_key].erase(listMap[list_key].begin());

            response = lrange_bulk_string(res);
        }
        else {
            clientQueue.push(client_fd);
            waitMap[list_key].push_back(true);
            bool found = false;
            
            // keep in perpetual state until its time to continuously check for rpush from diff client
            while (clientQueue.front() != client_fd) {
                while (indefiniteTime || steady_clock::now() <= end_time) {
                    if (!waitMap[list_key][0]) {
                        found = true;

                        vector<string> res = { list_key, queueMap[list_key][0] };
                        queueMap[list_key].erase(queueMap[list_key].begin());

                        response = lrange_bulk_string(res);
                        break;
                    }
                }
            }



            if (!found)
                response = "$-1\r\n";
        }
    }
    else {
        cerr << "Unknown command: " << tokens[0] << "\n";
        close(client_fd);
    }

    send(client_fd, response.c_str(), response.size(), 0);
    return;
}