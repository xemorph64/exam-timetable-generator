// =============================================================================
// MachineApi.cpp - structured (JSON) access to the same engine the menu uses.
// Every number reported here comes straight from the C++ algorithms.
// =============================================================================
#include "MachineApi.h"

#include "Backtracking.h"
#include "BranchAndBound.h"
#include "Samples.h"
#include "Tests.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <vector>

namespace {

constexpr long long kMaxNodeLimit = 1'000'000'000;
constexpr long long kMaxTimeLimitMs = 600'000;
constexpr long long kMaxCount = 1'000'000;

std::string jsonText(const std::string& text) {
    std::string out = "\"";
    for (const char ch : text) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    char escaped[8];
                    std::snprintf(escaped, sizeof(escaped), "\\u%04x", ch);
                    out += escaped;
                } else {
                    out += ch;
                }
        }
    }
    return out + "\"";
}

std::string jsonBool(bool value) { return value ? "true" : "false"; }

std::string jsonNumber(double value) {
    if (!std::isfinite(value)) return "0";
    std::ostringstream out;
    out << std::fixed << std::setprecision(4) << value;
    return out.str();
}

std::string jsonInts(const std::vector<int>& values) {
    std::string out = "[";
    for (std::size_t i = 0; i < values.size(); ++i) {
        out += (i ? "," : "") + std::to_string(values[i]);
    }
    return out + "]";
}

std::string errorJson(const std::string& message) {
    return "{\"ok\":false,\"error\":" + jsonText(message) + "}";
}

std::string okJson(const std::string& body = "") {
    return body.empty() ? "{\"ok\":true}" : "{\"ok\":true," + body + "}";
}

// Splits on '|' keeping empty fields, trimming each one.
std::vector<std::string> payloadFields(const std::string& payload) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t bar = payload.find('|', start);
        fields.push_back(ExamScheduler::trim(payload.substr(start, bar - start)));
        if (bar == std::string::npos) break;
        start = bar + 1;
    }
    return fields;
}

std::vector<std::string> payloadWords(const std::string& payload) {
    std::istringstream stream(payload);
    std::vector<std::string> words;
    std::string word;
    while (stream >> word) words.push_back(word);
    return words;
}

bool payloadNumber(const std::string& text, long long min, long long max, long long& value) {
    const std::string t = ExamScheduler::trim(text);
    if (t.empty()) return false;
    std::size_t used = 0;
    try {
        value = std::stoll(t, &used);
    } catch (const std::exception&) {
        return false;
    }
    return used == t.size() && value >= min && value <= max;
}

// Optional "[nodes] [ms]" arguments starting at words[first].
bool parseLimits(const std::vector<std::string>& words, std::size_t first, SearchLimits& limits,
                 std::string& error) {
    long long value = 0;
    if (words.size() > first) {
        if (!payloadNumber(words[first], 1, kMaxNodeLimit, value)) {
            error = "Node limit must be a whole number between 1 and 1,000,000,000.";
            return false;
        }
        limits.nodeLimit = value;
    }
    if (words.size() > first + 1) {
        if (!payloadNumber(words[first + 1], 1, kMaxTimeLimitMs, value)) {
            error = "Time limit must be between 1 and 600,000 ms.";
            return false;
        }
        limits.timeLimitMs = static_cast<double>(value);
    }
    return true;
}

}  // namespace

void MachineApi::handle(const std::string& line, std::ostream& out) {
    // "@<id> <verb> [payload]"
    std::string id;
    std::size_t pos = 1;
    while (pos < line.size() && std::isalnum(static_cast<unsigned char>(line[pos]))) {
        id += line[pos++];
    }
    const std::string rest = ExamScheduler::trim(line.substr(std::min(pos, line.size())));
    const std::size_t space = rest.find(' ');
    const std::string verb = rest.substr(0, space);
    const std::string payload =
        space == std::string::npos ? "" : ExamScheduler::trim(rest.substr(space + 1));

    std::string body;
    if (id.empty() || id.size() > 16) {
        id = "0";
        body = errorJson("Malformed request: expected '@<id> <verb> [payload]'.");
    } else {
        try {
            body = dispatch(verb, payload);
        } catch (const std::exception& ex) {
            body = errorJson(std::string("Engine error: ") + ex.what());
        }
    }
    out << "@@BEGIN " << id << '\n' << body << '\n' << "@@END " << id << '\n' << std::flush;
}

std::string MachineApi::dispatch(const std::string& verb, const std::string& payload) {
    std::string error;
    long long id = 0;
    const std::vector<std::string> fields = payloadFields(payload);
    auto done = [&error](bool ok) { return ok ? okJson() : errorJson(error); };

    if (verb == "state") return okJson(stateJson());
    if (verb == "run") return runCommand(payload);
    if (verb == "compare") return compareJson();
    if (verb == "tests") {
        std::ostringstream report;
        const int failures = runSelfTests(report);
        std::istringstream reader(report.str());
        std::string text;
        std::string list = "[";
        while (std::getline(reader, text)) {
            if (text.empty()) continue;
            list += (list.size() > 1 ? "," : "") + jsonText(text);
        }
        return okJson("\"failures\":" + std::to_string(failures) + ",\"lines\":" + list + "]");
    }
    if (verb == "sample") {
        const std::string which = ExamScheduler::toUpper(payload);
        if (which != "A" && which != "B" && which != "C") return errorJson("Unknown sample.");
        const SampleId sample = which == "A"   ? SampleId::SmallCollege
                                : which == "B" ? SampleId::CrownGraph
                                               : SampleId::StressTest;
        loadSample(scheduler_, sample);
        return okJson("\"description\":" + jsonText(sampleDescription(sample)));
    }
    if (verb == "clear") {
        scheduler_.clear();
        return okJson();
    }
    if (verb == "subject.add") {
        long long students = 0;
        if (fields.size() != 3) return errorJson("Expected CODE|Name|students.");
        if (!payloadNumber(fields[2], 0, kMaxCount, students)) {
            return errorJson("Student count must be a whole number between 0 and 1,000,000.");
        }
        if (!scheduler_.addSubject(fields[0], fields[1], static_cast<int>(students), error)) {
            return errorJson(error);
        }
        return okJson("\"id\":" + std::to_string(scheduler_.subjects().back().id));
    }
    if (verb == "batch.add") {
        long long students = 0;
        if (fields.size() != 3) return errorJson("Expected Name|students|id,id,...");
        if (!payloadNumber(fields[1], 1, kMaxCount, students)) {
            return errorJson("Student count must be a whole number between 1 and 1,000,000.");
        }
        std::vector<int> ids;
        std::string list = fields[2];
        std::replace(list.begin(), list.end(), ',', ' ');
        for (const std::string& word : payloadWords(list)) {
            if (!payloadNumber(word, 1, kMaxCount * 1000, id)) {
                return errorJson("Subject ids must be whole numbers.");
            }
            ids.push_back(static_cast<int>(id));
        }
        if (!scheduler_.addBatch(fields[0], static_cast<int>(students), ids, error)) {
            return errorJson(error);
        }
        return okJson("\"id\":" + std::to_string(scheduler_.batches().back().id));
    }
    if (verb == "slot.add") return done(scheduler_.addSlot(payload, error));
    if (verb == "slot.set") {
        long long flag = 0;
        if (fields.size() != 2 || !payloadNumber(fields[0], 1, kMaxNodeLimit, id) ||
            !payloadNumber(fields[1], 0, 1, flag)) {
            return errorJson("Expected <slot id>|1 or <slot id>|0.");
        }
        return done(scheduler_.setSlotAvailable(static_cast<int>(id), flag == 1, error));
    }
    if (verb == "subject.remove" || verb == "batch.remove" || verb == "slot.remove") {
        if (!payloadNumber(payload, 1, kMaxNodeLimit, id)) return errorJson("Expected an id.");
        const int target = static_cast<int>(id);
        if (verb == "subject.remove") return done(scheduler_.removeSubject(target, error));
        if (verb == "batch.remove") return done(scheduler_.removeBatch(target, error));
        return done(scheduler_.removeSlot(target, error));
    }
    if (verb == "save") return done(scheduler_.saveToFile(payload, error));
    if (verb == "load") return done(scheduler_.loadFromFile(payload, error));
    return errorJson("Unknown command '" + verb + "'.");
}

std::string MachineApi::runCommand(const std::string& payload) {
    const std::vector<std::string> words = payloadWords(payload);
    if (words.empty()) return errorJson("Expected: run greedy|backtrack|bnb ...");
    if (scheduler_.subjects().empty()) return errorJson("There are no subjects to schedule.");
    const Graph& graph = scheduler_.graph();
    std::string error;
    SearchLimits limits;
    ColoringResult result;

    if (words[0] == "greedy") {
        const std::string order = words.size() > 1 ? words[1] : "";
        if (order == "input") {
            result = greedyColoring(graph, GreedyOrder::Input);
        } else if (order == "welsh") {
            result = greedyColoring(graph, GreedyOrder::WelshPowell);
        } else if (order == "dsatur") {
            result = greedyColoring(graph, GreedyOrder::Dsatur);
        } else {
            return errorJson("Greedy order must be input, welsh or dsatur.");
        }
    } else if (words[0] == "backtrack") {
        long long k = 0;
        const long long n = static_cast<long long>(scheduler_.subjects().size());
        if (words.size() < 2 || !payloadNumber(words[1], 1, n, k)) {
            return errorJson("k must be between 1 and " + std::to_string(n) + ".");
        }
        if (!parseLimits(words, 2, limits, error)) return errorJson(error);
        result = BacktrackingColorer(graph, limits).canColor(static_cast<int>(k));
    } else if (words[0] == "bnb") {
        if (!parseLimits(words, 1, limits, error)) return errorJson(error);
        result = BranchAndBoundColorer(graph, limits).solve();
    } else {
        return errorJson("Unknown algorithm '" + words[0] + "'.");
    }

    if (result.success) scheduler_.recordResult(result);
    return okJson("\"result\":" + resultJson(result));
}

std::string MachineApi::compareJson() {
    if (scheduler_.subjects().empty()) return errorJson("There are no subjects to schedule.");
    const Graph& graph = scheduler_.graph();
    const ColoringResult greedy = greedyColoring(graph, GreedyOrder::Input);
    ColoringResult optimal = BranchAndBoundColorer(graph).solve();
    optimal.colors = alignColors(greedy.colors, optimal.colors);  // same as menu 9
    scheduler_.recordResult(optimal);

    std::string moved = "[";
    for (std::size_t v = 0; v < optimal.colors.size(); ++v) {
        if (optimal.colors[v] == greedy.colors[v]) continue;
        moved += std::string(moved.size() > 1 ? "," : "") + "{\"subject\":" + std::to_string(v) +
                 ",\"from\":" + std::to_string(greedy.colors[v]) +
                 ",\"to\":" + std::to_string(optimal.colors[v]) + "}";
    }
    return okJson("\"greedy\":" + resultJson(greedy) + ",\"optimal\":" + resultJson(optimal) +
                  ",\"moved\":" + moved + "]");
}

std::string MachineApi::resultJson(const ColoringResult& result) const {
    std::ostringstream out;
    out << "{\"method\":" << jsonText(result.method) << ",\"success\":" << jsonBool(result.success)
        << ",\"colors\":" << jsonInts(result.colors) << ",\"slotsUsed\":" << result.slotsUsed
        << ",\"timeMs\":" << jsonNumber(result.timeMs)
        << ",\"nodesExplored\":" << result.nodesExplored
        << ",\"nodesPruned\":" << result.nodesPruned << ",\"backtracks\":" << result.backtracks
        << ",\"provenOptimal\":" << jsonBool(result.provenOptimal)
        << ",\"cutOff\":" << jsonBool(result.cutOff) << ",\"lowerBound\":" << result.lowerBound
        << ",\"upperBound\":" << result.upperBound;

    if (result.colors.empty()) {
        out << ",\"valid\":false,\"clashes\":[],\"uncolored\":[],\"schedule\":null}";
        return out.str();
    }

    const ValidationReport report = validateColoring(scheduler_.graph(), result.colors);
    out << ",\"valid\":" << jsonBool(report.valid) << ",\"clashes\":[";
    for (std::size_t i = 0; i < report.clashes.size(); ++i) {
        out << (i ? "," : "") << '[' << report.clashes[i].first << ','
            << report.clashes[i].second << ']';
    }
    out << "],\"uncolored\":" << jsonInts(report.uncolored);

    const Schedule schedule = scheduler_.makeSchedule(result.colors);
    std::vector<int> slotIds;
    for (const TimeSlot& slot : schedule.slotForColor) slotIds.push_back(slot.id);
    out << ",\"schedule\":{\"feasible\":" << jsonBool(schedule.feasible)
        << ",\"slotsNeeded\":" << schedule.slotsNeeded
        << ",\"slotsAvailable\":" << schedule.slotsAvailable << ",\"slotIds\":" << jsonInts(slotIds)
        << ",\"partialColors\":" << jsonInts(schedule.partialColors)
        << ",\"outsideSlots\":" << jsonInts(schedule.outsideSlots)
        << ",\"unplaced\":" << jsonInts(schedule.unplaced) << "}}";
    return out.str();
}

std::string MachineApi::stateJson() const {
    const Graph& graph = scheduler_.graph();
    const auto& subjects = scheduler_.subjects();
    std::ostringstream out;

    out << "\"subjects\":[";
    for (std::size_t i = 0; i < subjects.size(); ++i) {
        const Subject& s = subjects[i];
        std::vector<int> batchIds;
        for (const Batch& b : scheduler_.batches()) {
            if (std::find(b.subjectIds.begin(), b.subjectIds.end(), s.id) != b.subjectIds.end()) {
                batchIds.push_back(b.id);
            }
        }
        const int index = static_cast<int>(i);
        out << (i ? "," : "") << "{\"id\":" << s.id << ",\"code\":" << jsonText(s.code)
            << ",\"name\":" << jsonText(s.name)
            << ",\"students\":" << scheduler_.studentsForSubject(index)
            << ",\"derived\":" << jsonBool(s.studentCount == 0)
            << ",\"degree\":" << graph.degree(index) << ",\"batches\":" << jsonInts(batchIds) << '}';
    }

    out << "],\"batches\":[";
    for (std::size_t i = 0; i < scheduler_.batches().size(); ++i) {
        const Batch& b = scheduler_.batches()[i];
        out << (i ? "," : "") << "{\"id\":" << b.id << ",\"name\":" << jsonText(b.name)
            << ",\"students\":" << b.studentCount << ",\"subjects\":" << jsonInts(b.subjectIds)
            << '}';
    }

    out << "],\"slots\":[";
    for (std::size_t i = 0; i < scheduler_.slots().size(); ++i) {
        const TimeSlot& t = scheduler_.slots()[i];
        out << (i ? "," : "") << "{\"id\":" << t.id << ",\"label\":" << jsonText(t.label)
            << ",\"available\":" << jsonBool(t.available) << '}';
    }

    out << "],\"edges\":[";
    for (std::size_t i = 0; i < graph.edges().size(); ++i) {
        const ConflictEdge& e = graph.edges()[i];
        out << (i ? "," : "") << "{\"u\":" << e.u << ",\"v\":" << e.v << ",\"weight\":" << e.weight
            << ",\"reasons\":[";
        for (std::size_t r = 0; r < e.reasons.size(); ++r) {
            out << (r ? "," : "") << jsonText(e.reasons[r]);
        }
        out << "]}";
    }

    int isolated = 0;
    for (int v = 0; v < graph.vertexCount(); ++v) isolated += graph.degree(v) == 0 ? 1 : 0;
    out << "],\"stats\":{\"vertices\":" << graph.vertexCount() << ",\"edges\":" << graph.edgeCount()
        << ",\"density\":" << jsonNumber(graph.density()) << ",\"maxDegree\":" << graph.maxDegree()
        << ",\"avgDegree\":" << jsonNumber(graph.avgDegree())
        << ",\"components\":" << graph.connectedComponents() << ",\"isolated\":" << isolated
        << ",\"clique\":" << jsonInts(greedyClique(graph))
        << ",\"totalStudents\":" << scheduler_.totalStudents()
        << ",\"availableSlots\":" << scheduler_.availableSlots().size() << '}'
        << ",\"limits\":{\"nodeLimit\":" << kDefaultNodeLimit
        << ",\"timeLimitMs\":" << jsonNumber(kDefaultTimeLimitMs) << '}';

    const auto& last = scheduler_.lastResult();
    out << ",\"last\":" << (last ? resultJson(*last) : "null");
    return out.str();
}
