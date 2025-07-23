#ifndef CLI_COMMANDS_HPP
#define CLI_COMMANDS_HPP

#include <vector>
#include <string>
#include <chrono>
#include <condition_variable>
#include <unordered_map>

using namespace std;

void handlePING(string& response);

void handleECHO(const vector<string>& tokens, string& response);

void handleSET(const vector<string>& tokens, string& response, unordered_map<string, string>& redisMap, unordered_map<string, steady_clock::time_point>& expiryMap);

void handleGET(const vector<string>& tokens, string& response, unordered_map<string, string>& redisMap, unordered_map<string, steady_clock::time_point>& expiryMap);

void handleRPUSH(const vector<string>& tokens, string& response, unordered_map<string, vector<string>>& listMap, unordered_map<string, queue<int>>& waitingClients);

void handleLPUSH(const vector<string>& tokens, string& response, unordered_map< string, vector<string>>& listMap);

void handleLLEN(const vector<string>& tokens, string& response, unordered_map<string, vector<string>>& listMap);

void handleLLRANGE(const vector<string>& tokens, string& response, unordered_map<string, vector<string>>& listMap);

void handleLPOP(const vector<string>& tokens, string& response, unordered_map<string, vector<string>>& listMap);

void handleBLPOP(const vector<string>& tokens, string& response, int client_fd, unordered_map<string, vector<string>>& listMap, unordered_map<string, condition_variable>& cvMap, unordered_map<string, mutex>& mtxMap, unordered_map<string, queue<int>>& waitingClients);

void handleINCR(const vector<string>& tokens, string& response, unordered_map<string, string>& redisMap);




#endif