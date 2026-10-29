// =============================================================================
// Coloring.cpp - verifier, greedy colourings, DSATUR, clique lower bound.
// =============================================================================
#include "Coloring.h"

#include <algorithm>
#include <numeric>
#include <set>

std::string greedyOrderName(GreedyOrder order) {
    switch (order) {
        case GreedyOrder::Input: return "Input order";
        case GreedyOrder::WelshPowell: return "Welsh-Powell (descending degree)";
        case GreedyOrder::Dsatur: return "DSATUR (saturation degree)";
    }
    return "Unknown";
}

int countColors(const std::vector<int>& colors) {
    int highest = kUncolored;
    for (int c : colors) highest = std::max(highest, c);
    return highest + 1;
}

ValidationReport validateColoring(const Graph& graph, const std::vector<int>& colors) {
    ValidationReport report;
    const int n = graph.vertexCount();
    for (int v = 0; v < n; ++v) {
        if (v >= static_cast<int>(colors.size()) || colors[v] == kUncolored) {
            report.uncolored.push_back(v);
        }
    }
    for (const ConflictEdge& edge : graph.edges()) {
        const bool bothColored = edge.u < static_cast<int>(colors.size()) &&
                                 edge.v < static_cast<int>(colors.size()) &&
                                 colors[edge.u] != kUncolored;
        if (bothColored && colors[edge.u] == colors[edge.v]) {
            report.clashes.emplace_back(edge.u, edge.v);
        }
    }
    report.valid = report.uncolored.empty() && report.clashes.empty();
    return report;
}

std::vector<int> greedyColorInOrder(const Graph& graph, const std::vector<int>& order) {
    const int n = graph.vertexCount();
    std::vector<int> colors(static_cast<std::size_t>(n), kUncolored);
    // usedBy[c] == v  means colour c is taken by a neighbour of v. Stamping with
    // the current vertex avoids clearing the array: O(deg(v)) per vertex.
    std::vector<int> usedBy(static_cast<std::size_t>(n) + 1, -1);

    for (int v : order) {
        for (int u : graph.neighbors(v)) {
            if (colors[u] != kUncolored) usedBy[colors[u]] = v;
        }
        int color = 0;
        while (usedBy[color] == v) ++color;
        colors[v] = color;
    }
    return colors;
}

std::vector<int> degreeOrder(const Graph& graph) {
    std::vector<int> order(static_cast<std::size_t>(graph.vertexCount()));
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&graph](int a, int b) {
        return graph.degree(a) > graph.degree(b);
    });
    return order;
}

std::vector<int> dsaturColoring(const Graph& graph, std::vector<int>* visitOrder) {
    const int n = graph.vertexCount();
    std::vector<int> colors(static_cast<std::size_t>(n), kUncolored);
    std::vector<std::set<int>> neighbourColors(static_cast<std::size_t>(n));
    std::vector<int> usedBy(static_cast<std::size_t>(n) + 1, -1);
    if (visitOrder != nullptr) visitOrder->clear();

    for (int step = 0; step < n; ++step) {
        // Pick the uncoloured vertex with max saturation, then max degree.
        int pick = -1;
        for (int v = 0; v < n; ++v) {
            if (colors[v] != kUncolored) continue;
            if (pick == -1) {
                pick = v;
                continue;
            }
            const auto satV = neighbourColors[v].size();
            const auto satP = neighbourColors[pick].size();
            if (satV > satP || (satV == satP && graph.degree(v) > graph.degree(pick))) {
                pick = v;
            }
        }

        // Smallest colour not used by a neighbour (same idea as plain greedy).
        for (int u : graph.neighbors(pick)) {
            if (colors[u] != kUncolored) usedBy[colors[u]] = pick;
        }
        int color = 0;
        while (usedBy[color] == pick) ++color;
        colors[pick] = color;
        if (visitOrder != nullptr) visitOrder->push_back(pick);

        for (int u : graph.neighbors(pick)) neighbourColors[u].insert(color);
    }
    return colors;
}

ColoringResult greedyColoring(const Graph& graph, GreedyOrder order) {
    ColoringResult result;
    result.method = "Greedy - " + greedyOrderName(order);
    const Stopwatch clock;

    switch (order) {
        case GreedyOrder::Input: {
            std::vector<int> identity(static_cast<std::size_t>(graph.vertexCount()));
            std::iota(identity.begin(), identity.end(), 0);
            result.colors = greedyColorInOrder(graph, identity);
            break;
        }
        case GreedyOrder::WelshPowell:
            result.colors = greedyColorInOrder(graph, degreeOrder(graph));
            break;
        case GreedyOrder::Dsatur:
            result.colors = dsaturColoring(graph);
            break;
    }

    result.timeMs = clock.elapsedMs();
    result.slotsUsed = countColors(result.colors);
    return result;
}

std::vector<int> greedyClique(const Graph& graph) {
    const std::vector<int> byDegree = degreeOrder(graph);
    std::vector<int> best;

    for (int start : byDegree) {
        // A clique through `start` can be no bigger than deg(start) + 1.
        if (graph.degree(start) + 1 <= static_cast<int>(best.size())) break;
        std::vector<int> clique{start};
        for (int candidate : byDegree) {
            if (candidate == start || !graph.hasEdge(start, candidate)) continue;
            const bool joinsAll = std::all_of(clique.begin(), clique.end(), [&](int member) {
                return graph.hasEdge(member, candidate);
            });
            if (joinsAll) clique.push_back(candidate);
        }
        if (clique.size() > best.size()) best = clique;
    }
    return best;
}

std::vector<int> alignColors(const std::vector<int>& reference, const std::vector<int>& target) {
    const int tgtColors = countColors(target);
    // Target colours are only permuted among themselves (0..tgtColors-1) so the
    // result stays compact; reference colours beyond that range are ignored.
    const int size = tgtColors;
    if (size == 0 || reference.size() != target.size()) return target;

    // overlap[t][r] = vertices with colour t in target and r in reference.
    std::vector<std::vector<int>> overlap(static_cast<std::size_t>(size),
                                          std::vector<int>(static_cast<std::size_t>(size), 0));
    for (std::size_t v = 0; v < target.size(); ++v) {
        if (target[v] != kUncolored && reference[v] != kUncolored && reference[v] < size) {
            ++overlap[target[v]][reference[v]];
        }
    }

    std::vector<int> rename(static_cast<std::size_t>(size), -1);
    std::vector<bool> taken(static_cast<std::size_t>(size), false);
    for (int round = 0; round < size; ++round) {
        int bestT = -1;
        int bestR = -1;
        for (int t = 0; t < size; ++t) {
            if (rename[t] != -1) continue;
            for (int r = 0; r < size; ++r) {
                if (taken[r]) continue;
                if (bestT == -1 || overlap[t][r] > overlap[bestT][bestR]) {
                    bestT = t;
                    bestR = r;
                }
            }
        }
        rename[bestT] = bestR;
        taken[bestR] = true;
    }

    std::vector<int> aligned(target);
    for (int& c : aligned) {
        if (c != kUncolored) c = rename[c];
    }
    return aligned;
}
