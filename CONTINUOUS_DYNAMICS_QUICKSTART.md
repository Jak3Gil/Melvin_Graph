# Continuous Dynamics — Quick Reference

## 🎯 What Changed

Melvin Core is now a **continuous, physics-like system** instead of a rule-driven program.

### Before → After

```c
// ACTIVATION
n->hat = (n->soma >= n->theta) ? 1 : 0;
→ n->hat = sigmoid((n->soma - n->theta) / ACTIVATION_SCALE);

// PRUNING
if (w < THRESH) delete_edge();
→ if (randf() < PRUNE_RATE * soft_below(w, REF)) delete_edge();

// EXPLORATION
epsilon *= DECAY;
→ epsilon = EPSILON_MIN + (EPSILON_MAX - EPSILON_MIN) * sigmoid(energy);

// LEARNING
if (useful) w += STEP;
→ w += STEP * tanh(usefulness * eligibility);
```

## 🧮 Key Equations

### Energy Field
```c
energy = 0.995 * energy + 0.1 * surprise²
```

### Epsilon (Exploration)
```c
ε = 0.05 + 0.25 * sigmoid(energy - 0.5)
```

### Activation
```c
a = sigmoid((soma - theta) / 64.0)
```

### Pruning Probability
```c
p_prune = 0.0005 * P(weak) * P(unused) * P(stale)
```

### Node Creation Probability
```c
p_create = 0.01 * sigmoid(novelty * 10 - 5) * (1 + energy)
```

## 🎛️ Tunable Parameters

Located at top of `melvin_core.c`:

```c
#define SIGMOID_K           0.5f    // Steepness of transitions
#define PRUNE_RATE_BASE     0.0005f // Base pruning rate
#define NODE_CREATE_RATE    0.01f   // Base creation rate
#define LAYER_EMERGE_RATE   0.001f  // Layer formation rate
#define ENERGY_ALPHA        0.1f    // Energy learning rate
#define ENERGY_DECAY        0.995f  // Energy decay
#define EPSILON_MIN         0.05f   // Min exploration
#define EPSILON_MAX         0.3f    // Max exploration
#define ACTIVATION_SCALE    64.0f   // Activation sensitivity
```

## 📊 What to Watch

### Key Metrics (in log output)
- `energy` — Should oscillate 0.0-1.0, stabilize around 0.3-0.5
- `ε` (epsilon) — Should vary with energy: 0.05-0.3
- `surprise` — Prediction error, drives energy
- `err` — Mean absolute error per edge

### Expected Behavior
1. **High surprise** → energy rises → epsilon rises → more exploration
2. **Good predictions** → energy decays → epsilon falls → exploitation
3. **New input patterns** → surprise spikes → energy rebounds → adaptation

## 🔧 Build & Run

```bash
# Compile
gcc -O2 -Wall -Wextra -o melvin_core melvin_core.c -lm

# Run with defaults
./melvin_core

# Run with custom capacity
./melvin_core --nodes 16384 --edges 131072

# Monitor in real-time
./melvin_core 2>&1 | tee melvin.log
```

## 🧪 Testing Dynamics

### Test Energy Response
```bash
# Feed random input → watch energy spike
echo "random noise pattern" | ./melvin_core

# Feed repetitive input → watch energy decay
yes "stable pattern" | ./melvin_core
```

### Test Exploration
```bash
# High energy → high epsilon → random outputs
# Low energy → low epsilon → consistent outputs
```

## 🚀 Performance Notes

- **Probabilistic operations** are called every tick but fail most of the time
- Pruning is **continuous** but acts on ~0.05% of edges per tick
- Node creation is **rare** (~1% when conditions are right)
- Layer emergence is **very rare** (~0.1% base + energy modulation)

## 🎨 Tuning Tips

### If system is too chaotic:
- ↓ `ENERGY_ALPHA` (slower energy buildup)
- ↓ `EPSILON_MAX` (less random exploration)
- ↑ `ENERGY_DECAY` (faster energy dissipation)
- ↓ `NODE_CREATE_RATE` (slower growth)

### If system is too stable:
- ↑ `ENERGY_ALPHA` (faster energy buildup)
- ↑ `EPSILON_MAX` (more exploration)
- ↓ `ENERGY_DECAY` (longer energy persistence)
- ↑ `NODE_CREATE_RATE` (faster growth)

### If activations are too sharp:
- ↑ `ACTIVATION_SCALE` (smoother gradients)
- ↓ `SIGMOID_K` (gentler transitions)

### If activations are too diffuse:
- ↓ `ACTIVATION_SCALE` (steeper gradients)
- ↑ `SIGMOID_K` (sharper transitions)

## 📈 Monitoring Commands

```bash
# Watch energy dynamics
./melvin_core | grep TICK | awk '{print $8}' | cut -d= -f2

# Watch epsilon changes
./melvin_core | grep TICK | awk '{print $9}' | cut -d= -f2

# Watch surprise levels
./melvin_core | grep TICK | awk '{print $7}' | cut -d= -f2

# Graph in real-time (requires gnuplot)
./melvin_core | grep TICK | awk '{print $3, $8}' | gnuplot -e "plot '-' using 1:2 with lines"
```

## 🧠 Conceptual Model

Think of the system as a **physical substrate**:

- **Neurons** are oscillators with smooth activation curves
- **Synapses** are springs with continuous strength
- **Energy** is like temperature in statistical mechanics
- **Pruning** is like evaporation (weak links fade)
- **Growth** is like crystallization (strong patterns solidify)
- **Exploration** is like thermal fluctuations

The entire system minimizes **prediction error** (free energy) through gradient descent in weight space, with stochastic perturbations modulated by the energy field.

## 🎓 Advanced: Energy as Free Energy

```c
// Surprise ≈ -log P(observation | prediction)
surprise = |actual - predicted|

// Energy accumulates surprise
energy += α * surprise²

// Energy decays (entropy increase)
energy *= 0.995

// System explores more when uncertain (high energy)
exploration_rate ~ sigmoid(energy)
```

This is analogous to **active inference** / **free energy principle** in neuroscience.

---

**The system now flows like water, not like clockwork.**

