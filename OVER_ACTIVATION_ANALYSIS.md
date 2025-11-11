# OVER-ACTIVATION PROBLEM

## Symptom
Every input activates EVERYTHING in the graph:
- 'red' → apple rose fire blue sky ocean water green grass tree leaf
- 'hello' → world goodbye the quick brown fox cat sat on mat 1 2 3 red blue green

## Root Cause
Multi-hop execution + no thresholds = unlimited propagation:
1. Input 'red' activates node 'red'
2. 'red' fires rule: red→apple
3. 'apple' now active, fires rule: apple→red  
4. 'red' fires again: red→rose
5. 'rose' fires: rose→red
6. ... infinite cascade through ALL connected nodes!

## Why It Happens
```
Training: "red apple", "blue sky", "green grass"

Graph structure:
  red → apple
  apple → (nothing, but generalizes to blue/green)
  blue → sky
  sky → (generalizes to other nodes)
  
Multi-hop with no restrictions:
  'red' → apple → blue → sky → green → grass → ...
```

## Solutions

### Option 1: Activity Decay
- Nodes lose energy each hop
- Only strong connections propagate far
- Weak connections die out quickly

### Option 2: Activation Budget
- Each input gets limited "activation energy"
- Distribute across activated nodes
- Forces selectivity

### Option 3: Contextual Inhibition  
- Active nodes INHIBIT unrelated nodes
- 'red' active → suppress 'blue' and 'green'
- Competitive dynamics

### Option 4: Hop Limit Already Exists!
- We already limit to 5 hops
- Problem: EVERYTHING reachable in 5 hops
- Solution: Stricter activation requirements per hop

### Option 5: Rule Strength
- Rules have strength (how often used together)
- Weak connections don't propagate
- Natural pruning

## Recommendation
Implement **Activity Decay per Hop**:
- Hop 0 (input): state = 1.0
- Hop 1: state = 0.8
- Hop 2: state = 0.6  
- Hop 3: state = 0.4
- Hop 4: state = 0.2
- Output only if state > 0.3

This creates natural distance-based filtering!
