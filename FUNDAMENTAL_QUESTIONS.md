# Fundamental Questions - What We Need to Answer

## The Problem We Keep Hitting

Every implementation hits the same wall:
- "cat" vs "car" can't be disambiguated
- One node per byte creates conflicts
- Adding features doesn't fix the architecture

---

## The User's Core Questions:

> "How can we build a coding language in the form of nodes and edges?"
> "That is coded with data and can solve its own intelligence problems over time?"
> "Rules that breed intelligence, not just survival?"

---

## Questions We MUST Answer Before Coding:

### 1. What Is The Atomic Unit?

**Option A: Byte-level** (current)
```
Atom: Individual bytes ('c', 'a', 't')
Pro: Simple, universal
Con: Can't disambiguate "cat" vs "car"
```

**Option B: Token-level** (your idea)
```
Atom: Variable-length sequences ("cat", "car", "ca")
Pro: No ambiguity, compression built-in
Con: How to discover optimal token boundaries?
```

**Option C: Context-sensitive**
```
Atom: (byte, context) pairs
  ('a', in-cat) ≠ ('a', in-car)
Pro: Disambiguation through context
Con: How to represent context?
```

### 2. What Is Context and Where Does It Live?

**Current**: No context representation
**Needed**: Some way to encode "we're processing word X not word Y"

**Options**:
- A) Separate context nodes
- B) Node activation patterns ARE context
- C) Edges encode context (edge type varies by context)
- D) Temporal sequences store context

### 3. How Does Data Create Structure?

**Current**: Seeing "cat" creates nodes for c, a, t

**Questions**:
- Should "cat" create 1 node or 3?
- Should seeing "cat" 10x create 10 nodes or strengthen 1?
- When does a pattern become a unit?
- Who decides: programmer or emergence?

### 4. What Are The Execution Semantics?

**Statistical** (current non-rule mode):
- Edges are probabilities
- Multiple paths can fire
- Output is combination

**Logical** (rule mode):
- Edges are implications
- IF src THEN dst MUST follow
- Output must satisfy all rules

**Which one? Or both? Or something else?**

### 5. How Does Intelligence Emerge vs Get Programmed?

**Programmed**: We code pattern detection, rule learning, etc.
**Emergent**: Simple rules → complex behavior appears

**What are the SIMPLEST rules that would make:**
- Disambiguation emerge?
- Generalization emerge?
- Variable-length tokens emerge?

---

## The Real Question:

### What if we said:

```
ONLY 3 RULES:
1. Nodes activate when input matches
2. Active nodes strengthen connections to next active node
3. Nodes with high prediction accuracy get more energy

THAT'S IT.
```

**Would disambiguation EMERGE from just these 3 rules?**

Or do we need to explicitly design:
- Token boundaries
- Context representation  
- Execution semantics

---

## My Hypothesis:

The user is saying: **"Stop adding features. Define the physics."**

Like:
- Gravity + momentum = orbits emerge
- Selection + variation = evolution emerges
- ??? + ??? = intelligence emerges

**What are the ??? rules?**

---

## Proposed Minimal Rule Set:

### Rule Set A (Prediction-Driven):
```
1. Nodes predict next input
2. Correct predictions get energy
3. High-energy nodes dominate
4. Nodes split when serving multiple roles
```

### Rule Set B (Information-Theoretic):
```
1. Input is a stream of bits
2. Nodes compress bit patterns  
3. Better compression = more energy
4. Nodes represent whatever compresses best
```

### Rule Set C (Evolutionary):
```
1. Nodes mutate (change what they represent)
2. Nodes reproduce (create variants)
3. Selection (best performers survive)
4. Environment = prediction accuracy
```

---

## What Should We Build?

Before writing ANY code, we need to decide:

**What atomic unit?**
**What context representation?**
**What execution model?**
**What minimal rules?**

Then build ONLY that. No features. Just rules. See what emerges.

---

## Your Call

Which path?
1. Answer these questions explicitly
2. Try minimal rule set and see what emerges
3. Build variable-length tokens (your original idea)
4. Something else?

**The code is ready. The question is: what should it BE?**

