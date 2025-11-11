<!-- 0cb4e68b-3996-4189-88a4-89bf4a2cbe82 e8398ab7-3f46-4356-b51b-7c3876510aba -->
# Pure Emergent Architecture Rebuild

## Philosophy

Transform Melvin from "pattern matcher with hardcoded operations" to "pure emergent learning system where operations are discovered, not programmed."

## Core Principles

1. **No hardcoded operations** - OP_SUM, OP_PRODUCT deleted from C
2. **No C learning logic** - Weight updates via pure Hebbian rule only
3. **No supervised cheating** - No direct answer activation
4. **Proper wiring** - Operations can actually execute
5. **Emergence** - Addition/multiplication learned from examples

---

## Part 1: DELETE THE LIES (Remove ~1500 lines)

### 1.1 Delete Hardcoded Operation Switch (Lines 804-975)

**Current:** Explicit C code for OP_SUM, OP_PRODUCT, OP_MAX, etc.

**Replace with:** Pure universal neuron `return sigmoid(soma - theta);`

**Impact:** Operations no longer "work" - must be learned!

### 1.2 Delete C Learning Code (Lines 2660-2919)

**Current:** 250 lines computing utilities, credit assignment, weight updates

**Replace with:** Pure Hebbian: `delta_w = eta * a_pre * a_post`

**Impact:** No more global credit assignment - only local correlation!

### 1.3 Delete Supervised Training Parser (Lines 4499-4542)

**Current:** Parses "1+1=2", activates answer directly (cheating!)

**Delete entirely:** No more "expected output"

**Impact:** System must learn from pure observation!

### 1.4 Delete Decorative Bootstrap Circuits (Lines 5594-5742)

**Current:** Learning circuits that are never executed

**Delete entirely:** Utility calculator, weight updater, credit assignment circuits

**Impact:** Simpler bootstrap, no fake "graph learning"

### 1.5 Delete Generalization Circuits (Lines 6017-6280)

**Current:** 70 nodes for pattern detection/slot abstraction that don't work

**Delete entirely:** Pattern detector, slot abstractor, rule extractor, pattern matcher

**Impact:** 70 fewer decorative nodes

### 1.6 Remove Supervised Hooks from Main Loop (Lines 4569-4570, 4607-4614)

**Current:** Calls `parse_supervised_training()` and `validate_output()`

**Delete calls:** Remove from main_loop()

---

## Part 2: FIX THE FOUNDATION (Core execution)

### 2.1 Implement Pure Universal Neuron (Lines 597-652)

**Replace entire execute_node_operation() with:**

```c
static inline float execute_node_operation(Node *n) {
    float soma = node_soma(n);
    float theta = n->data;
    
    uint8_t op = node_op_type(n);
    
    // Only 2 exceptions: OP_SPLICE and OP_FORK (structural meta-ops)
    if (op == OP_SPLICE) {
        // Hebbian edge creation (keep as primitive)
        // ... (simplified OP_SPLICE code)
        return 1.0f;
    }
    
    if (op == OP_FORK) {
        // Node creation (keep as primitive)
        // ... (simplified OP_FORK code)
        return 1.0f;
    }
    
    // EVERYTHING ELSE: Pure universal neuron!
    return sigmoid(soma - theta);
}
```

**Impact:** OP_SUM, OP_PRODUCT, OP_COMPARE now all behave identically! Must learn differences via wiring.

### 2.2 Implement Pure Hebbian Learning (Replace observe_and_update)

**Replace 250 lines with ~30:**

```c
void observe_and_update() {
    // Pure Hebbian: Δw = η * a_pre * a_post
    float eta = 0.01f;  // Small learning rate
    
    for (uint32_t i = 0; i < g_graph.edge_count; i++) {
        Edge *e = &g_graph.edges[i];
        if (e->src >= g_graph.node_count || e->dst >= g_graph.node_count) continue;
        
        Node *src = &g_graph.nodes[e->src];
        Node *dst = &g_graph.nodes[e->dst];
        
        // Co-activation strengthens, anti-correlation weakens
        float delta = eta * node_a_prev(src) * dst->a;
        
        float new_w = (float)e->w_fast + delta;
        if (new_w < 1.0f) new_w = 1.0f;
        if (new_w > 255.0f) new_w = 255.0f;
        e->w_fast = (uint8_t)new_w;
    }
}
```

**Impact:** No more utility calculation, no credit assignment - just pure correlation!

### 2.3 Simplify OP_SPLICE to Block Shortcuts (Lines 834-937)

**Current:** Complex hierarchical rules that still allow shortcuts

**Replace with:**

```c
case OP_SPLICE:
    if (node_soma(n) > 0.5f && g_sys.tick % 10 == 0) {
        // ONLY create edges between layers in correct order
        // Semantic → Operation (ONLY!)
        // Operation → Output (ONLY!)
        // NEVER Semantic → Output
        
        for (semantic in active_semantics) {
            for (operation in active_operations) {
                if (!edge_exists(semantic, operation)) {
                    edge_create(semantic, operation);
                }
            }
        }
        
        for (operation in active_operations) {
            for (output in active_outputs) {
                if (!edge_exists(operation, output)) {
                    edge_create(operation, output);
                }
            }
        }
        
        // ABSOLUTELY NO semantic→output creation!
    }
    break;
```

**Impact:** Forces proper hierarchical flow!

---

## Part 3: FIX THE BOOTSTRAP WIRING

### 3.1 Simplify Bootstrap - Remove Fake Circuits (Lines 5335-6400)

**Delete:**

- Learning circuits (utility, weight updater, credit) - Lines 5594-5673
- Convergence circuits - Lines 5675-5697
- Meta-op circuits - Lines 5700-5741
- Generalization circuits - Lines 6010-6280

**Keep:**

- Parameter nodes (28 nodes)
- Goal nodes (4 nodes)
- Semantic numbers (10 nodes)
- Semantic variables (26 nodes)
- Operation subgraphs (ADD, SUBTRACT, MULTIPLY)
- Result outputs (19 nodes)

**Impact:** Bootstrap drops from 200+ nodes to ~90 nodes. Honest and functional.

### 3.2 Fix Operation Subgraph Wiring (Lines 5770-5861)

**Current problem:** ADD circuit uses hardcoded OP_SUM which we're deleting!

**Fix:** Replace with universal neurons wired to compute sum:

```c
// ADD subgraph using universal neurons:
uint32_t add_input1 = CREATE_NODE(OP_MEMORY, 0.0f, 0.0f);  // Stores first number
uint32_t add_input2 = CREATE_NODE(OP_MEMORY, 0.0f, 0.0f);  // Stores second number
uint32_t add_accumulator = CREATE_NODE(OP_MEMORY, 0.0f, 0.0f);  // Universal neuron, theta=0
node_theta(&g_graph.nodes[add_accumulator]) = 0.0f;  // Low threshold = sums inputs
uint32_t add_output = CREATE_NODE(OP_MEMORY, 0.0f, 0.0f);

// Wire: both inputs → accumulator → output
edge_create(&g_graph, add_input1, add_accumulator);
edge_create(&g_graph, add_input2, add_accumulator);
edge_create(&g_graph, add_accumulator, add_output);

// Set equal weights so accumulator sees sum
g_graph.edges[edge_count-2].w_fast = 128;  // Weight = 1.0 in normalized space
g_graph.edges[edge_count-1].w_fast = 128;
```

**Impact:** ADD now uses universal neurons that ACTUALLY sum via proper wiring!

### 3.3 Fix Result Wiring - Selective Activation (Lines 6324-6331)

**Current problem:** add_output → ALL 19 results (can't be selective)

**Fix:** Make add_output a VALUE ENCODER:

```c
// Instead of connecting to all results, wire add_output to a DECODER circuit
uint32_t result_decoder = CREATE_NODE(OP_MEMORY, 0.0f, 0.0f);
edge_create(&g_graph, add_output, result_decoder);

// Decoder has weighted edges to each result
// Weight represents the value that activates that result
for (int i = 0; i < 19; i++) {
    uint32_t edge_idx = edge_create(&g_graph, result_decoder, g_semantic_result_outputs[i]);
    // Edge weight encodes: "activate result[i] when decoder sees value i"
    g_graph.edges[edge_idx].w_fast = (i == 0) ? 255 : 0;  // Initially only result[0]
    // Graph will learn to adjust these through Hebbian learning!
}
```

**Impact:** Selective result activation becomes learnable!

### 3.4 Remove Semantic→Output Prewiring (Line 6318-6326)

**Current:** Pattern matcher directly wired to outputs (shortcut!)

**Delete:** Those edges entirely

**Impact:** Forces learning through operations!

---

## Part 4: SIMPLIFY MAIN LOOP

### 4.1 Remove Supervised Phases (Lines 4564-4577)

**Delete calls:**

- `parse_supervised_training()` - Line 4570
- `parse_semantic_operators()` - Line 4573 (variable assignment hack)
- `execute_expression()` - Line 4576 (composition hack)
- `validate_output()` - Lines 4607-4610

**Keep only:**

- `parse_semantic_input()` - Activates semantic nodes
- `activate_input_bytes()` - Activates byte nodes

**Impact:** Clean input→activation path, no cheating!

### 4.2 Remove Decorative Circuit Activations (Lines 4640-4655)

**Delete:**

- `detect_activation_pattern()` - Stub
- `fitness_based_circuit_selection()` - Stub  
- `activate_generalization_circuits()` - Activates ghosts

**Impact:** Faster, no fake complexity!

---

## Part 5: THE CRITICAL FIX - How Does Graph Learn Operations?

### 5.1 Example Training Sequence

**Without supervised cheating, how does "1+1=2" teach the graph?**

```
Input: "1+1" (no answer provided!)

Tick 1-5: Activate semantic nodes
  - number[1].a = 1.0 (both instances)
  - g_semantic_op_add.a = 1.0

Tick 6-10: OP_SPLICE creates edges
  - number[1] → add_input1 (Hebbian: both active)
  - number[1] → add_input2 (Hebbian: both active)
  - Propagate through ADD subgraph
  - add_accumulator.soma = 1.0 + 1.0 = 2.0 (from weighted inputs!)
  - add_output.a = sigmoid(2.0 - 0.0) = 0.88

Tick 11-15: add_output propagates to result_decoder
  - result_decoder.a = 0.88
  - Propagates to ALL results equally (initially)
  - Random winner fires

Output: "3" (wrong, but it tried!)

User sees "3" → knows it's wrong → provides correct example
  
Next input: "1+1" again (repeat)

Tick 1-5: Same activation
Tick 11-15: Output "4" (different random)

After 20 trials:
  - Sometimes outputs "2" by chance
  - User KEEPS FEEDING "1+1" when they see "2"
  - Hebbian rule: number[1] + result[2] co-occur more than others
  - result[2] edge strengthens
  - Eventually: "1+1" reliably → "2"
```

**This requires IMPLICIT feedback via repetition, not explicit labels!**

### 5.2 Add Result Feedback Mechanism

**Problem:** How does graph know when it got something right?

**Solution:** Add self-observation of output:

```c
// In main_loop after emit_action():
// Re-feed output back as input (self-observation)
// If graph outputs "2" after seeing "1+1", it sees "1+1...2"
// Co-activation strengthens: number[1] + add + result[2] pathway
// Wrong outputs don't get reinforced (user doesn't repeat them)
```

**Implementation:** Already exists in `merge_output_into_input()` - just needs to work without supervised validation!

---

## Expected Behavior After Fix

### Initial State (First "1+1" input):

```
Output: Random (0-18), probably wrong
Reason: result_decoder edges are random
```

### After 5-10 trials of "1+1":

```
Output: Starts converging toward one answer
Reason: Hebbian strengthens frequently co-activated paths
```

### After 20-50 trials:

```
Output: Consistently "2"
Reason: number[1] → add → result[2] path is strongest
```

### After training "1+1", then "2+2":

```
Input "2+2":
  - number[2] activates
  - Routes through add (pre-wired)
  - add_output sees ~4.0
  - result_decoder → results
  - Initially random, but after repetition: "4"
  
NO INTERFERENCE because:
  - No shortcuts (blocked by OP_SPLICE)
  - Operations properly execute
  - Each number → operation → result path is distinct
```

### After training many facts:

```
1+1 → 2 ✓
2+2 → 4 ✓
3+3 → 6 ✓
10+10 → ? (needs decoder to learn value encoding)
```

---

## Implementation Order

1. **Delete** - Remove all fake code (~1500 lines)
2. **Simplify** - Pure universal neuron + Hebbian learning (~100 lines)
3. **Fix wiring** - Proper operation subgraphs in bootstrap (~200 lines)
4. **Test** - Verify emergent learning works

---

## Files Modified

- `melvin_core.c` - Massive simplification (~6448 → ~3500 lines)

## Risk Assessment

**High Risk:** This breaks everything initially - no operations work, no learning works

**Requires:** Patient training via repetition (20+ examples per fact)

**Payoff:** True emergent intelligence where graph discovers computation

---

## Success Criteria

Test after implementation:

```bash
# Train by repetition (no labels!)
for i in {1..30}; do echo "1+1" | ./melvin_core; done

# Test
echo "1+1" | ./melvin_core
# Expected: Eventually converges to "2"

# Train second fact
for i in {1..30}; do echo "2+2" | ./melvin_core; done

# Test both
echo "1+1" | ./melvin_core  # Should still be "2"
echo "2+2" | ./melvin_core  # Should be "4"
```

If both work independently without interference: **SUCCESS!**

### To-dos

- [ ] Delete hardcoded operation switch statement (lines 804-975), replace with pure universal neuron
- [ ] Delete C learning code in observe_and_update (lines 2660-2919), replace with pure Hebbian rule
- [ ] Delete supervised training parser and validation (lines 4499-4542, 4569-4570, 4607-4614)
- [ ] Delete decorative learning circuits from bootstrap (lines 5594-5742)
- [ ] Delete decorative generalization circuits from bootstrap (lines 6017-6280)
- [ ] Implement pure universal neuron execution with only OP_SPLICE/OP_FORK exceptions
- [ ] Implement pure Hebbian learning: delta_w = eta * a_pre * a_post
- [ ] Rewrite OP_SPLICE to ABSOLUTELY BLOCK semantic→output shortcuts
- [ ] Fix ADD subgraph to use universal neurons with proper weights for summation
- [ ] Create value decoder circuit that selectively activates correct result based on operation output
- [ ] Test learning single fact via repetition (1+1 repeated 30x)
- [ ] Test learning multiple facts without interference (1+1, 2+2, 3+3)