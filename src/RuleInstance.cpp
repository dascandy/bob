#include "RuleInstance.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "File.h"
#include "Funcs.h"
#include "Rule.h"

void RuleInstance::Invalidate() {
  somethingToDo = true;
  mainOutput->Invalidate();
  for (File *f : outputs) {
    f->Invalidate();
  }
}

void RuleInstance::Check() {
  if (verbose) printf("check %s ?\n", mainOutput->path.c_str());
  if (command.empty()) {
    if (verbose) printf("Rebuilding; pseudotarget\n");
    Invalidate();
    return;
  }
  if (!boost::filesystem::is_regular_file(mainOutput->path)) {
    if (verbose) printf("main output %s does not exist\n", mainOutput->path.c_str());
    Invalidate();
    return;
  }

  for (File *f : outputs) {
    if (!boost::filesystem::is_regular_file(f->path)) {
      Invalidate();
      if (verbose) printf("output %s does not exist\n", f->path.c_str());
      return;
    }
  }

  std::time_t youngestInput = 0;
  for (const auto &p : inputs) {
    if (p.second == BuildBefore) 
      continue;
    if (!boost::filesystem::is_regular_file(p.first->path)) {
      if (verbose) printf("input %s does not exist\n", p.first->path.c_str());
      Invalidate();
      return;
    }
    youngestInput = std::max(youngestInput, p.first->timestamp());
  }

  std::time_t oldestOutput = getOldestOutput();
  if (oldestOutput < youngestInput) {
    if (verbose) printf("oldest output is older than the newest input\n");
    Invalidate();
  }
}

std::time_t RuleInstance::getOldestOutput() {
  std::time_t oldestOutput = mainOutput->timestamp();

  for (File *f : outputs) {
    std::time_t t = f->timestamp();
    if (oldestOutput == 0 || (t != 0 && oldestOutput > t)) 
      oldestOutput = t;
  }
  return oldestOutput;
}

bool RuleInstance::CanRun() {
  if (!wantToRun) return false;
  for (const auto &p : inputs) {
    if (p.first->shouldRebuild) {
      return false;
    }
  }
  wantToRun = false;
  return true;
}

int execute_command(const std::string &cmd, const std::string &outfile) {
  return system(("(" + cmd + ") >" + outfile + " 2>&1").c_str());
}

bool RuleInstance::Run(std::mutex& m) {
  if (command.empty()) {
    // Allow for pseudotargets
    return false;
  }

  try {
    std::unordered_map<std::string, std::string> vars = rule->localVars;
    std::string cmd = command;
    vars["OUTPUT"] = mainOutput->path;
    std::string in = "";
    std::string inChanged = "";
    std::time_t oldestOutput = getOldestOutput();
    for (const auto &p : inputs) {
      if (p.second == GeneratingInput ||
          p.second == Input) {
        if (p.first->timestamp() > oldestOutput)
          inChanged += " " + p.first->path;
        in += " " + p.first->path;
      }
    }
    vars["INPUTS"] = in;
    vars["NEW_INPUTS"] = inChanged;

    replace_all(cmd, "$@", mainOutput->path);
    replace_all(cmd, "$^", in);

    cmd = replaceVars(cmd, vars);
    int rv;
    if (verbose && somethingToDo) {
      std::lock_guard<std::mutex> lock(m);
      printf("Building %s by running:\n%s\n", mainOutput->path.c_str(), cmd.c_str());
    } else if (dryrun && somethingToDo) {
      std::lock_guard<std::mutex> lock(m);
      printf("Building %s\n", mainOutput->path.c_str());
    }

    boost::filesystem::path logFile = boost::filesystem::path(mainOutput->path).parent_path() / (".out." + boost::filesystem::path(mainOutput->path).filename().string() + "._");
    if (dryrun) {
      rv = 0;
    } else if (somethingToDo || storedRv == -1) {
      boost::filesystem::path folder = boost::filesystem::path(mainOutput->path).parent_path();
      if (!folder.empty()) boost::filesystem::create_directories(folder);
      for (File *f : outputs) {
        boost::filesystem::path folder = boost::filesystem::path(f->path).parent_path();
        if (!folder.empty()) boost::filesystem::create_directories(folder);
      }
      for (File *f : cacheOutputs) {
        boost::filesystem::path folder = boost::filesystem::path(f->path).parent_path();
        if (!folder.empty()) boost::filesystem::create_directories(folder);
      }
      std::chrono::high_resolution_clock::time_point before = std::chrono::high_resolution_clock::now();
      storedRv = rv = execute_command(cmd, logFile.string());
      std::chrono::high_resolution_clock::time_point after = std::chrono::high_resolution_clock::now();
      if (runCount == 10) {
        runningAverageTimeTaken *= 0.9;
        runCount--;
      }
      runningAverageTimeTaken += (after - before);
      runCount++;
    } else {
      rv = storedRv;
      if (verbose) printf("using stored rv %d for %s\n", storedRv, mainOutput->path.c_str());
    }

    {
      std::lock_guard<std::mutex> lock(m);
      if (rv) {
        printf("Error %d building %s: \n", rv, mainOutput->path.c_str());
      } else if (verbose && somethingToDo) {
        printf("Built %s successfully\n", mainOutput->path.c_str());
      } else if (boost::filesystem::is_regular_file(logFile) && !boost::filesystem::is_empty(logFile)) {
        printf("While building %s\n", mainOutput->path.c_str());
      }
      if (boost::filesystem::is_regular_file(logFile) && !boost::filesystem::is_empty(logFile)) {
        system(("cat " + logFile.string()).c_str());
      }
    }
    if (somethingToDo && rv == 0) {
      std::lock_guard<std::mutex> lock(runnableM);
      mainOutput->SignalRebuilt();
      for (File *f : outputs) {
        f->SignalRebuilt();
      }
    }
    somethingToDo = false;
    return (rv != 0);
  } catch (int) { 
    return false;
  }
}

