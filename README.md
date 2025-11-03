# Melvin Core — Minimal Always-On Learning System

Single-file C implementation of an emergent learning system that runs a continuous perception→prediction→action loop.

## Core Concepts

**Four-Step Always-On Loop:**

```
1) What is happening?      (INPUT — read bytes, sensors fire)
2) What did I do?          (RECALL — observe own output)
3) What should happen?     (PREDICT — propagate, observe error, update weights, assign credit)
4) Do it.                  (OUTPUT — select and emit action)
```

## Features

- **Local Learning:** No global backprop; all learning is edge-local via predictive lift + surprise credit
- **Integer Math:** Binary activations {0,1}, integer weights [0,255], integer thresholds
- **Two-Timescale Plasticity:** Fast track (reactive) + slow track (consolidation)
- **Dynamic Graph:** Nodes and connections grow from co-activation patterns, prune when weak
- **Layer Emergence:** Meta-nodes form from dense clusters without supervision
- **Binary I/O:** Everything is bytes; detectors map patterns to node activations
- **Action Babbling:** ε-greedy macro selection with utility learning
- **Persistent State:** Saves/restores graph to `nodes.bin` and `edges.bin`

## Building

```bash
make
```

Or manually:

```bash
gcc -O2 -Wall -Wextra -std=c99 -o melvin_core melvin_core.c -lm
```

## Running

```bash
./melvin_core
```

Optional arguments:

```bash
./melvin_core --nodes 16384 --edges 131072
```

The system runs forever (Ctrl+C to stop) and:
- Reads from stdin (non-blocking)
- Writes to stdout
- Logs progress every 100 ticks
- Persists graph every 2000 ticks

## Architecture

### Data Structures

**Node:**
- `id`, `theta` (threshold), `a` (activation), `a_prev`
- `in_deg`, `out_deg`, `last_tick_seen`, `burst`
- `sig_history` (32-bit activation signature for pattern matching)
- `soma` (accumulated input), `hat` (predicted activation)

**Edge:**
- `src`, `dst`, `w_fast`, `w_slow` (two-timescale weights)
- `credit`, `use_count`, `stale_ticks`
- `eligibility` (trace for updates)
- `C11`, `C10` (predictive lift counters)
- `avg_U` (average usefulness)

**Detectors:**
- Map byte patterns to sensory nodes
- Examples: newline, "/dev/video", "error", JPEG header

**Macros:**
- Action sequences (byte strings)
- Each has `U_fast`, `U_slow` utility scores
- Start with safe alphabet + special chars

### Learning Algorithm

**Propagation:**
```
S_j = Σ_i w_eff(i,j) * a_i
hat_j = (S_j >= theta_j) ? 1 : 0
```

**Observation:**
```
surprise_j = |a_j_next - hat_j|
discrepancy_ij = a_i * (a_j_next - hat_j)
```

**Usefulness (two components):**

1. **Predictive lift:**
   ```
   u_ij = p(j|i) - p(j)
   where p(j|i) = C11 / (C11 + C10)
   ```

2. **Surprise credit:**
   ```
   e_ij = discrepancy_ij * surprise_j
   ```

3. **Blend:**
   ```
   U_ij = β * u_ij + (1-β) * e_ij
   ```

**Two-Timescale Updates:**
```
w_fast += η_fast * U_ij * eligibility_ij
w_slow += sign(avg_U_ij)  (occasional)
w_eff = γ * w_slow + (1-γ) * w_fast
```

### Growth & Pruning

**Node Creation:**
- Find nodes with co-activation frequency > threshold
- Check Hamming similarity of activation signatures
- Create new internal node connected to co-activators

**Edge Creation:**
- When nodes repeatedly predict each other with positive usefulness

**Meta-Node (Layer) Emergence:**
- Find dense clusters (density > 0.6, size ≥ 10)
- Create meta-node representing the cluster
- Connect meta ↔ members

**Pruning:**
- Delete edges with: `w < 2 AND use_count < 10 AND stale_ticks > 200`
- Delete nodes with: `in_deg = 0 AND out_deg = 0 AND stale > 1000`

## Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `TICK_MS` | 50 | Milliseconds per tick |
| `LAMBDA_DECAY` | 0.99 | Count decay factor |
| `BETA_BLEND` | 0.7 | Predictive vs error weight |
| `GAMMA_SLOW` | 0.8 | Slow weight fraction |
| `ETA_FAST` | 3 | Fast learning rate |
| `PRUNE_PERIOD` | 200 | Ticks between pruning |
| `LAYER_PERIOD` | 100 | Ticks between layer checks |
| `SNAPSHOT_PERIOD` | 2000 | Ticks between saves |

## Output Format

Logs every 100 ticks:
```
[TICK 1000] nodes=143 edges=487 active=23 err=0.342 created_e=487 pruned_e=12 created_n=15 layers=2
```

Every 2000 ticks saves:
```
[PERSIST] tick=2000 nodes=143 edges=487
```

## Files

- `melvin_core.c` — Complete implementation (single file, ~1400 lines)
- `nodes.bin` — Binary snapshot of nodes
- `edges.bin` — Binary snapshot of edges

## Design Philosophy

**Everything is local:**
- No global loss function
- No backpropagation through time
- Each edge learns from its own predictive success

**Emergence over engineering:**
- Abstractions form from reuse, not design
- Layers emerge from density
- Compression bias rewards parsimony

**Always-on, always learning:**
- No separate training/inference phases
- Learning happens inside prediction
- Graph continuously adapts

## Next Steps

To make Melvin interactive:

1. **Pipe input:** `echo "hello" | ./melvin_core`
2. **Connect to terminal:** Use PTY for bidirectional communication
3. **Add more detectors:** Extend pattern recognition
4. **Enrich macros:** Learn command sequences from success
5. **Visualization:** Parse logs for real-time graph visualization

## Theory

This implements:
- **Predictive coding** (minimize surprise)
- **Hebbian learning** (fire together, wire together)
- **Eligibility traces** (credit assignment)
- **Two-timescale consolidation** (fast plasticity + slow structure)
- **Sparse distributed representations** (binary activations)
- **Hierarchical temporal memory** (layers from density)

All without any ML libraries, just integer math and local rules.

---

**Build it. Run it. Watch it learn.**

