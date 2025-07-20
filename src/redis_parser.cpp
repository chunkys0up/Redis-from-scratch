#include <iostream>
#include "redis_parser.hpp"
#include <string>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

string resp_bulk_string(const string& data) {
    return "$" + to_string(data.size()) + "\r\n" + data + "\r\n";
}

void parse_redis_command(char* buffer, ssize_t buffer_size, int client_fd) {
    string request(buffer);

    cout << request << "\n";

    string copy = request;
    for (char& c : copy)
        c = tolower(static_cast<unsigned char>(c));

    if (copy.find("echo") != string::npos) {
        // isolate the text after
        int index = 0;
        string cap = "";
        while (cap.find("echo") == string::npos) {
            cap += copy[index];
            index++;
        }

        string response = resp_bulk_string(request.substr(index, request.length() - index));

        cout << "echo encode: " << response << "\n";

        send(client_fd, response.c_str(), response.size(), 0);
    }
    else if (copy.find("PING") != string::npos) {
        string response = "+PONG\r\n";
        send(client_fd, response.c_str(), response.size(), 0);
    }
    else {
        std::cerr << "request doesn't contain `PING`: Client disconnecting\n";
        close(client_fd);
        return;
    }


}


