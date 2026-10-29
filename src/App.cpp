// =============================================================================
// App.cpp - menu-driven console front end.
// =============================================================================
#include "App.h"

#include "Backtracking.h"
#include "BranchAndBound.h"
#include "Samples.h"
#include "Tests.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {

constexpr int kMaxStudents = 1'000'000;
const char* const kDefaultDataFile = "timetable_data.txt";
const char* const kSessionTimes[] = {"09:00 AM", "02:00 PM", "05:30 PM"};

std::string formatMs(double ms) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(ms < 10.0 ? 3 : 1) << ms;
    return out.str();
}

std::string formatPercent(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << value << '%';
    return out.str();
}

// Subject indices grouped by colour; colours < 0 are skipped.
std::vector<std::vector<int>> groupByColor(const std::vector<int>& colors) {
    std::vector<std::vector<int>> groups(static_cast<std::size_t>(countColors(colors)));
    for (std::size_t v = 0; v < colors.size(); ++v) {
        if (colors[v] >= 0) groups[colors[v]].push_back(static_cast<int>(v));
    }
    return groups;
}

// Joins items after `prefix`, wrapping at `width` with aligned continuation lines.
std::vector<std::string> wrapItems(const std::string& prefix, const std::vector<std::string>& items,
                                   std::size_t width) {
    std::vector<std::string> lines;
    std::string current = prefix;
    const std::string indent(prefix.size(), ' ');
    bool first = true;
    for (const std::string& item : items) {
        const std::string piece = (first ? "" : ", ") + item;
        if (!first && current.size() + piece.size() > width) {
            lines.push_back(current + ",");
            current = indent + item;
        } else {
            current += piece;
        }
        first = false;
    }
    if (items.empty()) current += "(empty)";
    lines.push_back(current);
    return lines;
}

}  // namespace

// ----------------------------------------------------------------------------
// main loop
// ----------------------------------------------------------------------------
int App::run() {
    try {
        while (true) {
            showMenu();
            const int choice = console_.readInt("Enter choice (1-16): ", 1, 16);
            if (!handleChoice(choice)) break;
        }
    } catch (const InputClosed&) {
        std::cout << "\nInput closed - exiting.\n";
    }
    std::cout << "Goodbye!\n";
    return 0;
}

void App::showMenu() const {
    const Graph& g = scheduler_.graph();
    std::cout << '\n';
    console_.separator('=');
    std::cout << "   " << console_.bold("SMART UNIVERSITY EXAMINATION TIMETABLE GENERATOR") << '\n';
    std::cout << "   Greedy | Graph Colouring | Backtracking | Branch and Bound\n";
    console_.separator('=');
    std::cout << "  Data: " << scheduler_.subjects().size() << " subjects | "
              << scheduler_.batches().size() << " batches | " << scheduler_.slots().size()
              << " slots (" << scheduler_.availableSlots().size() << " available) | "
              << g.edgeCount() << " conflicts\n";
    console_.separator();
    std::cout << "   1. Manage subjects (add / remove)\n"
              << "   2. Manage student batches (add / remove)\n"
              << "   3. Manage time slots (add / remove / enable / disable)\n"
              << "   4. View all data\n"
              << "   5. View conflict graph\n"
              << "   6. Generate timetable - Greedy\n"
              << "   7. Generate timetable - Backtracking (slot limit k)\n"
              << "   8. Generate optimal timetable - Branch and Bound\n"
              << "   9. COMPARE: Basic Greedy vs Optimal\n"
              << "  10. Display statistics\n"
              << "  11. Load built-in sample dataset\n"
              << "  12. Save / load data file\n"
              << "  13. Clear all data\n"
              << "  14. Exit\n"
              << "  15. Toggle ANSI colours [" << (console_.colorsEnabled() ? "ON" : "OFF") << "]\n"
              << "  16. Run self-tests\n";
    console_.separator();
}

bool App::handleChoice(int choice) {
    switch (choice) {
        case 1: manageSubjects(); break;
        case 2: manageBatches(); break;
        case 3: manageSlots(); break;
        case 4: viewAllData(); break;
        case 5: viewConflictGraph(); break;
        case 6: runGreedy(); break;
        case 7: runBacktracking(); break;
        case 8: runBranchAndBound(); break;
        case 9: compareGreedyVsOptimal(); break;
        case 10: showStatistics(); break;
        case 11: loadSampleMenu(); break;
        case 12: fileMenu(); break;
        case 13: clearAll(); break;
        case 14: return false;
        case 15:
            console_.toggleColors();
            std::cout << "ANSI colours " << (console_.colorsEnabled() ? "enabled" : "disabled")
                      << ". (If you see codes like [36m, your terminal lacks ANSI support.)\n";
            return true;
        case 16: runTests(); break;
        default: break;
    }
    console_.pause();
    return true;
}

// ----------------------------------------------------------------------------
// small helpers
// ----------------------------------------------------------------------------
std::string App::codeOf(int index) const {
    return scheduler_.subjects()[static_cast<std::size_t>(index)].code;
}

std::string App::codesOf(const std::vector<int>& indices) const {
    std::string text;
    for (int index : indices) text += (text.empty() ? "" : ", ") + codeOf(index);
    return text.empty() ? "-" : text;
}

bool App::requireSubjects() const {
    if (!scheduler_.subjects().empty()) return true;
    std::cout << console_.warn("No subjects yet. Add subjects (menu 1) or load a sample (menu 11).")
              << '\n';
    return false;
}

bool App::requireAvailableSlots() const {
    if (scheduler_.slots().empty()) {
        std::cout << console_.warn("No time slots defined. Please add slots first (menu 3).")
                  << '\n';
        return false;
    }
    if (scheduler_.availableSlots().empty()) {
        std::cout << console_.warn("All time slots are disabled. Enable or add slots (menu 3).")
                  << '\n';
        return false;
    }
    return true;
}

// Accepts subject codes or ids separated by spaces and/or commas.
bool App::parseSubjectList(const std::string& text, std::vector<int>& ids) const {
    std::string cleaned = text;
    std::replace(cleaned.begin(), cleaned.end(), ',', ' ');
    std::istringstream tokens(cleaned);
    std::string token;
    ids.clear();
    while (tokens >> token) {
        int index = scheduler_.subjectIndexByCode(token);
        const bool numeric = std::all_of(token.begin(), token.end(), [](char ch) {
            return std::isdigit(static_cast<unsigned char>(ch)) != 0;
        });
        if (index == -1 && numeric && token.size() < 9) {
            index = scheduler_.subjectIndexById(std::stoi(token));
        }
        if (index == -1) {
            std::cout << console_.bad("  Unknown subject '" + token + "'.") << '\n';
            return false;
        }
        const int id = scheduler_.subjects()[static_cast<std::size_t>(index)].id;
        if (std::find(ids.begin(), ids.end(), id) != ids.end()) {
            std::cout << console_.warn("  '" + token + "' listed twice - ignoring duplicate.")
                      << '\n';
            continue;
        }
        ids.push_back(id);
    }
    return true;
}

// ----------------------------------------------------------------------------
// 1. subjects
// ----------------------------------------------------------------------------
void App::manageSubjects() {
    console_.header("MANAGE SUBJECTS");
    std::cout << "  1. Add subject\n  2. Remove subject\n  0. Back\n";
    const int choice = console_.readInt("Choice: ", 0, 2);
    if (choice == 1) addSubject();
    if (choice == 2) removeSubject();
}

void App::addSubject() {
    const std::string code = console_.readText("Subject code (e.g. CS101): ", "Code");
    if (scheduler_.subjectIndexByCode(code) != -1) {
        std::cout << console_.bad("Subject code '" + ExamScheduler::toUpper(code) +
                                  "' already exists.")
                  << '\n';
        return;
    }
    const std::string name = console_.readText("Subject name: ", "Name");
    const int students = console_.readInt(
        "Student count (0 = work it out from batches): ", 0, kMaxStudents);
    std::string error;
    if (scheduler_.addSubject(code, name, students, error)) {
        const Subject& added = scheduler_.subjects().back();
        std::cout << console_.good("Added subject #" + std::to_string(added.id) + " " +
                                   added.code + ".")
                  << '\n';
    } else {
        std::cout << console_.bad(error) << '\n';
    }
}

void App::removeSubject() {
    if (!requireSubjects()) return;
    printSubjects();
    const int id = console_.readInt("Subject id to remove (0 = cancel): ", 0, 1'000'000'000);
    if (id == 0) return;
    const int index = scheduler_.subjectIndexById(id);
    if (index == -1) {
        std::cout << console_.bad("No subject with id " + std::to_string(id) + ".") << '\n';
        return;
    }
    const std::string batches = scheduler_.batchNamesForSubject(index);
    if (batches != "-" &&
        !console_.readYesNo(codeOf(index) + " is taken by: " + batches +
                            ". Remove it from those batches too?")) {
        return;
    }
    std::string error;
    if (scheduler_.removeSubject(id, error)) {
        std::cout << console_.good("Subject removed; batches and conflict graph updated.") << '\n';
    } else {
        std::cout << console_.bad(error) << '\n';
    }
}

// ----------------------------------------------------------------------------
// 2. batches
// ----------------------------------------------------------------------------
void App::manageBatches() {
    console_.header("MANAGE STUDENT BATCHES");
    std::cout << "  1. Add batch and assign subjects\n  2. Remove batch\n  0. Back\n";
    const int choice = console_.readInt("Choice: ", 0, 2);
    if (choice == 1) addBatch();
    if (choice == 2) removeBatch();
}

void App::addBatch() {
    if (!requireSubjects()) return;
    const std::string name = console_.readText("Batch name (e.g. CSE Year 1): ", "Batch name");
    const int students = console_.readInt("Number of students: ", 1, kMaxStudents);
    printSubjects();
    std::vector<int> ids;
    const std::string list =
        console_.readLine("Subjects for this batch (codes or ids, separated by spaces/commas): ");
    if (!parseSubjectList(list, ids)) return;
    if (ids.size() < 2) {
        std::cout << console_.warn("Note: a batch with fewer than 2 subjects creates no conflicts.")
                  << '\n';
    }
    std::string error;
    if (scheduler_.addBatch(name, students, ids, error)) {
        std::cout << console_.good("Batch added. Conflict graph now has " +
                                   std::to_string(scheduler_.graph().edgeCount()) + " edges.")
                  << '\n';
    } else {
        std::cout << console_.bad(error) << '\n';
    }
}

void App::removeBatch() {
    if (scheduler_.batches().empty()) {
        std::cout << console_.warn("No batches to remove.") << '\n';
        return;
    }
    printBatches();
    const int id = console_.readInt("Batch id to remove (0 = cancel): ", 0, 1'000'000'000);
    if (id == 0) return;
    std::string error;
    if (scheduler_.removeBatch(id, error)) {
        std::cout << console_.good("Batch removed; conflict graph updated.") << '\n';
    } else {
        std::cout << console_.bad(error) << '\n';
    }
}

// ----------------------------------------------------------------------------
// 3. slots
// ----------------------------------------------------------------------------
void App::manageSlots() {
    console_.header("MANAGE TIME SLOTS");
    printSlots();
    std::cout << "\n  1. Add a slot\n  2. Quick-add slots (days x sessions)\n"
              << "  3. Remove a slot\n  4. Enable a slot\n  5. Disable a slot\n  0. Back\n";
    const int choice = console_.readInt("Choice: ", 0, 5);
    std::string error;
    switch (choice) {
        case 1: {
            const std::string label =
                console_.readText("Slot label (e.g. Day 1 - 09:00 AM): ", "Label");
            if (scheduler_.addSlot(label, error)) {
                std::cout << console_.good("Slot added.") << '\n';
            } else {
                std::cout << console_.bad(error) << '\n';
            }
            break;
        }
        case 2: quickAddSlots(); break;
        case 3: {
            if (scheduler_.slots().empty()) break;
            const int id = console_.readInt("Slot id to remove: ", 1, 1'000'000'000);
            std::cout << (scheduler_.removeSlot(id, error) ? console_.good("Slot removed.")
                                                           : console_.bad(error))
                      << '\n';
            break;
        }
        case 4: toggleSlot(true); break;
        case 5: toggleSlot(false); break;
        default: break;
    }
}

void App::quickAddSlots() {
    const int days = console_.readInt("Number of exam days (1-30): ", 1, 30);
    const int sessions = console_.readInt("Sessions per day (1-3: 09:00 AM, 02:00 PM, 05:30 PM): ",
                                          1, 3);
    int added = 0;
    std::string error;
    for (int day = 1; day <= days; ++day) {
        for (int s = 0; s < sessions; ++s) {
            const std::string label = "Day " + std::to_string(day) + " - " + kSessionTimes[s];
            if (scheduler_.addSlot(label, error)) {
                ++added;
            } else {
                std::cout << console_.warn("  Skipped: " + error) << '\n';
            }
        }
    }
    std::cout << console_.good("Added " + std::to_string(added) + " slots.") << '\n';
}

void App::toggleSlot(bool available) {
    if (scheduler_.slots().empty()) return;
    const int id = console_.readInt("Slot id: ", 1, 1'000'000'000);
    std::string error;
    if (scheduler_.setSlotAvailable(id, available, error)) {
        std::cout << console_.good(std::string("Slot ") + (available ? "enabled." : "disabled."))
                  << '\n';
    } else {
        std::cout << console_.bad(error) << '\n';
    }
}

// ----------------------------------------------------------------------------
// 4. view data
// ----------------------------------------------------------------------------
void App::printSubjects() const {
    std::cout << '\n' << console_.bold("SUBJECTS") << '\n';
    if (scheduler_.subjects().empty()) {
        std::cout << "  (none)\n";
        return;
    }
    std::cout << std::left << "  " << std::setw(5) << "ID" << std::setw(9) << "Code"
              << std::setw(32) << "Name" << std::right << std::setw(9) << "Students" << "  "
              << "Batches\n";
    for (std::size_t i = 0; i < scheduler_.subjects().size(); ++i) {
        const Subject& s = scheduler_.subjects()[i];
        const int index = static_cast<int>(i);
        std::cout << std::left << "  " << std::setw(5) << s.id << std::setw(9) << s.code
                  << std::setw(32) << s.name.substr(0, 31) << std::right << std::setw(9)
                  << scheduler_.studentsForSubject(index) << (s.studentCount > 0 ? " " : "*")
                  << " " << scheduler_.batchNamesForSubject(index) << '\n';
    }
    std::cout << "  (* = derived from batches)\n";
}

void App::printBatches() const {
    std::cout << '\n' << console_.bold("STUDENT BATCHES") << '\n';
    if (scheduler_.batches().empty()) {
        std::cout << "  (none)\n";
        return;
    }
    std::cout << std::left << "  " << std::setw(5) << "ID" << std::setw(22) << "Name"
              << std::right << std::setw(9) << "Students" << "  Subjects\n";
    for (const Batch& b : scheduler_.batches()) {
        std::vector<int> indices;
        for (int id : b.subjectIds) indices.push_back(scheduler_.subjectIndexById(id));
        std::cout << std::left << "  " << std::setw(5) << b.id << std::setw(22)
                  << b.name.substr(0, 21) << std::right << std::setw(9) << b.studentCount
                  << "  " << codesOf(indices) << '\n';
    }
}

void App::printSlots() const {
    std::cout << '\n' << console_.bold("TIME SLOTS") << '\n';
    if (scheduler_.slots().empty()) {
        std::cout << "  (none - add slots to generate a timetable)\n";
        return;
    }
    std::cout << std::left << "  " << std::setw(5) << "ID" << std::setw(28) << "Label"
              << "Status\n";
    for (const TimeSlot& t : scheduler_.slots()) {
        std::cout << std::left << "  " << std::setw(5) << t.id << std::setw(28) << t.label
                  << (t.available ? console_.good("available") : console_.warn("disabled"))
                  << '\n';
    }
}

void App::viewAllData() const {
    console_.header("ALL DATA");
    if (scheduler_.empty()) {
        std::cout << "The dataset is empty. Load a sample (menu 11) or add data.\n";
        return;
    }
    printSubjects();
    printBatches();
    printSlots();
}

// ----------------------------------------------------------------------------
// 5. conflict graph
// ----------------------------------------------------------------------------
void App::viewConflictGraph() const {
    console_.header("CONFLICT GRAPH");
    if (!requireSubjects()) return;
    const Graph& g = scheduler_.graph();

    std::cout << console_.bold("Adjacency list") << " (vertex = subject, edge = shared batch)\n";
    for (int v = 0; v < g.vertexCount(); ++v) {
        std::cout << "  " << std::left << std::setw(8) << codeOf(v) << "(deg " << g.degree(v)
                  << ") -> " << codesOf(g.neighbors(v)) << '\n';
    }

    std::cout << '\n' << console_.bold("Conflicting pairs") << " (" << g.edgeCount() << ")\n";
    if (g.edgeCount() == 0) {
        std::cout << "  No conflicts - every exam could be held in a single slot.\n";
        return;
    }
    for (const ConflictEdge& e : g.edges()) {
        std::string reasons;
        for (const std::string& r : e.reasons) reasons += (reasons.empty() ? "" : ", ") + r;
        std::cout << "  " << std::left << std::setw(7) << codeOf(e.u) << " <-> " << std::setw(7)
                  << codeOf(e.v) << " | shared by: " << reasons << " | " << e.weight
                  << " students\n";
    }
}

// ----------------------------------------------------------------------------
// result presentation (used by menus 6, 7, 8)
// ----------------------------------------------------------------------------
void App::printTimetable(const std::vector<int>& colors,
                         const std::vector<TimeSlot>& labels) const {
    const auto groups = groupByColor(colors);
    std::cout << std::left << "  " << std::setw(6) << "Slot" << std::setw(24) << "Time"
              << "Exams\n";
    console_.separator();
    for (std::size_t c = 0; c < groups.size() && c < labels.size(); ++c) {
        const int slot = static_cast<int>(c);
        std::ostringstream left;
        left << std::left << "  " << std::setw(6) << (c + 1) << std::setw(24) << labels[c].label;
        if (groups[c].empty()) {
            std::cout << console_.slotColor(left.str(), slot) << "(free)\n";
        }
        for (std::size_t i = 0; i < groups[c].size(); ++i) {
            const int v = groups[c][i];
            const Subject& s = scheduler_.subjects()[static_cast<std::size_t>(v)];
            const std::string prefix = (i == 0) ? left.str() : std::string(32, ' ');
            std::cout << console_.slotColor(prefix, slot) << std::left << std::setw(8) << s.code
                      << s.name << " (" << scheduler_.studentsForSubject(v) << " students)\n";
        }
    }
    console_.separator();
}

void App::printVerification(const std::vector<int>& colors) const {
    const ValidationReport report = validateColoring(scheduler_.graph(), colors);
    if (report.valid) {
        std::cout << console_.good("Verified: conflict-free") << " ("
                  << scheduler_.subjects().size() << " subjects scheduled, "
                  << scheduler_.graph().edgeCount() << " conflict pairs checked)\n";
        return;
    }
    std::cout << console_.bad("Verification FAILED:") << '\n';
    for (const auto& [u, v] : report.clashes) {
        std::cout << "  - " << codeOf(u) << " and " << codeOf(v) << " share slot "
                  << colors[u] + 1 << '\n';
    }
    if (!report.uncolored.empty()) {
        std::cout << "  - Not scheduled: " << codesOf(report.uncolored) << '\n';
    }
}

void App::presentResult(const ColoringResult& result) {
    console_.header(result.method);
    std::cout << "  Slots used          : " << result.slotsUsed << '\n'
              << "  Time taken          : " << formatMs(result.timeMs) << " ms\n";
    if (result.nodesExplored > 0 || result.provenOptimal) {
        std::cout << "  Nodes explored      : " << result.nodesExplored << '\n';
    }
    if (result.backtracks > 0) std::cout << "  Backtracks          : " << result.backtracks << '\n';
    if (result.upperBound > 0) {
        std::cout << "  Nodes pruned        : " << result.nodesPruned << '\n'
                  << "  Initial bounds      : LB (clique) = " << result.lowerBound
                  << ", UB (best greedy) = " << result.upperBound << '\n'
                  << "  Optimality proven   : " << (result.provenOptimal ? "Yes" : "No") << '\n';
    }
    if (result.cutOff) {
        std::cout << console_.warn("  Search cut off - result may not be optimal") << '\n';
    }

    const Schedule schedule = scheduler_.makeSchedule(result.colors);
    std::cout << '\n';
    if (schedule.feasible) {
        printTimetable(result.colors, schedule.slotForColor);
    } else {
        std::cout << console_.bad("INFEASIBLE: Need " + std::to_string(schedule.slotsNeeded) +
                                  " slots, only " + std::to_string(schedule.slotsAvailable) +
                                  " available (short by " +
                                  std::to_string(schedule.slotsNeeded - schedule.slotsAvailable) +
                                  ").")
                  << '\n'
                  << "  Subjects that fall outside the available slots: "
                  << codesOf(schedule.outsideSlots) << '\n';
        if (console_.readYesNo("Show a partial timetable using the available slots?")) {
            std::cout << "\nPARTIAL TIMETABLE (subjects re-fitted where possible)\n";
            printTimetable(schedule.partialColors, schedule.slotForColor);
            if (schedule.unplaced.empty()) {
                std::cout << console_.good("All subjects were re-fitted into the available slots "
                                           "(try Branch and Bound for a better colouring).")
                          << '\n';
            } else {
                std::cout << console_.bad("Cannot be placed: " + codesOf(schedule.unplaced))
                          << '\n';
            }
        }
    }
    printVerification(result.colors);
    scheduler_.recordResult(result);
}

// ----------------------------------------------------------------------------
// 6, 7, 8. generators
// ----------------------------------------------------------------------------
void App::runGreedy() {
    console_.header("GREEDY COLOURING");
    if (!requireSubjects() || !requireAvailableSlots()) return;
    std::cout << "  1. Input order\n  2. Descending degree (Welsh-Powell)\n"
              << "  3. DSATUR (highest saturation first)\n";
    const int choice = console_.readInt("Ordering: ", 1, 3);
    const GreedyOrder order = choice == 1   ? GreedyOrder::Input
                              : choice == 2 ? GreedyOrder::WelshPowell
                                            : GreedyOrder::Dsatur;
    presentResult(greedyColoring(scheduler_.graph(), order));
}

void App::runBacktracking() {
    console_.header("BACKTRACKING (m-COLOURING)");
    if (!requireSubjects() || !requireAvailableSlots()) return;
    const int n = static_cast<int>(scheduler_.subjects().size());
    const int available = static_cast<int>(scheduler_.availableSlots().size());
    std::cout << "  Available slots: " << available
              << " | clique lower bound: " << greedyClique(scheduler_.graph()).size() << '\n';
    const int k = console_.readInt("Slot limit k (1-" + std::to_string(n) + "): ", 1, n);

    BacktrackingColorer solver(scheduler_.graph());
    const ColoringResult result = solver.canColor(k);
    if (result.success) {
        presentResult(result);
        return;
    }
    if (result.cutOff) {
        std::cout << console_.warn("Search cut off after " + std::to_string(result.nodesExplored) +
                                   " nodes (" + formatMs(result.timeMs) +
                                   " ms) - could not decide whether " + std::to_string(k) +
                                   " slots suffice.")
                  << '\n';
    } else {
        std::cout << console_.bad("Impossible: the exams cannot fit in " + std::to_string(k) +
                                  " slots.")
                  << "\n  Proof by exhaustive search: " << result.nodesExplored << " nodes, "
                  << result.backtracks << " backtracks, " << formatMs(result.timeMs) << " ms.\n";
    }
}

void App::runBranchAndBound() {
    console_.header("BRANCH AND BOUND (MINIMUM SLOTS)");
    if (!requireSubjects() || !requireAvailableSlots()) return;
    SearchLimits limits;
    if (!console_.readYesNo("Use default limits (5,000,000 nodes, 5 seconds)?")) {
        limits.nodeLimit = console_.readLong("Node limit (10 - 1,000,000,000): ", 10,
                                             1'000'000'000);
        limits.timeLimitMs =
            static_cast<double>(console_.readLong("Time limit in ms (1 - 600000): ", 1, 600'000));
    }
    presentResult(BranchAndBoundColorer(scheduler_.graph(), limits).solve());
}

// ----------------------------------------------------------------------------
// 9. comparison
// ----------------------------------------------------------------------------
void App::compareGreedyVsOptimal() {
    if (!requireSubjects()) return;
    const Graph& graph = scheduler_.graph();
    const ColoringResult greedy = greedyColoring(graph, GreedyOrder::Input);
    const ColoringResult optimal = BranchAndBoundColorer(graph).solve();
    // Rename optimal slots to match greedy where possible so "moves" are real.
    const std::vector<int> aligned = alignColors(greedy.colors, optimal.colors);
    const bool greedyOk = validateColoring(graph, greedy.colors).valid;
    const bool optimalOk = validateColoring(graph, aligned).valid;

    std::cout << "\n  " << console_.bold("======= GREEDY vs OPTIMAL =======") << '\n';
    auto row = [](const std::string& metric, const std::string& g, const std::string& o) {
        std::cout << "  " << std::left << std::setw(23) << metric << std::setw(14) << g << o
                  << '\n';
    };
    row("Metric", "Greedy", "Optimal");
    row("Slots used", std::to_string(greedy.slotsUsed), std::to_string(optimal.slotsUsed));
    row("Time taken (ms)", formatMs(greedy.timeMs), formatMs(optimal.timeMs));
    row("Nodes explored", "-", std::to_string(optimal.nodesExplored));
    row("Conflict-free?", greedyOk ? "Yes" : "No", optimalOk ? "Yes" : "No");
    row("Optimality proven?", "No", optimal.provenOptimal ? "Yes" : "No");
    if (optimal.cutOff) {
        std::cout << console_.warn("  Search cut off - result may not be optimal") << '\n';
    } else if (optimal.nodesExplored == 0 && optimal.slotsUsed > 0) {
        std::cout << "  (Best greedy UB = clique LB = " << optimal.slotsUsed
                  << ", so optimality was proven without searching.)\n";
    }

    // Side-by-side timetables.
    constexpr std::size_t kColumn = 34;
    const auto greedyGroups = groupByColor(greedy.colors);
    const auto optimalGroups = groupByColor(aligned);
    std::cout << "\n  " << std::left << std::setw(static_cast<int>(kColumn)) << "Greedy timetable"
              << "Optimal timetable\n";
    const std::size_t rows = std::max(greedyGroups.size(), optimalGroups.size());
    for (std::size_t c = 0; c < rows; ++c) {
        const std::string prefix = "Slot " + std::to_string(c + 1) + " | ";
        auto cell = [&](const std::vector<std::vector<int>>& groups) {
            std::vector<std::string> codes;
            if (c >= groups.size()) return std::vector<std::string>{};
            for (int v : groups[c]) codes.push_back(codeOf(v));
            return wrapItems(prefix, codes, kColumn - 2);
        };
        const auto leftLines = cell(greedyGroups);
        const auto rightLines = cell(optimalGroups);
        const int slot = static_cast<int>(c);
        for (std::size_t i = 0; i < std::max(leftLines.size(), rightLines.size()); ++i) {
            const std::string l = i < leftLines.size() ? leftLines[i] : "";
            const std::string r = i < rightLines.size() ? rightLines[i] : "";
            const std::string padded = l + std::string(kColumn - std::min(kColumn, l.size()), ' ');
            std::cout << "  " << console_.slotColor(padded, slot) << console_.slotColor(r, slot)
                      << '\n';
        }
    }

    // Verdict.
    const int saved = greedy.slotsUsed - optimal.slotsUsed;
    std::cout << '\n';
    if (saved > 0) {
        const int percent = (100 * saved + greedy.slotsUsed / 2) / greedy.slotsUsed;
        std::cout << console_.good("  Result: Optimal search saved " + std::to_string(saved) +
                                   (saved == 1 ? " slot (" : " slots (") +
                                   std::to_string(percent) + "% fewer slots).")
                  << '\n';
    } else {
        std::cout << console_.good("  Greedy already found the optimal answer for this input.")
                  << '\n';
    }

    // Subjects that changed slot between the two timetables.
    std::vector<std::string> moves;
    for (std::size_t v = 0; v < aligned.size(); ++v) {
        if (aligned[v] != greedy.colors[v]) {
            moves.push_back(codeOf(static_cast<int>(v)) + ": Slot " +
                            std::to_string(greedy.colors[v] + 1) + " -> Slot " +
                            std::to_string(aligned[v] + 1));
        }
    }
    std::cout << "  Subjects moved between slots: "
              << (moves.empty() ? "none" : std::to_string(moves.size())) << '\n';
    for (const std::string& move : moves) std::cout << "    " << move << '\n';

    const int available = static_cast<int>(scheduler_.availableSlots().size());
    if (scheduler_.slots().empty()) {
        std::cout << console_.warn("  Note: no time slots defined yet - add slots to turn this "
                                   "into a real timetable.")
                  << '\n';
    } else if (optimal.slotsUsed > available) {
        std::cout << console_.bad("  Note: even the optimum needs " +
                                  std::to_string(optimal.slotsUsed) + " slots, only " +
                                  std::to_string(available) + " available.")
                  << '\n';
    }

    std::cout << "\n  Greedy  : ";
    printVerification(greedy.colors);
    std::cout << "  Optimal : ";
    printVerification(aligned);

    ColoringResult recorded = optimal;
    recorded.colors = aligned;
    scheduler_.recordResult(recorded);
}

// ----------------------------------------------------------------------------
// 10. statistics
// ----------------------------------------------------------------------------
void App::showStatistics() const {
    console_.header("STATISTICS");
    const Graph& g = scheduler_.graph();
    const auto& subjects = scheduler_.subjects();
    const int available = static_cast<int>(scheduler_.availableSlots().size());
    const int totalSlots = static_cast<int>(scheduler_.slots().size());

    auto line = [](const std::string& label, const std::string& value) {
        std::cout << "  " << std::left << std::setw(34) << label << ": " << value << '\n';
    };
    std::ostringstream density;
    density << std::fixed << std::setprecision(3) << g.density();
    std::ostringstream avgDegree;
    avgDegree << std::fixed << std::setprecision(2) << g.avgDegree();

    int mostConflicted = -1;
    int isolated = 0;
    for (int v = 0; v < g.vertexCount(); ++v) {
        if (g.degree(v) == 0) ++isolated;
        if (mostConflicted == -1 || g.degree(v) > g.degree(mostConflicted)) mostConflicted = v;
    }

    line("Total subjects", std::to_string(subjects.size()));
    line("Total batches", std::to_string(scheduler_.batches().size()));
    line("Total slots (available/unavailable)", std::to_string(totalSlots) + " (" +
                                                    std::to_string(available) + "/" +
                                                    std::to_string(totalSlots - available) + ")");
    line("Total conflict edges", std::to_string(g.edgeCount()));
    line("Graph density 2E/(V(V-1))", density.str());
    line("Maximum degree", std::to_string(g.maxDegree()));
    line("Average degree", avgDegree.str());
    line("Most conflicted subject",
         mostConflicted == -1 ? "-"
                              : codeOf(mostConflicted) + " (" +
                                    std::to_string(g.degree(mostConflicted)) + " conflicts)");
    line("Isolated subjects", std::to_string(isolated));
    line("Connected components", std::to_string(g.connectedComponents()));
    line("Clique lower bound", std::to_string(greedyClique(g).size()) + " slots");
    line("Total students affected", std::to_string(scheduler_.totalStudents()));

    const auto& last = scheduler_.lastResult();
    std::cout << '\n' << console_.bold("Last generated timetable") << '\n';
    if (!last) {
        std::cout << "  (none yet - generate one with menu 6, 7, 8 or 9)\n";
        return;
    }
    const auto groups = groupByColor(last->colors);
    line("Method", last->method);
    line("Slots used", std::to_string(last->slotsUsed));
    std::size_t largest = 0;
    std::size_t smallest = 0;
    for (std::size_t c = 0; c < groups.size(); ++c) {
        if (groups[c].size() > groups[largest].size()) largest = c;
        if (groups[c].size() < groups[smallest].size()) smallest = c;
        line("  Slot " + std::to_string(c + 1) + " subjects",
             std::to_string(groups[c].size()) + " (" + codesOf(groups[c]) + ")");
    }
    if (!groups.empty()) {
        line("Largest slot", "Slot " + std::to_string(largest + 1) + " (" +
                                 std::to_string(groups[largest].size()) + " exams)");
        line("Smallest slot", "Slot " + std::to_string(smallest + 1) + " (" +
                                  std::to_string(groups[smallest].size()) + " exams)");
    }
    line("Slot utilisation", available == 0 ? "n/a (no available slots)"
                                            : formatPercent(100.0 * last->slotsUsed / available) +
                                                  " of available slots");
}

// ----------------------------------------------------------------------------
// 11, 12, 13. samples, files, clear
// ----------------------------------------------------------------------------
void App::loadSampleMenu() {
    console_.header("BUILT-IN SAMPLES");
    const SampleId ids[] = {SampleId::SmallCollege, SampleId::CrownGraph, SampleId::StressTest};
    for (int i = 0; i < 3; ++i) std::cout << "  " << i + 1 << ". " << sampleDescription(ids[i]) << '\n';
    std::cout << "  0. Back\n";
    const int choice = console_.readInt("Choice: ", 0, 3);
    if (choice == 0) return;
    if (!scheduler_.empty() && !console_.readYesNo("This replaces the current data. Continue?")) {
        return;
    }
    loadSample(scheduler_, ids[choice - 1]);
    std::cout << console_.good("Loaded " + sampleDescription(ids[choice - 1]) + ".") << '\n';
}

void App::fileMenu() {
    console_.header("SAVE / LOAD");
    std::cout << "  1. Save data to file\n  2. Load data from file\n  0. Back\n";
    const int choice = console_.readInt("Choice: ", 0, 2);
    if (choice == 0) return;
    std::string path = console_.readLine(std::string("File name [") + kDefaultDataFile + "]: ");
    if (path.empty()) path = kDefaultDataFile;
    std::string error;
    if (choice == 1) {
        std::cout << (scheduler_.saveToFile(path, error) ? console_.good("Saved to " + path + ".")
                                                         : console_.bad(error))
                  << '\n';
    } else {
        std::cout << (scheduler_.loadFromFile(path, error)
                          ? console_.good("Loaded " + path + ".")
                          : console_.bad("Load failed (current data kept): " + error))
                  << '\n';
    }
}

void App::clearAll() {
    if (scheduler_.empty()) {
        std::cout << "Nothing to clear.\n";
        return;
    }
    if (console_.readYesNo("Delete all subjects, batches and slots?")) {
        scheduler_.clear();
        std::cout << console_.good("All data cleared.") << '\n';
    }
}

void App::runTests() const {
    const int failures = runSelfTests(std::cout);
    std::cout << (failures == 0 ? console_.good("All self-tests passed.")
                                : console_.bad(std::to_string(failures) + " test(s) failed."))
              << '\n';
}
