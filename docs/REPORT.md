# Smart University Examination Timetable Generator: Project Report Material

This document covers deliverables 4 to 10: the sample run, the dry runs, the algorithm analysis, the comparison table, viva questions, the report outline and future scope.
For the folder structure and build instructions, see [README.md](../README.md).

---

## 4. Sample input and expected output (Sample B: crown graph)

### Input file `samples/sample_b.txt`

```
# Smart Exam Timetable data file v1
SUBJECT|1|A1|Algorithms Section A1|0
SUBJECT|2|B1|Biology Section B1|0
SUBJECT|3|A2|Algorithms Section A2|0
SUBJECT|4|B2|Biology Section B2|0
SUBJECT|5|A3|Algorithms Section A3|0
SUBJECT|6|B3|Biology Section B3|0
BATCH|1|Batch A1-B2|30|1,4
BATCH|2|Batch A1-B3|30|1,6
BATCH|3|Batch A2-B1|30|3,2
BATCH|4|Batch A2-B3|30|3,6
BATCH|5|Batch A3-B1|30|5,2
BATCH|6|Batch A3-B2|30|5,4
SLOT|1|Day 1 - 09:00 AM|1
SLOT|2|Day 1 - 02:00 PM|1
SLOT|3|Day 2 - 09:00 AM|1
SLOT|4|Day 2 - 02:00 PM|1
```

To produce the output, choose either menu **11 → 2** (built-in sample), or menu **12 → 2 → `samples/sample_b.txt`**.
Then choose menu **9**.

### Console output of menu 9 (captured from the real program)

```
  ======= GREEDY vs OPTIMAL =======
  Metric                 Greedy        Optimal
  Slots used             3             2
  Time taken (ms)        0.000         0.005
  Nodes explored         -             0
  Conflict-free?         Yes           Yes
  Optimality proven?     No            Yes
  (Best greedy UB = clique LB = 2, so optimality was proven without searching.)

  Greedy timetable                  Optimal timetable
  Slot 1 | A1, B1                   Slot 1 | A1, A2, A3
  Slot 2 | A2, B2                   Slot 2 | B1, B2, B3
  Slot 3 | A3, B3

  Result: Optimal search saved 1 slot (33% fewer slots).
  Subjects moved between slots: 4
    B1: Slot 1 -> Slot 2
    A2: Slot 2 -> Slot 1
    A3: Slot 3 -> Slot 1
    B3: Slot 3 -> Slot 2

  Greedy  : Verified: conflict-free (6 subjects scheduled, 6 conflict pairs checked)
  Optimal : Verified: conflict-free (6 subjects scheduled, 6 conflict pairs checked)
```

The two timing values depend on the machine. Every other line is deterministic.

**Why greedy fails here.** The edges are Ai–Bj for i ≠ j. In input order, A1 and B1 are not adjacent, so both get slot 1.
A2 is adjacent to B1, so it gets slot 2, and B2 is adjacent to A1, so it also gets slot 2.
A3 is adjacent to B1 (slot 1) and B2 (slot 2), so it is forced into slot 3, and B3 is forced into slot 3 in the same way.
The graph is bipartite, with all A subjects on one side and all B subjects on the other, so 2 slots are enough.

**Why B&B explores 0 nodes.** B&B first computes all three greedy colourings, and DSATUR already finds 2 slots, giving UB = 2.
Any single edge is a clique of size 2, giving LB = 2. Since UB = LB, the answer is proven optimal before any search begins.
For a case where B&B has to search, see the wheel W6 test (`--test`) or Sample C, where LB = 8 and UB = 9 and the search proves 9 in 56 nodes.

---

## 5. Dry run on a 4-vertex example

Subjects **P, Q, R, S** (vertex indices 0–3). Batches create these conflicts:

```
P — Q, Q — R, R — S, S — P   (a 4-cycle)
P — R                        (a chord)
```

```
    P ─── Q
    │ ╲   │
    │   ╲ │
    S ─── R
```

| Vertex | Neighbours | Degree |
|--------|-----------|--------|
| P | Q, R, S | 3 |
| Q | P, R    | 2 |
| R | P, Q, S | 3 |
| S | P, R    | 2 |

P, Q and R form a triangle, so χ ≥ 3.

### 5.1 Greedy (input order P, Q, R, S)

| Step | Vertex | Coloured neighbours (slot) | Used slots | Smallest free | Assign |
|------|--------|----------------------------|-----------|---------------|--------|
| 1 | P | none | {} | 0 | P = Slot 1 |
| 2 | Q | P(0) | {0} | 1 | Q = Slot 2 |
| 3 | R | P(0), Q(1) | {0,1} | 2 | R = Slot 3 |
| 4 | S | P(0), R(2) | {0,2} | 1 | S = Slot 2 |

Result: 3 slots, {P}, {Q, S}, {R}. The verifier checks all 5 edges and finds no clash.

### 5.2 DSATUR order (used by backtracking)

| Step | Saturation (uncoloured) | Pick (reason) | Slot |
|------|--------------------------|---------------|------|
| 1 | all 0 | P (tie on degree 3, lower index wins over R) | 0 |
| 2 | Q 1, R 1, S 1 | R (degree 3) | 1 |
| 3 | Q 2, S 2 | Q (tie, lower index) | 2 |
| 4 | S 2 | S | 2 |

The visit order is **P, R, Q, S**.

### 5.3 Backtracking with k = 2 (should fail because of the triangle)

```
solve(0)  node 1   P: try 0 -> safe            P=0
 solve(1) node 2   R: try 0 -> clash with P
                      try 1 -> safe            R=1
  solve(2) node 3  Q: try 0 -> clash with P
                      try 1 -> clash with R
                      no colour left -> return false
                   undo R=1   (backtrack #1); R has no more colours -> false
           undo P=0   (backtrack #2)
          P: try 1 -> safe                     P=1
 solve(1) node 4   R: try 0 -> safe            R=0
  solve(2) node 5  Q: try 0 -> clash with R
                      try 1 -> clash with P   -> return false
                   undo R=0   (backtrack #3); R: try 1 -> clash with P -> false
           undo P=1   (backtrack #4); P has no more colours -> false
RESULT: impossible with 2 slots. 5 nodes, 4 backtracks.
```

### 5.4 Backtracking with k = 3 (succeeds without backtracking)

```
solve(0) node 1  P: 0 safe              P=0
solve(1) node 2  R: 0 clash(P), 1 safe  R=1
solve(2) node 3  Q: 0 clash(P), 1 clash(R), 2 safe   Q=2
solve(3) node 4  S: 0 clash(P), 1 clash(R), 2 safe (S-Q not an edge)  S=2
solve(4) node 5  all vertices coloured -> true
RESULT: {P}, {R}, {Q, S}. 5 nodes, 0 backtracks.
```

The program reports exactly these node and backtrack counts. They were checked by running the code on this graph.

---

## 6. Algorithms and complexity analysis

### 6.1 Graph construction
For every batch with s subjects, the program adds an edge for each of the s(s−1)/2 pairs.
Duplicate edges are merged through a `std::map` keyed by (min, max): the edge's weight increases by the batch size and the batch name is added to its reasons.
Building the matrix costs O(V²).
**Time:** O(V² + Σ s_b² · log E). **Space:** O(V² + E).

### 6.2 Greedy colouring
The algorithm visits vertices in some order and gives each one the smallest colour that no already-coloured neighbour uses.
A "stamp" array avoids clearing between vertices, so each adjacency list is scanned exactly once.

* **Input order:** O(V + E) time, O(V) space.
* **Welsh–Powell** (descending degree): adds an O(V log V) sort. High-degree vertices are coloured while many colours are still free.
* **DSATUR:** always picks the vertex with the most distinct neighbour colours. This is O(V² + E log V) using a linear scan plus a `std::set` of neighbour colours per vertex.
  DSATUR is optimal on bipartite graphs, which is why it solves Sample B.

Greedy never uses more than Δ + 1 colours, where Δ is the maximum degree, but it can be arbitrarily worse than χ (see the crown graph).

### 6.3 Verifier
The verifier checks that every vertex is coloured and that every edge has two differently coloured endpoints.
It runs after every generated timetable. **Time:** O(V + E). **Space:** O(V).

### 6.4 Backtracking (m-colouring decision)
Backtracking is a depth-first search over assignments in DSATUR order.
`isSafe` checks the vertices already placed using the O(1) adjacency matrix, which costs O(V) per check.
**Time:** O(k^V) in the worst case. **Space:** O(V), plus O(V) recursion depth.

Pruning makes the search far smaller in practice. A clash discards all k^(remaining) completions of a partial assignment at once.
DSATUR order puts the most constrained subjects first, so failures happen near the root, where each cut removes the most work.

### 6.5 Branch and Bound (minimise slots)
* **UB** is the best of the three greedy colourings. **LB** is the size of a greedy clique.
  If UB = LB, the search stops immediately because the answer is optimal.
* **Search order:** clique vertices first, then the rest in DSATUR order.
* **Rule 1 (bound):** prune when `colorsUsed ≥ best`.
* **Rule 2 (symmetry):** a vertex can take an existing colour or exactly one new colour. This removes the k! relabellings of every colouring.
* **Rule 3 (forward checking):** a counter `conflictCount[v][c]` makes "does u still have a free colour below best−1?" an O(UB) check for each neighbour.
* **Safety limits:** the search stops at 5,000,000 nodes or 5 s by default (both are user-adjustable) and returns the best colouring so far, marked "may not be optimal".

**Time:** O(UB^V) in the worst case. **Space:** O(V · UB).
On Sample C, B&B proves χ = 9 in 56 nodes. Plain backtracking cannot even decide k = 8 within 5,000,000 nodes.

### 6.6 Complexity table

| Algorithm | Time | Space |
|---|---|---|
| Graph construction | O(V² + Σ s² log E) | O(V² + E) |
| Greedy, input order | O(V + E) | O(V) |
| Greedy, Welsh–Powell | O(V log V + E) | O(V) |
| DSATUR | O(V² + E log V) | O(V + E) |
| Verifier | O(V + E) | O(V) |
| Clique lower bound | O(V³) | O(V) |
| Backtracking `canColor(k)` | O(k^V) | O(V) |
| Branch and Bound | O(UB^V), heavily pruned | O(V · UB) |

### 6.7 Why graph colouring is NP-hard
* **Decision version:** "Is G k-colourable?" is NP-complete for every k ≥ 3. It is one of Karp's 21 NP-complete problems (1972), and 3-SAT reduces to 3-colouring.
  A proposed colouring can be checked in O(V + E), so the problem is in NP. The reduction makes it NP-hard.
* **Optimisation version:** finding χ(G) is NP-hard. A polynomial-time algorithm for χ would decide 3-colourability.
  Approximating χ within a factor of V^(1−ε) is also NP-hard (Zuckerman 2007).
* **Consequence:** no polynomial exact algorithm is known, so the project uses fast heuristics (greedy and DSATUR) for upper bounds.
  It uses exact searches (backtracking and B&B) with pruning and safety limits when optimality matters.
* **Easy special cases:** k = 1 (the graph has no edges) and k = 2 (bipartite check by BFS, O(V + E)) are polynomial.

---

## 7. Greedy vs Backtracking vs Branch and Bound

| | Greedy (input / Welsh–Powell / DSATUR) | Backtracking (`canColor(k)`) | Branch and Bound |
|---|---|---|---|
| Question answered | "Give me a valid timetable quickly" | "Does it fit in k slots?" | "What is the minimum number of slots?" |
| Optimal? | No (crown graph: 3 vs 2) | Exact for the given k | Exact (proven unless cut off) |
| Time | O(V + E) to O(V²) | O(k^V) worst case | O(UB^V) worst case, heavily pruned |
| Pros | Instant, simple, scales to thousands of subjects | Simple, answers a real constraint ("we only have 8 slots") | Finds and proves the optimum, and stops early when UB = LB |
| Cons | Quality depends on order, no guarantee | Must loop over k to find χ; proving "no" can explode | More complex code, still exponential in the worst case |
| When to use | Large inputs, first draft, upper bound | Fixed slot budget, small or medium inputs | Small or medium inputs where every slot matters |

---

## 8. Viva questions with short answers

1. **Why model the timetable as graph colouring?**
   Subjects that share students cannot be held at the same time, which is exactly the rule that adjacent vertices need different colours. A colour is a slot.
2. **What is the chromatic number?**
   It is the minimum number of colours in a proper colouring, χ(G). Here it is the minimum number of exam slots.
3. **Why is greedy not optimal? Give an example.**
   Greedy commits to early choices and never revises them. On the crown graph in order A1, B1, A2, B2, A3, B3 it uses 3 slots, but 2 are enough.
4. **What is DSATUR and why is it better?**
   It colours the vertex with the most distinct neighbour colours first, so the most constrained subjects are placed before they run out of options. It is exact for bipartite graphs.
5. **What does `isSafe` do, and why use an adjacency matrix?**
   It checks that no already-coloured neighbour has the same colour. The matrix gives an O(1) "are u and v in conflict?" test inside the search.
6. **Difference between backtracking and branch and bound?**
   Backtracking answers a yes/no question (k-colourable?) and prunes only infeasible branches. B&B optimises, so it also prunes branches that cannot beat the best solution found so far, using bounds.
7. **How are the bounds computed?**
   UB is the best greedy colouring. LB is the size of a clique found greedily, because a clique of size c needs c colours. If UB = LB, the answer is optimal immediately.
8. **What is symmetry breaking?**
   Colour names are interchangeable, so a vertex may take only an existing colour or the next new one. This removes up to k! duplicate subtrees.
9. **What is forward checking?**
   After a choice, the search checks that every uncoloured neighbour still has at least one legal colour under the limit. If one does not, it prunes immediately instead of discovering the dead end deeper in the tree.
10. **Why are there node and time limits?**
    The problem is NP-hard, so some inputs take exponential time. The limits keep the program responsive and return the best answer so far, flagged as possibly not optimal.
11. **What happens when there are fewer slots than colours?**
    The program reports the timetable as INFEASIBLE with the shortfall, lists the subjects that do not fit, and offers a partial timetable after trying to refit them greedily.
12. **How do you know a generated timetable is correct?**
    `validateColoring` checks every edge and every vertex in O(V + E) after every generation and prints "Verified: conflict-free" or lists the clashes. The self-tests also check against graphs with known χ: K5 = 5, C5 = 3, K3,3 = 2, crown = 2, and W6 = 4.

---

## 9. Project report outline

1. **Abstract:** the problem, the graph-colouring model, the four algorithms, and the key result (B&B saves slots that greedy wastes, with proof of optimality).
2. **Introduction:** why exam scheduling matters, the manual-scheduling pain points, and the project objectives.
3. **Problem definition:** inputs (subjects, batches, slots), the hard constraint (no batch has two exams in one slot), the objective (minimise slots), and the formal graph model.
4. **Data structures used:** `vector` records; adjacency list vs matrix (and why both are kept); `map` for merging edges; `set` for DSATUR saturation; the counter matrix for forward checking; `optional` for the last result.
5. **Algorithms:** greedy (three orderings), DSATUR, the verifier, the clique bound, backtracking, and B&B with its pruning rules. For each one, give pseudocode, the Section 5 dry run, and complexity.
6. **Implementation:** module diagram (Models → Graph → Coloring → Backtracking / B&B → Scheduler → App), menu flow, input validation, file format, and RAII with no raw `new`/`delete`.
7. **Results:** screenshots of the Sample A, B and C runs, the comparison table (Section 4), self-test output, and a slots/nodes/time table for each sample.
8. **Limitations:** exponential worst case; only hard constraints are modelled; no room capacity; the clique bound can be weak (χ − ω can be large); single-threaded.
9. **Conclusion:** greedy is fast but can waste slots, while B&B finds and proves the minimum for realistic sizes.
10. **Future scope:** see Section 10.
11. **References:** Brélaz (1979) "New methods to color the vertices of a graph"; Welsh & Powell (1967); Karp (1972); Cormen et al., *Introduction to Algorithms*.

---

## 10. Future extensions

* **Room capacity:** each slot has rooms with seat counts. A subject can share a slot only if its students fit, which turns the problem into colouring plus bin packing.
* **Invigilator allocation:** after colouring, assign staff to (slot, room) pairs as bipartite matching or flow, with a cap on duties per teacher.
* **Priority or fixed-slot subjects:** pre-colour some vertices, such as a university-wide exam fixed on Day 1, and colour the rest around them, which is list colouring.
* **Gap between exams for the same batch:** add soft constraints such as "no two exams on the same day for a batch", optimised with weighted penalties, local search, tabu search or simulated annealing.
* **Larger instances:** use stronger lower bounds (fractional or LP bounds), DSATUR-based B&B with dynamic ordering, or ILP/SAT solvers.
* **GUI or web front end:** a REST wrapper around `ExamScheduler` with a calendar view, and drag-and-drop with live conflict checking through the verifier.
* **Export:** CSV export using the same record layout as the data file, plus PDF or HTML timetables per batch and per room.
