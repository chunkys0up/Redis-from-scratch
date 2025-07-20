#ifndef REDIS_PARSER_HPP
#define REDIS_PARSER_HPP

#include <string>

// encode into redis
std::string resp_bulk_string(const std::string& str);

// check for ping, if not then check for echo command
void parse_redis_command(char *buffer, ssize_t buffer_size, int client_fd);

#endif