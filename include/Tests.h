// =============================================================================
// Tests.h - automated self-tests (menu 16 or `./timetable --test`).
// =============================================================================
#pragma once

#include <ostream>

// Runs every check, printing PASS/FAIL lines. Returns the number of failures.
int runSelfTests(std::ostream& out);
