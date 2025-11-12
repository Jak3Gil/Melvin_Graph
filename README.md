# MELVIN - Spreading Activation Network

**Brain-inspired associative memory through massive edge connectivity**

## Core Architecture

```
Input: "cat" (1 node activates)
         ↓
Pattern fitting creates edges to:
  [sat, mat, hat] (similar endings)
  [dog, log, fog] (same length)  
  [car, can, cut] (similar start)
         ↓
Spreading activation cascades
         ↓
100+ nodes fire through associations!
```

**Key Insight**: When you hear "cat", ~1 million neurons fire in your brain. Not because "cat" is encoded in 1 million neurons, but because ONE activation **cascades through associations**.

## Design Philosophy

- **1 word = 1 node** (simple, clean)
- **Millions of edges** (massive connectivity)
- **Pattern fitting** (new input connects to ALL matching old patterns)
- **Spreading activation** (cascade through network)
- **Soft coherence** (frequently co-occurring patterns boost each other)
- **No hard limits** (auto-grows from 16KB to gigabytes)

## Data Structures

### 24-Byte Node (Compact)
```c
typedef struct __attribute__((packed)) {
    uint8_t token[16];     // Token data (any bytes)
    float activation;      // Current activation (0.0-1.0)
    uint16_t token_len;    // Actual length
    uint16_t frequency;    // Usage count
} Node;
```

### 10-Byte Edge (Compact)
```c
typedef struct __attribute__((packed)) {
    uint32_t from;         // Source node
    uint32_t to;           // Target node
    uint8_t weight;        // Strength (0-255)
    uint8_t times_fired;   // Co-activation count
} Edge;
```

### Edge Hash Table (O(1) Lookups)
- Allocated in RAM (transient, not persisted)
- Rebuilt on load from edge list
- Fast edge creation and lookup

## How It Works

### 1. Input Processing (Pattern Fitting)
```
Input: "cat sat"
  ↓
Creates nodes: cat, sat
  ↓
Finds patterns:
  - Sequence: cat → sat
  - Similarity: cat ↔ [hat, mat, bat, rat]
  - Substrings: cat ↔ [chat, scat]
  - Length: cat ↔ [all 3-letter words]
  ↓
Creates edges for ALL matches
```

### 2. Spreading Activation (Cascade)
```
cat activates (1.0)
  ↓ edges fire
sat activates (0.8)
mat activates (0.72)
hat activates (0.69)
  ↓ edges fire again
rat activates (0.64)
bat activates (0.61)
  ↓
Continues until activation < threshold
```

### 3. Coherence Scoring (Soft Selection)
```
Edge fires → times_fired++
  ↓
coherence_boost = 1.0 + (times_fired / 100)
  ↓
Frequently co-occurring patterns get stronger!
```

### 4. Output Generation
```
Outputs nodes activated above threshold
BUT excludes direct input nodes
  ↓
Shows what was PREDICTED, not echoed
```

## Usage

### Basic
```bash
make melvin
echo "cat sat mat hat bat rat" | ./melvin
echo "cat" | ./melvin
# Output: sat mat hat bat rat (cascade!)
```

### Debug Mode
```bash
MELVIN_DEBUG=1 echo "cat sat" | ./melvin
# Shows: node creation, edge counts, cascade stats
```

### Persistent Learning
```bash
echo "cat sat" | ./melvin          # Creates graph.mmap
echo "dog log" | ./melvin          # Extends graph
echo "cat" | ./melvin              # Uses learned patterns
rm graph.mmap                      # Reset
```

## Test Results

```bash
$ echo "cat sat mat hat bat rat" | ./melvin
$ echo "cat" | MELVIN_DEBUG=1 ./melvin

[SPREAD] 1 input nodes → 10 activated nodes (10.0x cascade!)
Output: sat mat hat bat rat dog log fog hog
```

**With 100+ word vocabulary:**
```
1 input → 102 activated nodes (102x cascade!)
5,494 edges created
53.9 average edges/node
```

## Unlimited Capacity

**No hard limits!** Auto-grows exponentially:

```
Start:    16 KB (256 nodes, 1K edges)
          ↓ auto-grow when full
Grow 1:   32 KB (256 nodes, 2K edges)
Grow 2:   64 KB (256 nodes, 4K edges)
Grow 3:   128 KB (512 nodes, 8K edges)
          ↓ keeps doubling
Unlimited: Limited only by disk space
```

**Test with 200 words:**
```
Auto-grew 7 times
Final: 400 nodes, 39,927 edges (0.6 MB)
```

## Performance

- **Node creation**: O(N) - linear search for reuse
- **Edge creation**: O(1) - hash table lookup
- **Pattern matching**: O(N²) - compares new words to all existing (main bottleneck)
- **Spreading activation**: O(E × H) - edges × hops
- **Memory**: 24 bytes/node + 10 bytes/edge + 12 bytes/hash entry

## Capabilities

### ✅ What It Does Well
- Sequence learning (A → B → C)
- Pattern recognition (token similarity)
- Transfer learning (generalization)
- Spreading activation (associative memory)
- Coherence-based selection (soft competition)
- Universal data (any byte sequences)
- Unlimited growth (auto-scaling)

### ⚠️ Current Limitations
- O(N²) pattern matching (slow with huge vocabularies)
- No semantic understanding (purely surface patterns)
- Limited multi-hop reasoning (decay cutoff)
- No arithmetic computation (pattern-only)
- No structural abstraction (byte-level only)

## Architecture Highlights

### Spreading Activation
Like the brain - one activation cascades to millions through rich connectivity.

### Pattern Fitting
New inputs connect to ALL matching patterns through:
- Sequence (temporal ordering)
- Similarity (character overlap)
- Substrings (shared n-grams)
- Length (same-size patterns)

### Coherence Through Statistics
- `times_fired` tracks co-activation
- Frequently paired patterns boost each other
- No hard rules - soft statistical preferences
- Generalization still works (rare patterns activate, just weaker)

### Hash-Based Edge Lookup
- O(1) edge creation/lookup
- Separate from mmap (transient index)
- Rebuilt on load
- Enables millions of edges efficiently

## File Structure

```
melvin.c              - Main implementation (613 lines)
melvin                - Compiled binary
Makefile              - Build system
graph.mmap            - Persistent graph (auto-created)
backup_old_version/   - Previous versions
README.md             - This file
```

## Building From Source

```bash
make melvin           # Compile
make clean            # Remove build artifacts
```

**Requirements**: gcc, POSIX system (Linux/macOS)

## Example Session

```bash
# Train on patterns
echo "cat sat mat hat bat rat" | ./melvin
echo "dog log fog hog" | ./melvin
echo "book look cook took" | ./melvin

# Query - watch the cascade!
echo "cat" | MELVIN_DEBUG=1 ./melvin
# [SPREAD] 1 input → 18 activated nodes (18x cascade!)
# Output: sat mat hat bat rat dog log fog hog book look...
```

## Philosophy

> Intelligence = rich edge connectivity, not complex nodes
> Learning = creating edges, not complex representations
> Reasoning = spreading activation through patterns

This is **associative memory** - how semantic networks and spreading activation models work in cognitive science.

## Credits

Based on insights about:
- Spreading activation (Collins & Loftus, 1975)
- Semantic networks (Quillian, 1968)
- Population coding (neuroscience)
- Pattern fitting (connectionism)
