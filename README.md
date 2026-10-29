# Smart University Examination Timetable Generator

A C++17 console application that schedules university exams with no student clashes, using as few time slots as possible.
It models the problem as **graph colouring** and solves it with **Greedy**, **DSATUR**, **Backtracking** and **Branch and Bound**.

| Concept   | In the graph                                  |
|-----------|-----------------------------------------------|
| Subject   | Vertex                                        |
| Conflict  | Edge (the two subjects share a student batch) |
| Time slot | Colour                                        |
| Goal      | Proper colouring with the fewest colours (chromatic number χ) |

## 1. Folder structure

```
dsa_prj/
├── Makefile
├── README.md                 build / usage (this file)
├── main_single.cpp           single-file version for submission (generated)
├── include/
│   ├── Models.h              Subject, Batch, TimeSlot, ConflictEdge
│   ├── Graph.h               adjacency list + adjacency matrix
│   ├── Coloring.h            verifier, greedy, DSATUR, clique bound, shared types
│   ├── Backtracking.h        m-colouring decision search
│   ├── BranchAndBound.h      minimum-slot search with pruning
│   ├── Scheduler.h           data store, slot mapping, file I/O
│   ├── Samples.h             built-in datasets A, B, C
│   ├── Console.h             safe input, ANSI colours
│   ├── Tests.h               self-tests
│   └── App.h                 menu UI
├── src/                      one .cpp per header + main.cpp
├── samples/
│   ├── sample_a.txt          small college
│   ├── sample_b.txt          crown graph (greedy 3 vs optimal 2)
│   └── sample_c.txt          30-subject stress test
├── tools/make_single.sh      regenerates main_single.cpp
└── docs/REPORT.md            dry runs, analysis, viva questions, report outline
```

## 2. Build and run

Requires only `g++` with C++17 support. There are no external dependencies.

```bash
make
```

```bash
./timetable
```

```bash
make test
```

Without `make`:

```bash
g++ -std=c++17 -Wall -Wextra -O2 -Iinclude src/*.cpp -o timetable
```

Single-file version, which is identical code with the files concatenated:

```bash
g++ -std=c++17 -Wall -Wextra -O2 main_single.cpp -o timetable
```

After editing anything in `include/` or `src/`, run `make single` to regenerate `main_single.cpp` and build it.

Both builds compile with **zero warnings** using `-Wall -Wextra`, and also using `-Wpedantic -Wshadow`.
The self-tests run 30 checks, and all of them pass.

## 3. Menu

```
 1. Manage subjects (add / remove)           9. COMPARE: Basic Greedy vs Optimal
 2. Manage student batches (add / remove)   10. Display statistics
 3. Manage time slots                       11. Load built-in sample dataset
 4. View all data                           12. Save / load data file
 5. View conflict graph                     13. Clear all data
 6. Generate timetable - Greedy             14. Exit
 7. Generate timetable - Backtracking (k)   15. Toggle ANSI colours
 8. Generate optimal timetable - B&B        16. Run self-tests
```

Quick demo: choose **11 → 2** to load Sample B, then **9** to compare.
Choose **11 → 3** to load Sample C, then try **6**, **7** with k = 8, **8** with a node limit of 20, and **9** to see an infeasible greedy result, a backtracking cut-off, a B&B cut-off and a slot saved by B&B.

## 4. Data file format

The file is plain text with one record per line.
Fields are separated by `|`, so names cannot contain `|`. Lines starting with `#` are comments.

```
SUBJECT|id|code|name|studentCount        (0 = derive from batches)
BATCH|id|name|studentCount|subjectId,subjectId,...
SLOT|id|label|available                  (1 or 0)
```

The loader validates the whole file before replacing the current data, so a bad file never leaves you with half-loaded data.
