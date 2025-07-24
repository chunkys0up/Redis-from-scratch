#ifndef CLI_HELPER_HPP
#define CLI_HELPER_HPP

#include <vector>
#include <string>
#include <utility>

using namespace std;

bool isAllDigits(const string& str);

string resp_bulk_string(const string& data);

string lrange_bulk_string(const vector<string>& data);

string to_lower(const string& token);

vector<string> parse_resp_array(const string& input);

pair<string, string> parse_entry_id(const string& id);

#endif