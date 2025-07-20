#ifndef REDIS_PARSER_HPP
#define REDIS_PARSER_HPP

#include <unordered_map>
#include <string>

using namespace std;
// for GET and SET commands
inline unordered_map<string, string> redisMap;

// check for ping, if not then check for echo command
void parse_redis_command(char *buffer, int client_fd);

#endif