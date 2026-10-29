// =============================================================================
// Models.h - plain data records used by the whole project.
//
// Smart University Examination Timetable Generator
//   Vertex = subject (exam), Edge = shared student batch, Colour = time slot.
// =============================================================================
#pragma once

#include <string>
#include <vector>

// One examination paper. Becomes one vertex of the conflict graph.
struct Subject {
    int id = 0;               // auto-assigned, stable across edits
    std::string code;         // unique, stored upper-case, e.g. "CS101"
    std::string name;         // human-readable title
    int studentCount = 0;     // optional; 0 = derive from batches
};

// A group of students who all sit the same set of exams.
// Every pair of subjects inside one batch is a conflict edge.
struct Batch {
    int id = 0;
    std::string name;             // unique, e.g. "CSE Year 1"
    int studentCount = 0;
    std::vector<int> subjectIds;  // Subject::id values
};

// A period in which exams can be held. Only available slots are used.
struct TimeSlot {
    int id = 0;
    std::string label;        // e.g. "Day 1 - 09:00 AM"
    bool available = true;
};

// An undirected conflict between two subjects (vertex indices u < v).
struct ConflictEdge {
    int u = 0;
    int v = 0;
    int weight = 0;                    // total students shared by u and v
    std::vector<std::string> reasons;  // batch names that cause the conflict
};
