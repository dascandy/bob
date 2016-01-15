#include "Funcs.h"
#include "RuleInstance.h"
#include "File.h"
#include "Test.h"
#include "Rule.h"
#include <boost/filesystem/fstream.hpp>
#include <thread>
#include "re2/set.h"
#include "Profile.h"
static const int BOB_VERSION = 3;

static const RE2::Options &getopts() {
  static RE2::Options opts;
#ifndef WIN32
  opts.set_never_capture(true);
#endif
  opts.set_one_line(true);
  return opts;
}

std::unordered_set<RuleInstance*> runnable;
std::mutex runnableM;
RE2::Set depfiles(getopts(), RE2::ANCHOR_BOTH), generateds(getopts(), RE2::ANCHOR_BOTH);
std::unordered_map<std::string, std::string> vars;
std::string target = "all";
bool clean = false;
bool dryrun = false;
bool verbose = false;
bool help = false;
bool version = false;
bool deamon = false;
bool client = false;
bool testrun = false;
size_t workerCount = std::thread::hardware_concurrency() + 1;
const std::chrono::milliseconds onemillisec( 1 );

struct entry {
  int32_t lastBuildResult;
  int32_t runCount;
  uint64_t timeTaken;
  char name[0];
};

void LoadCache(const std::string &fileName, std::unordered_map<std::string, File *> &fileMap) {
  boost::system::error_code error;
  size_t fileSize = boost::filesystem::file_size(fileName, error);
  if (error) return;
  char *buffer = new char[fileSize];
  {
    boost::filesystem::ifstream fd(fileName);
    fd.read(buffer, fileSize);
  }
  entry *ent = (entry *)buffer, *end = (entry *)(buffer+fileSize - sizeof(entry));
  while (ent < end) {
    if (fileMap.find(ent->name) != fileMap.end()) {
      File *f = fileMap[ent->name];
      if (f->generatingRule) {
        RuleInstance *r = f->generatingRule;
        r->storedRv = ent->lastBuildResult;
        r->runningAverageTimeTaken = std::chrono::nanoseconds(ent->timeTaken);
        r->runCount = ent->runCount;
      }
    }
    ent = (entry *)(ent->name + strlen(ent->name) + 1);
  }
}

void StoreCache(const std::string &fileName, std::unordered_map<std::string, File *> &fileMap) {
  char buffer[2048];
  entry *ent = (entry *)buffer;
  boost::filesystem::ofstream fd(fileName);
  for (const auto &p : fileMap) {
    if (p.second->generatingRule) {
      RuleInstance *r = p.second->generatingRule;
      ent->runCount = r->runCount;
      ent->timeTaken = r->runningAverageTimeTaken.count();
      ent->lastBuildResult = r->storedRv;
      strcpy(ent->name, p.second->path.c_str());
      fd.write(buffer, sizeof(entry) + strlen(ent->name) + 1);
    }
  }
}

void runtests() {
  size_t tests = 0, failures = 0;
  for (basetest *test = basetest::head(); test; test = test->next) {
    try {
      tests++;
      test->runtest();
    } catch (std::exception&e) {
      failures++;
      printf("Test %s failed\n%s\n", test->name, e.what());
    }
  }
  printf("%lu failures in %lu tests\n", failures, tests);
}

int main(int, char **argv) {
  std::vector<Rule *> rules;
  std::vector<File *> files;
  std::unordered_map<std::string, File *> fileMap(524287);
  std::vector<RuleInstance *> instances;
  RE2::Set ruleset(getopts(), RE2::ANCHOR_BOTH);

  {
    PROFILE(Initial);
    char **arg = argv+1;
    while (*arg) {
      if (std::string(*arg) == "dryrun") {
        dryrun = true;
      } else if (std::string(*arg) == "testrun") {
        testrun = true;
      } else if (std::string(*arg) == "clean") {
        clean = true;
      } else if (std::string(*arg) == "verbose") {
        verbose = true;
      } else if (std::string(*arg) == "--help") {
        help = true;
      } else if (std::string(*arg) == "--version") {
        version = true;
      } else if (std::string(*arg).substr(0,2) == "-j") {
        workerCount = atoi(*arg + 2);
      } else if (std::string(*arg) == "-c") {
        client = true;
      } else if (std::string(*arg) == "-d") {
        deamon = true;
      } else {
        if (target == "all")
          target = *arg;
        else 
          target += std::string(" ") + *arg;
      }
      arg++;
    }

    if (testrun) {
      runtests();
    }

    if (version) {
      printf("BOB version %d\n", BOB_VERSION);
      exit(0);
    } else if (help) {
      puts("Usage: bob [-d/-c] [<target>] [--version] [--help] [dryrun] [clean] [verbose]");
      puts("  --version    print version information");
      puts("  --help       print this help information");
      puts("  dryrun       do not actually run any commands, but print what commands would be run instead");
      puts("  clean        remove any generated output");
      puts("  verbose      print profiling information and any commands that are run");
      puts("  -d           run as daemon (background task) that keeps the given targets up to date");
      puts("  -c           run as client that requests output for a given target from an already-running daemon");
      puts("  -jN          run with N parallel jobs at the same time. Defaults to CPU core count plus one");
      exit(0);
    }
  }
  {
    PROFILE(reading rule file)
    if (!readRuleFile(rules, fileMap, files)) {
      printf("Cannot find Rulefile or bobfile in any parent directory, stopping...\n");
      exit(-1);
    }
  }
  // Always read rule file first before finding files, as the rule file location determines the root of the build
  {
    PROFILE(reading files)
    getFiles(files);
  }
  {
    PROFILE(creating file map)
    for (File *f : files) { 
      fileMap[f->path] = f;
    }
  }
  std::vector<std::pair<std::string, std::vector<Rule *>>> ruleMap;
  {
    size_t rc = 0, ris = 0;
    PROFILE(precompiling regex set)
    for (Rule *r : rules) {
      rc++;
      bool found = false;
      for (auto &p : ruleMap) {
        if (p.first == r->simpleMatcherString) {
          p.second.push_back(r);
          found = true;
          break;
        }
      }
      if (!found) {
        ruleMap.push_back(std::pair<std::string, std::vector<Rule *>>(r->simpleMatcherString, std::vector<Rule *>()));
        ruleMap[ruleMap.size()-1].second.push_back(r);
        ruleset.Add(r->simpleMatcherString, NULL);
        ris++;
      }
    }
    if (verbose) printf("PROFILE: %lu rules, %lu unique regexes\n", rc, ris);
    ruleset.Compile();
  }
  {
    PROFILE(matching rules)
    RE2::Arg argv[10];
    const RE2::Arg* args[10] = {&argv[0], &argv[1], &argv[2], &argv[3], &argv[4], &argv[5], &argv[6], &argv[7], &argv[8], &argv[9]};
    std::string arg[10];
    for (size_t i = 0; i < 10; i++) {
      argv[i] = &arg[i];
    }
    size_t fc = 0, rcm = 0;
    while (true) {
      fc++;
      File *f;
      if (files.empty()) break;
      f = files.back();
      files.pop_back();
      std::vector<int> matchingRules;
      if (ruleset.Match(f->path, &matchingRules)) {
        for (int match : matchingRules) {
          std::vector<Rule *> &mrules = ruleMap[match].second;
          rcm++;
          if (!RE2::FullMatchN(f->path, mrules.front()->inputMatcher, args, std::min(10, mrules.front()->inputMatcher.NumberOfCapturingGroups())))
            continue;

          for (Rule *r : mrules) {
            r->Match(f, instances, fileMap, files, arg);
          }
        }
      }
    }
    if (verbose) printf("PROFILE: tried %lu files to match, %lu regex evaluations\n", fc, rcm);
  }
  // Load dependencies after matching the rules, as only dependencies for valid targets are taken into account
  {
    PROFILE(loading dependency files)
      depfiles.Compile();
      for (auto p : fileMap) {
        loadDependenciesFrom(p.second->path, rules, fileMap, files);
      }
  }
  // prune files that are irrelevant for building or stale
  {
    PROFILE(pruning irrelevant files)
      generateds.Compile();
      for (auto it = fileMap.begin(); it != fileMap.end();) {
        if (it->second->generatingRule == NULL) {
          std::vector<int> v;
          if (generateds.Match(it->second->path, &v)) {
            // File is an input, but should have been generated and the source responsible for doing so is now gone
            if (verbose) printf("Found stale generated output %s that does not have a generating source\n", it->second->path.c_str());
            for (auto &dep : it->second->dependencies) {
              dep->inputs.erase(it->second);
            }
            if (!dryrun)
              boost::filesystem::remove(it->second->path);
            delete it->second;
            it = fileMap.erase(it);
          } else if (it->second->dependencies.empty()) {
            // File is not an input or output
            delete it->second;
            it = fileMap.erase(it);
          } else if (!boost::filesystem::is_regular_file(it->second->path)) {
            // File is an input (of sorts), but does not exist and won't be generated
            // Do not print log typically, because dependency files get stale occasionally and this results in scary logging that's not relevant
            if (verbose) printf("Found non-existant file %s\nrequired to build %s\n", it->second->path.c_str(), it->second->dependencies[0]->mainOutput->path.c_str());
            ++it;
          } else {
            // File is just input
            ++it;
          }
        } else {
          ++it;
        }
      }
  }
  {
    PROFILE(Loading previous run info)
    LoadCache(".bob.cache", fileMap);
  }
  bool anyFail = false;
  if (clean) {
    PROFILE(running clean)
    for (auto p : fileMap) {
      if (p.second->generatingRule && 
          boost::filesystem::is_regular_file(p.second->path)) {
        if (dryrun || verbose)
          printf("rm %s\n", p.second->path.c_str());
        if (!dryrun)
          boost::filesystem::remove(p.second->path);
      }
    }
  } else {
    {
      PROFILE(determining what to build)
      if (target.empty())
        target = "all";

      std::vector<std::string> targets = split(target, ' ');
      std::vector<RuleInstance *> toCheck;
      for (auto t : targets) {
        File *f = fileMap[t];
        if (!f) {
          printf("Invalid target specified: %s\n", t.c_str());
          return 1;
        }
        if (f->generatingRule)
          toCheck.push_back(f->generatingRule);
      }
      while (!toCheck.empty()) {
        RuleInstance *r = toCheck.back();
        toCheck.pop_back();
        if (r->wantToRun) 
          continue; // avoid double-checking things; this also breaks loops
        r->Check();
        r->wantToRun = true;
        for (const auto &p : r->inputs)
          if (p.first->generatingRule) 
            toCheck.push_back(p.first->generatingRule);
      }
    }
    {
      PROFILE(spawning workers and building)
      for (RuleInstance *r : instances) {
        if (r->CanRun()) runnable.insert(r);
      }
      std::vector<std::thread*> workers;
      std::mutex outputMutex;
      size_t workersIdle = workerCount;
      bool shouldStop = false;
      for (size_t i = 0; i < workerCount; ++i) {
        workers.push_back(new std::thread([&anyFail, &outputMutex, &shouldStop, &workersIdle]{
          RuleInstance *r = NULL;
          while (!shouldStop) {
            {
              std::lock_guard<std::mutex> lock(runnableM);
              if (runnable.empty()) {
                if (r) workersIdle++;
                r = NULL;
              } else {
                if (!r) workersIdle--;
                r = *runnable.begin();
                runnable.erase(runnable.begin());
              }
            }
            if (r) {
              bool fail = r->Run(outputMutex);
              if (fail && !anyFail) {
                anyFail = true;
                printf("Failing build because building %s failed\n", r->mainOutput->path.c_str());
              }
            } else {
              std::this_thread::sleep_for( onemillisec );
            }
          }
        }));
      }
      std::this_thread::sleep_for( onemillisec );
      while (workersIdle != workers.size()) {
        std::this_thread::sleep_for( onemillisec );
      }
      shouldStop = true;
      for (auto &t : workers) {
        t->join();
        delete t;
      }
    }
  }
  {
    PROFILE(storing info for next run)
    StoreCache(".bob.cache", fileMap);
  }
  {
    PROFILE(build complete)
  }
  return anyFail;
}

