#include "redis_parser.hpp"
#include "redis_helper_cmds.hpp"

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

#include <mutex>
#include <condition_variable>
#include <queue>

using namespace std;
using namespace std::chrono;

bool isMultiQueued = false;
extern queue<string> multiQueue;

unordered_map<string, string> redisMap;
unordered_map<string, steady_clock::time_point> expiryMap;

unordered_map<string, vector<string>> listMap;
// unordered_map<string, vector<bool>> waitMap;
// queue<int> clientQueue;

unordered_map<string, condition_variable> cvMap;
unordered_map<string, mutex> mtxMap;
unordered_map<string, queue<int>> waitingClients;

// Command line
void parse_redis_command(char* buffer, int client_fd) {
    string request(buffer);
    string response = "";

    if (isMultiQueued) {
        multiQueue.push(request);
    }
    else {
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

            unique_lock<mutex> lock(mtxMap[list_key]);
            waitingClients[list_key].push(client_fd);

            bool timed_out = !cvMap[list_key].wait_for(lock, milliseconds((int)(wait_time * 1000)), [&]() {
                return !listMap[list_key].empty();
                });

            // if wait_time = 0, then it should be infinite
            if (wait_time == 0) {
                cvMap[list_key].wait(lock, [&]() {
                    return !listMap[list_key].empty();
                    });

                vector<string> res = { list_key, listMap[list_key][0] };
                listMap[list_key].erase(listMap[list_key].begin());
                response = lrange_bulk_string(res);

                cout << "Valid BLPOP\n";
            }
            else if (timed_out) {
                response = "$-1\r\n";
            }
            else {
                vector<string> res = { list_key, listMap[list_key][0] };
                listMap[list_key].erase(listMap[list_key].begin());
                response = lrange_bulk_string(res);

                cout << "Valid BLPOP\n";
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
        else if (tokens[0] == "MULTI") {
            isMultiQueued = !isMultiQueued;
            response = "+OK\r\n";
        }
        else {
            cerr << "Unknown command: " << tokens[0] << "\n";
            close(client_fd);
        }
    }

    send(client_fd, response.c_str(), response.size(), 0);
    return;
}