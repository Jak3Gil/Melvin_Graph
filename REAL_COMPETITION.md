# THE PROBLEM: Fake Competition

## Current (Broken) Behavior

```c
for (uint32_t i = 0; i < g.node_count; i++) {
    Node *rule = &g.nodes[i];
    if (rule->type != NODE_RULE) continue;
    
    // Calculate strength
    rule->rule_strength = times_executed / (times_executed + 10);
    
    // But then just process in order!
    if (all_active) {
        if (rule_strength < 0.05) continue;  // Weak filter
        
        // First to process wins!
        g.nodes[out_id].state = output_strength;
    }
}
```

**Issues:**
1. Rules processed sequentially (order-dependent)
2. First rule locks the output
3. Later rules skip if output already active
4. Strength is calculated but doesn't determine winner
5. Winner = first in array, not strongest

## Real Competition Should Be

### Option 1: Sort by Strength
```c
// Collect all candidate rules
RuleCandidate candidates[1000];
int candidate_count = 0;

for (rules) {
    if (can_fire) {
        candidates[candidate_count++] = {rule_id, strength};
    }
}

// Sort by strength (strongest first)
qsort(candidates, candidate_count, ...);

// Fire in order of strength
for (each candidate) {
    fire_rule(candidate);
}
```

### Option 2: Accumulate + Winner Takes All
```c
// All rules vote with their strength
float votes[MAX_NODES] = {0};

for (rules) {
    if (can_fire) {
        for (outputs) {
            votes[output] += rule_strength * activation;
        }
    }
}

// Strongest vote wins each output
for (nodes) {
    if (votes[i] > threshold) {
        g.nodes[i].state = votes[i];
    }
}
```

### Option 3: Weighted Sum (Most Natural)
```c
// Let multiple rules contribute, weighted by strength
for (rules) {
    if (can_fire) {
        float contribution = rule_strength * input_activation;
        
        for (outputs) {
            // Add contribution (don't lock!)
            g.nodes[output].state += contribution;
        }
    }
}

// Normalize if needed
for (nodes) {
    if (g.nodes[i].state > 1.0) {
        g.nodes[i].state = 1.0;
    }
}
```

## Which to Use?

**Option 3 (Weighted Sum) is best because:**
- Natural: Multiple rules can contribute
- Strength matters: Stronger rules contribute more
- No order dependence: All rules processed
- Emergent: Combined effect of multiple patterns
- No locking: Flexible, dynamic

**Implementation:**
- Remove "if already active, skip" logic
- Change from assignment (=) to accumulation (+=)
- Weight by rule strength
- Let strongest patterns naturally dominate

## Example

Training:
- 'cat animal' (100x) → strength=0.91
- 'cat pet' (100x) → strength=0.91  
- 'cat noise' (1x) → strength=0.09

Input: 'cat'

**Current (Broken):**
- Whichever rule is processed first wins
- Others blocked
- Random based on array order

**Fixed (Weighted):**
- animal: 0.91 activation
- pet: 0.91 activation
- noise: 0.09 activation
- Output: animal, pet (both strong!)
- noise suppressed (too weak)

**Real competition emerges naturally!**
