#include <stdio.h>
#include <string.h>

// Minimal prototype to demonstrate emergent reasoning

typedef struct {
    char name[32];
    float strength;      // Competition: 0-1
    int inhibit_count;   // Inhibition: how many to suppress
    char inhibits[32];   // What to suppress
} Rule;

void demonstrate_competition() {
    printf("=== COMPETITION (Context Selection) ===\n\n");
    
    Rule rules[3] = {
        {"at_home_sleep", 0.91, 0, ""},       // 100 uses
        {"at_work_code", 0.91, 0, ""},        // 100 uses  
        {"at_random_noise", 0.09, 0, ""}      // 1 use
    };
    
    printf("Input: 'at'\n");
    printf("All rules match, but:\n\n");
    
    for (int i = 0; i < 3; i++) {
        printf("  %s: strength=%.2f ", rules[i].name, rules[i].strength);
        if (rules[i].strength > 0.5) {
            printf("→ FIRE!\n");
        } else {
            printf("→ blocked (too weak)\n");
        }
    }
    
    printf("\nResult: Only strong rules fire!\n");
    printf("Output: 'home sleep' and 'work code' (not noise)\n\n");
}

void demonstrate_inhibition() {
    printf("=== INHIBITION (Negation) ===\n\n");
    
    printf("Training: 'car is NOT animal'\n");
    printf("Creates rule: car → vehicle, INHIBIT(animal)\n\n");
    
    printf("Input: 'car is'\n");
    printf("Processing:\n");
    printf("  1. 'car' activates (1.0)\n");
    printf("  2. Rule fires:\n");
    printf("     - 'vehicle' += 0.8 (excite)\n");
    printf("     - 'animal' += -0.8 (inhibit!)\n");
    printf("  3. Final activations:\n");
    printf("     - vehicle: 0.8 ✓\n");
    printf("     - animal: -0.8 (suppressed)\n\n");
    
    printf("Output: 'vehicle' (not animal!)\n\n");
    printf("Emergent property: Logical negation!\n\n");
}

void demonstrate_together() {
    printf("=== COMPETITION + INHIBITION Together ===\n\n");
    
    printf("Training:\n");
    printf("  'bat animal' (100x) - the creature\n");
    printf("  'bat baseball' (100x) - the equipment\n");
    printf("  'bat cave' (1x) - noise\n\n");
    
    printf("Plus inhibition:\n");
    printf("  animal INHIBITS baseball\n");
    printf("  baseball INHIBITS animal\n\n");
    
    printf("Input: 'bat'\n");
    printf("All 3 rules compete:\n");
    printf("  1. bat→animal (strength=0.91) ✓\n");
    printf("  2. bat→baseball (strength=0.91) ✓\n");
    printf("  3. bat→cave (strength=0.09) ✗ blocked\n\n");
    
    printf("But they inhibit each other!\n");
    printf("  animal activates → inhibits baseball\n");
    printf("  baseball activates → inhibits animal\n");
    printf("  Result: Whichever fires FIRST wins!\n\n");
    
    printf("Add more context:\n");
    printf("Input: 'bat flies'\n");
    printf("  'flies' → activates 'animal' context\n");
    printf("  animal rule gets boost → wins!\n");
    printf("  → 'animal' (disambiguated by context!)\n\n");
}

int main() {
    demonstrate_competition();
    demonstrate_inhibition();
    demonstrate_together();
    
    printf("=== KEY INSIGHT ===\n\n");
    printf("Just TWO simple rules:\n");
    printf("  1. Competition (strongest wins)\n");
    printf("  2. Inhibition (negative activation)\n\n");
    printf("Enable:\n");
    printf("  ✓ Context selection\n");
    printf("  ✓ Logical negation\n");
    printf("  ✓ Disambiguation\n");
    printf("  ✓ Mutual exclusion\n\n");
    printf("REASONING EMERGES from simple rules!\n");
    
    return 0;
}
