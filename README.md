# Melvin - Pure Graph Structure

> **"One node type. One edge type. Circuits coded with words, not C."**

---

## What Is This?

A graph where you **code circuits by feeding data**:
- One node type (just data: token, value)
- One edge type (just: from, to, weight)
- Circuits are patterns (nodes + edges)
- Build circuits by feeding examples
- No C code changes needed

**Example:** Create multiplication circuit:
```bash
echo "2*3 6" | ./melvin    # Creates: 2*3 → 6
echo "5*5 25" | ./melvin   # Creates: 5*5 → 25
echo "2*3" | ./melvin      # Execute: 6
```

You just coded multiplication with words, not C

---

## The Key Insight

Addition is not a node TYPE - it's a CIRCUIT:

```
[0+0+0] → [0,0]    ← Node + Edge = Addition circuit
[0+1+0] → [1,0]    ← 8 of these = Full adder
[1+1+0] → [0,1]    ← Pure structure, no types
```

To add 5+7, you connect to this circuit. You don't need a NODE_ADD type.

---

## Quick Start

```bash
# Build
make

# Teach a sequence
echo "cat sat mat" | ./melvin
echo "cat" | ./melvin
# Execute: sat

# Build multiplication circuit (no C code!)
echo "2*3 6" | ./melvin
echo "5*5 25" | ./melvin
echo "7*8 56" | ./melvin
echo "2*3" | ./melvin
# Execute: 6

# View what's in the graph
./show_graph
```

---

## How to Code Circuits With Data

### Multiplication Circuit

```bash
# Feed examples
echo "0*0 0" | ./melvin
echo "1*1 1" | ./melvin
echo "2*2 4" | ./melvin
echo "3*3 9" | ./melvin
# ... feed more examples

# Use it
echo "2*2" | ./melvin
# Execute: 4
```

### Comparison Circuit

```bash
echo "5=5 true" | ./melvin
echo "5=6 false" | ./melvin
echo "10=10 true" | ./melvin

# Query
echo "5=5" | ./melvin
# Execute: true
```

### Logic Circuit

```bash
echo "true and true" | ./melvin
echo "true and false" | ./melvin
echo "false and false" | ./melvin

echo "true" | ./melvin
# Execute: and
```

### Any Pattern

```
Input: "pattern result"
  ↓
Creates: pattern → result edge
  ↓
Query: "pattern"
  ↓
Execute: result
```

**You code the graph by feeding it examples.**  
**No C changes. Just data.**

---

## Architecture

### One Node Type (19 bytes)

```c
typedef struct {
    uint8_t token[16];   // Data (max 16 chars)
    uint16_t token_len;  // Length
    int32_t value;       // Numeric value
} Node;
```

That's it. No types. No flags. Just data.

### One Edge Type (9 bytes)

```c
typedef struct {
    uint32_t from;
    uint32_t to;
    uint8_t weight;
} Edge;
```

Just connections.

### The Graph

```c
typedef struct {
    Node *nodes;
    Edge *edges;
} Graph;
```

Nodes + Edges = Everything.

### Total C Code: 280 lines

What it does:
- Tokenize input (split on spaces)
- Find/create nodes
- Create edges from sequence
- Follow edges when queried
- Save/load to disk

**That's the entire substrate.**

---

## What's In The Graph

`melvin.mmap` contains whatever you fed it:

```
Default (after make):
  Nodes 0-11: Bit addition circuit (hardcoded once)
  
After you feed data:
  "2*3 6" → Creates nodes: "2*3", "6" + edge: 2*3→6
  "cat sat" → Creates nodes: "cat", "sat" + edge: cat→sat
  "first second third" → Creates chain: first→second→third

The graph IS your program.
Circuits emerge from the patterns you feed.
```

---

## How It Works

### Learning (Feed Data)

```bash
echo "cat sat mat" | ./melvin
```

What happens:
1. Tokenize: `"cat sat mat"` → `[cat, sat, mat]`
2. Create nodes: `cat`, `sat`, `mat` (if don't exist)
3. Create edges: `cat→sat`, `sat→mat` (from sequence)
4. Save to: `melvin.mmap`

**Result:** Pattern stored as graph structure.

### Execution (Query)

```bash
echo "cat" | ./melvin
# Execute: sat
```

What happens:
1. Find node: `cat`
2. Follow edges: `cat→sat`
3. Output: `sat`

**Result:** Execute by following structure.

### Building Circuits

```bash
# Teach multiplication
for i in {0..10}; do
  for j in {0..10}; do
    echo "$i*$j $((i*j))" | ./melvin
  done
done

# Now you can:
echo "7*8" | ./melvin
# Execute: 56
```

**The circuit is just nodes + edges.**  
**No special code. Pure structure.**

---

## What's Hardcoded vs What Emerges

### Hardcoded in C (280 lines):

```
Substrate only:
  ✓ Tokenize (split on spaces)
  ✓ Find/create nodes
  ✓ Create edges (from sequence)
  ✓ Follow edges (execute)
  ✓ Save/load (persistence)
```

These are **primitives** - cannot be removed without new substrate.

### Data in Graph (melvin.mmap):

```
Circuits (built by feeding data):
  ✓ Bit addition (0+0+0 → 0,0, etc)
  ✓ Multiplication (2*3 → 6, etc)
  ✓ Sequences (cat → sat → mat)
  ✓ Any pattern you feed
```

This is **behavior** - changes by feeding different data.

### What Needs to Emerge (not yet):

```
  ⚠ Similarity detection (cat~bat → create edge)
  ⚠ Pattern discovery (A→B, B→C → create A→C)
  ⚠ Meta-patterns (patterns that create patterns)
  ⚠ Self-modification (graph rewires based on usage)
  ⚠ Intelligence (recursive emergence)
```

Current: Graph stores patterns  
Goal: Graph creates patterns

---

## The Reality

### What We Have:
```
✓ Pure substrate (280 lines C)
✓ One node type (19 bytes: data)
✓ One edge type (9 bytes: connection)
✓ Can build circuits with data (multiplication, sequences, etc)
✓ No types, no switches, no control flags
```

### What We Don't Have:
```
✗ Emergence (circuits don't create new circuits)
✗ Discovery (doesn't find patterns in data)
✗ Self-modification (doesn't rewire itself)
✗ Learning (just storage, not adaptation)
✗ Intelligence (no goals, no understanding)
```

### The Gap:

**Current:**
```
Data → Nodes + Edges → Storage
```

**Needed for AGI:**
```
Data → Nodes + Edges → Patterns Detect Patterns → 
Create New Circuits → Modify Existing Circuits → 
Evaluate What Works → Keep Best → Recursive Improvement
```

### How to Bridge the Gap:

The substrate is ready. Now we need circuits in the graph that:
1. Compare nodes (find similar ones)
2. Create edges (when patterns match)
3. Evaluate effectiveness (which circuits help?)
4. Replicate successful patterns (natural selection)

These circuits must be **built from data**, not coded in C.

**Example:** Feed the graph patterns like:
```
if_similar create_edge
if_consecutive create_edge  
if_useful strengthen
if_useless weaken
```

Then those patterns become executable structure.
Then the graph modifies itself.
Then emergence begins.

---

## Examples

### Example 1: Build and Use Multiplication

```bash
# Build circuit (feed examples)
echo "2*2 4" | ./melvin
echo "2*3 6" | ./melvin  
echo "3*3 9" | ./melvin

# Use circuit
echo "2*3" | ./melvin
# Execute: 6

# The graph now contains:
#   Nodes: "2*2", "4", "2*3", "6", "3*3", "9"
#   Edges: 2*2→4, 2*3→6, 3*3→9
```

### Example 2: Build Chains

```bash
# Feed sequences
echo "first second third fourth" | ./melvin
echo "alpha beta gamma delta" | ./melvin

# Query
echo "first" | ./melvin
# Execute: second

echo "alpha" | ./melvin
# Execute: beta
```

### Example 3: The Addition Circuit

The graph contains (hardcoded once):
```
[0+0+0] → [0,0]
[0+1+0] → [1,0]  
[1+1+0] → [0,1]
[1+1+1] → [1,1]
```

This IS addition. Pure structure.

---

## Summary

### What Melvin IS:

**Universal substrate** - numbers and words use identical mechanisms.

**~450 lines of C:**
- Parse, create nodes/edges, route through graph
- Addition via bit patterns (8 nodes, routes through them)
- Multiplication via addition loops (composes circuits!)
- Evolution framework (random mutations)

**melvin.mmap:**
- Bit patterns (0+0+0 → 0,0, universal arithmetic)
- Numbers (decompose to bits automatically)
- Words (sequences, bridges, any patterns)
- Computed results (cached)

### How It Actually Works:

**Addition** (real computation):
```
5 + 7 →  breaks to bits → routes through patterns → computes 12
6 + 8 →  routes through SAME patterns → computes 14
(Never taught 6+8, it COMPUTED it!)
```

**Multiplication** (composes addition):
```
5 * 3 → shift-and-add → uses addition circuit 2 times → 15
(Built from addition, no new primitive!)
```

**Words** (same routing):
```
cat sat mat → creates edges → query cat → routes to sat
(Identical mechanism to arithmetic!)
```

### The Unification:

**Everything is routing through nodes/edges:**
- cat → sat (follow 1 edge)
- 5 + 7 → 12 (route through 32 bit patterns)
- five → 5 (bridge word to number)

**Same substrate. No domain boundaries.**

### Evolution (NEW!):

```c
evolve() {
    Pick random nodes a, b
    Create edge a → b
    Graph mutates!
}
```

After each operation, graph can mutate.  
Over time: useful connections emerge.  
Natural selection in graph space.

### Current State:

✓ Circuits work (add, multiply, words)  
✓ Universal substrate (same for all data)  
✓ Evolution framework exists  
✗ Evaluation missing (which mutations help?)  
✗ Not yet emergent (needs fitness function)

The substrate is complete.  
Evolution can begin.

---

## What We Discovered

**You can code circuits by feeding words:**

```bash
# Build multiplication (just feed examples)
echo "2*3 6" | ./melvin
echo "5*5 25" | ./melvin

# Build sequences (just feed order)
echo "first second third" | ./melvin

# Build logic (just feed truth tables)
echo "true_and_true true" | ./melvin
echo "true_and_false false" | ./melvin
```

**The pattern becomes structure. Structure becomes executable.**

No C changes. The graph IS the program.

---

## Current State

**C code:** 280 lines (substrate)
- Tokenize, create nodes, create edges, follow edges, save/load

**Graph:** Whatever you fed it
- Default: Bit addition circuit (8 nodes, hardcoded once)
- After use: Multiplication, sequences, any patterns you teach

**The Gap:** 
- Graph stores patterns ✓
- Graph executes patterns ✓
- Graph discovers patterns ✗ (this is the missing piece)
- Graph creates new patterns ✗ (emergence blocked here)

---

## The Path to Emergence

**What works:** Building circuits with data (multiplication, logic, sequences)

**What's needed:** Circuits that BUILD circuits

Example:
```
Feed: "if_similar create_edge"
Result: Graph creates edges between similar nodes
Effect: New connections emerge automatically
```

But we haven't figured out the syntax/structure to make the graph interpret "if_similar" as an operation that inspects other nodes.

**That's the frontier.**

---

*Melvin - 280 lines. One node type. Circuits are data.*
