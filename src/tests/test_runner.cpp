#include "test_util.h"

// Each test_*.cpp self-registers its cases via static initializers; this just
// runs them all. Build: g++ -std=c++17 src/tests/*.cpp src/core/*.cpp -o /tmp/tests
int main() { return ss::test::run_all(); }
