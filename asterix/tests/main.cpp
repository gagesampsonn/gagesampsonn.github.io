#include "test.hpp"

#include <cstdio>
#include <cstring>
#include <exception>

namespace test {

namespace {
int g_failures_in_test = 0;
}

std::vector<TestCase>& registry() {
    static std::vector<TestCase> cases;
    return cases;
}

void fail(const char* file, int line, const std::string& message) {
    ++g_failures_in_test;
    std::printf("  %s:%d: %s\n", file, line, message.c_str());
}

}  // namespace test

// Usage: asterix-tests [-v] [NAME_SUBSTRING...]
int main(int argc, char** argv) {
    bool verbose = false;
    std::vector<const char*> filters;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else {
            filters.push_back(argv[i]);
        }
    }

    int run = 0;
    int failed = 0;
    for (const test::TestCase& t : test::registry()) {
        bool selected = filters.empty();
        for (const char* f : filters) {
            selected = selected || std::strstr(t.name, f) != nullptr;
        }
        if (!selected) {
            continue;
        }
        ++run;
        test::g_failures_in_test = 0;
        try {
            t.fn();
        } catch (const test::Abort&) {
        } catch (const std::exception& e) {
            test::fail(__FILE__, __LINE__, std::string("unexpected exception: ") + e.what());
        }
        if (test::g_failures_in_test != 0) {
            ++failed;
            std::printf("[FAIL] %s\n", t.name);
        } else if (verbose) {
            std::printf("[ OK ] %s\n", t.name);
        }
    }
    std::printf("%d of %d tests passed\n", run - failed, run);
    return failed == 0 && run > 0 ? 0 : 1;
}
