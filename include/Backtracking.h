// =============================================================================
// Backtracking.h - m-colouring decision problem: "can the exams fit in k slots?"
//
// Idea: colour vertices one at a time (DSATUR order, so tightly constrained
// subjects come first). For each vertex try colours 0..k-1; keep the first
// colour that is safe (no adjacent subject already has it) and recurse. If
// no colour works, undo and return to the previous vertex (backtrack).
//
// Time : O(k^V) worst case - every vertex may try every colour.
// Space: O(V) for the colour array + O(V) recursion depth (+ O(V^2) matrix).
//
// Why pruning helps in practice: isSafe() rejects a colour as soon as one
// neighbour clashes, so a whole subtree of k^(remaining) assignments is
// discarded in O(V) time. Colouring high-saturation vertices first makes
// those failures happen near the root, where each cut removes the most work.
// =============================================================================
#pragma once

#include "Coloring.h"
#include "Graph.h"

#include <cstddef>
#include <vector>

class BacktrackingColorer {
public:
    explicit BacktrackingColorer(const Graph& graph, SearchLimits limits = SearchLimits{});

    // -------------------------------------------------------------------------
    // canColor
    //   Purpose : decide whether the graph can be coloured with at most k colours.
    //   Input   : k >= 0.
    //   Output  : ColoringResult; success == true and colors filled if possible.
    //             If a limit is hit first, cutOff == true and success == false
    //             means "unknown", not "impossible".
    //   Time    : O(k^V) worst case    Space: O(V)
    // -------------------------------------------------------------------------
    ColoringResult canColor(int k);

private:
    bool solve(std::size_t position);
    bool isSafe(std::size_t position, int color) const;  // O(V) via the matrix
    bool limitReached();

    const Graph& graph_;
    SearchLimits limits_;
    int k_ = 0;
    std::vector<int> order_;
    std::vector<int> colors_;
    long long nodes_ = 0;
    long long backtracks_ = 0;
    bool cutOff_ = false;
    Stopwatch clock_;
};
