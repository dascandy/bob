#include "RuleInstance.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "File.h"
#include "Funcs.h"
#include "Rule.h"
#include <unistd.h>
#include <errno.h>

bool Comparer::operator()(RuleInstance* first, RuleInstance* second) {
  return first->GetDelay() < second->GetDelay();
}

uint64_t RuleInstance::GetDelay() const {
  if (cachedDelay == 0) {
    cachedDelay = runningAverageTimeTaken.count(); // Only for if there's a cycle
    uint64_t curDelay = 0;
    for (auto& out : outputs) {
      for (auto& dep : out->dependencies) {
        if (dep->wantToRun && dep->somethingToDo)
          if (dep->GetDelay() > curDelay) curDelay = dep->GetDelay();
      }
    }
    cachedDelay = curDelay + runningAverageTimeTaken.count();
  }
  return cachedDelay;
}

void RuleInstance::Invalidate() {
  somethingToDo = true;
  mainOutput->Invalidate();
  for (File *f : outputs) {
    f->Invalidate();
  }
}

void RuleInstance::Check() {
  if (verbose) printf("check for %s ?\n", mainOutput->path.c_str());
  if (command.empty()) {
    if (verbose) printf("Rebuilding; pseudotarget\n");
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
    return;
  }

  if (storedRv) {
    if (verbose) printf("last build was not successful; have to rebuild\n");
    Invalidate();
    return;
  }

  if (verbose) printf("not rebuilding, all inputs up to date and no error on last run\n");
}

std::time_t RuleInstance::getOldestOutput() {
  std::time_t oldestOutput = 0;

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

static int execute_command(const std::string &cmd, const std::string &outfile = "") {
  int pid = fork();
  if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
  } else if (pid == 0) {
    if (outfile != "") {
      int fd = open(outfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0755);
      dup2(fd, 1);
      close(fd);
    }
    close(0);
    dup2(1, 2);
    execlp("bash", "bash", "-c", cmd.c_str(), 0);
    exit(-1);
  } else {
    printf("fork fail\n");
    return -1;
  }
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
    std::string out;
    for (const auto &p : outputs) {
      out += " " + p->path;
    }
    vars["OUTPUTS"] = out;
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
    } else if (somethingToDo) {
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
        for (File *f : outputs) {
          if (!boost::filesystem::is_regular_file(f->path)) {
            printf("Rule did not result in actual output file after successful run: %s => %s\n", in.c_str(), f->path.c_str());
          }
        }
        for (File *f : cacheOutputs) {
          if (!boost::filesystem::is_regular_file(f->path)) {
            printf("Rule did not result in actual output file after successful run: %s => %s\n", in.c_str(), f->path.c_str());
          }
        }
      } else if (boost::filesystem::is_regular_file(logFile) && !boost::filesystem::is_empty(logFile)) {
        printf("While building %s\n", mainOutput->path.c_str());
      }
      if (boost::filesystem::is_regular_file(logFile) && !boost::filesystem::is_empty(logFile)) {
        system(("cat " + logFile.string()).c_str());
      }
    }
    if (somethingToDo && rv == 0) {
      std::lock_guard<std::mutex> lock(runnableM);
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

