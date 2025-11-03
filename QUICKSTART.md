# Quick Start Guide — Melvin Core

Get Melvin running in under 2 minutes.

## Build

```bash
make
```

That's it. You'll get a `melvin_core` executable.

## Run (Basic)

```bash
./melvin_core
```

The system will:
- Initialize with 8K nodes, 64K edges
- Load previous state if `nodes.bin` and `edges.bin` exist
- Start the always-on loop (50ms ticks)
- Log stats every 100 ticks
- Save state every 2000 ticks

**To stop:** Press `Ctrl+C`

## Run (With Input)

### Pipe text input

```bash
echo "hello world" | ./melvin_core
```

### Interactive mode (with PTY)

```bash
./melvin_core | tee output.txt
```

Type input, and Melvin will:
1. Detect patterns (newline, keywords, etc.)
2. Learn which patterns predict others
3. Generate output (currently random babbling)
4. Observe its own output next tick

### Test script

```bash
./test_melvin.sh
```

Runs Melvin for 10 seconds with sample input including:
- Text strings
- Detector triggers (`/dev/video`, `error`, `success`)
- Prompts

## What You'll See

```
=== MELVIN CORE STARTING ===
Tick period: 50 ms
Always-on loop active. Press Ctrl+C to stop.

[INIT] System initialized: 8192 nodes, 65536 edges, 9 detectors, 73 macros
[TICK 100] nodes=9 edges=0 active=2 err=0.000 created_e=0 pruned_e=0 created_n=0 layers=0
[TICK 200] nodes=11 edges=3 active=4 err=0.125 created_e=3 pruned_e=0 created_n=2 layers=0
[TICK 300] nodes=13 edges=8 active=5 err=0.087 created_e=8 pruned_e=0 created_n=4 layers=0
...
[PERSIST] tick=2000 nodes=47 edges=124
```

**Interpretation:**
- `nodes`: Total nodes (starts with 9 detectors)
- `edges`: Total connections
- `active`: Currently firing nodes
- `err`: Mean prediction error (lower = better learning)
- `created_e/n`: Cumulative growth
- `pruned_e`: Cumulative pruning
- `layers`: Meta-nodes created

## Analyze the Graph

After running:

```bash
./analyze_graph.py
```

Shows:
- Node/edge counts and statistics
- Degree distributions
- Top hub nodes (most connected)
- Strongest edges (highest weights)
- High-credit edges (most useful)

Example output:

```
============================================================
MELVIN GRAPH ANALYSIS
============================================================

Total Nodes: 47
  Active: 5 (10.6%)
  Avg Threshold: 128.0
  Avg In-Degree: 2.64
  Avg Out-Degree: 2.64

Top 5 Hub Nodes:
  1. Node 12: in=8, out=7, total=15
  2. Node 5: in=6, out=6, total=12
  ...

Total Edges: 124
  Avg Fast Weight: 45.23
  Avg Slow Weight: 38.17
  Avg Credit: 12.45
  ...
```

## Command-Line Options

```bash
./melvin_core --nodes 16384    # Increase node capacity
./melvin_core --edges 131072   # Increase edge capacity
./melvin_core --help           # Show help
```

## Files Generated

| File | Description | Size |
|------|-------------|------|
| `nodes.bin` | Serialized nodes | ~320KB (8K nodes) |
| `edges.bin` | Serialized edges | ~2.5MB (64K edges) |

These are loaded on restart, so learning persists across sessions.

## What It's Doing

Every 50ms tick:

1. **INPUT:** Read bytes from stdin, run detectors
2. **RECALL:** Merge last output into input (self-observation)
3. **PREDICT:** 
   - Propagate activations through graph
   - Compare predictions to reality
   - Update edge weights based on error
   - Create new nodes from co-activation patterns
4. **OUTPUT:** Select and emit a macro (byte sequence)

Over time:
- Weak connections get pruned
- Strong patterns get reinforced
- Internal abstractions emerge from repeated co-activation
- Meta-nodes form from dense clusters
- Prediction error decreases

## Understanding the Learning

### Detectors (Sensory Nodes)

Built-in patterns that fire when seen:
- `\n` — Newline character
- `/dev/video` — Video device string
- `error`, `Error` — Error indicators
- `success`, `created` — Success indicators
- `\xFF\xD8\xFF` — JPEG header
- `$` — Shell prompt

These are the "eyes" — the system learns what predicts what.

### Edges (Connections)

Each edge `i→j` learns:
- **Fast weight:** Quick adaptation to recent patterns
- **Slow weight:** Stable long-term structure
- **Credit:** How useful this connection is

If node `i` fires and node `j` fires next tick, the edge strengthens.

### Internal Nodes

When two nodes often fire together:
- A new node is created
- It connects to both original nodes
- It represents the **co-activation pattern**

This is how abstractions form — no programmer designed them.

### Meta-Nodes (Layers)

When a group of nodes becomes densely connected:
- A meta-node is created
- It summarizes the cluster
- Higher-level patterns can build on it

This is how hierarchy emerges — from local density, not global design.

## Debugging

### No output?

- Melvin starts with random babbling — it needs time to learn
- Try feeding more input to trigger detectors
- Check logs to see if edges are being created

### Error increases?

- Normal during exploration (high epsilon)
- Should decrease after ~500-1000 ticks as patterns stabilize
- If it stays high, increase input variety

### No edges created?

- Detectors need to fire
- Send input that triggers patterns: `error`, `success`, etc.
- Co-activation creates edges after ~10 ticks

### Memory issues?

Reduce capacities:
```bash
./melvin_core --nodes 4096 --edges 32768
```

## Next Steps

1. **Feed real data:** Connect to a terminal, file stream, or sensor
2. **Add detectors:** Edit `system_init()` to add domain-specific patterns
3. **Enrich macros:** Teach it useful commands by adding to `macro_init()`
4. **Visualize:** Plot the graph using NetworkX or Graphviz
5. **Tune parameters:** Adjust learning rates in the header

## Theory → Practice

| Concept | Implementation | Observable Effect |
|---------|----------------|-------------------|
| Predictive coding | `propagate()` → `observe_and_update()` | Error decreases |
| Hebbian learning | `C11`, `C10` counters | Edges strengthen with co-activation |
| Eligibility traces | `e->eligibility` | Temporal credit assignment |
| Two-timescale | `w_fast`, `w_slow` | Fast adaptation + slow consolidation |
| Emergence | `try_create_nodes()`, `try_layer_emergence()` | Graph grows organically |
| Pruning | `prune()` | Weak edges disappear |

## Philosophy

**"The system doesn't know it's learning."**

There's no:
- Global loss function
- Backpropagation
- Training/inference split
- Labeled data
- Optimization objective

Just:
- Local prediction
- Local error
- Local adjustment

And from these simple rules, **structure emerges.**

---

**Now go run it and watch patterns form from chaos.**

