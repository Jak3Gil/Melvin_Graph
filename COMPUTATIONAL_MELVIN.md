# Computational Melvin - Implementation Guide

## Overview

Transform Melvin from associative memory → **self-programming computational system**

**Core Idea**: The computer can already calculate. Melvin just needs to **arrange activations** to trigger the right computations.

## Phase 1: Add Computational Node Types

### Extend Node Structure

```c
typedef struct __attribute__((packed)) {
    uint8_t token[16];
    float activation;
    uint16_t token_len;
    uint16_t frequency;
    uint8_t node_type;    // NEW: Type of node
    uint8_t reserved;     // Padding to maintain alignment
} Node;  // Still 26 bytes (acceptable growth)

// Node types
#define NODE_DATA      0  // Regular data/word
#define NODE_OPERATOR  1  // +, -, *, /, =, if, then
#define NODE_NUMBER    2  // Numeric value (parse as integer)
#define NODE_VARIABLE  3  // X, Y, Z (pattern variables)
#define NODE_PATTERN   4  // Template like "X + Y"
```

### Parse Node Types on Input

```c
uint8_t infer_node_type(uint8_t *token, uint32_t len) {
    // Check if number
    int is_number = 1;
    for (uint32_t i = 0; i < len && i < 16; i++) {
        if (token[i] < '0' || token[i] > '9') {
            is_number = 0;
            break;
        }
    }
    if (is_number) return NODE_NUMBER;
    
    // Check if operator
    if (len == 1) {
        if (token[0] == '+' || token[0] == '-' || token[0] == '*' || 
            token[0] == '/' || token[0] == '=' || token[0] == '<' ||
            token[0] == '>') {
            return NODE_OPERATOR;
        }
    }
    
    // Check if variable (single uppercase letter)
    if (len == 1 && token[0] >= 'A' && token[0] <= 'Z') {
        return NODE_VARIABLE;
    }
    
    // Check if keyword operator
    if (len == 2 && memcmp(token, "if", 2) == 0) return NODE_OPERATOR;
    if (len == 4 && memcmp(token, "then", 4) == 0) return NODE_OPERATOR;
    if (len == 4 && memcmp(token, "else", 4) == 0) return NODE_OPERATOR;
    
    return NODE_DATA;  // Default
}
```

## Phase 2: Pattern Recognition

### Detect Computational Patterns

```c
typedef struct {
    uint32_t node_ids[10];  // Max 10 nodes in pattern
    uint8_t count;
    uint8_t pattern_type;
} ActivationPattern;

#define PATTERN_NONE       0
#define PATTERN_BINARY_OP  1  // X OP Y
#define PATTERN_EQUATION   2  // X = Y
#define PATTERN_IF_THEN    3  // if X then Y else Z

ActivationPattern detect_pattern(uint32_t *active_nodes, uint32_t count) {
    ActivationPattern p = {0};
    
    // Look for "X OP Y = Z" pattern
    // Example: "2 + 3 = 5"
    int has_operator = 0;
    int has_equals = 0;
    uint32_t op_idx = UINT32_MAX;
    uint32_t eq_idx = UINT32_MAX;
    
    for (uint32_t i = 0; i < count && i < 10; i++) {
        Node *n = &g.nodes[active_nodes[i]];
        
        if (n->node_type == NODE_OPERATOR) {
            if (n->token_len == 1 && n->token[0] == '=') {
                has_equals = 1;
                eq_idx = i;
            } else {
                has_operator = 1;
                op_idx = i;
            }
        }
        
        p.node_ids[p.count++] = active_nodes[i];
    }
    
    // Pattern: NUM OP NUM = NUM
    if (has_operator && has_equals && count >= 4) {
        p.pattern_type = PATTERN_EQUATION;
        return p;
    }
    
    // Pattern: NUM OP NUM (query, no result yet)
    if (has_operator && !has_equals && count >= 3) {
        p.pattern_type = PATTERN_BINARY_OP;
        return p;
    }
    
    return p;
}
```

## Phase 3: Execution Engine

### Execute Detected Patterns

```c
void execute_computational_patterns() {
    // Collect highly activated nodes
    uint32_t active_nodes[1000];
    uint32_t active_count = 0;
    
    for (uint32_t i = 0; i < g.node_count && active_count < 1000; i++) {
        if (g.nodes[i].activation > 0.99f) {  // Direct input only
            active_nodes[active_count++] = i;
        }
    }
    
    // Detect pattern
    ActivationPattern pattern = detect_pattern(active_nodes, active_count);
    
    if (pattern.pattern_type == PATTERN_EQUATION) {
        learn_equation(&pattern);
    } else if (pattern.pattern_type == PATTERN_BINARY_OP) {
        compute_query(&pattern);
    }
}

void learn_equation(ActivationPattern *p) {
    // Parse "X OP Y = Z" and create strong edge
    // Example: "2 + 3 = 5"
    //   → Create edge: (2,+,3) → 5 with weight 255
    
    // For simplicity, create a compound pattern node
    // Pattern node token: "2+3" → "5"
    
    uint32_t lhs_node = create_compound_pattern(p->node_ids, p->count);
    
    // Find the result (after =)
    uint32_t result_node = UINT32_MAX;
    int found_equals = 0;
    for (uint32_t i = 0; i < p->count; i++) {
        Node *n = &g.nodes[p->node_ids[i]];
        if (found_equals && n->node_type == NODE_NUMBER) {
            result_node = p->node_ids[i];
            break;
        }
        if (n->token_len == 1 && n->token[0] == '=') {
            found_equals = 1;
        }
    }
    
    if (lhs_node != UINT32_MAX && result_node != UINT32_MAX) {
        create_edge(lhs_node, result_node, 255);  // Strong computational edge!
        
        if (debug) {
            fprintf(stderr, "[LEARN] Pattern → Result edge created\n");
        }
    }
}

void compute_query(ActivationPattern *p) {
    // User asked "2 + 3 = ?"
    // Search for matching learned pattern
    
    uint32_t query_pattern = create_compound_pattern(p->node_ids, p->count);
    
    // Use spreading activation to find answer!
    // If we learned "2 + 3 = 5", the edge will fire
    // and activate the "5" node
    
    spreading_activation();  // Let the network compute!
}

uint32_t create_compound_pattern(uint32_t *nodes, uint32_t count) {
    // Combine tokens into pattern signature
    // "2" + "+" + "3" → "2+3"
    uint8_t pattern_token[16] = {0};
    uint32_t pattern_len = 0;
    
    for (uint32_t i = 0; i < count && pattern_len < 16; i++) {
        Node *n = &g.nodes[nodes[i]];
        
        // Skip "=" and "?" 
        if (n->token_len == 1 && (n->token[0] == '=' || n->token[0] == '?')) {
            break;
        }
        
        // Concatenate tokens
        for (uint32_t j = 0; j < n->token_len && pattern_len < 16; j++) {
            pattern_token[pattern_len++] = n->token[j];
        }
    }
    
    return create_node(pattern_token, pattern_len);
}
```

## Phase 4: Meta-Operations (Self-Modification)

### Allow Melvin to Create Edges

```c
void execute_meta_operation(ActivationPattern *p) {
    // Detect patterns like:
    // "connect cat sat"
    // "link X Y strength 200"
    
    // For now, simple: if we see pattern frequently, strengthen it
    // This happens automatically through existing edge strengthening!
    
    // Future: Allow explicit meta-commands
    // "create_edge cat sat 255" → Melvin modifies himself
}
```

## Phase 5: Update Main Loop

### Integrate Computational Execution

```c
int main() {
    // ... existing setup ...
    
    while (idle < 100) {
        ssize_t n = read(STDIN_FILENO, input, sizeof(input));
        
        if (n > 0) {
            sense_input(input, n);        // Parse input + create nodes
            
            // NEW: Check if input is computational pattern
            execute_computational_patterns();
            
            spreading_activation();       // CASCADE through network
            emit_output();                // Show activated nodes
            g.tick++;
            idle = 0;
        } else {
            idle++;
            usleep(10000);
        }
    }
    
    // ... existing cleanup ...
}
```

## Testing the Computational System

### Test 1: Basic Arithmetic

```bash
# Teach addition
echo "1 + 1 = 2" | ./melvin
echo "1 + 2 = 3" | ./melvin  
echo "2 + 2 = 4" | ./melvin
echo "2 + 3 = 5" | ./melvin

# Query (the magic moment!)
echo "1 + 1 = ?" | ./melvin
# Expected output: 2 (via spreading activation!)

echo "2 + 3 = ?" | ./melvin  
# Expected output: 5
```

### Test 2: Generalization Through Similarity

```bash
# Teach some sums
echo "5 + 5 = 10" | ./melvin
echo "5 + 6 = 11" | ./melvin
echo "6 + 6 = 12" | ./melvin

# Query similar pattern
echo "5 + 6 = ?" | ./melvin
# Should activate "11" strongly (exact match)

echo "5 + 7 = ?" | ./melvin
# Should activate nodes near 12 (similar to 5+6=11 and 6+6=12)
```

### Test 3: Pattern Composition

```bash
# Teach identity
echo "5 + 0 = 5" | ./melvin
echo "7 + 0 = 7" | ./melvin
echo "23 + 0 = 23" | ./melvin

# Query new number
echo "999 + 0 = ?" | ./melvin
# Should generalize: activate 999 (X + 0 = X pattern!)
```

## Advanced: Variable Patterns

### Future Enhancement: True Pattern Matching

```c
// Instead of exact token matching, match STRUCTURE
// "2 + 3" matches pattern "X + Y"
// Where X=2, Y=3

typedef struct {
    uint32_t template_nodes[10];  // Pattern with variables
    uint32_t binding[26];         // X→node_id, Y→node_id, etc.
} PatternMatch;

PatternMatch match_pattern_template(uint32_t *input_nodes, uint32_t input_count) {
    PatternMatch match = {0};
    
    // Search for pattern templates in graph
    // Example template: "X + Y = Z" (stored as edges)
    // Try to bind input to template variables
    
    // If successful, use edge to find Z!
    
    return match;
}
```

## Key Benefits of This Approach

### 1. **Incremental Learning**
Every equation creates edges. No retraining needed.

### 2. **Generalization Through Similarity**
Similar patterns activate similar results:
- "2 + 3 = 5" helps with "2 + 4 = ?"
- Character similarity spreads activation

### 3. **Composition**
Complex computation emerges from simple rules:
- "5 + 5 = 10" + "10 + 5 = 15" → "5 + 5 + 5 = 15"

### 4. **Explainable**
Every computation is traceable:
- Which edges fired?
- What activations led to result?
- Why this answer?

### 5. **Self-Improving**
The more examples, the stronger the patterns:
- Edges strengthen with use
- Coherence boosts frequently co-occurring patterns

## Minimal Implementation Plan

### Step 1: Add Node Types (30 minutes)
- Add `node_type` field (26 bytes total)
- Implement `infer_node_type()`
- Update `create_node()` to set type

### Step 2: Pattern Detection (1 hour)
- Implement `detect_pattern()`
- Detect "X OP Y = Z" equations
- Detect "X OP Y" queries

### Step 3: Execution (2 hours)
- Implement `learn_equation()` 
- Implement `compute_query()`
- Create compound pattern nodes
- Create computational edges

### Step 4: Integration (30 minutes)
- Call `execute_computational_patterns()` in main loop
- Test with basic arithmetic

### Step 5: Testing & Refinement (ongoing)
- Test generalization
- Tune activation parameters
- Add more operators

## The Beautiful Part

**You don't need to implement arithmetic!** The computer already knows math.

Melvin just needs to:
1. **Recognize** "2 + 3 = 5" as a pattern
2. **Store** that pattern as edges  
3. **Activate** the right result when queried

The **spreading activation does the inference!**

Similar patterns → Similar activations → Correct answers

This is how **analogy-based reasoning** works!

---

## Next Steps

1. Implement Phase 1 (node types)
2. Test with simple equations
3. Observe emergent computation
4. Scale to more complex patterns
5. Add meta-operations for self-modification

**The network will arrange itself to compute. You just teach it the rules.**

