// =============================================================================
// BranchAndBound.h - minimum number of slots (chromatic number).
//
// Bounds
//   UB (upper bound) = best of the three greedy colourings - a real timetable.
//   LB (lower bound) = size of a greedily found clique - any clique of size c
//                      needs c distinct slots. If UB == LB we are done.
// Branching
//   Vertices are coloured in a fixed order (clique first, then DSATUR order).
//   colorsUsed = number of distinct colours in the partial colouring.
// Pruning
//   (1) Bound      : colorsUsed >= best  -> this branch cannot improve, cut.
//   (2) Symmetry   : a vertex may take an existing colour or exactly one new
//                    colour (index colorsUsed); colourings that only rename
//                    colours are never explored twice.
//   (3) Forward    : after a choice, every uncoloured neighbour must still have
//       checking     a free colour below best - 1; otherwise cut immediately.
//
// Time : O(best^V) worst case (NP-hard), usually far less thanks to pruning.
// Space: O(V * UB) neighbour-colour counters + O(V) recursion depth.
// =============================================================================
#pragma once

#include "Coloring.h"
#include "Graph.h"

#include <cstddef>
#include <vector>

class BranchAndBoundColorer {
public:
    explicit BranchAndBoundColorer(const Graph& graph, SearchLimits limits = SearchLimits{});

    // -------------------------------------------------------------------------
    // solve
    //   Purpose : find a colouring with the fewest colours.
    //   Input   : the graph given to the constructor.
    //   Output  : best colouring found; provenOptimal == true unless a safety
    //             limit cut the search off (then cutOff == true).
    //   Time    : exponential worst case    Space: O(V * UB)
    // -------------------------------------------------------------------------
    ColoringResult solve();

private:
    void search(std::size_t position, int colorsUsed);
    void assign(int vertex, int color);
    void unassign(int vertex, int color);
    bool forwardCheck(int vertex, int colorLimit) const;
    bool limitReached();
    std::vector<int> buildOrder(const std::vector<int>& clique) const;

    const Graph& graph_;
    SearchLimits limits_;
    std::vector<int> order_;
    std::vector<int> colors_;
    // conflictCount_[v][c] = how many neighbours of v currently have colour c.
    std::vector<std::vector<int>> conflictCount_;
    std::vector<int> bestColors_;
    int best_ = 0;
    int lowerBound_ = 0;
    long long nodes_ = 0;
    long long pruned_ = 0;
    bool cutOff_ = false;
    bool stop_ = false;
    Stopwatch clock_;
};
