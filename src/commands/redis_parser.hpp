#ifndef REDIS_PARSER_HPP
#define REDIS_PARSER_HPP

#include <unordered_map>
#include <string>
#include <chrono>
#include <vector>
#include <queue>
#include <condition_variable>
#include<mutex>

using namespace std;
using namespace std::chrono;

extern unordered_map<string, string> redisMap;
extern unordered_map<string, steady_clock::time_point> expiryMap;

extern unordered_map<string, vector<string>> listMap;

//extern unordered_map<string, vector<bool>> waitMap;
//extern unordered_map<string, vector<string>> queueMap;
//extern queue<int> clientQueue;

unordered_map<string, condition_variable> cvMap;
unordered_map<string, mutex> mtxMap;
unordered_map<string, queue<int>> waitingClients;

// check for ping, if not then check for echo command
void parse_redis_command(char* buffer, int client_fd);

#endif