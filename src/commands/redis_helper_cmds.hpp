#ifndef REDIS_HELPER_CMDS_HPP
#define REDIS_HELPER_CMDS_HPP

#include <vector>

using namespace std;

bool isAllDigits(const string& str);

string resp_bulk_string(const string& data);

string lrange_bulk_string(const vector<string>& data);

string to_lower(const string& token);

vector<string> parse_resp_array(const string& input);


#endif