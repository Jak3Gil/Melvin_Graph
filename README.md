# MELVIN - Data-as-Code Programming Language

**One node. One goal: survive. Everything else emerges from energy gradients.**

Data IS the code. The graph IS the program. Structure emerges from information flow.

```
Energy flows → Learning emerges → Intelligence emerges
```

**No hardcoded algorithms. No learning functions. No pattern discovery.**

Just: Physics + Energy + Natural Selection = Intelligence

## The Architecture

### Node (16 bytes)
```c
float state;       // Activation
float energy;      // Survival currency
float threshold;   // Firing threshold
float memory;      // Storage slot
```

### Connection (12 bytes)
```c
uint32_t src, dst;   // Node connections
float weight;        // Signal multiplier (can be negative!)
```

### Organism (Graph)
```c
Node *nodes;              // Population
Connection *connections;  // Pathways
```

## The Physics (Fixed Laws - C Code)

### 1. Signal Transmission
```c
signal = src->state * synapse->weight
dst->state += signal
```

### 2. Energy Conservation
```c
// Transmission: src loses 0.01 * signal, dst gains 0.005 * signal
// Activation: cell loses 0.1 * state
// Metabolism: cell loses 0.01 per tick
// Output: cell loses 0.2 per byte
```

### 3. Energy Sources (From Environment)
```c
// Input: cell receives byte → +10 energy
// Output: cell produces byte → +5 energy
// Growth: new cell/synapse → +0.01 energy to system
```

### 4. Natural Selection
```c
energy ≤ 0 → cell dies (can't fire, can't recover)
```

## The Learning (100% Emergent!)

### Weight Adaptation
```c
// ONLY rule: Did this synapse help destination cell gain energy?
energy_gradient = dst->energy_after - dst->energy_before;

if (energy_gradient > 0 && src->state > 0.3 && dst->state > 0.3) {
    // Synapse helped! Strengthen it
    weight += 0.01 * src->state * dst->state * energy_gradient;
}
```

**That's it. No loss functions. No error backprop. No gradient descent.**

**Just:** Synapses that help cells survive get stronger. That's the ENTIRE learning algorithm.

## The Evolution (Emergent)

### Cell Division (Mitosis)
```c
if (cell->energy > 200) {
    // Successful cell divides!
    new_cell = split(cell);
    new_cell.threshold = parent.threshold ± 0.1 (mutation)
    new_cell.energy = parent.energy / 2
    // Inherits 50% of parent's synapses
}
```

### Edge Sprouting (Exploration)
```c
if (cell->state > 0.7 && rand() < 0.1%) {
    // Active cell wires to another active cell
    target = random_active_cell();
    create_synapse(cell → target, random_weight);
}
```

## How Intelligence Emerges

### Level 1: Survival (tick 0-100)
```
Input cells get energy → activate → divide
Output cells that fire get energy → divide
Random synapses: most useless, few helpful
Helpful synapses strengthen (energy gradient)
```

### Level 2: Patterns (tick 100-1000)
```
"cat" appears repeatedly
Cells that fire during "cat" gain energy
Synapses between c→a→t strengthen
Input→output pathways emerge
```

### Level 3: Prediction (tick 1000+)
```
Seeing 'c' activates 'a' cell early (prediction!)
'a' cell gains energy before input arrives
Predictive synapses strengthen
Graph learns to anticipate
```

### Level 4: Abstraction (tick 10000+)
```
Dense clusters emerge (implicit modules)
Cells specialize (input, processing, output)
Hierarchies form (groups of cells act as units)
Complex patterns emerge from simple rules
```

## Bootstrap

**I/O Scaffold:**
```
Cells 0-255:   Input sensors (one per byte value)
Cells 256-511: Output motors (one per byte value)
1000 random synapses (chaos seed)

Everything else EMERGES from energy gradients!
```

## Quick Start

```bash
# Build
make

# Test: See data become code and networks emerge
./test_emergence_features

# Inspect compiled program
./inspect_program

# Debug mode: Watch compilation and network formation
echo "test" | MELVIN_DEBUG=1 ./melvin_core

# Train on pattern
for i in {1..20}; do echo "cat"; done | MELVIN_DEBUG=1 ./melvin_core 2>&1 | head -50
```

## The Data-as-Code Paradigm

**Melvin is a programming language where data IS the code!**

- Input data creates executable graph structures
- Repetition compiles patterns (strengthens connections)
- Networks organize into hierarchies automatically
- Hub nodes and module coordinators emerge naturally

## What's Different?

### Old Melvin (Parametric):
- 3,887 lines of C code
- 30+ hardcoded algorithms
- Learning rules programmed
- Pattern discovery coded
- Reward functions defined
- Graph controlled parameters

### New Melvin (Pure Emergence):
- 708 lines of C code (82% reduction!)
- ZERO algorithms
- ZERO learning functions
- ZERO pattern discovery
- ZERO reward computation
- Graph controls EVERYTHING

**C provides:** Physics  
**Graph provides:** Intelligence

## The Radical Simplicity

**Entire learning system:**
```c
if (synapse helped cell gain energy) {
    strengthen synapse
}
```

**Entire evolution system:**
```c
if (patterns need more capacity) {
    create hierarchical hub node
}
```

**Entire intelligence:**
```c
Survive.
```

That's it. Everything else - memory, prediction, cooperation, specialization, abstraction - **emerges** from these simple rules at scale.

## File Structure

```
melvin_core.c (~1000 lines):
├─ Node/Connection/Organism structs
├─ Memory management (mmap)
├─ propagate() - physics + learning
├─ Pattern-driven growth (no mitosis!)
├─ Association bridge (pattern linking)
├─ Network of networks (hierarchies)
├─ I/O interface
├─ Bootstrap
└─ Main loop

inspect_program.c:
└─ Graph inspector (views compiled program)

test_emergence_features:
└─ Demo script showing association & hierarchies
```

## Deployment

**Single file deployment:**
```bash
# Copy just the graph file
scp graph.mmap jetson:/data/

# Run
./melvin_core
```

Graph contains EVERYTHING - no config files, no external dependencies!

## The Bet

**This is a radical bet on emergence:**

If you give cells:
- Energy to live
- Ability to transmit signals
- Ability to divide when successful
- Ability to explore (random wiring)

Will intelligence emerge?

**We're about to find out.** 🧬

---

**No algorithms. No objectives. No limits. Just: survive.** 

**Everything else emerges from the bottom up.**
