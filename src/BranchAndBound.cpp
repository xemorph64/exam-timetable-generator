// =============================================================================
// BranchAndBound.cpp - exact minimum colouring with bounds and pruning.
// =============================================================================
#include "BranchAndBound.h"

#include <algorithm>
#include <utility>

BranchAndBoundColorer::BranchAndBoundColorer(const Graph& graph, SearchLimits limits)
    : graph_(graph), limits_(limits) {}

ColoringResult BranchAndBoundColorer::solve() {
    clock_.restart();
    const int n = graph_.vertexCount();
    nodes_ = 0;
    pruned_ = 0;
    cutOff_ = false;
    stop_ = false;

    ColoringResult result;
    result.method = "Branch and Bound (optimal)";
    if (n == 0) {
        result.provenOptimal = true;
        result.timeMs = clock_.elapsedMs();
        return result;
    }

    // Upper bound: the best greedy timetable we can build cheaply.
    ColoringResult incumbent = greedyColoring(graph_, GreedyOrder::Input);
    for (GreedyOrder order : {GreedyOrder::WelshPowell, GreedyOrder::Dsatur}) {
        ColoringResult candidate = greedyColoring(graph_, order);
        if (candidate.slotsUsed < incumbent.slotsUsed) incumbent = std::move(candidate);
    }
    best_ = incumbent.slotsUsed;
    bestColors_ = incumbent.colors;

    // Lower bound: a clique of size c needs c slots.
    const std::vector<int> clique = greedyClique(graph_);
    lowerBound_ = static_cast<int>(clique.size());
    result.upperBound = best_;
    result.lowerBound = lowerBound_;

    if (best_ > lowerBound_) {
        order_ = buildOrder(clique);
        colors_.assign(static_cast<std::size_t>(n), kUncolored);
        conflictCount_.assign(static_cast<std::size_t>(n),
                              std::vector<int>(static_cast<std::size_t>(best_), 0));
        search(0, 0);
    }

    result.colors = bestColors_;
    result.slotsUsed = best_;
    result.nodesExplored = nodes_;
    result.nodesPruned = pruned_;
    result.cutOff = cutOff_;
    result.provenOptimal = !cutOff_ || best_ == lowerBound_;
    result.timeMs = clock_.elapsedMs();
    return result;
}

std::vector<int> BranchAndBoundColorer::buildOrder(const std::vector<int>& clique) const {
    // Clique vertices first: they are forced to take colours 0..c-1, which
    // fixes the colour names early. Then DSATUR order for the rest.
    std::vector<int> order(clique);
    std::vector<int> dsaturOrder;
    dsaturColoring(graph_, &dsaturOrder);
    for (int v : dsaturOrder) {
        if (std::find(clique.begin(), clique.end(), v) == clique.end()) order.push_back(v);
    }
    return order;
}

bool BranchAndBoundColorer::limitReached() {
    if (nodes_ >= limits_.nodeLimit) return true;
    return (nodes_ & 4095) == 0 && clock_.elapsedMs() >= limits_.timeLimitMs;
}

void BranchAndBoundColorer::assign(int vertex, int color) {
    colors_[vertex] = color;
    for (int u : graph_.neighbors(vertex)) ++conflictCount_[u][color];
}

void BranchAndBoundColorer::unassign(int vertex, int color) {
    colors_[vertex] = kUncolored;
    for (int u : graph_.neighbors(vertex)) --conflictCount_[u][color];
}

bool BranchAndBoundColorer::forwardCheck(int vertex, int colorLimit) const {
    // Only neighbours of the vertex just coloured lost a colour, so only they
    // can have become impossible to colour within the limit.
    for (int u : graph_.neighbors(vertex)) {
        if (colors_[u] != kUncolored) continue;
        bool hasOption = false;
        for (int c = 0; c < colorLimit && !hasOption; ++c) {
            hasOption = (conflictCount_[u][c] == 0);
        }
        if (!hasOption) return false;
    }
    return true;
}

void BranchAndBoundColorer::search(std::size_t position, int colorsUsed) {
    if (stop_) return;
    if (limitReached()) {
        cutOff_ = true;
        stop_ = true;
        return;
    }
    ++nodes_;

    // Rule 1 (bound): cannot beat the best timetable already found.
    if (colorsUsed >= best_) {
        ++pruned_;
        return;
    }

    if (position == order_.size()) {
        // Every subject placed with fewer slots than before: new incumbent.
        best_ = colorsUsed;
        bestColors_ = colors_;
        if (best_ == lowerBound_) stop_ = true;  // matches the clique: optimal
        return;
    }

    const int vertex = order_[position];
    // Rule 2 (symmetry): colours 0..colorsUsed only (colorsUsed = one new
    // colour). The bound requires the final count to stay <= best_ - 1, so
    // the colour index itself must stay <= best_ - 2. best_ may shrink while
    // we loop, so the condition is re-checked each iteration.
    for (int color = 0; color <= colorsUsed && color <= best_ - 2; ++color) {
        if (conflictCount_[vertex][color] != 0) continue;  // a neighbour has it
        assign(vertex, color);
        // Rule 3 (forward checking) under the "best_ - 1 colours" limit.
        if (forwardCheck(vertex, best_ - 1)) {
            search(position + 1, std::max(colorsUsed, color + 1));
        } else {
            ++pruned_;
        }
        unassign(vertex, color);
        if (stop_) return;
    }
}
