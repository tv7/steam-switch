// Run-at-startup helper. The registry write itself is Windows-only (verified on
// real HW); here the pure parts + the POSIX no-op contract are covered.

#include "test_util.h"

#include "../core/autostart.h"
#include "../core/platform.h"

using namespace ss;

TEST_CASE(autostart_run_value_is_quoted_exe_path) {
    CHECK_EQ(autostart::runValueData("C:\\Program Files\\ORBIT\\Orbit.exe"),
             std::string("\"C:\\Program Files\\ORBIT\\Orbit.exe\""));
}

TEST_CASE(autostart_contract_off_windows) {
#if !defined(_WIN32)
    CHECK_EQ(autostart::supported(), false);
    CHECK_EQ(autostart::enabled(), false);                       // registry-less
    CHECK_EQ(autostart::setEnabled(true, "/bin/app"), false);    // no-op, reports failure
    CHECK_EQ(platform::regDeleteValue(platform::Hive::CurrentUser, "any", "name"), false);
#else
    CHECK_EQ(autostart::supported(), true);
#endif
}
