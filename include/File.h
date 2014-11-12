#ifndef FILE_H
#define FILE_H

#include <string>
#include <cstddef>
#include <boost/filesystem.hpp>
#include <vector>
#include <unordered_map>

class Rule;
class RuleInstance;

struct File {
  File(const std::string &path) 
  : path(path)
  , generatingRule(NULL)
  , shouldRebuild(false)
  , lastWrite(1) 
  {
  }
  void Invalidate();
  void SignalRebuilt();
  std::time_t timestamp() { 
    if (lastWrite == 1) {
      if (boost::filesystem::is_regular_file(path)) 
        lastWrite = boost::filesystem::last_write_time(path);
      else
        lastWrite = 0;
    }
    return lastWrite;
  }
  std::string path;
  RuleInstance *generatingRule;
  bool shouldRebuild;
  std::time_t lastWrite;
  std::vector<RuleInstance *> dependencies;
};

File* create_file(const std::string &fileName, std::unordered_map<std::string, File*> &fileMap, std::vector<File *>& files);
void readFile(std::vector<Rule *> &rules, const std::string &path, std::unordered_map<std::string, File *> &fileMap, std::vector<File *>& files);
bool readRuleFile(std::vector<Rule *> &rules, std::unordered_map<std::string, File *> &fileMap, std::vector<File *>& files);
void loadDependenciesFrom(std::string &file, std::vector<Rule*> &rules, std::unordered_map<std::string, File *> &fileMap, std::vector<File *> &files);
void getFiles(std::vector<File *> &files);

#endif


