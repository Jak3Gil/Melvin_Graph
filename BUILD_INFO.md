# MELVIN BUILD INFO

## Current Build

**Executable**: `melvin`
**Language**: C99
**Size**: ~530 lines of code
**Compiled**: Ready to use

## Features Included

### Core System
✅ Variable-length tokens (no cat/car confusion)
✅ Executable rule nodes (data writes code)
✅ Similarity-based generalization
✅ Multi-hop chaining (2-3 hops)
✅ Activation decay (clean output)
✅ Universal data (any bytes)

### Reasoning (NEW!)
✅ Competition (context selection)
✅ Inhibition (logical negation)
✅ Rule strength (usage-based)
✅ Emergent reasoning

## How to Use

### Basic Usage
```bash
# Train and test
echo "cat sat" | ./melvin
echo "cat" | ./melvin

# Persistent learning
echo "hello world" | ./melvin  # Creates graph.mmap
echo "goodbye world" | ./melvin  # Loads and extends
echo "hello" | ./melvin  # Uses learned patterns
```

### Debug Mode
```bash
MELVIN_DEBUG=1 echo "test" | ./melvin
```

### Reset Graph
```bash
rm graph.mmap  # Start fresh
```

## Compilation

```bash
make clean      # Remove old builds
make melvin     # Build system
```

**Compiler**: gcc with `-O2` optimization
**Warnings**: Enabled (`-Wall -Wextra`)
**Standard**: C99

## System Capabilities

### What It Does Well
- Pattern recognition
- Sequence learning  
- Generalization (few-shot)
- Multi-hop chaining
- Context selection
- Universal data types

### Current Limits
- Short chains (2-3 hops due to decay)
- No arithmetic computation (yet)
- No structural abstraction
- Memory cap (10,000 nodes)

## Files

- `melvin.c` - Main system (530 lines)
- `graph.mmap` - Persistent memory
- `Makefile` - Build configuration

## Performance

- **Fast**: Pattern matching in microseconds
- **Compact**: 530 lines of code
- **Persistent**: Graph saved to disk
- **Efficient**: mmap for zero-copy memory

## Next Steps

To enhance reasoning:
1. Add "NOT" keyword detection
2. Implement operator nodes (arithmetic)
3. Add confidence scoring
4. Expand memory capacity

But current system is FUNCTIONAL and POWERFUL!
