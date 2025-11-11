# EMERGENT REASONING - Minimal Rules

## Philosophy
Don't hardcode reasoning. Add simple rules that make reasoning EMERGE.

## Proposed Rules

### Rule 1: COMPETITION (Context Selection)
**What**: When multiple rules could fire, the STRONGEST wins.
**Why**: Enables context-aware selection.
**How**: 
```c
rule_strength = times_executed / (times_executed + 10)
Only fire rule if strength > competing_rules
```

**Enables**:
- 'at home' trained 100x → wins over 'at work' trained 1x
- Frequent patterns dominate rare ones
- Context emerges from usage

### Rule 2: INHIBITION (Negation)
**What**: Rules can SUPPRESS other nodes.
**Why**: Enables "NOT" and mutual exclusion.
**How**:
```c
Add: inhibit_outputs[] (nodes to suppress)
If rule fires: target nodes get NEGATIVE activation
```

**Enables**:
- 'car is NOT animal' → suppresses 'animal' node
- Mutually exclusive categories
- Logical negation

### Rule 3: OPERATORS (Computation)
**What**: Special nodes that EXECUTE functions.
**Why**: Enables arithmetic and transformations.
**How**:
```c
NODE_OPERATOR: executes function(inputs) → outputs
Operators: ADD, SUB, MULT, DIV, etc.
```

**Enables**:
- '7 + 7' → ADD(7,7) → '14'
- Real computation, not just pattern matching
- Math reasoning

### Rule 4: CONFIDENCE (Uncertainty)
**What**: Rules have confidence scores.
**Why**: Enables reasoning under uncertainty.
**How**:
```c
confidence = successful_predictions / total_activations
Only propagate if confidence > threshold
```

**Enables**:
- Strong patterns propagate farther
- Weak patterns die out quickly
- Natural pruning

### Rule 5: TEMPORAL MARKERS
**What**: Nodes track when they were created/activated.
**Why**: Enables "before" vs "after".
**How**:
```c
Add: timestamp or sequence_index
Rules only fire if temporal constraints met
```

**Enables**:
- 'eat breakfast' before 'go to work'
- Temporal ordering
- Event sequences

## Implementation Priority

### Phase 1: COMPETITION + INHIBITION
These two enable the most reasoning with minimal changes:
1. Rule strength → context selection
2. Negative activation → negation

### Phase 2: OPERATORS
Enable actual computation:
- ADD, SUB nodes that execute
- Transforms pattern matching into calculation

### Phase 3: CONFIDENCE + TEMPORAL
Fine-tune reasoning:
- Uncertainty handling
- Time-aware patterns

## How They Enable Reasoning

### Example 1: Context Selection (Competition)
```
Training:
  'at home sleep' (100x)
  'at work code' (100x)
  'at home' (1x random noise)

Input: 'at'

Without competition:
  → home sleep work code (everything!)

With competition:
  home_rule.strength = 100/110 = 0.91
  work_rule.strength = 100/110 = 0.91
  noise_rule.strength = 1/11 = 0.09
  → home sleep (stronger rule wins!)
```

### Example 2: Negation (Inhibition)
```
Training:
  'car is NOT animal' 
  Creates: RULE(car → vehicle, INHIBIT: animal)

Input: 'car is'

Without inhibition:
  → vehicle animal (both activate)

With inhibition:
  vehicle activates
  animal gets -0.5 activation (suppressed)
  → vehicle (only!)
```

### Example 3: Arithmetic (Operators)
```
Training:
  '2 + 3 = 5'
  Creates: DATA(2), OPERATOR(ADD), DATA(3), DATA(5)

Input: '7 + 7 ='

Without operators:
  → pattern match → wrong answer

With operators:
  Recognizes '+' as ADD operator
  ADD.execute(7, 7) → 14
  → 14 (computed!)
```

## Key Insights

### 1. Emergence > Hardcoding
Don't implement full reasoning engine.
Add simple rules, let complexity emerge.

### 2. Competition Creates Intelligence
When rules compete, the system learns what works.
Strength = intelligence signal.

### 3. Inhibition Enables Logic
Excitation alone can't create "NOT".
Need suppression for logical reasoning.

### 4. Operators Bridge Patterns to Computation
Pattern engine finds relationships.
Operators execute transformations.
Together = symbolic reasoning!

### 5. Keep It Minimal
Each rule should be 5-10 lines of code.
Complexity should emerge, not be coded.

## Implementation Plan

Start with Competition + Inhibition (most impact, least code):

```c
// Add to Node struct:
float rule_strength;        // Competition
uint32_t inhibit_outputs[]; // Inhibition
uint8_t inhibit_count;

// In execute_rules():
1. Calculate rule_strength for all rules
2. Sort by strength
3. Fire strongest rules first
4. Apply inhibition (negative activation)
5. Weaker rules blocked by stronger ones
```

This enables:
✅ Context selection
✅ Negation
✅ Mutual exclusion
✅ Logical reasoning

With ~50 lines of code!
