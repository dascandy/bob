#include "Funcs.h"
#include <regex>
#include "Test.h"

std::vector<std::string> split(const std::string&str, char splitToken) {
  std::vector<std::string> rv;
  size_t curIndex = str.find_first_not_of(splitToken);
  while (curIndex != str.npos) {
    size_t endPos = str.find_first_of(splitToken, curIndex);
    rv.push_back(std::string(&str[curIndex], endPos != str.npos ? endPos - curIndex : str.size() - curIndex));
    curIndex = str.find_first_not_of(splitToken, endPos);
  }
  return rv;
}

void replace_all(std::string &input, const char *toReplace, const std::string &replaceant) {
  size_t pos = 0;
  while ((pos = input.find(toReplace, pos)) != input.npos) {
    input.replace(pos, 2, replaceant);
  }
}

std::string replace_matches(const std::string &input, const std::string* matches, char prefix, size_t count) {
  size_t pos = 0;
  std::string output = input;
  while ((pos = output.find(prefix, pos)) < output.size() - 1) {
    if (output[pos+1] >= '1' && output[pos+1] < (char)('1' + count)) {
      output.replace(pos, 2, matches[output[pos+1] - '1']);
    } else {
      pos++;
    }
  }
  return output;
}

size_t find_end_brace_balanced(const std::string& arg, size_t pos) {
  int braces = 1;
  while (braces && pos != arg.size()) {
    if (arg[pos] == '(') braces++;
    else if (arg[pos] == ')') braces--;
    pos++;
  }
  return braces == 0 ? pos : arg.npos;
}

std::string replace_with_pattern(const std::string& item, const RE2& pattern, const std::string& target) {
  RE2::Arg argv[10];
  const RE2::Arg* args[10] = {&argv[0], &argv[1], &argv[2], &argv[3], &argv[4], &argv[5], &argv[6], &argv[7], &argv[8], &argv[9]};
  std::string arg[10];
  for (size_t i = 0; i < 10; i++) {
    argv[i] = &arg[i];
  }
  if (!RE2::FullMatchN(item, pattern, args, pattern.NumberOfCapturingGroups()))
    return item;

  return replace_matches(target, arg, '@', pattern.NumberOfCapturingGroups());
}

TEST(splitSimpleTest) {
  std::vector<std::string> spl = split("a b c", ' ');
  ASSERT_EQ(spl.size(), 3);
  ASSERT_STREQ(spl[0], "a");
  ASSERT_STREQ(spl[1], "b");
  ASSERT_STREQ(spl[2], "c");
}

TEST(splitIgnoresWhitespaceAtFront) {
  std::vector<std::string> spl = split("   a", ' ');
  ASSERT_EQ(spl.size(), 1);
  ASSERT_STREQ(spl[0], "a");
}

TEST(splitIgnoresWhitespaceAtEnd) {
  std::vector<std::string> spl = split("a   ", ' ');
  ASSERT_EQ(spl.size(), 1);
  ASSERT_STREQ(spl[0], "a");
}

TEST(splitAlsoWorksWithOtherSeparators) {
  std::vector<std::string> spl = split("b//c", '/');
  ASSERT_EQ(spl.size(), 2);
  ASSERT_STREQ(spl[0], "b");
  ASSERT_STREQ(spl[1], "c");
}


