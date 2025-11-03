# Self-Tuning Quickstart

## What Changed?

All parameters now **adapt automatically** based on graph health metrics.

---

## 🎯 The System Maintains These Targets

```c
Target Density:    15% of max edges
Target Activity:   10% of nodes active
Target Accuracy:   85% prediction success
```

---

## 🔧 9 Self-Adjusting Parameters

| Parameter | Controls | Adapts Based On |
|-----------|----------|-----------------|
| `prune_rate` | Edge removal | Current density vs target |
| `create_rate` | Node growth | Density + prediction quality |
| `activation_scale` | Firing threshold | Current activity vs target |
| `energy_alpha` | Energy learning | Prediction accuracy |
| `energy_decay` | Energy dissipation | Prediction stability |
| `epsilon_max` | Max exploration | Prediction quality |
| `sigmoid_k` | Transition sharpness | Activity distribution |
| `layer_rate` | Hierarchy formation | Density × accuracy |

Plus: **Capacity management** (auto-reduces growth near limits)

---

## 📊 Enhanced Log Output

```bash
[TICK 1000] nodes=245 edges=1203 active=24 err=0.145 energy=0.320 ε=0.187 | 
            density=0.0201 activity=0.098 acc=0.855 | 
            prune_r=0.00052 create_r=0.0098 activ_s=63.2
```

**What to watch:**
- `density` → converges to 0.15
- `activity` → converges to 0.1
- `acc` → converges to 0.85
- `prune_r`, `create_r`, `activ_s` → oscillate around equilibrium

---

## 🧪 Try This

### Watch Adaptation in Real-Time

```bash
# Compile
gcc -O2 -Wall -Wextra -o melvin_core melvin_core.c -lm

# Run and watch parameters adapt
./melvin_core | tee adaptation.log

# Extract parameter evolution
grep TICK adaptation.log | awk '{print $3, $18, $19, $20}' > params.dat
# Plot with gnuplot:
# plot "params.dat" using 1:2 title "prune_rate", "" using 1:3 title "create_rate"
```

### Test Perturbation Response

```bash
# Start with stable state
echo "stable pattern" | yes | head -1000 | ./melvin_core

# Inject surprise
echo "NEW UNEXPECTED PATTERN!!!" | ./melvin_core

# Watch:
# - energy spike
# - epsilon increase (more exploration)
# - create_rate increase (learn new structure)
# - parameters return to equilibrium after ~500 ticks
```

---

## 🎛️ Only 4 Meta-Parameters to Tune (Optional)

These control the **speed** of adaptation:

```c
ADAPT_RATE              0.001   // How fast parameters change
TARGET_DENSITY          0.15    // Desired edge density
TARGET_ACTIVITY         0.1     // Desired node activity  
TARGET_PREDICTION_ACC   0.85    // Desired accuracy
```

### When to Adjust

**System oscillates too much?**
→ Decrease `ADAPT_RATE` to 0.0005

**System too slow to adapt?**
→ Increase `ADAPT_RATE` to 0.002

**Want denser graph?**
→ Increase `TARGET_DENSITY` to 0.2

**Want sparser graph?**
→ Decrease `TARGET_DENSITY` to 0.1

**Want more active neurons?**
→ Increase `TARGET_ACTIVITY` to 0.15

---

## 🌊 Typical Adaptation Timeline

```
Tick 0:        Random initialization
Tick 100:      Parameters adjusting rapidly
Tick 500:      Approaching targets
Tick 1000:     Near equilibrium
Tick 2000+:    Stable oscillation around targets
```

---

## 🔬 Behind the Scenes

Every 10 ticks, the system:

1. Measures current density, activity, prediction accuracy
2. Compares to targets
3. Adjusts each parameter proportionally to error
4. Clamps to safe ranges

**Example: Pruning Rate**
```c
error = current_density - 0.15
prune_rate += 0.001 * error

// Too dense (0.20)? → increase pruning
// Too sparse (0.10)? → decrease pruning
```

All parameters work together to maintain homeostasis.

---

## ✅ Benefits

- ❌ **No more manual tuning**
- ✅ **Self-optimizes for any task**
- ✅ **Adapts to changing inputs**
- ✅ **Robust to perturbations**
- ✅ **Never runs out of capacity** (auto-manages)

---

## 🎯 Key Insight

> You don't tune the parameters.  
> **The graph tunes the parameters.**

The system knows:
- When it's too dense → prunes more
- When predictions fail → explores more
- When neurons are silent → activates easier
- When near capacity → grows slower

**This is true autonomy.**

---

## 📖 Full Details

See `HOMEOSTATIC_SELF_TUNING.md` for:
- Complete adaptation rules
- Mathematical framework
- Biological analogies
- Design principles
- Future enhancements

---

## 🚀 Bottom Line

**Run it. It adapts. Done.**

No configuration files. No hyperparameter search. No manual tuning.

Just continuous, emergent self-regulation.

