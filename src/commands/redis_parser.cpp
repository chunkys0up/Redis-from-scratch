#include "redis_parser.hpp"
#include "cli_cmds/cli_commands.hpp"
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

#include <mutex>
#include <condition_variable>
#include <queue>

using namespace std;
using namespace std::chrono;

bool isMultiQueued = false;
queue<string> multiQueue;

unordered_map<string, string> redisMap;
unordered_map<string, steady_clock::time_point> expiryMap;

unordered_map<string, vector<string>> listMap;

unordered_map<string, condition_variable> cvMap;
unordered_map<string, mutex> mtxMap;
unordered_map<string, queue<int>> waitingClients;

void registeredCommands(const vector<string>& tokens, string& response, int client_fd) {
    if (tokens[0] == "PING") {
        handlePING(tokens, response);
    }
    else if (tokens[0] == "ECHO") {
        handleECHO(tokens, response);
    }
    else if (tokens[0] == "SET") {
        handleSET(tokens, response, redisMap, expiryMap);
    }
    else if (tokens[0] == "GET") {
        handleGET(tokens, respnose, redisMap, expiryMap);
    }
    else if (tokens[0] == "RPUSH") {
        handleRPUSH(tokens, response, listMap, waitingClients);
    }
    else if (tokens[0] == "LPUSH") {
        handleLPUSH(tokens, response, listMap);
    }
    else if (tokens[0] == "LLEN") {
        handleLLEN(tokens, response, listMap);
    }
    else if (tokens[0] == "LLRANGE") {
        handleLLRANGE(tokens, response, listMap);
    }
    else if (tokens[0] == "LPOP") {
        handleLPOP(tokens, response, listMap);
    }
    else if (tokens[0] == "BLPOP") {
        handleBLPOP(tokens, response, client_fd, listMap, cvMap, mtxMap, waitingClients);
    }
    else if (tokens[0] == "INCR") {
        handleINCR(tokens, response, redisMap);
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

        if (tokens[0] == "MULTI") {
            isMultiQueued = !isMultiQueued;
            response = "+OK\r\n";
        }
        else if (tokens[0] == "EXEC") {
            while (!multiQueue.empty()) {
                string req = multiQueue.front();
                multiQueue.pop();

                vector<string> cmdTokens = parse_resp_array(req);
                registeredCommands(cmdTokens, response, client_fd);
            }
        }
        else {
            registeredCommands(tokens, response, client_fd);
        }
    }

    send(client_fd, response.c_str(), response.size(), 0);
    return 0;
}