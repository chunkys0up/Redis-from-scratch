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

string resp_bulk_string(const string& data) {
    return "$" + to_string(data.size()) + "\r\n" + data + "\r\n";
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
        std::cerr << "Invalid request format\n";
        close(client_fd);
        return;
    }

    // check if ping
    if (tokens[0] == "PING") {
        response = "+PONG\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    // now check for echo (echo can only have 2 tokens, 1: echo, 2: msg)
    if (tokens.size() == 2 && to_lower(tokens[0]) == "echo") {
        response = resp_bulk_string(tokens[1]);

        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    // check for SET
    if (tokens[0] == "SET") {
        string key = tokens[1], value = tokens[2];

        redisMap[key] = value;
        response = "+OK\r\n";

        // check if there's an expiry time
        if ((tokens.size() == 4) && (to_lower(tokens[3]) == "px")) {
            int time = std::stoi(tokens[4]);
            expiryMap[key] = steady_clock::now() + milliseconds(time);
        }

        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    // check for GET
    if (tokens[0] == "GET") {
        string key = tokens[1];

        if (redisMap.find(key) != redisMap.end()) {
            string data = redisMap[key];

            // check if time now is > expiry time (is expired)
            if (expiryMap.find(key) != expiryMap.end() && steady_clock::now() > expiryMap[key]) {
                redisMap.erase(key);
                expiryMap.erase(key);
            }
            
            // check if the key still exists after possible expiry
            if(redisMap.find(key) != redisMap.end())
                response = resp_bulk_string(redisMap[key]);
            else    
                response = "$-1\r\n";
        }

        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }


    std::cerr << "Uknown command: " << tokens[0] << "\n";
    close(client_fd);
}