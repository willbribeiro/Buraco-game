#include "test_framework.hpp"

#include <cstdio>
#include <cstring>
#include <exception>

namespace testfw {
namespace {

bool s_currentFailed = false;

}  // namespace

std::vector<TestCase>& registry() {
    static std::vector<TestCase> cases;
    return cases;
}

Registrar::Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }

void recordFailure(const char* file, int line, const std::string& message) {
    s_currentFailed = true;
    std::fprintf(stderr, "%s:%d: %s\n", file, line, message.c_str());
}

bool currentTestFailed() { return s_currentFailed; }

int runAll(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0;
    int failed = 0;
    for (const TestCase& test : registry()) {
        if (filter && std::strstr(test.name, filter) == nullptr) continue;
        ++run;
        s_currentFailed = false;
        std::printf("[ RUN  ] %s\n", test.name);
        try {
            test.fn();
        } catch (const RequirementFailed&) {
            // recordFailure() already printed the expression.
        } catch (const std::exception& e) {
            recordFailure("<unknown>", 0, std::string("unexpected exception: ") + e.what());
        } catch (...) {
            recordFailure("<unknown>", 0, "unexpected non-standard exception");
        }
        if (s_currentFailed) {
            ++failed;
            std::printf("[ FAIL ] %s\n", test.name);
        } else {
            std::printf("[  OK  ] %s\n", test.name);
        }
    }
    std::printf("\n%d test(s) run, %d failed\n", run, failed);
    return failed == 0 ? 0 : 1;
}

}  // namespace testfw

int main(int argc, char** argv) { return testfw::runAll(argc, argv); }
