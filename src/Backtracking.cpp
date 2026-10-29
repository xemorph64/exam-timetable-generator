// =============================================================================
// Backtracking.cpp - m-colouring by depth-first search with undo.
// =============================================================================
#include "Backtracking.h"

#include <string>

BacktrackingColorer::BacktrackingColorer(const Graph& graph, SearchLimits limits)
    : graph_(graph), limits_(limits) {}

ColoringResult BacktrackingColorer::canColor(int k) {
    const int n = graph_.vertexCount();
    k_ = k;
    nodes_ = 0;
    backtracks_ = 0;
    cutOff_ = false;
    colors_.assign(static_cast<std::size_t>(n), kUncolored);
    dsaturColoring(graph_, &order_);  // only the visit order is used
    clock_.restart();

    const bool found = (n == 0) || (k > 0 && solve(0));

    ColoringResult result;
    result.method = "Backtracking (k = " + std::to_string(k) + ")";
    result.timeMs = clock_.elapsedMs();
    result.success = found;
    result.cutOff = cutOff_ && !found;
    result.nodesExplored = nodes_;
    result.backtracks = backtracks_;
    if (found) {
        result.colors = colors_;
        result.slotsUsed = countColors(colors_);
    }
    return result;
}

bool BacktrackingColorer::limitReached() {
    if (nodes_ >= limits_.nodeLimit) return true;
    // Reading the clock is relatively slow, so only do it every 4096 nodes.
    return (nodes_ & 4095) == 0 && clock_.elapsedMs() >= limits_.timeLimitMs;
}

bool BacktrackingColorer::isSafe(std::size_t position, int color) const {
    // Only vertices placed earlier in the order are coloured.
    const int vertex = order_[position];
    for (std::size_t i = 0; i < position; ++i) {
        const int other = order_[i];
        if (colors_[other] == color && graph_.hasEdge(vertex, other)) return false;
    }
    return true;
}

bool BacktrackingColorer::solve(std::size_t position) {
    if (cutOff_) return false;
    if (limitReached()) {
        cutOff_ = true;
        return false;
    }
    ++nodes_;
    if (position == order_.size()) return true;  // every vertex coloured

    const int vertex = order_[position];
    for (int color = 0; color < k_; ++color) {
        if (!isSafe(position, color)) continue;
        colors_[vertex] = color;
        if (solve(position + 1)) return true;
        colors_[vertex] = kUncolored;  // undo and try the next colour
        ++backtracks_;
        if (cutOff_) return false;
    }
    return false;  // no colour fits: caller must change an earlier vertex
}
