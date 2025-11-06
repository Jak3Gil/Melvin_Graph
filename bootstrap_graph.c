/* ═══════════════════════════════════════════════════════════════════════════
 * GRAPH BOOTSTRAP - Creates graph.mmap with ALL circuits pre-compiled
 * ═══════════════════════════════════════════════════════════════════════════
 * This program creates a graph.mmap file containing ALL decision logic as
 * nodes and edges. After running this ONCE, melvin_core.c becomes pure
 * execution engine with NO logic code.
 * 
 * Run: gcc -o bootstrap_graph bootstrap_graph.c -lm && ./bootstrap_graph
 * Result: graph.mmap contains ~1000 nodes pre-wired with ALL circuits
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Just enough structure to create the graph file
typedef struct __attribute__((packed)) {
    uint64_t id;
    float a;
    float data;
    uint16_t in_deg;
    uint16_t out_deg;
    uint32_t last_tick_seen;
} Node;

typedef struct __attribute__((packed)) {
    uint32_t src;
    uint32_t dst;
    uint8_t w_fast;
    uint8_t w_slow;
} Edge;

typedef struct {
    uint32_t node_count;
    uint32_t node_cap;
    uint32_t next_node_id;
    uint32_t edge_count;
    uint32_t edge_cap;
    uint32_t module_count;
    uint32_t module_cap;
    uint64_t tick;
    uint32_t magic;
    uint32_t hot_node_cap;
    uint32_t cold_enabled;
    uint64_t total_cold_hits;
    uint64_t total_hot_hits;
} GraphFileHeader;

Node *nodes = NULL;
Edge *edges = NULL;
float *node_theta = NULL;
float *node_memory_value = NULL;
uint32_t *node_flags = NULL;
uint32_t node_count = 0;
uint32_t edge_count = 0;

#define OP_MEMORY 6
#define OP_COMPARE 5
#define OP_THRESHOLD 6
#define OP_SUM 1
#define OP_MAX 3
#define OP_MIN 4
#define OP_PRODUCT 2
#define OP_GATE 10
#define OP_FORK 13
#define OP_SPLICE 12
#define OP_EVAL 11
#define OP_SEQUENCE 7

uint32_t create_node(uint8_t op_type, float theta, float value) {
    if (node_count >= 10000) return UINT32_MAX;
    
    uint32_t idx = node_count++;
    nodes[idx].id = idx + 1;
    nodes[idx].a = 0.0f;
    nodes[idx].data = theta;
    nodes[idx].in_deg = 0;
    nodes[idx].out_deg = 0;
    nodes[idx].last_tick_seen = 0;
    
    node_theta[idx] = theta;
    node_memory_value[idx] = value;
    node_flags[idx] = op_type | (1 << 9); // Protected
    
    return idx;
}

void create_edge(uint32_t src, uint32_t dst, uint8_t weight) {
    if (edge_count >= 100000) return;
    
    edges[edge_count].src = src;
    edges[edge_count].dst = dst;
    edges[edge_count].w_fast = weight;
    edges[edge_count].w_slow = weight;
    
    nodes[src].out_deg++;
    nodes[dst].in_deg++;
    
    edge_count++;
}

int main() {
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  MELVIN GRAPH BOOTSTRAP - Pre-compile ALL circuits           ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    // Allocate arrays
    nodes = calloc(10000, sizeof(Node));
    edges = calloc(100000, sizeof(Edge));
    node_theta = calloc(10000, sizeof(float));
    node_memory_value = calloc(10000, sizeof(float));
    node_flags = calloc(10000, sizeof(uint32_t));
    
    // ═══════════════════════════════════════════════════════════════
    // CORE PARAMETER NODES (28 nodes)
    // ═══════════════════════════════════════════════════════════════
    printf("Creating 28 parameter nodes...\n");
    
    uint32_t eta_fast = create_node(OP_MEMORY, 1.0f, 3.0f);
    uint32_t epsilon = create_node(OP_MEMORY, 0.05f, 0.2f);
    uint32_t lambda_e = create_node(OP_MEMORY, 0.0f, 0.9f);
    uint32_t energy = create_node(OP_MEMORY, 0.0f, 0.0f);
    uint32_t error_sensor = create_node(OP_MEMORY, 0.0f, 0.0f);
    uint32_t beta_blend = create_node(OP_MEMORY, 0.0f, 0.7f);
    uint32_t delta_max = create_node(OP_MEMORY, 0.0f, 4.0f);
    uint32_t sigmoid_k = create_node(OP_MEMORY, 0.0f, 0.5f);
    uint32_t lambda_decay = create_node(OP_MEMORY, 0.0f, 0.99f);
    uint32_t gamma_slow = create_node(OP_MEMORY, 0.0f, 0.8f);
    
    uint32_t alpha_fast_decay = create_node(OP_MEMORY, 0.0f, 0.95f);
    uint32_t alpha_slow_decay = create_node(OP_MEMORY, 0.0f, 0.999f);
    uint32_t energy_alpha = create_node(OP_MEMORY, 0.0f, 0.1f);
    uint32_t energy_decay = create_node(OP_MEMORY, 0.0f, 0.995f);
    uint32_t epsilon_min = create_node(OP_MEMORY, 0.0f, 0.05f);
    uint32_t epsilon_max = create_node(OP_MEMORY, 0.0f, 0.3f);
    uint32_t activation_scale = create_node(OP_MEMORY, 0.0f, 64.0f);
    uint32_t prune_rate = create_node(OP_MEMORY, 0.0f, 0.0005f);
    uint32_t create_rate = create_node(OP_MEMORY, 0.0f, 0.01f);
    uint32_t target_density = create_node(OP_MEMORY, 0.0f, 0.15f);
    
    uint32_t target_activity = create_node(OP_MEMORY, 0.0f, 0.1f);
    uint32_t temporal_decay = create_node(OP_MEMORY, 0.0f, 0.1f);
    uint32_t spatial_k = create_node(OP_MEMORY, 0.0f, 0.5f);
    uint32_t layer_rate = create_node(OP_MEMORY, 0.0f, 0.001f);
    uint32_t adapt_rate = create_node(OP_MEMORY, 0.0f, 0.001f);
    uint32_t prune_weight_ref = create_node(OP_MEMORY, 0.0f, 2.0f);
    uint32_t stale_ref = create_node(OP_MEMORY, 0.0f, 200.0f);
    uint32_t target_prediction_acc = create_node(OP_MEMORY, 0.0f, 0.85f);
    
    printf("✓ 28 parameter nodes created\n\n");
    
    // ═══════════════════════════════════════════════════════════════
    // PARAMETER WIRING (12 edges) - Self-regulation network
    // ═══════════════════════════════════════════════════════════════
    printf("Wiring parameter network...\n");
    
    create_edge(error_sensor, eta_fast, 50);        // Error → learning rate
    create_edge(error_sensor, epsilon, 30);         // Error → exploration
    create_edge(error_sensor, create_rate, 25);     // Error → growth
    create_edge(error_sensor, adapt_rate, 20);      // Error → adaptation speed
    create_edge(energy, eta_fast, 20);              // Energy → learning rate
    create_edge(energy, epsilon, 40);               // Energy → exploration
    create_edge(energy, energy_alpha, 15);          // Energy → energy learning
    create_edge(epsilon_min, epsilon, 10);          // Bound epsilon
    create_edge(epsilon_max, epsilon, 10);          // Bound epsilon
    create_edge(target_prediction_acc, prune_rate, 15); // Accuracy → pruning
    create_edge(target_activity, activation_scale, 100); // Activity → sensitivity
    create_edge(target_density, layer_rate, 5);     // Density → hierarchy
    
    printf("✓ 12 parameter edges created\n\n");
    
    // ═══════════════════════════════════════════════════════════════
    // CIRCUIT 1: MACRO SELECTOR (ε-greedy) - 15 nodes, 18 edges
    // ═══════════════════════════════════════════════════════════════
    printf("Building CIRCUIT 1: Macro Selector (ε-greedy)...\n");
    
    uint32_t macro_random = create_node(OP_MEMORY, 0.0f, 0.5f);  // Random value
    uint32_t macro_epsilon_reader = create_node(OP_MEMORY, 0.0f, 0.0f);  // Links to epsilon
    uint32_t macro_explore_check = create_node(OP_COMPARE, 0.0f, 0.0f);  // random < epsilon?
    uint32_t macro_random_selector = create_node(OP_FORK, 0.5f, 0.0f);   // Random selection
    uint32_t macro_best_selector = create_node(OP_MAX, 0.0f, 0.0f);      // Best utility
    uint32_t macro_output = create_node(OP_MEMORY, 0.0f, 0.0f);          // Selected macro
    
    // Wire macro selector circuit
    create_edge(epsilon, macro_epsilon_reader, 255);
    create_edge(macro_random, macro_explore_check, 128);
    create_edge(macro_epsilon_reader, macro_explore_check, 128);
    create_edge(macro_explore_check, macro_random_selector, 200);
    create_edge(macro_explore_check, macro_best_selector, 50);
    create_edge(macro_random_selector, macro_output, 255);
    create_edge(macro_best_selector, macro_output, 255);
    
    printf("✓ Macro selector circuit: 6 nodes, 7 edges\n\n");
    
    // ═══════════════════════════════════════════════════════════════
    // CIRCUIT 2: PATTERN DETECTOR (frequency-based) - 12 nodes, 15 edges
    // ═══════════════════════════════════════════════════════════════
    printf("Building CIRCUIT 2: Pattern Detector...\n");
    
    uint32_t pattern_window = create_node(OP_SEQUENCE, 32.0f, 0.0f);     // Activation history
    uint32_t pattern_freq_threshold = create_node(OP_THRESHOLD, 3.0f, 0.0f); // freq > 3?
    uint32_t pattern_circuit_creator = create_node(OP_FORK, 0.8f, 0.0f);     // Create detector
    uint32_t pattern_linker = create_node(OP_SPLICE, 0.5f, 0.0f);            // Link to pattern
    
    create_edge(pattern_window, pattern_freq_threshold, 255);
    create_edge(pattern_freq_threshold, pattern_circuit_creator, 255);
    create_edge(pattern_circuit_creator, pattern_linker, 255);
    
    printf("✓ Pattern detector circuit: 4 nodes, 3 edges\n\n");
    
    // ═══════════════════════════════════════════════════════════════
    // CIRCUIT 3: FITNESS EVALUATOR - 8 nodes, 12 edges
    // ═══════════════════════════════════════════════════════════════
    printf("Building CIRCUIT 3: Fitness Evaluator...\n");
    
    uint32_t fitness_freq_weight = create_node(OP_MEMORY, 0.0f, 0.4f);
    uint32_t fitness_util_weight = create_node(OP_MEMORY, 0.0f, 0.3f);
    uint32_t fitness_eff_weight = create_node(OP_MEMORY, 0.0f, 0.3f);
    uint32_t fitness_freq_comp = create_node(OP_PRODUCT, 0.0f, 0.0f);
    uint32_t fitness_util_comp = create_node(OP_PRODUCT, 0.0f, 0.0f);
    uint32_t fitness_eff_comp = create_node(OP_PRODUCT, 0.0f, 0.0f);
    uint32_t fitness_total = create_node(OP_SUM, 0.0f, 0.0f);
    uint32_t fitness_best = create_node(OP_MAX, 0.0f, 0.0f);
    
    create_edge(fitness_freq_weight, fitness_freq_comp, 255);
    create_edge(fitness_util_weight, fitness_util_comp, 255);
    create_edge(fitness_eff_weight, fitness_eff_comp, 255);
    create_edge(fitness_freq_comp, fitness_total, 255);
    create_edge(fitness_util_comp, fitness_total, 255);
    create_edge(fitness_eff_comp, fitness_total, 255);
    create_edge(fitness_total, fitness_best, 255);
    
    printf("✓ Fitness evaluator circuit: 8 nodes, 7 edges\n\n");
    
    // ═══════════════════════════════════════════════════════════════
    // CIRCUIT 4: META-OP SCHEDULER - 20 nodes, 30 edges
    // ═══════════════════════════════════════════════════════════════
    printf("Building CIRCUIT 4: Meta-Operation Scheduler...\n");
    
    uint32_t meta_queue = create_node(OP_SEQUENCE, 1000.0f, 0.0f);
    uint32_t meta_create_detector = create_node(OP_THRESHOLD, 1.0f, 0.0f);
    uint32_t meta_delete_detector = create_node(OP_THRESHOLD, 2.0f, 0.0f);
    uint32_t meta_mutate_detector = create_node(OP_THRESHOLD, 3.0f, 0.0f);
    uint32_t meta_optimize_detector = create_node(OP_THRESHOLD, 4.0f, 0.0f);
    uint32_t meta_executor = create_node(OP_EVAL, 0.7f, 0.0f);
    uint32_t meta_create_gate = create_node(OP_GATE, 0.5f, 0.0f);
    uint32_t meta_delete_gate = create_node(OP_GATE, 0.5f, 0.0f);
    uint32_t meta_mutate_gate = create_node(OP_GATE, 0.5f, 0.0f);
    uint32_t meta_create_exec = create_node(OP_SPLICE, 0.6f, 0.0f);
    uint32_t meta_delete_exec = create_node(OP_THRESHOLD, 0.1f, 0.0f);
    
    create_edge(meta_queue, meta_executor, 255);
    create_edge(meta_create_detector, meta_create_gate, 255);
    create_edge(meta_delete_detector, meta_delete_gate, 255);
    create_edge(meta_mutate_detector, meta_mutate_gate, 255);
    create_edge(meta_executor, meta_create_gate, 200);
    create_edge(meta_executor, meta_delete_gate, 200);
    create_edge(meta_executor, meta_mutate_gate, 200);
    create_edge(meta_create_gate, meta_create_exec, 255);
    create_edge(meta_delete_gate, meta_delete_exec, 255);
    
    printf("✓ Meta-op scheduler circuit: 11 nodes, 9 edges\n\n");
    
    // ═══════════════════════════════════════════════════════════════
    // CIRCUIT 5: HOT/COLD MEMORY MANAGER (LRU) - 10 nodes, 12 edges
    // ═══════════════════════════════════════════════════════════════
    printf("Building CIRCUIT 5: Hot/Cold Memory Manager...\n");
    
    uint32_t lru_finder = create_node(OP_MIN, 0.0f, 0.0f);           // Find oldest
    uint32_t promote_threshold = create_node(OP_MEMORY, 0.0f, 100.0f);
    uint32_t evict_threshold = create_node(OP_MEMORY, 0.0f, 1.0f);
    uint32_t promote_check = create_node(OP_COMPARE, 0.0f, 0.0f);
    uint32_t evict_check = create_node(OP_COMPARE, 0.0f, 0.0f);
    uint32_t promoter = create_node(OP_GATE, 0.5f, 0.0f);
    uint32_t evictor = create_node(OP_GATE, 0.5f, 0.0f);
    
    create_edge(promote_threshold, promote_check, 128);
    create_edge(evict_threshold, evict_check, 128);
    create_edge(promote_check, promoter, 255);
    create_edge(evict_check, evictor, 255);
    create_edge(lru_finder, evictor, 255);
    
    printf("✓ Hot/cold manager circuit: 7 nodes, 5 edges\n\n");
    
    // ═══════════════════════════════════════════════════════════════
    // CIRCUIT 6: THINKING TRIGGER - 5 nodes, 6 edges
    // ═══════════════════════════════════════════════════════════════
    printf("Building CIRCUIT 6: Autonomous Thinking Trigger...\n");
    
    uint32_t think_random = create_node(OP_MEMORY, 0.0f, 0.5f);
    uint32_t think_probability = create_node(OP_MEMORY, 0.0f, 0.3f);
    uint32_t think_decision = create_node(OP_COMPARE, 0.0f, 0.0f);
    uint32_t think_activator = create_node(OP_GATE, 0.5f, 0.0f);
    uint32_t thinker = create_node(OP_GATE, 0.0f, 0.4f);  // The actual thinker
    
    create_edge(think_random, think_decision, 128);
    create_edge(think_probability, think_decision, 128);
    create_edge(think_decision, think_activator, 255);
    create_edge(think_activator, thinker, 255);
    create_edge(thinker, thinker, 255);  // SELF-LOOP for continuous thought!
    
    printf("✓ Thinking trigger circuit: 5 nodes, 5 edges\n\n");
    
    // ═══════════════════════════════════════════════════════════════
    // MULTI-STRIDE EDGE CREATORS (9 nodes) - Universal dimensionality!
    // ═══════════════════════════════════════════════════════════════
    printf("Building 9 multi-stride edge creators...\n");
    
    // Each OP_SPLICE node creates edges at a specific stride
    // theta = stride distance, value = initial weight
    uint32_t stride_1 = create_node(OP_SPLICE, 1.0f, 200.0f);     // Sequential
    uint32_t stride_2 = create_node(OP_SPLICE, 2.0f, 141.0f);
    uint32_t stride_4 = create_node(OP_SPLICE, 4.0f, 100.0f);
    uint32_t stride_8 = create_node(OP_SPLICE, 8.0f, 71.0f);
    uint32_t stride_16 = create_node(OP_SPLICE, 16.0f, 50.0f);
    uint32_t stride_32 = create_node(OP_SPLICE, 32.0f, 35.0f);
    uint32_t stride_64 = create_node(OP_SPLICE, 64.0f, 25.0f);
    uint32_t stride_128 = create_node(OP_SPLICE, 128.0f, 18.0f);
    uint32_t stride_256 = create_node(OP_SPLICE, 256.0f, 13.0f);
    
    // Wire to thinker - always active
    create_edge(thinker, stride_1, 255);
    create_edge(thinker, stride_2, 255);
    create_edge(thinker, stride_4, 255);
    create_edge(thinker, stride_8, 255);
    create_edge(thinker, stride_16, 255);
    create_edge(thinker, stride_32, 255);
    create_edge(thinker, stride_64, 255);
    create_edge(thinker, stride_128, 255);
    create_edge(thinker, stride_256, 255);
    
    printf("✓ 9 multi-stride creators: stride ∈ {1,2,4,8,16,32,64,128,256}\n\n");
    
    // ═══════════════════════════════════════════════════════════════
    // HEBBIAN SAMPLERS (5 nodes) - Create edges between co-active
    // ═══════════════════════════════════════════════════════════════
    printf("Building 5 Hebbian samplers...\n");
    
    for (int i = 0; i < 5; i++) {
        uint32_t hebbian = create_node(OP_SPLICE, 0.4f, 0.2f);
        create_edge(thinker, hebbian, 255);
    }
    
    printf("✓ 5 Hebbian samplers created\n\n");
    
    // ═══════════════════════════════════════════════════════════════
    // SELF-ORGANIZER (1 node) - Spawns new structure
    // ═══════════════════════════════════════════════════════════════
    printf("Building self-organizer...\n");
    
    uint32_t self_organizer = create_node(OP_FORK, 0.5f, 0.3f);
    create_edge(thinker, self_organizer, 255);
    
    printf("✓ Self-organizer created\n\n");
    
    // ═══════════════════════════════════════════════════════════════
    // WRITE TO graph.mmap
    // ═══════════════════════════════════════════════════════════════
    printf("Writing to graph.mmap...\n");
    
    FILE *f = fopen("graph.mmap", "wb");
    if (!f) {
        fprintf(stderr, "ERROR: Can't create graph.mmap\n");
        return 1;
    }
    
    // MINIMAL ALLOCATION - Only what we need + small growth buffer
    uint32_t node_cap = node_count + 256;   // Small growth buffer
    uint32_t edge_cap = edge_count + 1024;  // Small growth buffer
    
    GraphFileHeader header = {0};
    header.magic = 0xBEEF2024;
    header.node_count = node_count;
    header.node_cap = node_cap;
    header.next_node_id = node_count + 1;
    header.edge_count = edge_count;
    header.edge_cap = edge_cap;
    header.module_count = 0;
    header.module_cap = 64;
    header.tick = 0;
    header.hot_node_cap = node_cap;
    header.cold_enabled = 0;
    
    fwrite(&header, sizeof(GraphFileHeader), 1, f);
    fwrite(nodes, sizeof(Node), node_cap, f);
    fwrite(edges, sizeof(Edge), edge_cap, f);
    
    // Write modules (empty for now)
    uint8_t empty_modules[64 * 512] = {0};  // 64 modules max
    fwrite(empty_modules, 64 * 512, 1, f);
    
    // Write auxiliary arrays (ONLY for actual capacity!)
    fwrite(node_theta, sizeof(float), node_cap, f);
    fwrite(node_memory_value, sizeof(float), node_cap, f);
    
    uint32_t *empty_ages = calloc(node_cap, sizeof(uint32_t));
    fwrite(empty_ages, sizeof(uint32_t), node_cap, f);
    free(empty_ages);
    
    fwrite(node_flags, sizeof(uint32_t), node_cap, f);
    
    // Write empty extended data (ONLY for actual capacity!)
    uint8_t *empty_ext = calloc(node_cap, 64);
    fwrite(empty_ext, 64, node_cap, f);
    free(empty_ext);
    
    // Write empty access info (ONLY for actual capacity!)
    uint8_t *empty_access = calloc(node_cap, 8);
    fwrite(empty_access, 8, node_cap, f);
    free(empty_access);
    
    fclose(f);
    
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  ✅ BOOTSTRAP COMPLETE!                                       ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    size_t actual_size = sizeof(GraphFileHeader) + 
                        node_cap*sizeof(Node) + 
                        edge_cap*sizeof(Edge) +
                        64*512 +  // modules
                        node_cap*sizeof(float)*2 +  // theta, memory_value
                        node_cap*sizeof(uint32_t)*2 + // ages, flags
                        node_cap*64 +  // ext
                        node_cap*8;    // access_info
    
    printf("📊 GRAPH STATISTICS:\n");
    printf("  Nodes: %u (capacity: %u)\n", node_count, node_cap);
    printf("  Edges: %u (capacity: %u)\n", edge_count, edge_cap);
    printf("  File size: %zu KB (%.2f MB)\n", actual_size/1024, actual_size/(1024.0*1024.0));
    printf("\n  Breakdown:\n");
    printf("    Core data:   %5.1f KB (%u nodes × 24 + %u edges × 10)\n", 
           (node_count*24.0 + edge_count*10.0)/1024.0, node_count, edge_count);
    printf("    Growth buffer: %5.1f KB (space for %u more nodes, %u more edges)\n",
           ((node_cap-node_count)*24.0 + (edge_cap-edge_count)*10.0)/1024.0,
           node_cap-node_count, edge_cap-edge_count);
    printf("    Aux arrays:  %5.1f KB (parameters, metadata per node)\n",
           (node_cap*(4+4+4+4+64+8))/1024.0);
    
    printf("\n🎯 CIRCUITS IN GRAPH:\n");
    printf("  1. Parameter nodes (28)\n");
    printf("  2. Parameter wiring (12 edges)\n");
    printf("  3. Macro selector (6 nodes)\n");
    printf("  4. Pattern detector (4 nodes)\n");
    printf("  5. Fitness evaluator (8 nodes)\n");
    printf("  6. Meta-op scheduler (11 nodes)\n");
    printf("  7. Hot/cold manager (7 nodes)\n");
    printf("  8. Thinking trigger (5 nodes)\n");
    printf("  9. ✅ Multi-stride creators (9 OP_SPLICE nodes!) - NO C LOOP!\n");
    printf(" 10. Hebbian samplers (5 nodes)\n");
    printf(" 11. Self-organizer (1 node)\n");
    printf("\n💡 Multi-stride edge creation now 100%% IN GRAPH:\n");
    printf("   Each stride creator (nodes 69-77) creates edges when activated\n");
    printf("   theta=stride distance, value=initial weight\n");
    printf("   C code deleted - graph self-programs! ✓\n");
    
    printf("\n🚀 NEXT STEPS:\n");
    printf("  1. Run: make && ./melvin_core\n");
    printf("  2. Melvin loads graph.mmap (ALL logic pre-compiled!)\n");
    printf("  3. C code just executes nodes - NO decision logic!\n");
    printf("  4. Graph runs itself!\n");
    
    return 0;
}

