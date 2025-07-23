#ifndef CLI_COMMANDS_HPP
#define CLI_COMMANDS_HPP

#include <vector>
#include <string>
#include <chrono>
#include <condition_variable>
#include <unordered_map>
#include <queue>
#include <mutex> // needed for std::mutex

void handlePING(std::string& response);

void handleECHO(const std::vector<std::string>& tokens, std::string& response);

void handleSET(const std::vector<std::string>& tokens, std::string& response, std::unordered_map<std::string, std::string>& redisMap, std::unordered_map<std::string, std::chrono::steady_clock::time_point>& expiryMap);

void handleGET(const std::vector<std::string>& tokens, std::string& response, std::unordered_map<std::string, std::string>& redisMap, std::unordered_map<std::string, std::chrono::steady_clock::time_point>& expiryMap);

void handleRPUSH(const std::vector<std::string>& tokens, std::string& response, std::unordered_map<std::string, std::condition_variable>& cvMap, std::unordered_map<std::string, std::vector<std::string>>& listMap, std::unordered_map<std::string, std::queue<int>>& waitingClients);

void handleLPUSH(const std::vector<std::string>& tokens, std::string& response, std::unordered_map<std::string, std::vector<std::string>>& listMap);

void handleLLEN(const std::vector<std::string>& tokens, std::string& response, std::unordered_map<std::string, std::vector<std::string>>& listMap);

void handleLLRANGE(const std::vector<std::string>& tokens, std::string& response, std::unordered_map<std::string, std::vector<std::string>>& listMap);

void handleLPOP(const std::vector<std::string>& tokens, std::string& response, std::unordered_map<std::string, std::vector<std::string>>& listMap);

void handleBLPOP(const std::vector<std::string>& tokens, std::string& response, int client_fd, std::unordered_map<std::string, std::vector<std::string>>& listMap, std::unordered_map<std::string, std::condition_variable>& cvMap, std::unordered_map<std::string, std::mutex>& mtxMap, std::unordered_map<std::string, std::queue<int>>& waitingClients);

void handleINCR(const std::vector<std::string>& tokens, std::string& response, std::unordered_map<std::string, std::string>& redisMap);

#endif
