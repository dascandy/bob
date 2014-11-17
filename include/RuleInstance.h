#ifndef RULEINSTANCE_H
#define RULEINSTANCE_H

#include <chrono>
#include <cstddef>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <mutex>
#include <ctime>

class Rule;
struct File;

enum Relation { None, BuildBefore, IndirectInput, Input, GeneratingInput };

struct RuleInstance {
  RuleInstance(Rule *rule) 
  : rule(rule)
  , mainOutput(NULL)
  , wantToRun(false)
  , somethingToDo(false)
  , storedRv(-1)
  , runningAverageTimeTaken(0)
  , runCount(0)
  {
  }
  Rule *rule;
  std::time_t getOldestOutput();
  std::unordered_map<File*, Relation> inputs;
  File* mainOutput;
  std::unordered_set<File*> outputs;
  std::unordered_set<File*> cacheOutputs;
  std::string command;
  bool wantToRun;
  bool somethingToDo;
  void Invalidate();
  int storedRv;
  std::chrono::nanoseconds runningAverageTimeTaken;
  size_t runCount;
  bool CanRun();
  bool Run(std::mutex&);
  void Check();
};

#endif


