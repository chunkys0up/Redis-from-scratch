#include "cli_commands.hpp"
#include "../cli_helper/cli_helper.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <queue>
#include <chrono>
#include <mutex>
#include <condition_variable>

using namespace std;
using namespace std::chrono;

void handlePING(string& response) {
    response = "+PONG\r\n";
}

void handleECHO(const vector<string>& tokens, string& response) {
    response = resp_bulk_string(tokens[1]);
}

void handleSET(const vector<string>& tokens, string& response, unordered_map<string, string>& redisMap, unordered_map<string, steady_clock::time_point>& expiryMap) {
    string list_key = tokens[1], value = tokens[2];

    redisMap[list_key] = value;

    // check if there's an expiry time
    if ((tokens.size() == 5) && (to_lower(tokens[3]) == "px")) {
        int time = stoi(tokens[4]);
        expiryMap[list_key] = steady_clock::now() + milliseconds(time);
    }

    response = "+OK\r\n";
}

void handleGET(const vector<string>& tokens, string& response, unordered_map<string, string>& redisMap, unordered_map<string, steady_clock::time_point>& expiryMap) {
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

void handleRPUSH(const vector<string>& tokens, string& response, unordered_map< string, vector<string>>& listMap, unordered_map<string, queue<int>>& waitingClients) {
    string list_key = tokens[1];

    for (int i = 2;i < tokens.size();i++) {
        listMap[list_key].push_back(tokens[i]);
    }

    if (!waitingClients[list_key].empty()) {
        int cli_fd = waitingClients[list_key].front();
        waitingClients[list_key].pop();

        cvMap[list_key].notify_one();
    }

    // return number of elements in RESP Integer format
    response = ":" + to_string(listMap[list_key].size()) + "\r\n";
}

void handleLPUSH(const vector<string>& tokens, string& response, unordered_map< string, vector<string>>& listMap) {
    string list_key = tokens[1];

    for (int i = 2;i < tokens.size();i++) {
        listMap[list_key].insert(listMap[list_key].begin(), tokens[i]);
    }

    // return number of elements in RESP Integer format
    response = ":" + to_string(listMap[list_key].size()) + "\r\n";
}

void handleLLEN(const vector<string>& tokens, string& response, unordered_map<string, vector<string>>& listMap) {
    string list_key = tokens[1];

    // return number of elements in RESP Integer format
    response = ":" + to_string(listMap[list_key].size()) + "\r\n";
}

void handleLLRANGE(const vector<string>& tokens, string& response, unordered_map<string, vector<string>>& listMap) {
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

void handleLPOP(const vector<string>& tokens, string& response, unordered_map<string, vector<string>>& listMap) {
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

void handleBLPOP(const vector<string>& tokens, string& response, int client_fd, unordered_map<string, vector<string>>& listMap, unordered_map<string, condition_variable>& cvMap, unordered_map<string, mutex>& mtxMap, unordered_map<string, queue<int>>& waitingClients) {
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
    }
    else if (timed_out) {
        response = "$-1\r\n";
    }
    else {
        vector<string> res = { list_key, listMap[list_key][0] };
        listMap[list_key].erase(listMap[list_key].begin());
        response = lrange_bulk_string(res);
    }
}

void handleINCR(const vector<string>& tokens, string& response, unordered_map<string, string>& redisMap) {
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

