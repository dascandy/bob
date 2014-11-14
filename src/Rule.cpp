#include "Rule.h"
#include "File.h"
#include "RuleInstance.h"
#include "Funcs.h"

void Rule::Match(File *file, std::vector<RuleInstance*> &rules, std::unordered_map<std::string, File*> &fileMap, std::vector<File *>& files, std::string *arg)
{
  try {
    std::string outputLine = replaceVars(replace_matches(this->outputLine, arg, '\\', inputMatcher.NumberOfCapturingGroups()), localVars);
    std::vector<std::string> outFiles = split(outputLine, ' ');
    bool mainOutputIsOptional = (outFiles[0][0] == '[');
    if (mainOutputIsOptional) {
      outFiles[0] = outFiles[0].substr(1, outFiles[0].size() - 2);
    }
    File *mainOutputFile = create_file(outFiles[0], fileMap, files);
    RuleInstance *&rule = mainOutputFile->generatingRule;
    if (!rule) {
      rule = new RuleInstance(this);
      rules.push_back(rule);
      mainOutputFile->generatingRule = rule;
      rule->mainOutput = mainOutputFile;
      if (mainOutputIsOptional) {
        rule->cacheOutputs.insert(mainOutputFile);
      } else {
        rule->outputs.insert(mainOutputFile);
      }
      rule->command = replace_matches(command, arg, '\\', inputMatcher.NumberOfCapturingGroups());

      // If our build log is older than the output, it's not the result of that build. Better re-run it to make sure we don't give stale build output.
      File *logOutputFile = create_file(outFiles[0] + ".out", fileMap, files);
      logOutputFile->generatingRule = rule;
      rule->cacheOutputs.insert(logOutputFile);
    }
    for (size_t idx = 1; idx != outFiles.size(); ++idx) {
      if (outFiles[idx][0] == '[') {
        File *file = create_file(outFiles[idx].substr(1, outFiles[idx].size() - 2), fileMap, files);
        file->generatingRule = rule;
        rule->cacheOutputs.insert(file);
      } else {
        File *file = create_file(outFiles[idx], fileMap, files);
        file->generatingRule = rule;
        rule->outputs.insert(file);
      }
    }
    rule->inputs[file] = GeneratingInput;
    file->dependencies.push_back(rule);

    std::vector<std::string> inputs = split(replaceVars(replace_matches(this->inputLine, arg, '\\', inputMatcher.NumberOfCapturingGroups()), localVars), ' ');
    for (const auto &str : inputs) {
      File *file;
      if (str[0] == '[') {
        file = create_file(str.substr(1, str.size() - 2), fileMap, files);
        rule->inputs[file] = IndirectInput;
      } else if (str[0] == '<') {
        file = create_file(str.substr(1, str.size() - 2), fileMap, files);
        rule->inputs[file] = BuildBefore;
      } else {
        file = create_file(str, fileMap, files);
        rule->inputs[file] = Input;
      }
      file->dependencies.push_back(rule);
    }
  } catch (int) {}
}


