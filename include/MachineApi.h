// =============================================================================
// MachineApi.h - line-based machine protocol on the same stdin/stdout as the
// menu. Used by the web UI (Slotwise) and usable from any script.
//
// At the main-menu prompt, a line of the form
//     @<id> <verb> [payload]
// runs one command and prints exactly one block:
//     @@BEGIN <id>
//     {"ok":true, ...single-line JSON...}
//     @@END <id>
// Payload fields are separated by '|', the same character the data layer
// already forbids inside names. Verbs:
//     state                          subjects, batches, slots, edges, stats
//     run greedy input|welsh|dsatur  greedy colouring
//     run backtrack <k> [nodes] [ms] m-colouring decision
//     run bnb [nodes] [ms]           branch and bound
//     compare                        input-order greedy vs branch and bound
//     sample a|b|c                   load a built-in dataset
//     subject.add CODE|Name|students   subject.remove <id>
//     batch.add Name|students|id,id    batch.remove <id>
//     slot.add Label   slot.remove <id>   slot.set <id>|1|0
//     clear   save <path>   load <path>   tests
// =============================================================================
#pragma once

#include "Coloring.h"
#include "Scheduler.h"

#include <ostream>
#include <string>

class MachineApi {
public:
    explicit MachineApi(ExamScheduler& scheduler) : scheduler_(scheduler) {}

    // Handles one "@..." line and writes the response block to `out`.
    void handle(const std::string& line, std::ostream& out);

private:
    std::string dispatch(const std::string& verb, const std::string& payload);
    std::string runCommand(const std::string& payload);
    std::string stateJson() const;
    std::string resultJson(const ColoringResult& result) const;
    std::string compareJson();

    ExamScheduler& scheduler_;
};
