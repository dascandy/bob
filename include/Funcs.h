#ifndef FUNCS_H
#define FUNCS_H

#include <unordered_set>
#include <mutex>
#include "re2/re2.h"
#include <unordered_map>
#include <string>
#include <vector>

class RuleInstance;

extern std::unordered_set<RuleInstance*> runnable;
extern std::mutex runnableM;
extern RE2 *depfiles;
extern std::unordered_map<std::string, std::string> vars;
extern std::string target;
extern bool dryrun;
extern bool verbose;

std::string replaceVars(const std::string &arg, const std::unordered_map<std::string, std::string> &instancedVars);
std::string regexToNoMatching(const std::string &r);
std::vector<std::string> split(const std::string&str, char splitToken);
void replace_all(std::string &input, const char *toReplace, const std::string &replaceant);
std::string replace_matches(const std::string &input, const std::string* matches, char prefix, size_t count);
size_t find_end_brace_balanced(const std::string& arg, size_t pos);
std::string replace_with_pattern(const std::string& item, const RE2& pattern, const std::string& target);

#endif


