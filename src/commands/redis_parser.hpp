#ifndef REDIS_PARSER_HPP
#define REDIS_PARSER_HPP

#pragma once

#include <unordered_map>
#include <string>
#include <chrono>

using namespace std;
using namespace std::chrono;

// Declare (inline makes it OK to include in multiple .cpp files)
extern unordered_map<string, string> redisMap;
extern unordered_map<string, steady_clock::time_point> expiryMap;

// check for ping, if not then check for echo command
void parse_redis_command(char* buffer, int client_fd);

#endif