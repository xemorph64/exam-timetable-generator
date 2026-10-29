// =============================================================================
// App.h - menu-driven console front end.
// =============================================================================
#pragma once

#include "Coloring.h"
#include "Console.h"
#include "MachineApi.h"
#include "Scheduler.h"

#include <string>
#include <vector>

class App {
public:
    int run();

private:
    void showMenu() const;
    bool handleChoice(int choice);  // false = exit

    // Menu actions
    void manageSubjects();
    void manageBatches();
    void manageSlots();
    void viewAllData() const;
    void viewConflictGraph() const;
    void runGreedy();
    void runBacktracking();
    void runBranchAndBound();
    void compareGreedyVsOptimal();
    void showStatistics() const;
    void loadSampleMenu();
    void fileMenu();
    void clearAll();
    void runTests() const;

    // Sub-actions
    void addSubject();
    void removeSubject();
    void addBatch();
    void removeBatch();
    void quickAddSlots();
    void toggleSlot(bool available);

    // Shared helpers
    bool requireSubjects() const;
    bool requireAvailableSlots() const;
    bool parseSubjectList(const std::string& text, std::vector<int>& ids) const;
    void printSubjects() const;
    void printBatches() const;
    void printSlots() const;
    void presentResult(const ColoringResult& result);
    void printTimetable(const std::vector<int>& colors, const std::vector<TimeSlot>& labels) const;
    void printVerification(const std::vector<int>& colors) const;
    std::string codeOf(int index) const;
    std::string codesOf(const std::vector<int>& indices) const;

    ExamScheduler scheduler_;
    Console console_;
    MachineApi machine_{scheduler_};  // "@..." lines from the web UI
};
