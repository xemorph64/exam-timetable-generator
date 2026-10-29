// =============================================================================
// Samples.h - built-in datasets.
//   A: small realistic college (6 subjects, 4 batches, chromatic number 3)
//   B: crown graph - input-order greedy uses 3 slots, optimum is 2
//   C: seeded random stress test (30 subjects) for the search limits
// =============================================================================
#pragma once

#include "Scheduler.h"

#include <string>

enum class SampleId { SmallCollege, CrownGraph, StressTest };

std::string sampleDescription(SampleId id);

// Replaces the scheduler's contents with the chosen sample.
void loadSample(ExamScheduler& scheduler, SampleId id);
