#pragma once

// A tiny self-contained unit test framework (no external dependencies).
//
//   TEST(name) { CHECK(cond); CHECK_EQ(a, b); CHECK_NEAR(a, b, eps); REQUIRE(cond); }

#include <cmath>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace test {

struct TestCase {
    const char* name;
    void (*fn)();
};

std::vector<TestCase>& registry();

struct Registrar {
    Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

/// Thrown by REQUIRE to abandon the current test.
struct Abort {};

void fail(const char* file, int line, const std::string& message);

template <class T>
struct is_optional : std::false_type {};
template <class T>
struct is_optional<std::optional<T>> : std::true_type {};

template <class T>
std::string show(const T& v) {
    if constexpr (is_optional<T>::value) {
        return v ? show(*v) : std::string("nullopt");
    } else if constexpr (std::is_enum_v<T>) {
        return std::to_string(static_cast<long long>(v));
    } else if constexpr (std::is_same_v<T, bool>) {
        return v ? "true" : "false";
    } else if constexpr (std::is_integral_v<T>) {
        return std::to_string(static_cast<long long>(v));
    } else if constexpr (std::is_floating_point_v<T>) {
        std::ostringstream os;
        os.precision(17);
        os << v;
        return os.str();
    } else if constexpr (std::is_convertible_v<T, std::string>) {
        return "\"" + std::string(v) + "\"";
    } else {
        return "<value>";
    }
}

}  // namespace test

#define TEST(name)                                                                   \
    static void test_fn_##name();                                                    \
    static const ::test::Registrar test_registrar_##name(#name, &test_fn_##name);    \
    static void test_fn_##name()

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) ::test::fail(__FILE__, __LINE__, "CHECK(" #cond ") failed");    \
    } while (0)

#define REQUIRE(cond)                                                                \
    do {                                                                             \
        if (!(cond)) {                                                               \
            ::test::fail(__FILE__, __LINE__, "REQUIRE(" #cond ") failed");           \
            throw ::test::Abort{};                                                   \
        }                                                                            \
    } while (0)

#define CHECK_EQ(a, b)                                                               \
    do {                                                                             \
        const auto& check_a_ = (a);                                                  \
        const auto& check_b_ = (b);                                                  \
        if (!(check_a_ == check_b_))                                                 \
            ::test::fail(__FILE__, __LINE__,                                         \
                         "CHECK_EQ(" #a ", " #b ") failed: " + ::test::show(check_a_) + \
                             " != " + ::test::show(check_b_));                       \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                        \
    do {                                                                             \
        const double check_a_ = (a);                                                 \
        const double check_b_ = (b);                                                 \
        if (!(std::fabs(check_a_ - check_b_) <= (eps)))                              \
            ::test::fail(__FILE__, __LINE__,                                         \
                         "CHECK_NEAR(" #a ", " #b ") failed: " + ::test::show(check_a_) + \
                             " vs " + ::test::show(check_b_));                       \
    } while (0)
