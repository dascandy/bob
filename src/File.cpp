#include "File.h"
#include <boost/filesystem/fstream.hpp>
#include "Rule.h"
#include "Funcs.h"
#include "re2/set.h"
#include "RuleInstance.h"

File* create_file(const std::string &fileName, std::unordered_map<std::string, File*> &fileMap, std::vector<File *>& files) {
  File *&file = fileMap[fileName];
  if (!file) {
    file = new File(fileName);
    files.push_back(file);
  }
  return file;
}

static void instantiateRule(const std::string &inputRegex, const std::string &inputLine, const std::string &outputLine, const std::string &buffer, std::unordered_map<std::string, std::string> &localVars, const std::vector<std::pair<std::string, std::vector<std::string>>> &args, std::vector<Rule *> &rules) {
  if (args.empty()) {
    std::unordered_map<std::string, std::string> escapedVars;
    for (auto p : localVars) {
      escapedVars[p.first] = RE2::QuoteMeta(p.second);
    }
    rules.push_back(new Rule(replaceVars(inputRegex, escapedVars), inputLine, outputLine, buffer, localVars));
  } else {
    auto argc = args;
    auto p = argc.back();
    argc.pop_back();
    for (auto s : p.second) {
      localVars[p.first] = s;
      instantiateRule(inputRegex, inputLine, outputLine, buffer, localVars, argc, rules);
    }
  }
}

void readFile(std::vector<Rule *> &rules, const std::string &path, std::unordered_map<std::string, File *> &fileMap, std::vector<File *>& files) {
  boost::filesystem::ifstream in(path);
  static char buffer[262144];
  std::vector<std::pair<std::string, std::vector<std::string>>> args;
  while (in.good()) {
    in.getline(buffer, 262144);
    while (in.good() && buffer[strlen(buffer)-1] == '\\') {
      in.getline(buffer+strlen(buffer) - 1, 262144-strlen(buffer) + 1);
    }
    char *firstHash = strchr(buffer, '#');
    size_t endPos = (firstHash ? firstHash - buffer : strlen(buffer));
    std::string line(buffer, endPos);
    size_t off;
    if ((off = line.find(" => ")) != line.npos) {
      std::string inputRegex = line.substr(0, off), 
                  inputLine = "",
                  outputLine = line.substr(off+4);
      size_t endPos = inputRegex.find_first_of(" ");
      if (endPos != inputRegex.npos) {
        inputLine = inputRegex.substr(endPos+1);
        inputRegex = inputRegex.substr(0, endPos);
      }
      in.getline(buffer, 1024);
      std::unordered_map<std::string, std::string> localVars;
      instantiateRule(inputRegex, inputLine, outputLine, buffer, localVars, args, rules);
    } else if (line.substr(0, 8) == "depfiles") {
      for (const auto& str : split(line.substr(9), ' ')) {
        depfiles.Add(str, NULL);
      }
    } else if (line.substr(0, 9) == "generated") {
      for (const auto& str : split(line.substr(10), ' ')) {
        generateds.Add(str, NULL);
      }
    } else if (line.substr(0, 7) == "include") {
      readFile(rules, line.substr(8), fileMap, files);
    } else if (line.substr(0, 4) == "each") {
      std::string l = line.substr(5);
      size_t colonPos = l.find_first_of(':');
      args.push_back(std::make_pair(l.substr(0, colonPos), split(l.substr(colonPos+1), ' ')));
    } else if (line == "endeach") {
      args.pop_back();
    } else if (line.find("=") != line.npos) {
      std::string name = line.substr(0, line.find("="));
      std::string value = line.substr(line.find("=")+1);
      vars[name] = value;
    } else if (line.find(":") != line.npos) {
      size_t pos = line.find(":");
      std::string outFile = line.substr(0, pos);
      File *f = create_file(outFile, fileMap, files);
      if (!f->generatingRule) {
        continue;
      }
      std::vector<std::string> deps = split(line.substr(pos+1), ' ');
      for (auto dep : deps) {
        File *depfile = create_file(dep, fileMap, files);
        if (f->generatingRule->inputs[depfile] == None)
        {
          f->generatingRule->inputs[depfile] = IndirectInput;
          depfile->dependencies.push_back(f->generatingRule);
        }
      }
    }
  }
}

bool readRuleFile(std::vector<Rule *> &rules, std::unordered_map<std::string, File *> &fileMap, std::vector<File *>& files) {
  boost::filesystem::path current = boost::filesystem::current_path();
  while (!current.empty()) {
    boost::filesystem::path rulefile = current / "Rulefile.bob",
                            bobfile = current / current.filename().replace_extension(".bob"),
                            simpleRulefile = current / "Rulefile",
                            targetfile = current / "target.bob";
    if (boost::filesystem::is_regular_file(targetfile) && 
        target == "all") {
      boost::filesystem::ifstream in(targetfile);
      char targetname[1024];
      in.getline(targetname, 1024);
      target = targetname;
      if (verbose)
        printf("Using target override %s\n", targetname);
    }

    if (boost::filesystem::is_regular_file(rulefile)) {
      boost::filesystem::current_path(current);
      readFile(rules, rulefile.string(), fileMap, files); 
      return true;
    } else if (boost::filesystem::is_regular_file(bobfile)) {
      boost::filesystem::current_path(current);
      readFile(rules, bobfile.string(), fileMap, files); 
      return true;
    } else if (boost::filesystem::is_regular_file(simpleRulefile)) {
      boost::filesystem::current_path(current);
      readFile(rules, simpleRulefile.string(), fileMap, files); 
      return true;
    }
    current = current.parent_path();
  }
  return false;
}

void loadDependenciesFrom(std::string &file, std::vector<Rule*> &rules, std::unordered_map<std::string, File *> &fileMap, std::vector<File *> &files) {
  if (!boost::filesystem::is_regular_file(file)) return;
  std::vector<int> m;
  if (!depfiles.Match(file, &m)) {
    return;
  }
  readFile(rules, file, fileMap, files);
}

void getFiles(std::vector<File *> &files) {
  boost::filesystem::recursive_directory_iterator it("."), end;
  for (;it != end; ++it) {
    if (boost::filesystem::is_regular_file(it->path())) {
      files.push_back(new File(it->path().string().substr(2)));
    }
  }
}

void File::Invalidate() {
  if (!shouldRebuild) {
    shouldRebuild = true;
    for (RuleInstance *d : dependencies) {
      d->Invalidate();
    }
  }
}

void File::SignalRebuilt() {
  if (shouldRebuild) {
    shouldRebuild = false;
    for (RuleInstance *d : dependencies) {
      if (d->CanRun()) {
        runnable.insert(d);
      }
    }
  }
}

