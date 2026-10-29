// =============================================================================
// Samples.cpp - built-in datasets.
// =============================================================================
#include "Samples.h"

#include <algorithm>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

// Sample data is hard-coded, so any rejection here is a programming error.
void require(bool ok, const std::string& error) {
    if (!ok) throw std::logic_error("Built-in sample is invalid: " + error);
}

void addSubjects(ExamScheduler& s, const std::vector<std::pair<std::string, std::string>>& list) {
    std::string error;
    for (const auto& [code, name] : list) require(s.addSubject(code, name, 0, error), error);
}

void addBatchByCodes(ExamScheduler& s, const std::string& name, int students,
                     const std::vector<std::string>& codes) {
    std::vector<int> ids;
    for (const std::string& code : codes) {
        const int index = s.subjectIndexByCode(code);
        require(index != -1, "unknown code " + code);
        ids.push_back(s.subjects()[index].id);
    }
    std::string error;
    require(s.addBatch(name, students, ids, error), error);
}

void addSlots(ExamScheduler& s, int days, const std::vector<std::string>& times) {
    std::string error;
    for (int day = 1; day <= days; ++day) {
        for (const std::string& time : times) {
            require(s.addSlot("Day " + std::to_string(day) + " - " + time, error), error);
        }
    }
}

void loadSmallCollege(ExamScheduler& s) {
    addSubjects(s, {{"CS101", "Programming Fundamentals"},
                    {"CS102", "Data Structures"},
                    {"MA101", "Engineering Mathematics I"},
                    {"PH101", "Engineering Physics"},
                    {"EN101", "Technical English"},
                    {"EE101", "Basic Electrical Engineering"}});
    addBatchByCodes(s, "CSE Year 1", 60, {"CS101", "MA101", "PH101"});
    addBatchByCodes(s, "IT Year 1", 45, {"CS101", "EN101", "EE101"});
    addBatchByCodes(s, "ECE Year 1", 50, {"MA101", "EE101"});
    addBatchByCodes(s, "CSE Year 2", 55, {"CS102", "EN101"});
    addSlots(s, 3, {"09:00 AM", "02:00 PM"});
}

// Crown graph S3^0: Ai conflicts with Bj exactly when i != j. Subjects are
// added in the order A1, B1, A2, B2, A3, B3 so input-order greedy is fooled:
// A1=1, B1=1, A2=2, B2=2, A3=3, B3=3, while {A1,A2,A3} / {B1,B2,B3} needs 2.
void loadCrownGraph(ExamScheduler& s) {
    addSubjects(s, {{"A1", "Algorithms Section A1"},
                    {"B1", "Biology Section B1"},
                    {"A2", "Algorithms Section A2"},
                    {"B2", "Biology Section B2"},
                    {"A3", "Algorithms Section A3"},
                    {"B3", "Biology Section B3"}});
    addBatchByCodes(s, "Batch A1-B2", 30, {"A1", "B2"});
    addBatchByCodes(s, "Batch A1-B3", 30, {"A1", "B3"});
    addBatchByCodes(s, "Batch A2-B1", 30, {"A2", "B1"});
    addBatchByCodes(s, "Batch A2-B3", 30, {"A2", "B3"});
    addBatchByCodes(s, "Batch A3-B1", 30, {"A3", "B1"});
    addBatchByCodes(s, "Batch A3-B2", 30, {"A3", "B2"});
    addSlots(s, 2, {"09:00 AM", "02:00 PM"});
}

// 30 subjects, 45 random batches of 3-5 subjects. std::mt19937 is fully
// specified by the standard, and we avoid std::uniform_int_distribution
// (implementation-defined), so every compiler builds the same dataset.
void loadStressTest(ExamScheduler& s) {
    constexpr int kSubjects = 30;
    constexpr int kBatches = 45;
    std::mt19937 rng(20241029u);
    auto pick = [&rng](int low, int high) {
        return low + static_cast<int>(rng() % static_cast<unsigned>(high - low + 1));
    };

    std::string error;
    for (int i = 1; i <= kSubjects; ++i) {
        std::ostringstream code;
        code << "EL" << std::setw(2) << std::setfill('0') << i;
        require(s.addSubject(code.str(), "Elective Paper " + std::to_string(i), 0, error), error);
    }
    for (int b = 1; b <= kBatches; ++b) {
        const int size = pick(3, 5);
        std::vector<int> ids;
        while (static_cast<int>(ids.size()) < size) {
            const int id = s.subjects()[pick(0, kSubjects - 1)].id;
            if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
        }
        require(s.addBatch("Group " + std::to_string(b), pick(20, 80), ids, error), error);
    }
    addSlots(s, 3, {"09:00 AM", "02:00 PM", "05:30 PM"});
}

}  // namespace

std::string sampleDescription(SampleId id) {
    switch (id) {
        case SampleId::SmallCollege:
            return "Sample A - small college (6 subjects, 4 batches, 6 slots)";
        case SampleId::CrownGraph:
            return "Sample B - crown graph (greedy needs 3 slots, optimum is 2)";
        case SampleId::StressTest:
            return "Sample C - stress test (30 subjects, 45 random batches, 9 slots)";
    }
    return "Unknown sample";
}

void loadSample(ExamScheduler& scheduler, SampleId id) {
    scheduler.clear();
    switch (id) {
        case SampleId::SmallCollege: loadSmallCollege(scheduler); break;
        case SampleId::CrownGraph: loadCrownGraph(scheduler); break;
        case SampleId::StressTest: loadStressTest(scheduler); break;
    }
}
