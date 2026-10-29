// =============================================================================
// Scheduler.cpp - data management, slot mapping and persistence.
// =============================================================================
#include "Scheduler.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

namespace {

constexpr char kFieldSeparator = '|';
const char* const kFileHeader = "# Smart Exam Timetable data file v1";

std::vector<std::string> splitFields(const std::string& line, char separator) {
    std::vector<std::string> fields;
    std::string field;
    std::istringstream stream(line);
    while (std::getline(stream, field, separator)) fields.push_back(field);
    if (!line.empty() && line.back() == separator) fields.emplace_back();
    return fields;
}

bool parseInt(const std::string& text, int& value) {
    const std::string trimmed = ExamScheduler::trim(text);
    if (trimmed.empty()) return false;
    std::size_t used = 0;
    try {
        value = std::stoi(trimmed, &used);
    } catch (const std::exception&) {
        return false;
    }
    return used == trimmed.size();
}

bool sameIgnoringCase(const std::string& a, const std::string& b) {
    return ExamScheduler::toUpper(a) == ExamScheduler::toUpper(b);
}

}  // namespace

// ----------------------------------------------------------------------------
// helpers
// ----------------------------------------------------------------------------
std::string ExamScheduler::trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string ExamScheduler::toUpper(std::string text) {
    for (char& ch : text) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return text;
}

bool ExamScheduler::validText(const std::string& text, const std::string& what,
                              std::string& error) {
    if (trim(text).empty()) {
        error = what + " cannot be empty.";
        return false;
    }
    if (text.find(kFieldSeparator) != std::string::npos) {
        error = what + " cannot contain the '|' character.";
        return false;
    }
    return true;
}

int ExamScheduler::subjectIndexById(int id) const {
    for (std::size_t i = 0; i < subjects_.size(); ++i) {
        if (subjects_[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

int ExamScheduler::subjectIndexByCode(const std::string& code) const {
    const std::string wanted = toUpper(trim(code));
    for (std::size_t i = 0; i < subjects_.size(); ++i) {
        if (subjects_[i].code == wanted) return static_cast<int>(i);
    }
    return -1;
}

int ExamScheduler::batchIndexById(int id) const {
    for (std::size_t i = 0; i < batches_.size(); ++i) {
        if (batches_[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

int ExamScheduler::slotIndexById(int id) const {
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (slots_[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

void ExamScheduler::rebuildGraph() {
    graph_.rebuildFromBatches(subjects_, batches_);
    lastResult_.reset();  // any old timetable no longer matches the graph
}

// ----------------------------------------------------------------------------
// subjects
// ----------------------------------------------------------------------------
bool ExamScheduler::insertSubject(const Subject& subject, std::string& error) {
    if (!validText(subject.code, "Subject code", error) ||
        !validText(subject.name, "Subject name", error)) {
        return false;
    }
    if (subject.id <= 0 || subjectIndexById(subject.id) != -1) {
        error = "Subject id " + std::to_string(subject.id) + " is invalid or already used.";
        return false;
    }
    if (subjectIndexByCode(subject.code) != -1) {
        error = "Subject code '" + toUpper(trim(subject.code)) + "' already exists.";
        return false;
    }
    if (subject.studentCount < 0) {
        error = "Student count cannot be negative.";
        return false;
    }
    Subject clean = subject;
    clean.code = toUpper(trim(subject.code));
    clean.name = trim(subject.name);
    subjects_.push_back(clean);
    nextSubjectId_ = std::max(nextSubjectId_, clean.id + 1);
    rebuildGraph();
    return true;
}

bool ExamScheduler::addSubject(const std::string& code, const std::string& name, int students,
                               std::string& error) {
    Subject subject;
    subject.id = nextSubjectId_;
    subject.code = code;
    subject.name = name;
    subject.studentCount = students;
    return insertSubject(subject, error);
}

bool ExamScheduler::removeSubject(int id, std::string& error) {
    const int index = subjectIndexById(id);
    if (index == -1) {
        error = "No subject with id " + std::to_string(id) + ".";
        return false;
    }
    subjects_.erase(subjects_.begin() + index);
    for (Batch& batch : batches_) {
        auto& ids = batch.subjectIds;
        ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
    }
    rebuildGraph();
    return true;
}

// ----------------------------------------------------------------------------
// batches
// ----------------------------------------------------------------------------
bool ExamScheduler::insertBatch(const Batch& batch, std::string& error) {
    if (!validText(batch.name, "Batch name", error)) return false;
    if (batch.id <= 0 || batchIndexById(batch.id) != -1) {
        error = "Batch id " + std::to_string(batch.id) + " is invalid or already used.";
        return false;
    }
    for (const Batch& existing : batches_) {
        if (sameIgnoringCase(existing.name, trim(batch.name))) {
            error = "Batch '" + trim(batch.name) + "' already exists.";
            return false;
        }
    }
    if (batch.studentCount < 0) {
        error = "Student count cannot be negative.";
        return false;
    }
    for (std::size_t i = 0; i < batch.subjectIds.size(); ++i) {
        const int id = batch.subjectIds[i];
        if (subjectIndexById(id) == -1) {
            error = "Unknown subject id " + std::to_string(id) + ".";
            return false;
        }
        if (std::count(batch.subjectIds.begin(), batch.subjectIds.end(), id) > 1) {
            error = "Subject id " + std::to_string(id) + " is listed twice.";
            return false;
        }
    }
    Batch clean = batch;
    clean.name = trim(batch.name);
    batches_.push_back(clean);
    nextBatchId_ = std::max(nextBatchId_, clean.id + 1);
    rebuildGraph();
    return true;
}

bool ExamScheduler::addBatch(const std::string& name, int students,
                             const std::vector<int>& subjectIds, std::string& error) {
    Batch batch;
    batch.id = nextBatchId_;
    batch.name = name;
    batch.studentCount = students;
    batch.subjectIds = subjectIds;
    return insertBatch(batch, error);
}

bool ExamScheduler::removeBatch(int id, std::string& error) {
    const int index = batchIndexById(id);
    if (index == -1) {
        error = "No batch with id " + std::to_string(id) + ".";
        return false;
    }
    batches_.erase(batches_.begin() + index);
    rebuildGraph();
    return true;
}

// ----------------------------------------------------------------------------
// slots (changing slots never changes the graph, so the last result is kept)
// ----------------------------------------------------------------------------
bool ExamScheduler::insertSlot(const TimeSlot& slot, std::string& error) {
    if (!validText(slot.label, "Slot label", error)) return false;
    if (slot.id <= 0 || slotIndexById(slot.id) != -1) {
        error = "Slot id " + std::to_string(slot.id) + " is invalid or already used.";
        return false;
    }
    for (const TimeSlot& existing : slots_) {
        if (sameIgnoringCase(existing.label, trim(slot.label))) {
            error = "Slot '" + trim(slot.label) + "' already exists.";
            return false;
        }
    }
    TimeSlot clean = slot;
    clean.label = trim(slot.label);
    slots_.push_back(clean);
    nextSlotId_ = std::max(nextSlotId_, clean.id + 1);
    return true;
}

bool ExamScheduler::addSlot(const std::string& label, std::string& error) {
    TimeSlot slot;
    slot.id = nextSlotId_;
    slot.label = label;
    return insertSlot(slot, error);
}

bool ExamScheduler::removeSlot(int id, std::string& error) {
    const int index = slotIndexById(id);
    if (index == -1) {
        error = "No slot with id " + std::to_string(id) + ".";
        return false;
    }
    slots_.erase(slots_.begin() + index);
    return true;
}

bool ExamScheduler::setSlotAvailable(int id, bool available, std::string& error) {
    const int index = slotIndexById(id);
    if (index == -1) {
        error = "No slot with id " + std::to_string(id) + ".";
        return false;
    }
    slots_[index].available = available;
    return true;
}

void ExamScheduler::clear() {
    *this = ExamScheduler{};
}

// ----------------------------------------------------------------------------
// queries
// ----------------------------------------------------------------------------
std::vector<TimeSlot> ExamScheduler::availableSlots() const {
    std::vector<TimeSlot> result;
    for (const TimeSlot& slot : slots_) {
        if (slot.available) result.push_back(slot);
    }
    return result;
}

int ExamScheduler::studentsForSubject(int index) const {
    if (index < 0 || index >= static_cast<int>(subjects_.size())) return 0;
    const Subject& subject = subjects_[index];
    if (subject.studentCount > 0) return subject.studentCount;
    int total = 0;
    for (const Batch& batch : batches_) {
        const auto& ids = batch.subjectIds;
        if (std::find(ids.begin(), ids.end(), subject.id) != ids.end()) {
            total += batch.studentCount;
        }
    }
    return total;
}

int ExamScheduler::totalStudents() const {
    int total = 0;
    for (const Batch& batch : batches_) total += batch.studentCount;
    return total;
}

std::string ExamScheduler::batchNamesForSubject(int index) const {
    if (index < 0 || index >= static_cast<int>(subjects_.size())) return "";
    std::string names;
    for (const Batch& batch : batches_) {
        const auto& ids = batch.subjectIds;
        if (std::find(ids.begin(), ids.end(), subjects_[index].id) != ids.end()) {
            names += (names.empty() ? "" : ", ") + batch.name;
        }
    }
    return names.empty() ? "-" : names;
}

Schedule ExamScheduler::makeSchedule(const std::vector<int>& colors) const {
    Schedule schedule;
    schedule.colors = colors;
    schedule.slotsNeeded = countColors(colors);
    const std::vector<TimeSlot> available = availableSlots();
    schedule.slotsAvailable = static_cast<int>(available.size());
    schedule.feasible = schedule.slotsNeeded <= schedule.slotsAvailable;

    // Colour c -> c-th available slot, in the order the user defined them.
    const int mapped = std::min(schedule.slotsNeeded, schedule.slotsAvailable);
    schedule.slotForColor.assign(available.begin(), available.begin() + mapped);

    // Partial timetable: keep subjects whose colour has a slot, then try to
    // squeeze the rest into any available slot without a clash (greedy refit).
    schedule.partialColors = colors;
    for (std::size_t v = 0; v < colors.size(); ++v) {
        if (colors[v] >= schedule.slotsAvailable) {
            schedule.partialColors[v] = kUncolored;
            schedule.outsideSlots.push_back(static_cast<int>(v));
        }
    }
    for (int v : schedule.outsideSlots) {
        for (int slot = 0; slot < schedule.slotsAvailable; ++slot) {
            const auto& nbrs = graph_.neighbors(v);
            const bool clash = std::any_of(nbrs.begin(), nbrs.end(), [&](int u) {
                return schedule.partialColors[u] == slot;
            });
            if (!clash) {
                schedule.partialColors[v] = slot;
                break;
            }
        }
        if (schedule.partialColors[v] == kUncolored) schedule.unplaced.push_back(v);
    }
    return schedule;
}

// ----------------------------------------------------------------------------
// persistence
//   SUBJECT|id|code|name|students
//   BATCH|id|name|students|subjectId,subjectId,...
//   SLOT|id|label|available(1/0)
// ----------------------------------------------------------------------------
bool ExamScheduler::saveToFile(const std::string& path, std::string& error) const {
    std::ofstream out(path);
    if (!out) {
        error = "Cannot open '" + path + "' for writing.";
        return false;
    }
    out << kFileHeader << '\n';
    for (const Subject& s : subjects_) {
        out << "SUBJECT|" << s.id << '|' << s.code << '|' << s.name << '|' << s.studentCount
            << '\n';
    }
    for (const Batch& b : batches_) {
        out << "BATCH|" << b.id << '|' << b.name << '|' << b.studentCount << '|';
        for (std::size_t i = 0; i < b.subjectIds.size(); ++i) {
            out << (i ? "," : "") << b.subjectIds[i];
        }
        out << '\n';
    }
    for (const TimeSlot& t : slots_) {
        out << "SLOT|" << t.id << '|' << t.label << '|' << (t.available ? 1 : 0) << '\n';
    }
    if (!out) {
        error = "Write to '" + path + "' failed.";
        return false;
    }
    return true;
}

bool ExamScheduler::loadFromFile(const std::string& path, std::string& error) {
    std::ifstream in(path);
    if (!in) {
        error = "Cannot open '" + path + "' for reading.";
        return false;
    }

    // Load into a scratch object; only replace *this if the whole file is valid.
    ExamScheduler loaded;
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        const std::vector<std::string> f = splitFields(line, kFieldSeparator);
        const std::string where = "Line " + std::to_string(lineNo) + ": ";
        const std::string kind = f.empty() ? "" : toUpper(f[0]);
        bool ok = false;

        if (kind == "SUBJECT" && f.size() == 5) {
            Subject s;
            s.code = f[2];
            s.name = f[3];
            ok = parseInt(f[1], s.id) && parseInt(f[4], s.studentCount);
            if (!ok) error = "bad number in SUBJECT record.";
            ok = ok && loaded.insertSubject(s, error);
        } else if (kind == "BATCH" && f.size() == 5) {
            Batch b;
            b.name = f[2];
            ok = parseInt(f[1], b.id) && parseInt(f[3], b.studentCount);
            for (const std::string& idText : splitFields(f[4], ',')) {
                int id = 0;
                if (trim(idText).empty()) continue;
                ok = ok && parseInt(idText, id);
                b.subjectIds.push_back(id);
            }
            if (!ok) error = "bad number in BATCH record.";
            ok = ok && loaded.insertBatch(b, error);
        } else if (kind == "SLOT" && f.size() == 4) {
            TimeSlot t;
            t.label = f[2];
            int flag = 1;
            ok = parseInt(f[1], t.id) && parseInt(f[3], flag) && (flag == 0 || flag == 1);
            t.available = (flag == 1);
            if (!ok) error = "bad number in SLOT record.";
            ok = ok && loaded.insertSlot(t, error);
        } else {
            error = "unrecognised record '" + line + "'.";
        }

        if (!ok) {
            error = where + error;
            return false;
        }
    }
    *this = std::move(loaded);
    return true;
}
