// =============================================================================
// Scheduler.h - owns all data (subjects, batches, slots), keeps the conflict
// graph in sync, maps colours to real time slots and handles file I/O.
// =============================================================================
#pragma once

#include "Coloring.h"
#include "Graph.h"
#include "Models.h"

#include <optional>
#include <string>
#include <vector>

// A colouring translated into real, available time slots.
struct Schedule {
    std::vector<int> colors;           // colour per subject index (input)
    int slotsNeeded = 0;               // colours used
    int slotsAvailable = 0;
    bool feasible = false;             // slotsNeeded <= slotsAvailable
    std::vector<TimeSlot> slotForColor;   // colour c -> c-th available slot
    // Partial timetable when infeasible: colour per subject, kUncolored if
    // the subject could not be placed in any available slot.
    std::vector<int> partialColors;
    std::vector<int> outsideSlots;     // subjects whose colour had no slot
    std::vector<int> unplaced;         // still unplaced after re-fitting
};

class ExamScheduler {
public:
    // --- subjects ------------------------------------------------------------
    bool addSubject(const std::string& code, const std::string& name, int students,
                    std::string& error);
    bool removeSubject(int id, std::string& error);  // also updates batches

    // --- batches -------------------------------------------------------------
    bool addBatch(const std::string& name, int students, const std::vector<int>& subjectIds,
                  std::string& error);
    bool removeBatch(int id, std::string& error);

    // --- slots ---------------------------------------------------------------
    bool addSlot(const std::string& label, std::string& error);
    bool removeSlot(int id, std::string& error);
    bool setSlotAvailable(int id, bool available, std::string& error);

    void clear();

    // --- queries -------------------------------------------------------------
    const std::vector<Subject>& subjects() const { return subjects_; }
    const std::vector<Batch>& batches() const { return batches_; }
    const std::vector<TimeSlot>& slots() const { return slots_; }
    const Graph& graph() const { return graph_; }
    bool empty() const { return subjects_.empty() && batches_.empty() && slots_.empty(); }

    int subjectIndexById(int id) const;                     // -1 if unknown
    int subjectIndexByCode(const std::string& code) const;  // case-insensitive
    std::vector<TimeSlot> availableSlots() const;
    int studentsForSubject(int index) const;  // explicit count or batch sum
    int totalStudents() const;                // sum over batches
    std::string batchNamesForSubject(int index) const;

    // Maps colours to available slots; builds a partial timetable if short.
    Schedule makeSchedule(const std::vector<int>& colors) const;

    // The last generated timetable (cleared when subjects/batches change).
    void recordResult(const ColoringResult& result) { lastResult_ = result; }
    const std::optional<ColoringResult>& lastResult() const { return lastResult_; }

    // --- persistence (pipe-separated text, see README) -----------------------
    bool saveToFile(const std::string& path, std::string& error) const;
    bool loadFromFile(const std::string& path, std::string& error);

    // Validation helpers shared with the UI.
    static std::string trim(const std::string& text);
    static std::string toUpper(std::string text);
    static bool validText(const std::string& text, const std::string& what, std::string& error);

private:
    bool insertSubject(const Subject& subject, std::string& error);
    bool insertBatch(const Batch& batch, std::string& error);
    bool insertSlot(const TimeSlot& slot, std::string& error);
    int batchIndexById(int id) const;
    int slotIndexById(int id) const;
    void rebuildGraph();

    std::vector<Subject> subjects_;
    std::vector<Batch> batches_;
    std::vector<TimeSlot> slots_;
    Graph graph_;
    std::optional<ColoringResult> lastResult_;
    int nextSubjectId_ = 1;
    int nextBatchId_ = 1;
    int nextSlotId_ = 1;
};
