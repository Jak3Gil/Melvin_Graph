# CURRENT LIMITS TO REMOVE

## Hard Caps (Remove These)

1. **Memory Cap**: 10,000 nodes
   - Make dynamic
   - Grow as needed

2. **Token Size**: 64 bytes max
   - Make dynamic
   - Support any length

3. **Hop Limit**: 5 hops max
   - Remove limit
   - Let it propagate naturally (decay stops it)

4. **Array Sizes**: Fixed arrays
   - rule_inputs[16]
   - rule_outputs[16]
   - inhibit_outputs[8]
   - Make dynamic

## Design Choices (Keep These)

✓ **Activation Decay**: Prevents over-activation (keep!)
✓ **Competition**: Filters noise (keep!)
✓ **Output Threshold**: Ensures quality (keep!)

These aren't "limits" - they're essential design features.

## Implementation Plan

### Phase 1: Dynamic Memory
```c
// Instead of fixed:
Node nodes[10000];

// Use dynamic:
Node *nodes;
size_t node_cap;
size_t node_count;

// Grow when needed:
if (node_count >= node_cap) {
    node_cap *= 2;
    nodes = realloc(nodes, node_cap * sizeof(Node));
}
```

### Phase 2: Dynamic Tokens
```c
// Instead of:
uint8_t token[64];

// Use:
uint8_t *token;
size_t token_len;
size_t token_cap;
```

### Phase 3: Remove Hop Limit
```c
// Instead of:
for (int hop = 0; hop < 5; hop++)

// Use:
while (any_fired && activation_strength > 0.1)
// Natural termination from decay!
```

### Phase 4: Dynamic Arrays
```c
// Instead of:
uint32_t rule_inputs[16];

// Use:
uint32_t *rule_inputs;
size_t input_count;
size_t input_cap;
```

## Benefits

✅ No artificial memory limits
✅ Support huge datasets
✅ Support long tokens
✅ Unlimited complexity
✅ Natural termination (decay)

## Trade-offs

⚠️ More complex memory management
⚠️ Need proper cleanup
⚠️ Slightly slower (realloc)
⚠️ Harder to debug

But: UNLIMITED POTENTIAL!
