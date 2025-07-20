#ifndef REDIS_PARSER_HPP
#define REDIS_PARSER_HPP

#include <string>

// check for ping, if not then check for echo command
void parse_redis_command(char *buffer, int client_fd);

#endif