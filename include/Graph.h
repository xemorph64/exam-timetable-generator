// =============================================================================
// Graph.h - undirected conflict graph between subjects.
//
// Two representations are kept in sync:
//   * adjacency list   vector<vector<int>>  - O(V + E) space, fast neighbour scans
//   * adjacency matrix vector<vector<bool>> - O(V^2) space, O(1) hasEdge(), used
//                                             by backtracking and branch & bound
// Vertex i always corresponds to the i-th subject in the scheduler's list.
// =============================================================================
#pragma once

#include "Models.h"

#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

class Graph {
public:
    Graph() = default;
    explicit Graph(int vertexCount);

    // Appends an isolated vertex and returns its index. O(V) (matrix row/col).
    int addVertex();

    // Adds (or merges into) the edge {u, v}. A repeated edge accumulates its
    // weight and records the extra reason. Self-loops / bad indices are ignored.
    void addEdge(int u, int v, int weight = 0, const std::string& reason = "");

    bool hasEdge(int u, int v) const;                 // O(1)
    int degree(int v) const;                          // O(1)
    const std::vector<int>& neighbors(int v) const;   // O(1)

    int vertexCount() const;
    int edgeCount() const;
    const std::vector<ConflictEdge>& edges() const;

    void clear();

    // Rebuilds the whole graph from scratch: one vertex per subject and, for
    // every batch, an edge between every pair of its subjects.
    // Time: O(V^2 + sum over batches of s_b^2 * log E), Space: O(V^2 + E).
    void rebuildFromBatches(const std::vector<Subject>& subjects,
                            const std::vector<Batch>& batches);

    double density() const;          // 2E / (V(V-1)), 0 when V < 2
    int maxDegree() const;
    double avgDegree() const;        // 2E / V
    int connectedComponents() const; // BFS, O(V + E)

private:
    bool validVertex(int v) const;

    std::vector<std::vector<int>> adj_;
    std::vector<std::vector<bool>> matrix_;
    std::vector<ConflictEdge> edges_;
    std::map<std::pair<int, int>, std::size_t> edgeLookup_;  // (min,max) -> edges_ index
};
