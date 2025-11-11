#include <stdio.h>
#include <stdint.h>

typedef struct {
    float state, energy, threshold;
    uint32_t last_active_tick, activation_sequence;
    uint8_t is_hub, hub_level;
    uint16_t in_degree, out_degree;
    uint8_t is_pattern;
    uint32_t pattern_members[16];
    uint8_t pattern_member_count;
    uint8_t token[16];
    uint8_t token_len;
    uint32_t frequency;
    uint32_t predicted_next_node;
    float prediction_confidence;
    uint32_t predictions_correct, predictions_wrong;
} Node;

typedef struct {
    uint32_t src, dst;
    float weight;
    uint8_t is_rule, rule_strength, times_satisfied, times_violated;
    uint8_t is_implication, is_inhibitory;
} Connection;

int main() {
    printf("Node size: %zu bytes\n", sizeof(Node));
    printf("Connection size: %zu bytes\n", sizeof(Connection));
    return 0;
}
