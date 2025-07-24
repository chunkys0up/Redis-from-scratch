#include "cli_helper.hpp"
#include <vector>
#include <string>
#include <cctype>
#include <algorithm>
#include <utility>

using namespace std;

bool isAllDigits(const string& str) {
    if (str.empty())
        return false;

    for (auto& c : str) {
        if (!isdigit(c))
            return false;
    }

    return true;
}

string resp_bulk_string(const string& data) {
    return "$" + to_string(data.length()) + "\r\n" + data + "\r\n";
}

string lrange_bulk_string(const vector<string>& data) {
    string response = "*" + to_string(data.size()) + "\r\n";

    for (auto& element : data)
        response += "$" + to_string(element.length()) + "\r\n" + element + "\r\n";

    return response;
}

string to_lower(const string& token) {
    string res = token;
    transform(res.begin(), res.end(), res.begin(), ::tolower);
    return res;
}

vector<string> parse_resp_array(const string& input) {
    vector<string> result;
    size_t i = 0;

    if (input[i] != '*') return result;

    // test with this command *2\r\n$4\r\nECHO\r\n$3\r\nhey\r\n

    // Expect '$4\r\nECHO\r\n'
    while (input[i] != '\n') i++;
    i++;

    // get multiple elements if needed
    while (i < input.size()) {
        if (input[i] != '$') break;
        i++;

        // parse length
        int len = 0;
        while (isdigit(input[i])) {
            len = len * 10 + (input[i] - '0');
            i++;
        }

        // skip \r\n
        i += 2;

        //extract the string of length len
        string arg = input.substr(i, len);
        result.push_back(arg);

        // skip string + \r\n
        i += len + 2;
    }

    return result;
}

pair<string, string> parse_entry_id(const string& id) {
    int index;
    for (index = 0;index < id.length();index++) {
        if (id[index] == "-")
            break;
    }

    string millisecondsTime = id.substr(0, index);
    string sequenceNumber = id.substr(index + 1);

    pair<string, string> res = { millisecondsTime, sequenceNumber };
    return res;
}