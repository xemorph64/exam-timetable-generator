// =============================================================================
// Graph.cpp - conflict graph implementation.
// =============================================================================
#include "Graph.h"

#include <algorithm>
#include <queue>
#include <unordered_map>

Graph::Graph(int vertexCount) {
    for (int i = 0; i < vertexCount; ++i) addVertex();
}

bool Graph::validVertex(int v) const {
    return v >= 0 && v < vertexCount();
}

int Graph::addVertex() {
    const int index = vertexCount();
    adj_.emplace_back();
    for (auto& row : matrix_) row.push_back(false);
    matrix_.emplace_back(static_cast<std::size_t>(index + 1), false);
    return index;
}

void Graph::addEdge(int u, int v, int weight, const std::string& reason) {
    if (!validVertex(u) || !validVertex(v) || u == v) return;
    const std::pair<int, int> key{std::min(u, v), std::max(u, v)};

    auto found = edgeLookup_.find(key);
    if (found == edgeLookup_.end()) {
        ConflictEdge edge;
        edge.u = key.first;
        edge.v = key.second;
        edgeLookup_[key] = edges_.size();
        edges_.push_back(edge);
        adj_[u].push_back(v);
        adj_[v].push_back(u);
        matrix_[u][v] = true;
        matrix_[v][u] = true;
        found = edgeLookup_.find(key);
    }

    // Merge duplicates: accumulate shared students and remember every reason.
    ConflictEdge& edge = edges_[found->second];
    edge.weight += weight;
    if (!reason.empty() &&
        std::find(edge.reasons.begin(), edge.reasons.end(), reason) == edge.reasons.end()) {
        edge.reasons.push_back(reason);
    }
}

bool Graph::hasEdge(int u, int v) const {
    return validVertex(u) && validVertex(v) && matrix_[u][v];
}

int Graph::degree(int v) const {
    return validVertex(v) ? static_cast<int>(adj_[v].size()) : 0;
}

const std::vector<int>& Graph::neighbors(int v) const {
    return adj_.at(static_cast<std::size_t>(v));
}

int Graph::vertexCount() const { return static_cast<int>(adj_.size()); }

int Graph::edgeCount() const { return static_cast<int>(edges_.size()); }

const std::vector<ConflictEdge>& Graph::edges() const { return edges_; }

void Graph::clear() {
    adj_.clear();
    matrix_.clear();
    edges_.clear();
    edgeLookup_.clear();
}

void Graph::rebuildFromBatches(const std::vector<Subject>& subjects,
                               const std::vector<Batch>& batches) {
    clear();
    std::unordered_map<int, int> indexOfId;
    for (const Subject& subject : subjects) {
        indexOfId[subject.id] = addVertex();
    }

    for (const Batch& batch : batches) {
        // Resolve ids to vertex indices, skipping unknown ids and duplicates.
        std::vector<int> members;
        for (int id : batch.subjectIds) {
            const auto it = indexOfId.find(id);
            if (it != indexOfId.end() &&
                std::find(members.begin(), members.end(), it->second) == members.end()) {
                members.push_back(it->second);
            }
        }
        // A batch with fewer than two subjects creates no edges.
        for (std::size_t i = 0; i < members.size(); ++i) {
            for (std::size_t j = i + 1; j < members.size(); ++j) {
                addEdge(members[i], members[j], batch.studentCount, batch.name);
            }
        }
    }
}

double Graph::density() const {
    const double v = vertexCount();
    if (v < 2) return 0.0;
    return 2.0 * edgeCount() / (v * (v - 1.0));
}

int Graph::maxDegree() const {
    int best = 0;
    for (int v = 0; v < vertexCount(); ++v) best = std::max(best, degree(v));
    return best;
}

double Graph::avgDegree() const {
    if (vertexCount() == 0) return 0.0;
    return 2.0 * edgeCount() / vertexCount();
}

int Graph::connectedComponents() const {
    std::vector<bool> seen(static_cast<std::size_t>(vertexCount()), false);
    int components = 0;
    for (int start = 0; start < vertexCount(); ++start) {
        if (seen[start]) continue;
        ++components;
        std::queue<int> frontier;
        frontier.push(start);
        seen[start] = true;
        while (!frontier.empty()) {
            const int v = frontier.front();
            frontier.pop();
            for (int u : adj_[v]) {
                if (!seen[u]) {
                    seen[u] = true;
                    frontier.push(u);
                }
            }
        }
    }
    return components;
}
