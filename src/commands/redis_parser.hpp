#ifndef REDIS_PARSER_HPP
#define REDIS_PARSER_HPP

#include <unordered_map>
#include <string>

// for GET and SET commands
inline std::unordered_map<std::string, std::string> redisMap;

// check for ping, if not then check for echo command
void parse_redis_command(char *buffer, int client_fd);

#endif