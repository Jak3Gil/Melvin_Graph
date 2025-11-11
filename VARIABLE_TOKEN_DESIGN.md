# Variable-Length Token Design

## Core Concept

Instead of one node per byte, nodes represent **variable-length sequences**.

### Example:
```
Input: "cat car cap"

Token nodes created:
  Node 21: "cat" (3 bytes)
  Node 22: "car" (3 bytes)
  Node 23: "cap" (3 bytes)
  Node 24: "ca" (2 bytes, shared prefix)

No ambiguity! Each word is distinct.
```

---

## Architecture

### Node Structure:
```c
typedef struct {
    float state;
    float energy;
    float threshold;
    
    // TOKEN PAYLOAD (variable-length!)
    uint8_t token[16];      // Can store up to 16 bytes
    uint8_t token_len;      // Actual length (1-16)
    
    // Hierarchy emerges:
    // token_len=1: Letter nodes
    // token_len=2-3: Syllable nodes  
    // token_len=4-10: Word nodes
    // token_len=10+: Phrase nodes
} Node;
```

### Token Discovery:
```
Input stream: "cat car cat"

Algorithm:
1. Try all n-grams (1-16 bytes)
2. Find or create node for each n-gram
3. Strengthen connections between sequential n-grams
4. Reward frequently-occurring n-grams (higher energy)

After training:
  "cat" appears 2x → Node strengthened
  "car" appears 1x → Node exists but weaker
  "ca" appears 3x → Shared prefix node, very strong!
```

### Disambiguation:
```
Input: "c"
  → Activates nodes: "cat", "car", "cap", "ca", "c"
  → All have partial match!
  
Wait for more input: "ca"
  → Now: "cat", "car", "cap", "ca" all match
  → Still ambiguous
  
Wait for more input: "cat"
  → EXACT MATCH on "cat" node!
  → Output: "cat" (unambiguous!)
```

### Pattern Completion:
```
Input: "c"
Nodes activated:
  - "c" (exact, activation=1.0)
  - "ca" (prefix, activation=0.5)  
  - "cat" (prefix, activation=0.33)
  - "car" (prefix, activation=0.33)
  
Propagation:
  "ca" has strong edge to "t" and "r" (disambiguation!)
  "cat" has edge to next word
  
Output: Whichever has strongest total activation
```

---

## Implementation Plan

### 1. Modify Node struct to hold token payload
### 2. Change sense_input() to create n-gram nodes
### 3. Update matching to find best n-gram match
### 4. Update output to emit full tokens
### 5. Test "cat" vs "car" disambiguation

This solves ALL our problems!

Ready to implement?

