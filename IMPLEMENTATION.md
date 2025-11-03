# Melvin Core — Implementation Details

This document describes the exact mathematics and algorithms implemented in `melvin_core.c`.

## Table of Contents

1. [Four-Step Loop](#four-step-loop)
2. [Data Structures](#data-structures)
3. [Learning Algorithm](#learning-algorithm)
4. [Dynamic Graph Operations](#dynamic-graph-operations)
5. [Action Selection](#action-selection)
6. [Parameters](#parameters)

---

## Four-Step Loop

The core loop runs every **50ms** (configurable via `TICK_MS`):

```c
while (1) {
    // (1) INPUT — "what is happening?"
    read_input();                    // Non-blocking stdin read
    slice_frame();                   // Extract up to 4KB frame
    
    // (2) RECALL — "what did I do?"
    merge_output_into_input();       // Append last output to input
    detector_run_all();              // Fire sensory nodes
    
    // (3) PREDICT — "what should happen?"
    propagate();                     // Compute S_j, hat_j
    observe_and_update();            // Learn from error
    try_create_nodes();              // Grow graph (every 10 ticks)
    
    // (4) OUTPUT — "do it"
    emit_action();                   // ε-greedy macro selection
    
    // Housekeeping (periodic)
    if (tick % 200 == 0) prune();
    if (tick % 100 == 0) try_layer_emergence();
    if (tick % 2000 == 0) persist_graph();
    
    tick++;
    usleep(50000);                   // 50ms sleep
}
```

---

## Data Structures

### Node Structure

```c
typedef struct {
    uint32_t id;                  // Unique identifier
    uint8_t  a;                   // Current activation {0,1}
    uint8_t  a_prev;              // Previous activation
    uint16_t theta;               // Firing threshold
    uint16_t in_deg, out_deg;     // Degree counts
    uint32_t last_tick_seen;      // Last active tick
    uint16_t burst;               // Short-term spike count
    uint32_t sig_history;         // 32-bit activation signature
    uint8_t  is_meta;             // Is this a meta-node?
    uint32_t cluster_id;          // Cluster membership
    int32_t  soma;                // Accumulated input Σw·a
    uint8_t  hat;                 // Predicted activation
    uint16_t total_active_ticks;  // Statistics
} Node;
```

**Key fields:**
- `soma`: Sum of weighted inputs from active predecessors
- `hat`: Binary prediction based on `soma >= theta`
- `sig_history`: Bit vector of last 32 activations for pattern matching

### Edge Structure

```c
typedef struct {
    uint32_t src, dst;            // Node indices
    uint8_t  w_fast;              // Fast weight [0,255]
    uint8_t  w_slow;              // Slow weight [0,255]
    int16_t  credit;              // Signed credit accumulator
    uint16_t use_count;           // Usage counter
    uint16_t stale_ticks;         // Ticks since last use
    float    eligibility;         // Eligibility trace
    float    C11, C10;            // Predictive lift counters
    float    avg_U;               // Average usefulness
    uint16_t slow_update_countdown;
} Edge;
```

**Key fields:**
- `w_fast`, `w_slow`: Two-timescale weights
- `C11`, `C10`: Decayed counts for conditional probability estimation
- `eligibility`: Trace for temporal credit assignment

### Graph Storage

```c
typedef struct {
    Node    *nodes;               // Flat array
    uint32_t node_count, node_cap;
    uint32_t *node_free_list;     // Recycled indices
    
    Edge    *edges;
    uint32_t edge_count, edge_cap;
    uint32_t *edge_free_list;
} Graph;
```

**Free-list management:** Deleted nodes/edges go into free-lists for reuse, avoiding heap fragmentation.

---

## Learning Algorithm

### Step 1: Propagation

For each node `j`, compute the **soma** (accumulated weighted input):

```
S_j = Σ_i w_eff(i,j) * a_i
```

where effective weight is:

```
w_eff(i,j) = γ * w_slow(i,j) + (1-γ) * w_fast(i,j)
             with γ = 0.8
```

**Predicted activation:**

```
hat_j = { 1  if S_j >= theta_j
        { 0  otherwise
```

### Step 2: Observation

After detectors update actual activations `a_j`:

**Surprise:**
```
s_j = |a_j - hat_j| ∈ {0, 1}
```

**Per-edge discrepancy** (when source was active):
```
d_ij = a_i * (a_j - hat_j) ∈ {-1, 0, +1}
```

### Step 3: Usefulness Computation

Two components blended together:

#### (a) Predictive Lift

Estimate conditional probability:

```
p(j|i) = C11_ij / (C11_ij + C10_ij + ε)
```

where:
- `C11_ij`: decayed count of `(a_i=1, a_j_next=1)` events
- `C10_ij`: decayed count of `(a_i=1, a_j_next=0)` events
- Both decay: `C11 ← λ * C11`, `C10 ← λ * C10` with `λ=0.99`

Baseline probability:

```
p(j) = P1_j / (P1_j + P0_j + ε)
```

**Predictive lift:**

```
u_ij = p(j|i) - p(j)
```

Edge is useful if it raises the probability of `j` firing.

#### (b) Surprise Credit

Instantaneous error-driven signal:

```
e_ij = d_ij * s_j
```

Positive when edge should have been stronger, negative when too strong.

#### (c) Blend

```
U_ij = β * u_ij + (1-β) * e_ij
       with β = 0.7
```

### Step 4: Two-Timescale Weight Updates

#### Fast Track (reactive)

**Eligibility trace:**
```
E_ij ← λ_e * E_ij + a_i
      with λ_e = 0.9
```

**Fast weight update:**
```
Δw_fast = clamp( η_fast * U_ij * E_ij, -δ_max, +δ_max )
w_fast ← clamp( w_fast + Δw_fast, 0, 255 )
```

with `η_fast=3`, `δ_max=4`.

**Decay:**
```
w_fast ← α_fast * w_fast + (1-α_fast) * Δw_fast
         with α_fast = 0.95
```

#### Slow Track (consolidation)

Every 50 ticks, update based on sign of average usefulness:

```
if avg_U > 0.05:  w_slow ← clamp(w_slow + 1, 0, 255)
if avg_U < -0.05: w_slow ← clamp(w_slow - 1, 0, 255)
```

where:
```
avg_U ← 0.95 * avg_U + 0.05 * U_ij
```

### Step 5: Credit Accumulation

```
if surprise == 0 and U_ij > 0:
    credit ← credit + 1
if surprise == 1 and U_ij < 0:
    credit ← credit - 1
```

Credit is clamped to `[-10000, +10000]`.

---

## Dynamic Graph Operations

### Node Creation

**Trigger:** Two nodes `i` and `j` both active with:
1. Co-activation frequency > 10 (Hamming weight of `sig_i AND sig_j`)
2. Similarity > 0.6 (measured as `1 - |sig_i XOR sig_j|/32`)

**Action:**
```c
uint32_t new_idx = node_create(&g_graph);
edge_create(&g_graph, i, new_idx);
edge_create(&g_graph, j, new_idx);
```

New node represents the **co-activation pattern**.

### Edge Creation

**Trigger:** 
- Node `i` repeatedly active before `j` becomes active
- Positive usefulness signal
- No existing edge `i→j`

**Action:**
```c
edge_create(&g_graph, i, j);
// Initial weights: w_fast = w_slow = 32
```

### Pruning

#### Edge Pruning

Delete edge if **all** conditions met:
```
w_eff < 2  AND  use_count < 10  AND  stale_ticks > 200
```

#### Node Pruning

Delete node if:
```
in_deg == 0  AND  out_deg == 0  AND  (tick - last_tick_seen) > 1000
```

### Layer (Meta-Node) Emergence

**Every 100 ticks:**

For each node `i`:
1. Count active neighbors
2. Compute density: `active_neighbors / total_neighbors`
3. If `density > 0.6` and `total_neighbors >= 10`:
   - Create meta-node `M`
   - Connect `i → M`
   - Mark `M.is_meta = 1`

**Purpose:** Meta-nodes represent dense clusters, forming hierarchical layers.

---

## Action Selection

### Macros

**Structure:**
```c
typedef struct {
    uint8_t  bytes[256];    // Action sequence
    uint16_t len;
    float    U_fast, U_slow;  // Utility scores
    uint32_t use_count;
    uint32_t last_used_tick;
} Macro;
```

**Initialization:** Start with safe alphabet (a-z, A-Z, 0-9) + special chars (newline, space, etc.)

### ε-Greedy Selection

```python
if random() < epsilon:
    idx = random_macro()
else:
    idx = argmax( γ * U_slow + (1-γ) * U_fast )
```

**Epsilon decay:**
```
epsilon ← epsilon * 0.9995
epsilon = max(epsilon, 0.05)
```

### Utility Update

After action, update macro utility based on current error:

```
reward = 1.0 - mean_error

U_fast ← α_fast * U_fast + (1-α_fast) * reward
U_slow ← α_slow * U_slow + (1-α_slow) * reward
```

---

## Detectors (Byte Patterns → Nodes)

**Simple pattern matching:**

```c
typedef struct {
    char pattern[64];      // e.g., "\n", "/dev/video", "error"
    uint32_t node_id;      // Associated sensory node
    uint8_t type;          // 0=exact, 1=contains
} Detector;
```

**Every tick:**
```c
for each detector d:
    if pattern appears in current_frame:
        nodes[d.node_id].a = 1
    else:
        nodes[d.node_id].a = 0
```

**Default detectors:**
- `\n` (newline)
- `/dev/video` (camera)
- `error`, `Error` (errors)
- `\xFF\xD8\xFF` (JPEG header)
- `success`, `created`, `failed`
- `$` (shell prompt)

---

## I/O Subsystem

### Ring Buffers

**RX (input):** 16KB circular buffer for stdin
**TX (output):** 16KB circular buffer for self-observation

```c
typedef struct {
    uint8_t *buf;
    uint32_t size, head, tail, count;
} RingBuffer;
```

### Frame Slicing

Each tick:
1. Read available bytes from stdin → RX ring
2. Extract up to 4KB → `current_frame`
3. Append last output → `current_frame` (self-observation)
4. Run detectors on merged frame

### Output Mirroring

Every emitted byte is **mirrored to TX ring** so the system observes its own actions:

```c
write(STDOUT_FILENO, macro.bytes, macro.len);
ring_write(&tx_ring, macro.bytes, macro.len);
memcpy(last_output_frame, macro.bytes, macro.len);
```

---

## Persistence

### Snapshot Format

**nodes.bin:**
```
[node_count:4] [next_node_id:4] [Node*node_count]
```

**edges.bin:**
```
[edge_count:4] [Edge*edge_count]
```

Saved every **2000 ticks** (~100 seconds at 50ms/tick).

On restart, graph is restored from disk.

---

## Parameters

| Symbol | Value | Meaning |
|--------|-------|---------|
| `TICK_MS` | 50 | Milliseconds per tick |
| `λ` | 0.99 | Count decay (predictive lift) |
| `λ_e` | 0.9 | Eligibility trace decay |
| `β` | 0.7 | Predictive vs error blend |
| `γ` | 0.8 | Slow weight fraction |
| `η_fast` | 3 | Fast learning rate |
| `δ_max` | 4 | Max weight change per tick |
| `α_fast` | 0.95 | Fast weight persistence |
| `α_slow` | 0.999 | Slow weight persistence |
| `τ_prune` | 2 | Weight threshold for pruning |
| `T_stale` | 200 | Stale ticks threshold |
| `T_node_stale` | 1000 | Node stale threshold |
| `ε_start` | 0.3 | Initial exploration rate |
| `ε_decay` | 0.9995 | Epsilon decay per tick |
| `ε_min` | 0.05 | Minimum exploration rate |

---

## Complexity

**Per-tick operations:**

| Operation | Complexity | Notes |
|-----------|------------|-------|
| Read input | O(1) | Non-blocking read |
| Detectors | O(D·F) | D detectors, F frame size |
| Propagation | O(E) | E edges |
| Observation | O(E) | Per-edge learning |
| Output | O(1) | Macro emission |
| Node creation | O(N²) | Every 10 ticks, limited to first 1000 nodes |
| Pruning | O(E+N) | Every 200 ticks |
| Layer emergence | O(N·E) | Every 100 ticks, limited to first 500 nodes |

**Memory:**
- Nodes: `node_cap * sizeof(Node)` ≈ 8K * 40 bytes = 320KB
- Edges: `edge_cap * sizeof(Edge)` ≈ 64K * 40 bytes = 2.5MB
- Buffers: 32KB (rings) + 8KB (frames) = 40KB
- **Total:** ~3MB with default capacities

---

## Theory Foundations

This implementation combines:

1. **Predictive Coding** (Rao & Ballard, 1999): Minimize prediction error
2. **Hebbian Learning** (Hebb, 1949): "Fire together, wire together"
3. **Eligibility Traces** (Sutton & Barto, 2018): Temporal credit assignment
4. **Two-Timescale Learning** (Benna & Fusi, 2016): Fast plasticity + slow consolidation
5. **Sparse Distributed Representations** (Kanerva, 1988): Binary activations
6. **Hierarchical Temporal Memory** (Hawkins & Blakeslee, 2004): Layer emergence

All implemented with **local rules** and **no global coordination**.

---

## Future Extensions

1. **Attention:** Bias propagation toward high-burst nodes
2. **Working Memory:** Maintain short-term activation buffers
3. **Sequence Learning:** Temporal pooling for action chains
4. **Multi-Modal:** Multiple detector banks (vision, audio, text)
5. **Dreaming:** Offline replay with random activation patterns
6. **Meta-Learning:** Adjust learning rates based on stability

---

**Core Philosophy:** Global intelligence emerges from local interactions. The system doesn't know it's learning; it just predicts, observes error, and adjusts—continuously, forever.

