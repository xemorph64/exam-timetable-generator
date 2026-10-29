// =============================================================================
// Coloring.h - shared colouring types, the verifier, greedy colouring
// (input order / Welsh-Powell / DSATUR) and the greedy clique lower bound.
//
// Colour c (0-based) means "the (c+1)-th available time slot".
// =============================================================================
#pragma once

#include "Graph.h"

#include <chrono>
#include <string>
#include <utility>
#include <vector>

inline constexpr int kUncolored = -1;
inline constexpr long long kDefaultNodeLimit = 5'000'000;
inline constexpr double kDefaultTimeLimitMs = 5000.0;

// Safety limits for the exponential searches (backtracking, branch & bound).
struct SearchLimits {
    long long nodeLimit = kDefaultNodeLimit;
    double timeLimitMs = kDefaultTimeLimitMs;
};

// Everything an algorithm reports back to the user.
struct ColoringResult {
    std::string method;             // e.g. "Greedy (DSATUR)"
    std::vector<int> colors;        // colour per vertex, kUncolored if none
    int slotsUsed = 0;
    double timeMs = 0.0;
    long long nodesExplored = 0;    // search algorithms only
    long long nodesPruned = 0;      // branch & bound only
    long long backtracks = 0;       // backtracking only
    bool success = true;            // false: backtracking found no k-colouring
    bool provenOptimal = false;     // true only when B&B finished (or UB == LB)
    bool cutOff = false;            // a safety limit stopped the search
    int lowerBound = 0;             // clique bound (B&B)
    int upperBound = 0;             // initial greedy bound (B&B)
};

// Result of validateColoring(): empty lists mean conflict-free.
struct ValidationReport {
    bool valid = true;
    std::vector<int> uncolored;                 // vertices with no slot
    std::vector<std::pair<int, int>> clashes;   // edges whose ends share a slot
};

enum class GreedyOrder { Input, WelshPowell, Dsatur };

std::string greedyOrderName(GreedyOrder order);

// Small RAII timer: starts on construction.
class Stopwatch {
public:
    Stopwatch() : start_(std::chrono::steady_clock::now()) {}
    void restart() { start_ = std::chrono::steady_clock::now(); }
    double elapsedMs() const {
        const auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(now - start_).count();
    }

private:
    std::chrono::steady_clock::time_point start_;
};

// Number of slots a colouring uses (largest colour + 1). O(V).
int countColors(const std::vector<int>& colors);

// -----------------------------------------------------------------------------
// validateColoring
//   Purpose : prove a timetable is conflict-free.
//   Input   : graph, colour per vertex.
//   Output  : report listing uncoloured vertices and clashing edges.
//   Time    : O(V + E)      Space: O(V) for the report in the worst case
// -----------------------------------------------------------------------------
ValidationReport validateColoring(const Graph& graph, const std::vector<int>& colors);

// -----------------------------------------------------------------------------
// greedyColorInOrder
//   Purpose : visit vertices in the given order and give each the smallest
//             colour not used by an already-coloured neighbour.
//   Input   : graph, permutation of the vertices.
//   Output  : colour per vertex (compact: 0..k-1).
//   Time    : O(V + E) - each adjacency list is scanned once.
//   Space   : O(V)
// -----------------------------------------------------------------------------
std::vector<int> greedyColorInOrder(const Graph& graph, const std::vector<int>& order);

// Vertices sorted by descending degree, ties by index (Welsh-Powell order).
// Time O(V log V), Space O(V).
std::vector<int> degreeOrder(const Graph& graph);

// -----------------------------------------------------------------------------
// dsaturColoring  (Brelaz 1979)
//   Purpose : greedy colouring whose order is chosen dynamically: always colour
//             the vertex with the most distinct colours among its neighbours
//             (saturation), ties broken by degree, then by index.
//   Input   : graph; optional out-parameter receiving the visit order.
//   Output  : colour per vertex.
//   Time    : O(V^2 + E log V)  (linear scan for the next vertex, set inserts)
//   Space   : O(V + E)
// -----------------------------------------------------------------------------
std::vector<int> dsaturColoring(const Graph& graph, std::vector<int>* visitOrder = nullptr);

// Runs one greedy variant, timed, and packs the result.
ColoringResult greedyColoring(const Graph& graph, GreedyOrder order);

// -----------------------------------------------------------------------------
// greedyClique
//   Purpose : find a large clique quickly. A clique of size c forces c colours,
//             so its size is a valid lower bound on the chromatic number.
//   Method  : from every start vertex, add neighbours (highest degree first)
//             that are adjacent to everything chosen so far; keep the largest.
//   Time    : O(V^3) worst case    Space: O(V)
// -----------------------------------------------------------------------------
std::vector<int> greedyClique(const Graph& graph);

// Renames the colours of `target` so they overlap `reference` as much as
// possible (greedy max-overlap matching). Used by the comparison screen so
// "moved subjects" are meaningful. The result still uses colours 0..k-1.
// Time O(V + k^3), Space O(k^2).
std::vector<int> alignColors(const std::vector<int>& reference, const std::vector<int>& target);
