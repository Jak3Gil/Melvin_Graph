# MELVIN - Universal Neuron Learning System

A minimal graph-based learning system that uses Hebbian learning and meta-operations for self-modification.

## What It Does

- **Universal neurons**: Single neuron type (sigmoid activation) handles all computation
- **Hebbian learning**: Weights strengthen based on co-activation
- **Pattern detection**: Automatically creates modules from repeated patterns
- **Meta-operations**: Graph nodes that modify graph structure (self-programming)
- **Persistent storage**: mmap-based graph persistence across runs

## Architecture

**Core Components** (~1800 lines of C):
- `Node` (24 bytes): id, activation, data/theta, degrees, last_tick
- `Edge` (10 bytes): src, dst, w_fast (learning), w_slow (correlation tracking)
- `Module` (~160 bytes): Pattern abstraction with internal nodes
- Edge hash table for O(1) lookups
- Ring buffer for input
- Meta-operations for self-modification

**What Was Removed** (dead code):
- Hot/cold storage (never implemented)
- TX ring buffer (unused)
- Execution stack (abandoned feature)
- OP_SPLICE/OP_FORK (useless opcodes)
- Module hierarchy fields (never populated)
- Conversation history (wasteful)

## Build & Run

```bash
# Compile
make

# Run
echo "hello world" | ./melvin_core

# Test
./test_all.sh

# Clean
make clean
rm -f graph.mmap  # Delete persistent graph
```

## How It Works

### Bootstrap
System creates 9 seed nodes on first run:
1. **Thinker** - Always-active driver node
2. **Edge Creator** - Wires co-active nodes
3. **Correlator** - Tracks pattern co-occurrence  
4. **Pattern Detector** - Creates modules from patterns
5-9. **Self-optimization circuits** - Monitor performance, adjust thresholds, compute rewards

### Learning Loop
1. Input bytes → create/activate byte nodes
2. Multi-stride edges predict future bytes (1, 2, 4, 8, 16, 32 byte offsets)
3. Propagate (5 hops): edges fire, neurons activate
4. Learn: Hebbian weight updates based on co-activation
5. Meta-ops execute: detect patterns, create modules, adjust parameters
6. Output: activated output nodes

### Meta-Operations (Graph Self-Modification)

Meta-ops are encoded in `node.data` field (values ≥1000.0):

| Code | Operation | What It Does |
|------|-----------|--------------|
| 1000.0 | META_COUNT_ACTIVE | Counts active neighbors |
| 1001.0 | META_CORRELATE | Tracks co-activation (increments w_slow) |
| 1002.0 | META_THRESHOLD_CREATE | Creates module when correlation > threshold |
| 1003.0 | META_WIRE_PATTERN | Wires currently active nodes |
| 1004.0 | META_GROUP_MODULE | Groups active nodes into module |
| 1005.0 | META_MEASURE_PERFORMANCE | Tracks learning metrics |
| 1006.0 | META_ADJUST_THRESHOLD | Self-correcting threshold tuning |
| 1007.0 | META_TUNE_LEARNING | Adjusts learning rate |
| 1008.0 | META_COMPUTE_REWARD | Measures prediction accuracy |
| 1009.0 | META_DISCOVER_OBJECTIVE | Infers goals from data structure |

## Files

**Essential:**
- `melvin_core.c` - Main system (~1900 lines, cleaned)
- `Makefile` - Build configuration
- `graph.mmap` - Persistent graph storage (auto-created)
- `bootstrap_graph` - Binary for graph initialization
- `inspect_graph` - Binary for graph inspection

**Tests:**
- `test_all.sh` - Run all tests
- `test_echo.sh` - Basic I/O test
- `test_pattern.sh` - Pattern detection test
- `test_association.sh` - Association learning test
- `test_self_improvement.sh` - Self-optimization test

## Performance

**Current State:**
- Node capacity: Starts at 256, auto-grows
- Edge capacity: Starts at 1024, auto-grows
- Module capacity: Starts at 1024, auto-grows
- Hash table: 16K slots, auto-grows at 70% load
- Memory: ~0.3MB initial, grows as needed

**Bottlenecks:**
- 9 meta-operations execute every tick (some do O(N) or O(E) scans)
- No indexing of meta-op nodes (linear search)
- 5 propagation hops per tick

## What Actually Works

✅ **Working:**
- Basic I/O (bytes in → bytes out)
- Hebbian learning (weights change)
- Pattern detection (modules created)
- Edge creation (META_WIRE_PATTERN)
- Module auto-growth
- Mmap persistence
- Edge hash table (O(1) lookups)

✅ **Partially Working:**
- Self-optimization (meta-ops execute but need tuning)
- Prediction (works but threshold too high by default)

❌ **Removed (Never Worked):**
- Hot/cold storage
- Hierarchical module nesting
- Byte-as-code execution
- Conversation history

## Known Issues

1. **High correlation threshold**: Default threshold (50.0) is too high for small inputs. Lower to ~10.0 in line 1052 for more modules.

2. **Meta-op efficiency**: Some meta-ops scan entire graph each tick. Could optimize with indexing.

3. **Module creation threshold**: Pattern detector needs significant repetition before triggering. This is by design but may need tuning.

## License

Public domain / MIT / Your choice

## Notes

This is a **cleaned and condensed** version. Removed ~400 lines of dead code and misleading documentation.

The system demonstrates:
- Self-modifying code (graph modifies itself)
- Emergent behavior (patterns detected automatically)
- Minimal core (~1900 lines handles everything)
- No external dependencies (just libc + math)

