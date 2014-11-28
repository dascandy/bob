#include "Funcs.h"
#include <algorithm>

std::string replaceVars(const std::string &arg, const std::unordered_map<std::string, std::string> &instancedVars) {
  size_t pos = arg.find("$(");
  if (pos == arg.npos)
    return arg;

  size_t posEndBrace = find_end_brace_balanced(arg, pos+2);
  if (posEndBrace == arg.npos) {
    printf("No closing brace found in %s\n", arg.c_str());
    throw 1;
  }
  std::string outBase = arg.substr(0, pos),
              outEnd = arg.substr(posEndBrace),
              outMiddleI = arg.substr(pos+2, posEndBrace-pos-3);
  size_t posSpace = outMiddleI.find_first_of(" ");
  if (posSpace == outMiddleI.npos ||
      outMiddleI.find_first_not_of(" ", posSpace) == outMiddleI.npos) {
    std::string outMiddle = replaceVars(outMiddleI, instancedVars);
    if (instancedVars.find(outMiddle) != instancedVars.end()) {
      auto r = *instancedVars.find(outMiddle);
      return outBase + replaceVars(r.second + outEnd, instancedVars);
    } else if (vars.find(outMiddle) != vars.end()) {
      return outBase + replaceVars(vars[outMiddle] + outEnd, instancedVars);
    } else {
      printf("Used variable that's not defined: %s\n", outMiddle.c_str());
      printf("      in fixing up %s\n", arg.c_str());
      throw 1;
    }
  } else {
    std::string function = outMiddleI.substr(0, posSpace);
    std::string argV = outMiddleI.substr(posSpace+1);
    if (function == "sub") {
      size_t pos1 = argV.find(",");
      size_t pos2 = argV.find(",", pos1+1);
      RE2 pattern(argV.substr(0, pos1));
      std::string target = argV.substr(pos1+1, pos2-pos1-1);
      std::string data = replaceVars(argV.substr(pos2+1), instancedVars);
      while (RE2::Replace(&data, pattern, target)) { }
      return outBase + data + replaceVars(outEnd, instancedVars);
    } else if (function == "filter") {
      size_t pos = argV.find(",");
      RE2 pattern(argV.substr(0, pos));
      std::vector<std::string> items = split(replaceVars(argV.substr(pos+1), instancedVars), ' ');
      std::string value;
      for (const auto &item : items) {
        if (!RE2::FullMatch(item, pattern))
          value += " " + item;
      }
      return outBase + value + replaceVars(outEnd, instancedVars);
    } else if (function == "subst" ||
               function == "rep_subst") {
      size_t pos1 = argV.find(",");
      size_t pos2 = argV.find(",", pos1+1);
      RE2 pattern(argV.substr(0, pos1));
      std::string target = argV.substr(pos1+1, pos2-pos1-1);
      std::string data = replaceVars(argV.substr(pos2+1), instancedVars);
      bool repeat = (function == "rep_subst");
      std::vector<std::string> items = split(data, ' ');
      do {
        std::vector<std::string> newItems;
        for (const auto &item : items) {
          auto repl = replaceVars(replace_with_pattern(item, pattern, target), instancedVars);
          std::vector<std::string> afterReplace = split(repl, ' ');
          for (auto it : afterReplace) {
            newItems.push_back(it);
          }
        }
        if (repeat) {
          size_t inputItems = items.size();
          items.clear();
          std::reverse(newItems.begin(), newItems.end());
          std::unordered_set<std::string> alreadyFound;
          for (const auto &s : newItems) {
            if (alreadyFound.find(s) == alreadyFound.end()) {
              items.push_back(s);
              alreadyFound.insert(s);
            }
          }
          std::reverse(items.begin(), items.end());
          if (items.size() == inputItems) {
            // TODO: this does not work quite entirely for non-cyclic dependencies, where this would stabilize but after this point.
            // To fix still; this doesn't help the cyclic case though and badly hurts performance for them.
            break;
          }
        } else {
          swap(items, newItems);
        }
      } while (repeat);
      data = "";
      for (const auto &i : items) {
        data += i + " ";
      }
      return outBase + data + replaceVars(outEnd, instancedVars);
    } else {
      printf("Unknown function: %s\n", function.c_str());
      throw 1;
    }
  }
}

