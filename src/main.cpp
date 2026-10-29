// =============================================================================
// main.cpp - entry point.
//   ./timetable          interactive menu
//   ./timetable --test   run the self-tests and exit (exit code = failures)
//
// COMPLEXITY SUMMARY  (V = subjects, E = conflict edges, B = batches,
//                      s = subjects in a batch, k = colours / slots)
// ---------------------------------------------------------------------------
//  Algorithm                  Time                       Space
//  Graph construction         O(V^2 + sum s^2 * log E)   O(V^2 + E)
//  Greedy (input / degree)    O(V + E)  (+ V log V sort) O(V)
//  DSATUR                     O(V^2 + E log V)           O(V + E)
//  Verifier                   O(V + E)                   O(V)
//  Clique lower bound         O(V^3)                     O(V)
//  Backtracking  canColor(k)  O(k^V) worst case          O(V) + recursion O(V)
//  Branch and bound           O(UB^V) worst case         O(V * UB) + O(V)
// Graph colouring is NP-hard, so no known exact algorithm is polynomial; the
// limits in SearchLimits keep both exact searches responsive.
// =============================================================================
#include "App.h"
#include "Tests.h"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--test") {
        return runSelfTests(std::cout) == 0 ? 0 : 1;
    }
    App app;
    return app.run();
}
