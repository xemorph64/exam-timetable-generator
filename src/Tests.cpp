// =============================================================================
// Tests.cpp - known graphs with known chromatic numbers.
//
// For every graph we check:
//   * input-order greedy uses the expected number of slots (when specified)
//   * branch and bound returns the chromatic number and proves it
//   * backtracking succeeds with k = chi and fails with k = chi - 1
//   * every colouring produced passes validateColoring()
// =============================================================================
#include "Tests.h"

#include "Backtracking.h"
#include "BranchAndBound.h"
#include "Coloring.h"
#include "Samples.h"
#include "Scheduler.h"

#include <cstdio>
#include <iomanip>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kSkip = -1;

struct GraphCase {
    std::string name;
    Graph graph;
    int expectedGreedy;   // input-order greedy, or kSkip
    int expectedOptimal;  // chromatic number
};

Graph graphFromEdges(int n, const std::vector<std::pair<int, int>>& edges) {
    Graph g(n);
    for (const auto& [u, v] : edges) g.addEdge(u, v, 1, "test");
    return g;
}

Graph completeGraph(int n) {
    Graph g(n);
    for (int u = 0; u < n; ++u) {
        for (int v = u + 1; v < n; ++v) g.addEdge(u, v, 1, "test");
    }
    return g;
}

class TestReporter {
public:
    explicit TestReporter(std::ostream& out) : out_(out) {}

    void check(const std::string& name, bool passed, const std::string& detail) {
        out_ << "  [" << (passed ? "PASS" : "FAIL") << "] " << std::left << std::setw(46) << name
             << detail << '\n';
        if (!passed) ++failures_;
    }

    int failures() const { return failures_; }

private:
    std::ostream& out_;
    int failures_ = 0;
};

std::string expectation(int expected, int actual) {
    return "expected " + std::to_string(expected) + ", got " + std::to_string(actual);
}

void runGraphCase(TestReporter& report, const GraphCase& test) {
    const Graph& g = test.graph;

    const ColoringResult greedy = greedyColoring(g, GreedyOrder::Input);
    if (test.expectedGreedy != kSkip) {
        report.check(test.name + ": greedy (input order)",
                     greedy.slotsUsed == test.expectedGreedy &&
                         validateColoring(g, greedy.colors).valid,
                     expectation(test.expectedGreedy, greedy.slotsUsed));
    }

    const ColoringResult optimal = BranchAndBoundColorer(g).solve();
    report.check(test.name + ": branch and bound",
                 optimal.slotsUsed == test.expectedOptimal && optimal.provenOptimal &&
                     validateColoring(g, optimal.colors).valid,
                 expectation(test.expectedOptimal, optimal.slotsUsed) +
                     (optimal.provenOptimal ? " (proven)" : " (NOT proven)"));

    BacktrackingColorer backtracker(g);
    const ColoringResult fits = backtracker.canColor(test.expectedOptimal);
    const bool fitsOk = fits.success && validateColoring(g, fits.colors).valid;
    bool tighterFails = true;
    if (test.expectedOptimal > 0) {
        tighterFails = !backtracker.canColor(test.expectedOptimal - 1).success;
    }
    report.check(test.name + ": backtracking",
                 fitsOk && tighterFails,
                 "k=" + std::to_string(test.expectedOptimal) + (fitsOk ? " yes" : " no") +
                     (test.expectedOptimal > 0
                          ? ", k=" + std::to_string(test.expectedOptimal - 1) +
                                (tighterFails ? " no" : " yes")
                          : ""));
}

void runSchedulerChecks(TestReporter& report) {
    // Sample B goes through the full data layer (batches -> graph).
    ExamScheduler scheduler;
    loadSample(scheduler, SampleId::CrownGraph);
    const Graph& g = scheduler.graph();
    report.check("Sample B: graph built from batches", g.vertexCount() == 6 && g.edgeCount() == 6,
                 "V=" + std::to_string(g.vertexCount()) + ", E=" + std::to_string(g.edgeCount()));
    const int greedy = greedyColoring(g, GreedyOrder::Input).slotsUsed;
    const int optimal = BranchAndBoundColorer(g).solve().slotsUsed;
    report.check("Sample B: greedy 3 vs optimal 2", greedy == 3 && optimal == 2,
                 "greedy " + std::to_string(greedy) + ", optimal " + std::to_string(optimal));

    // The verifier must reject a bad timetable.
    std::vector<int> bad(6, 0);
    const ValidationReport rejected = validateColoring(g, bad);
    report.check("Verifier detects clashes", !rejected.valid && rejected.clashes.size() == 6,
                 std::to_string(rejected.clashes.size()) + " clashes found (expected 6)");

    // Duplicate codes, unknown ids and removal must keep data consistent.
    std::string error;
    const bool duplicateRejected = !scheduler.addSubject("a1", "Dup", 0, error);
    const bool unknownRejected = !scheduler.addBatch("Ghost", 10, {999}, error);
    report.check("Rejects duplicate code / unknown id", duplicateRejected && unknownRejected,
                 duplicateRejected && unknownRejected ? "both rejected" : "accepted bad input");

    scheduler.removeSubject(scheduler.subjects()[0].id, error);  // A1 has degree 2
    report.check("Remove subject updates graph",
                 scheduler.graph().vertexCount() == 5 && scheduler.graph().edgeCount() == 4,
                 "V=" + std::to_string(scheduler.graph().vertexCount()) +
                     ", E=" + std::to_string(scheduler.graph().edgeCount()));

    // Slot shortfall: 2 colours needed, keep only 1 slot available.
    loadSample(scheduler, SampleId::CrownGraph);
    for (const TimeSlot& slot : std::vector<TimeSlot>(scheduler.slots())) {
        scheduler.setSlotAvailable(slot.id, false, error);
    }
    scheduler.setSlotAvailable(scheduler.slots()[0].id, true, error);
    const Schedule schedule =
        scheduler.makeSchedule(BranchAndBoundColorer(scheduler.graph()).solve().colors);
    report.check("Infeasible when slots are short", !schedule.feasible && schedule.unplaced.size() == 3,
                 "need " + std::to_string(schedule.slotsNeeded) + ", have " +
                     std::to_string(schedule.slotsAvailable) + ", unplaced " +
                     std::to_string(schedule.unplaced.size()));

    // Save / load round trip.
    const std::string path = "selftest_roundtrip.tmp";
    loadSample(scheduler, SampleId::SmallCollege);
    ExamScheduler reloaded;
    const bool saved = scheduler.saveToFile(path, error);
    const bool loaded = saved && reloaded.loadFromFile(path, error);
    std::remove(path.c_str());
    report.check("Save / load round trip",
                 loaded && reloaded.subjects().size() == scheduler.subjects().size() &&
                     reloaded.graph().edgeCount() == scheduler.graph().edgeCount() &&
                     reloaded.slots().size() == scheduler.slots().size(),
                 loaded ? "data identical" : "failed: " + error);
}

}  // namespace

int runSelfTests(std::ostream& out) {
    out << "\n=== SELF-TESTS ===\n";
    TestReporter report(out);

    // Crown graph in the order A1, B1, A2, B2, A3, B3 (indices 0..5).
    const std::vector<std::pair<int, int>> crown = {{0, 3}, {0, 5}, {2, 1}, {2, 5}, {4, 1}, {4, 3}};
    const std::vector<GraphCase> cases = {
        {"Empty graph", Graph(0), 0, 0},
        {"Single vertex", Graph(1), 1, 1},
        {"No edges (5 vertices)", Graph(5), 1, 1},
        {"Complete graph K5", completeGraph(5), 5, 5},
        {"Odd cycle C5", graphFromEdges(5, {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 0}}), 3, 3},
        {"Bipartite K3,3", graphFromEdges(6, {{0, 3}, {0, 4}, {0, 5}, {1, 3}, {1, 4}, {1, 5},
                                              {2, 3}, {2, 4}, {2, 5}}), 2, 2},
        {"Crown graph", graphFromEdges(6, crown), 3, 2},
        {"Wheel W6 (C5 + hub)", graphFromEdges(6, {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 0},
                                                   {5, 0}, {5, 1}, {5, 2}, {5, 3}, {5, 4}}),
         kSkip, 4},
    };
    for (const GraphCase& test : cases) runGraphCase(report, test);
    runSchedulerChecks(report);

    out << "=== " << report.failures() << " failure(s) ===\n";
    return report.failures();
}
