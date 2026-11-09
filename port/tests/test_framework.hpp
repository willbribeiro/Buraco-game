// Minimal dependency-free test framework for the Buraco port.
//
// The suite used to depend on Catch2, which is not part of the toolchain and
// made the tests impossible to build.  This header keeps the same shape
// (TEST_CASE / CHECK / REQUIRE) but needs nothing beyond the standard library.
#pragma once

#include <sstream>
#include <string>
#include <vector>

namespace testfw {

struct TestCase {
    const char* name;
    void (*fn)();
};

std::vector<TestCase>& registry();

struct Registrar {
    Registrar(const char* name, void (*fn)());
};

// Thrown by REQUIRE to abandon the current test case.
struct RequirementFailed {
    std::string message;
};

void recordFailure(const char* file, int line, const std::string& message);
bool currentTestFailed();
int runAll(int argc, char** argv);

template <typename T>
std::string toString(const T& value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

}  // namespace testfw

#define TESTFW_CONCAT_INNER(a, b) a##b
#define TESTFW_CONCAT(a, b) TESTFW_CONCAT_INNER(a, b)

#define TEST_CASE(description)                                              \
    static void TESTFW_CONCAT(testfwCase_, __LINE__)();                     \
    static ::testfw::Registrar TESTFW_CONCAT(testfwReg_, __LINE__)(        \
        description, &TESTFW_CONCAT(testfwCase_, __LINE__));                \
    static void TESTFW_CONCAT(testfwCase_, __LINE__)()

#define CHECK(expression)                                                   \
    do {                                                                    \
        if (!(expression))                                                  \
            ::testfw::recordFailure(__FILE__, __LINE__,                     \
                                    "CHECK(" #expression ")");              \
    } while (0)

#define REQUIRE(expression)                                                 \
    do {                                                                    \
        if (!(expression)) {                                                \
            ::testfw::recordFailure(__FILE__, __LINE__,                     \
                                    "REQUIRE(" #expression ")");            \
            throw ::testfw::RequirementFailed{"REQUIRE(" #expression ")"};  \
        }                                                                   \
    } while (0)

#define CHECK_EQ(actual, expected)                                          \
    do {                                                                    \
        const auto& testfwActual = (actual);                                \
        const auto& testfwExpected = (expected);                            \
        if (!(testfwActual == testfwExpected))                              \
            ::testfw::recordFailure(                                        \
                __FILE__, __LINE__,                                         \
                "CHECK_EQ(" #actual ", " #expected ") -> " +                \
                    ::testfw::toString(testfwActual) + " != " +             \
                    ::testfw::toString(testfwExpected));                    \
    } while (0)
