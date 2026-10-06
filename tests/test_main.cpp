#include "test_framework.h"

int g_pass = 0, g_fail = 0;

static const int MAX_TESTS = 256;
static const char* s_names[MAX_TESTS];
static TestFn s_fns[MAX_TESTS];
static int s_count = 0;

TestReg::TestReg(const char* name, TestFn fn) {
  if (s_count < MAX_TESTS) { s_names[s_count] = name; s_fns[s_count] = fn; s_count++; }
}

int main() {
  for (int i = 0; i < s_count; i++) {
    const int before = g_fail;
    s_fns[i]();
    std::printf("%s %s\n", g_fail == before ? "ok  " : "FEL ", s_names[i]);
  }
  std::printf("\n%d kontroller OK, %d fel, %d tester\n", g_pass, g_fail, s_count);
  return g_fail ? 1 : 0;
}
