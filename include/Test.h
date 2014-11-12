#ifndef TEST_H
#define TEST_H

struct basetest {
  basetest(const char *name)
  : name(name)
  {
    registerTest(this);
  }
  virtual void runtest() = 0;
  const char *name;
  static void registerTest(basetest* test) {
    test->next = head();
    head() = test;
  }
  basetest *next;
  static basetest *&head() { static basetest *_head = 0; return _head; }
};

#define ASSERT_EQ(a, b) if (a != b) { printf("%s:%d: assert failed: %d != %d\n", __FILE__, __LINE__, (int)a, (int)b); throw std::exception(); } else (void)0
#define ASSERT_STREQ(a, b) if (std::string(a) != std::string(b)) { printf("%s:%d: assert failed: %s != %s\n", __FILE__, __LINE__, std::string(a).c_str(), std::string(b).c_str()); throw std::exception(); } else (void)0

#define TEST(x) static struct __test_##x : public basetest{\
        void runtest();\
        __test_##x() : basetest(#x) {}\
} _inst_##x;\
        void __test_##x::runtest()

#endif


