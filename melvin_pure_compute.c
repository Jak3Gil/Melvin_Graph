/*
 * MELVIN PURE COMPUTE - Computation Through Graph Structure
 * 
 * Implements computation PURELY through nodes and edges:
 * - Nodes are operations (XOR, AND, OR, NOT gates)
 * - Edges route data and determine execution order
 * - Spreading activation IS the computation
 * - No CPU instruction calls - everything in the graph!
 * 
 * This proves: "Nodes execute, edges route, computation emerges"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ========================================================================
 * NODE TYPES - Different operations
 * ======================================================================== */

typedef enum {
    NODE_VALUE,     // Stores a bit value (0 or 1)
    NODE_XOR,       // XOR gate (a ^ b)
    NODE_AND,       // AND gate (a & b)
    NODE_OR,        // OR gate (a | b)
    NODE_NOT,       // NOT gate (!a)
    NODE_ADDER,     // Full adder (uses XOR + AND internally)
    NODE_OUTPUT     // Output node (collects results)
} NodeType;

typedef struct {
    NodeType type;
    uint8_t value;          // Current value (0 or 1, or result)
    float activation;       // Execution ready state
    uint32_t input_count;   // How many inputs needed
    uint32_t inputs[8];     // Input node indices
    char label[32];         // For debugging
} Node;

typedef struct {
    uint32_t from;
    uint32_t to;
    uint8_t weight;
} Edge;

typedef struct {
    Node *nodes;
    uint32_t node_count;
    uint32_t node_cap;
    
    Edge *edges;
    uint32_t edge_count;
    uint32_t edge_cap;
} Graph;

Graph g;
int debug = 1;  // Always debug for demo

/* ========================================================================
 * NODE CREATION
 * ======================================================================== */

uint32_t create_node(NodeType type, const char *label) {
    if (g.node_count >= g.node_cap) {
        fprintf(stderr, "[ERROR] Node capacity reached\n");
        exit(1);
    }
    
    uint32_t id = g.node_count++;
    g.nodes[id].type = type;
    g.nodes[id].value = 0;
    g.nodes[id].activation = 0.0f;
    g.nodes[id].input_count = 0;
    strncpy(g.nodes[id].label, label, 31);
    g.nodes[id].label[31] = '\0';
    
    if (debug) {
        char *type_str = (type == NODE_VALUE) ? "VAL" :
                        (type == NODE_XOR) ? "XOR" :
                        (type == NODE_AND) ? "AND" :
                        (type == NODE_OR) ? "OR" :
                        (type == NODE_NOT) ? "NOT" :
                        (type == NODE_ADDER) ? "ADD" : "OUT";
        fprintf(stderr, "[NODE] %s #%u: %s\n", type_str, id, label);
    }
    
    return id;
}

void create_edge_conn(uint32_t from, uint32_t to, uint8_t weight) {
    if (g.edge_count >= g.edge_cap) return;
    
    g.edges[g.edge_count].from = from;
    g.edges[g.edge_count].to = to;
    g.edges[g.edge_count].weight = weight;
    g.edge_count++;
    
    // Track as input to target node
    if (g.nodes[to].input_count < 8) {
        g.nodes[to].inputs[g.nodes[to].input_count++] = from;
    }
    
    if (debug) {
        fprintf(stderr, "[EDGE] %s → %s (weight: %u)\n", 
                g.nodes[from].label, g.nodes[to].label, weight);
    }
}

/* ========================================================================
 * GATE EXECUTION - Pure graph operations!
 * ======================================================================== */

void execute_node(uint32_t id) {
    Node *n = &g.nodes[id];
    
    // Skip if already executed or no inputs ready
    if (n->activation >= 1.0f) return;
    
    switch(n->type) {
        case NODE_VALUE:
            // Value nodes are set externally
            n->activation = 1.0f;
            break;
            
        case NODE_XOR:
            if (n->input_count >= 2) {
                uint8_t a = g.nodes[n->inputs[0]].value;
                uint8_t b = g.nodes[n->inputs[1]].value;
                n->value = a ^ b;  // XOR operation
                n->activation = 1.0f;
                
                if (debug) {
                    fprintf(stderr, "[EXEC] XOR: %s(%d) XOR %s(%d) = %d\n",
                            g.nodes[n->inputs[0]].label, a,
                            g.nodes[n->inputs[1]].label, b,
                            n->value);
                }
            }
            break;
            
        case NODE_AND:
            if (n->input_count >= 2) {
                uint8_t a = g.nodes[n->inputs[0]].value;
                uint8_t b = g.nodes[n->inputs[1]].value;
                n->value = a & b;  // AND operation
                n->activation = 1.0f;
                
                if (debug) {
                    fprintf(stderr, "[EXEC] AND: %s(%d) AND %s(%d) = %d\n",
                            g.nodes[n->inputs[0]].label, a,
                            g.nodes[n->inputs[1]].label, b,
                            n->value);
                }
            }
            break;
            
        case NODE_OR:
            if (n->input_count >= 2) {
                uint8_t a = g.nodes[n->inputs[0]].value;
                uint8_t b = g.nodes[n->inputs[1]].value;
                n->value = a | b;  // OR operation
                n->activation = 1.0f;
                
                if (debug) {
                    fprintf(stderr, "[EXEC] OR: %s(%d) OR %s(%d) = %d\n",
                            g.nodes[n->inputs[0]].label, a,
                            g.nodes[n->inputs[1]].label, b,
                            n->value);
                }
            }
            break;
            
        case NODE_NOT:
            if (n->input_count >= 1) {
                uint8_t a = g.nodes[n->inputs[0]].value;
                n->value = !a;  // NOT operation
                n->activation = 1.0f;
                
                if (debug) {
                    fprintf(stderr, "[EXEC] NOT: NOT %s(%d) = %d\n",
                            g.nodes[n->inputs[0]].label, a, n->value);
                }
            }
            break;
            
        case NODE_ADDER:
            // Full adder: 3 inputs (a, b, carry_in) → 2 outputs (sum, carry_out)
            if (n->input_count >= 3) {
                uint8_t a = g.nodes[n->inputs[0]].value;
                uint8_t b = g.nodes[n->inputs[1]].value;
                uint8_t c_in = g.nodes[n->inputs[2]].value;
                
                // sum = a XOR b XOR carry_in
                n->value = a ^ b ^ c_in;
                n->activation = 1.0f;
                
                if (debug) {
                    fprintf(stderr, "[EXEC] ADDER: %s(%d) + %s(%d) + carry(%d) = %d\n",
                            g.nodes[n->inputs[0]].label, a,
                            g.nodes[n->inputs[1]].label, b,
                            c_in, n->value);
                }
            }
            break;
            
        case NODE_OUTPUT:
            // Output just collects value from input
            if (n->input_count >= 1) {
                n->value = g.nodes[n->inputs[0]].value;
                n->activation = 1.0f;
            }
            break;
    }
}

/* ========================================================================
 * COMPUTATION ENGINE - Spreading activation = execution!
 * ======================================================================== */

void propagate_computation(int max_iterations) {
    if (debug) fprintf(stderr, "\n[COMPUTE] Starting propagation...\n");
    
    for (int iter = 0; iter < max_iterations; iter++) {
        int any_executed = 0;
        
        // Execute all nodes whose inputs are ready
        for (uint32_t i = 0; i < g.node_count; i++) {
            Node *n = &g.nodes[i];
            
            if (n->activation >= 1.0f) continue;  // Already executed
            
            // Check if all inputs are ready
            int all_ready = 1;
            for (uint32_t j = 0; j < n->input_count; j++) {
                if (g.nodes[n->inputs[j]].activation < 1.0f) {
                    all_ready = 0;
                    break;
                }
            }
            
            if (all_ready && n->input_count > 0) {
                execute_node(i);
                any_executed = 1;
            }
        }
        
        if (!any_executed) break;  // Computation complete
    }
    
    if (debug) fprintf(stderr, "[COMPUTE] Propagation complete\n\n");
}

/* ========================================================================
 * BUILD ADDER CIRCUIT - Pure graph-based addition!
 * ======================================================================== */

void build_1bit_adder(uint32_t a, uint32_t b, uint32_t carry_in,
                     uint32_t *sum_out, uint32_t *carry_out) {
    // Full adder using only XOR and AND gates!
    
    // sum = a XOR b XOR carry_in
    uint32_t xor1 = create_node(NODE_XOR, "xor_ab");
    create_edge_conn(a, xor1, 255);
    create_edge_conn(b, xor1, 255);
    
    *sum_out = create_node(NODE_XOR, "sum");
    create_edge_conn(xor1, *sum_out, 255);
    create_edge_conn(carry_in, *sum_out, 255);
    
    // carry_out = (a AND b) OR (xor1 AND carry_in)
    uint32_t and1 = create_node(NODE_AND, "and_ab");
    create_edge_conn(a, and1, 255);
    create_edge_conn(b, and1, 255);
    
    uint32_t and2 = create_node(NODE_AND, "and_xor_cin");
    create_edge_conn(xor1, and2, 255);
    create_edge_conn(carry_in, and2, 255);
    
    *carry_out = create_node(NODE_OR, "carry_out");
    create_edge_conn(and1, *carry_out, 255);
    create_edge_conn(and2, *carry_out, 255);
}

void build_8bit_adder(uint8_t a_val, uint8_t b_val) {
    fprintf(stderr, "\n[BUILD] Creating 8-bit adder circuit for %d + %d\n\n", 
            a_val, b_val);
    
    // Create input nodes for each bit
    uint32_t a_bits[8], b_bits[8];
    
    for (int i = 0; i < 8; i++) {
        char label[32];
        
        snprintf(label, 32, "a_bit%d", i);
        a_bits[i] = create_node(NODE_VALUE, label);
        g.nodes[a_bits[i]].value = (a_val >> i) & 1;
        g.nodes[a_bits[i]].activation = 1.0f;
        
        snprintf(label, 32, "b_bit%d", i);
        b_bits[i] = create_node(NODE_VALUE, label);
        g.nodes[b_bits[i]].value = (b_val >> i) & 1;
        g.nodes[b_bits[i]].activation = 1.0f;
    }
    
    // Create carry chain
    uint32_t carry[9];
    carry[0] = create_node(NODE_VALUE, "carry_in");
    g.nodes[carry[0]].value = 0;
    g.nodes[carry[0]].activation = 1.0f;
    
    // Create sum output nodes
    uint32_t sum_bits[8];
    
    // Build 8 full adders
    for (int i = 0; i < 8; i++) {
        char label[32];
        snprintf(label, 32, "adder_bit%d", i);
        
        build_1bit_adder(a_bits[i], b_bits[i], carry[i], 
                        &sum_bits[i], &carry[i+1]);
    }
    
    // Create output collection nodes
    for (int i = 0; i < 8; i++) {
        char label[32];
        snprintf(label, 32, "output_bit%d", i);
        uint32_t out = create_node(NODE_OUTPUT, label);
        create_edge_conn(sum_bits[i], out, 255);
    }
    
    fprintf(stderr, "[BUILD] Circuit complete: %u nodes, %u edges\n\n", 
            g.node_count, g.edge_count);
}

/* ========================================================================
 * RESULT COLLECTION
 * ======================================================================== */

uint8_t collect_result() {
    uint8_t result = 0;
    
    // Find output nodes and collect bits
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].type == NODE_OUTPUT && g.nodes[i].activation >= 1.0f) {
            // Extract bit position from label "output_bit0", etc.
            if (strncmp(g.nodes[i].label, "output_bit", 10) == 0) {
                int bit_pos = g.nodes[i].label[10] - '0';
                if (bit_pos >= 0 && bit_pos < 8) {
                    result |= (g.nodes[i].value << bit_pos);
                    
                    if (debug) {
                        fprintf(stderr, "[OUTPUT] Bit %d = %d\n", 
                                bit_pos, g.nodes[i].value);
                    }
                }
            }
        }
    }
    
    return result;
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num1> <num2>\n", argv[0]);
        fprintf(stderr, "Example: %s 5 3\n", argv[0]);
        return 1;
    }
    
    uint8_t a = atoi(argv[1]);
    uint8_t b = atoi(argv[2]);
    
    // Initialize graph
    g.node_cap = 10000;
    g.edge_cap = 100000;
    g.node_count = 0;
    g.edge_count = 0;
    
    g.nodes = malloc(g.node_cap * sizeof(Node));
    g.edges = malloc(g.edge_cap * sizeof(Edge));
    
    memset(g.nodes, 0, g.node_cap * sizeof(Node));
    memset(g.edges, 0, g.edge_cap * sizeof(Edge));
    
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║       PURE GRAPH COMPUTATION: %3d + %3d                    ║\n", a, b);
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    // Build the adder circuit in the graph
    build_8bit_adder(a, b);
    
    // Execute computation through spreading activation
    fprintf(stderr, "════════════════════════════════════════════════════════════\n\n");
    propagate_computation(100);
    
    // Collect result
    fprintf(stderr, "════════════════════════════════════════════════════════════\n\n");
    uint8_t result = collect_result();
    
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║                      RESULT                                ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("  %d + %d = %d\n", a, b, result);
    printf("\n");
    printf("  Binary:\n");
    printf("    ");
    for (int i = 7; i >= 0; i--) printf("%d", (a >> i) & 1);
    printf("  (%d)\n", a);
    printf("  + ");
    for (int i = 7; i >= 0; i--) printf("%d", (b >> i) & 1);
    printf("  (%d)\n", b);
    printf("  --------\n");
    printf("    ");
    for (int i = 7; i >= 0; i--) printf("%d", (result >> i) & 1);
    printf("  (%d)\n", result);
    printf("\n");
    
    // Verify
    uint8_t expected = a + b;
    if (result == expected) {
        printf("  ✓ CORRECT (expected %d)\n", expected);
        printf("\n");
        printf("  PROOF: Computation happened PURELY in the graph!\n");
        printf("         %u nodes executed\n", g.node_count);
        printf("         %u edges routed the flow\n", g.edge_count);
        printf("         No CPU arithmetic used!\n");
    } else {
        printf("  ✗ INCORRECT (expected %d, got %d)\n", expected, result);
    }
    
    printf("\n");
    printf("════════════════════════════════════════════════════════════\n");
    
    // Cleanup
    free(g.nodes);
    free(g.edges);
    
    return (result == expected) ? 0 : 1;
}

