#pragma once
// Minimalt testramverk för PC-testerna. Varje TEST registrerar sig själv.
#include <cstdio>
#include <cstring>

extern int g_pass, g_fail;

typedef void (*TestFn)();
struct TestReg { TestReg(const char* name, TestFn fn); };

#define TEST(name) static void name(); static TestReg reg_##name(#name, name); static void name()

#define CHECK(cond) do { if (cond) { ++g_pass; } else { ++g_fail; \
  std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define CHECK_EQ(a, b) do { long long _a = (long long)(a), _b = (long long)(b); \
  if (_a == _b) { ++g_pass; } else { ++g_fail; \
  std::printf("  FAIL %s:%d: %s == %s (%lld != %lld)\n", __FILE__, __LINE__, #a, #b, _a, _b); } } while (0)

#define CHECK_STR_CONTAINS(s, sub) do { if (std::strstr((s), (sub))) { ++g_pass; } else { ++g_fail; \
  std::printf("  FAIL %s:%d: \"%s\" saknar \"%s\"\n", __FILE__, __LINE__, (s), (sub)); } } while (0)
