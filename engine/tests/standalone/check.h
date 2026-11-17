#pragma once

// 不依赖 Catch2 的最小断言工具：本机 / CI 没装 Catch2 时这些测试照样构建、照样跑。
//   CHECK(cond)           失败计数 + 打印位置，继续执行
//   CHECK_NEAR(a, b, eps)
//   TEST(name) { ... }    自动注册；RUN_ALL() 在 main 里调用

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace jtest {

inline int& failures() { static int f = 0; return f; }
inline int& checks() { static int c = 0; return c; }

struct Case { const char* name; std::function<void()> fn; };
inline std::vector<Case>& cases() { static std::vector<Case> v; return v; }

struct Reg {
    Reg(const char* n, std::function<void()> f) { cases().push_back({n, std::move(f)}); }
};

inline int run_all() {
    for (auto& c : cases()) {
        const int before = failures();
        c.fn();
        std::printf("[%s] %s\n", failures() == before ? " OK " : "FAIL", c.name);
    }
    std::printf("%d checks, %d failures, %zu cases\n", checks(), failures(), cases().size());
    return failures() == 0 ? 0 : 1;
}

}  // namespace jtest

#define JT_CAT2(a, b) a##b
#define JT_CAT(a, b) JT_CAT2(a, b)

#define TEST(name)                                                              \
    static void JT_CAT(jt_fn_, __LINE__)();                                     \
    static jtest::Reg JT_CAT(jt_reg_, __LINE__)(name, &JT_CAT(jt_fn_, __LINE__)); \
    static void JT_CAT(jt_fn_, __LINE__)()

#define CHECK(cond)                                                             \
    do {                                                                        \
        ++jtest::checks();                                                      \
        if (!(cond)) {                                                          \
            ++jtest::failures();                                                \
            std::printf("  %s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
        }                                                                       \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                   \
    do {                                                                        \
        ++jtest::checks();                                                      \
        const double jt_a = (a), jt_b = (b);                                    \
        if (!(std::fabs(jt_a - jt_b) <= (eps))) {                               \
            ++jtest::failures();                                                \
            std::printf("  %s:%d: CHECK_NEAR(%s, %s) failed: %g vs %g (eps %g)\n", \
                        __FILE__, __LINE__, #a, #b, jt_a, jt_b, double(eps));    \
        }                                                                       \
    } while (0)

#define RUN_ALL() jtest::run_all()
