#include <iostream>
#include "redis_parser.hpp"
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <cctype>
#include <algorithm>

using namespace std;

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

    if(input[i] != '*') return result;

    // test with this command *2\r\n$4\r\nECHO\r\n$3\r\nhey\r\n

    // Expect '$4\r\nECHO\r\n'
    while(input[i] != '\n') i++;
    i++;

    // get multiple elements if needed
    while(i < input.size()) {
        if(input[i] != '$') break;
        i++;

        // parse length
        int len = 0;
        while(isdigit(input[i])) {
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

void parse_redis_command(char* buffer, int client_fd) {
    string request(buffer);

    vector<string> tokens = parse_resp_array(request);

    // check if echo
    if(strcmp(tokens[0], "PING") == 0) {
        string response = "+PONG\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    // now check for echo
    if(strcmp(to_lower(tokens[0], "echo") == 0)) {
        string response = "";
        for(const auto& token : tokens)
            response += resp_bulk_string(token);

        send(client_fd, response.c_str(), response.size(), 0);
        return;
    }

    std::cerr << "request doesn't contain `PING`: Client disconnecting\n";
    close(client_fd);
}