// Test: Does propagate() actually activate node 29 when node 26 fires?
#include <stdio.h>

int main() {
    printf("After sense_input('cat'):\n");
    printf("  Node 26 ('cat') state = 3.0 (from input)\n");
    printf("\n");
    printf("After propagate():\n");
    printf("  Node 26 → 29 (weight=2.5)\n");
    printf("  Expected: Node 29 ('sat') state should be > 0.5\n");
    printf("\n");
    printf("In emit_output():\n");
    printf("  Checks: node->state > 0.5 && node->energy > 1.0\n");
    printf("  If Node 29 active → should output 'sat'\n");
    printf("\n");
    printf("But we're getting 'at' instead of 'sat'!\n");
    printf("This means either:\n");
    printf("  1. propagate() not activating node 29\n");
    printf("  2. emit_output() not checking node 29\n");
    printf("  3. Output using wrong nodes\n");
    return 0;
}
