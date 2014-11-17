#ifndef RULE_H
#define RULE_H

#include <string>
#include <unordered_map>
#include "re2/re2.h"
#include <vector>

struct File;
struct RuleInstance;

class Rule {
public:
  Rule(const std::string& input, const std::string &inputLine, const std::string &outputLine, const std::string &command, const std::unordered_map<std::string, std::string> &localVars) 
  : inputMatcher(input)
  , simpleMatcherString(input)
  , inputLine(inputLine)
  , outputLine(outputLine)
  , command(command)
  , localVars(localVars)
  {
  }
  RE2 inputMatcher;
  std::string simpleMatcherString;
  std::string inputLine;
  std::string outputLine;
  std::string command;
  std::unordered_map<std::string, std::string> localVars;
  void Match(File *file, std::vector<RuleInstance*> &rules, std::unordered_map<std::string, File*>& fileMap, std::vector<File *>& files, std::string*);
};

#endif


