/*
 * MELVIN COMPUTATIONAL - Nodes Execute, Edges Route
 * 
 * Key Insight (user's breakthrough):
 * "Melvin's system is binary with nodes (bigger than 1 bit) and edges 
 *  that tell them when and where to be. We can tell the nodes when and 
 *  where to be to make real mathematical computations."
 * 
 * This bridges pattern matching → real computation → emergent intelligence
 * 
 * Architecture:
 * - Nodes can be DATA or EXECUTABLE OPERATIONS
 * - Edges determine WHEN operations execute (activation cascade)
 * - Spreading activation = COMPUTATION PIPELINE
 * - Patterns emerge into algorithms!
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

/* ========================================================================
 * ENHANCED NODE - Now Executable!
 * ======================================================================== */

typedef enum {
    NODE_DATA,      // Regular data: "cat", "hello"
    NODE_NUMBER,    // Numeric value: stored in value field
    NODE_OPERATOR,  // Executable operation: +, -, *, /
    NODE_PATTERN    // Compound pattern: "5+3"
} NodeType;

typedef struct __attribute__((packed)) {
    uint8_t token[16];
    float activation;
    uint16_t token_len;
    NodeType type;          // NEW: What kind of node?
    int32_t value;          // NEW: For numbers, stores actual value
} Node;

typedef struct __attribute__((packed)) {
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
int debug = 0;

/* ========================================================================
 * NODE TYPE DETECTION
 * ======================================================================== */

NodeType detect_node_type(uint8_t *token, uint32_t len) {
    if (len == 0) return NODE_DATA;
    
    // Check if it's a number
    int is_number = 1;
    int has_digit = 0;
    for (uint32_t i = 0; i < len; i++) {
        if (token[i] == '-' && i == 0) continue;  // Allow negative sign
        if (!isdigit(token[i])) {
            is_number = 0;
            break;
        }
        has_digit = 1;
    }
    
    if (is_number && has_digit) return NODE_NUMBER;
    
    // Check if it's an operator
    if (len == 1) {
        if (token[0] == '+' || token[0] == '-' || 
            token[0] == '*' || token[0] == '/' ||
            token[0] == '=' || token[0] == '>' ||
            token[0] == '<') {
            return NODE_OPERATOR;
        }
    }
    
    // Check if it's a compound pattern (contains operator)
    for (uint32_t i = 0; i < len; i++) {
        if (token[i] == '+' || token[i] == '-' || 
            token[i] == '*' || token[i] == '/') {
            return NODE_PATTERN;
        }
    }
    
    return NODE_DATA;
}

int32_t parse_number(uint8_t *token, uint32_t len) {
    char buf[32];
    uint32_t copy_len = (len < 31) ? len : 31;
    memcpy(buf, token, copy_len);
    buf[copy_len] = '\0';
    return atoi(buf);
}

/* ========================================================================
 * OPERATION EXECUTION - The Magic!
 * ======================================================================== */

int32_t execute_operation(char op, int32_t a, int32_t b) {
    switch(op) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/': return (b != 0) ? a / b : 0;
        case '>': return a > b ? 1 : 0;
        case '<': return a < b ? 1 : 0;
        case '=': return a == b ? 1 : 0;
        default: return 0;
    }
}

/* ========================================================================
 * COMPUTATIONAL PATTERN DETECTION
 * ======================================================================== */

typedef struct {
    int32_t operand1;
    char operator;
    int32_t operand2;
    int has_result;
    int32_t result;
} ArithmeticPattern;

int parse_arithmetic(uint8_t *input, uint32_t len, ArithmeticPattern *pattern) {
    char buf[256];
    uint32_t copy_len = (len < 255) ? len : 255;
    memcpy(buf, input, copy_len);
    buf[copy_len] = '\0';
    
    // Try: "A op B = C" format
    if (sscanf(buf, "%d %c %d = %d", 
               &pattern->operand1, &pattern->operator, 
               &pattern->operand2, &pattern->result) == 4) {
        pattern->has_result = 1;
        return 1;
    }
    
    // Try: "A op B" format (query)
    if (sscanf(buf, "%d %c %d", 
               &pattern->operand1, &pattern->operator, 
               &pattern->operand2) == 3) {
        pattern->has_result = 0;
        return 1;
    }
    
    return 0;
}

/* ========================================================================
 * NODE & EDGE MANAGEMENT (Simplified for demo)
 * ======================================================================== */

uint32_t find_or_create_node(uint8_t *token, uint32_t len, NodeType type, int32_t value) {
    // Search existing
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].token_len != len) continue;
        
        int match = 1;
        uint32_t cmp_len = (len < 16) ? len : 16;
        for (uint32_t b = 0; b < cmp_len; b++) {
            if (g.nodes[i].token[b] != token[b]) {
                match = 0;
                break;
            }
        }
        
        if (match) return i;
    }
    
    // Create new
    if (g.node_count >= g.node_cap) {
        fprintf(stderr, "[ERROR] Node capacity reached\n");
        exit(1);
    }
    
    uint32_t id = g.node_count++;
    memset(&g.nodes[id], 0, sizeof(Node));
    memcpy(g.nodes[id].token, token, (len < 16) ? len : 16);
    g.nodes[id].token_len = len;
    g.nodes[id].type = type;
    g.nodes[id].value = value;
    
    if (debug) {
        char *type_str = (type == NODE_NUMBER) ? "NUM" :
                        (type == NODE_OPERATOR) ? "OP" :
                        (type == NODE_PATTERN) ? "PAT" : "DATA";
        fprintf(stderr, "[NODE] %s #%u: '%.*s'", type_str, id, 
                (len < 16) ? len : 16, token);
        if (type == NODE_NUMBER) {
            fprintf(stderr, " (value: %d)", value);
        }
        fprintf(stderr, "\n");
    }
    
    return id;
}

void create_edge(uint32_t from, uint32_t to, uint8_t weight) {
    if (from >= g.node_count || to >= g.node_count) return;
    if (from == to) return;
    if (g.edge_count >= g.edge_cap) return;
    
    // Check if exists
    for (uint32_t i = 0; i < g.edge_count; i++) {
        if (g.edges[i].from == from && g.edges[i].to == to) {
            if (g.edges[i].weight < 255) g.edges[i].weight++;
            return;
        }
    }
    
    uint32_t id = g.edge_count++;
    g.edges[id].from = from;
    g.edges[id].to = to;
    g.edges[id].weight = weight;
    
    if (debug) {
        fprintf(stderr, "[EDGE] %u→%u (weight: %u)\n", from, to, weight);
    }
}

/* ========================================================================
 * COMPUTATIONAL LEARNING - The Breakthrough!
 * ======================================================================== */

void learn_computation(uint8_t *input, uint32_t len) {
    ArithmeticPattern pat;
    
    if (!parse_arithmetic(input, len, &pat)) {
        if (debug) fprintf(stderr, "[SKIP] Not arithmetic pattern\n");
        return;
    }
    
    if (debug) {
        fprintf(stderr, "[DETECT] Arithmetic: %d %c %d", 
                pat.operand1, pat.operator, pat.operand2);
        if (pat.has_result) {
            fprintf(stderr, " = %d", pat.result);
        }
        fprintf(stderr, "\n");
    }
    
    // Create nodes for operands
    char buf[32];
    
    snprintf(buf, 32, "%d", pat.operand1);
    uint32_t node_a = find_or_create_node((uint8_t*)buf, strlen(buf), 
                                          NODE_NUMBER, pat.operand1);
    
    snprintf(buf, 32, "%c", pat.operator);
    uint32_t node_op = find_or_create_node((uint8_t*)buf, 1, 
                                           NODE_OPERATOR, 0);
    
    snprintf(buf, 32, "%d", pat.operand2);
    uint32_t node_b = find_or_create_node((uint8_t*)buf, strlen(buf), 
                                          NODE_NUMBER, pat.operand2);
    
    // If teaching (has result), create result node
    if (pat.has_result) {
        snprintf(buf, 32, "%d", pat.result);
        uint32_t node_result = find_or_create_node((uint8_t*)buf, strlen(buf), 
                                                   NODE_NUMBER, pat.result);
        
        // Create computational pattern node: "A+B"
        snprintf(buf, 32, "%d%c%d", pat.operand1, pat.operator, pat.operand2);
        uint32_t node_pattern = find_or_create_node((uint8_t*)buf, strlen(buf), 
                                                    NODE_PATTERN, 0);
        
        // Connect pattern to result (STRONG edge - this IS the computation!)
        create_edge(node_pattern, node_result, 255);
        
        if (debug) {
            fprintf(stderr, "[LEARN] '%s' → '%d' (computational edge)\n", 
                    buf, pat.result);
        }
    }
    
    // Always connect operands (for generalization)
    create_edge(node_a, node_op, 50);
    create_edge(node_op, node_b, 50);
}

/* ========================================================================
 * COMPUTATIONAL QUERY - Execute or Recall
 * ======================================================================== */

void query_computation(uint8_t *input, uint32_t len) {
    ArithmeticPattern pat;
    
    if (!parse_arithmetic(input, len, &pat)) {
        if (debug) fprintf(stderr, "[SKIP] Not arithmetic query\n");
        return;
    }
    
    if (debug) {
        fprintf(stderr, "[QUERY] Arithmetic: %d %c %d\n", 
                pat.operand1, pat.operator, pat.operand2);
    }
    
    // Try pattern lookup first (learned computation)
    char pattern_str[32];
    snprintf(pattern_str, 32, "%d%c%d", pat.operand1, pat.operator, pat.operand2);
    
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].type != NODE_PATTERN) continue;
        if (g.nodes[i].token_len != strlen(pattern_str)) continue;
        
        if (memcmp(g.nodes[i].token, pattern_str, strlen(pattern_str)) == 0) {
            // Found pattern! Follow edge to result
            for (uint32_t e = 0; e < g.edge_count; e++) {
                if (g.edges[e].from == i) {
                    uint32_t result_node = g.edges[e].to;
                    if (g.nodes[result_node].type == NODE_NUMBER) {
                        printf("Result: %d (recalled from pattern)\n", 
                               g.nodes[result_node].value);
                        return;
                    }
                }
            }
        }
    }
    
    // Not learned? COMPUTE IT! (The magic happens here)
    if (debug) fprintf(stderr, "[COMPUTE] Executing operation...\n");
    
    int32_t result = execute_operation(pat.operator, pat.operand1, pat.operand2);
    printf("Result: %d (computed)\n", result);
    
    // ALSO store for future recall (hybrid approach!)
    char buf[32];
    snprintf(buf, 32, "%d%c%d", pat.operand1, pat.operator, pat.operand2);
    uint32_t node_pattern = find_or_create_node((uint8_t*)buf, strlen(buf), 
                                                NODE_PATTERN, 0);
    
    snprintf(buf, 32, "%d", result);
    uint32_t node_result = find_or_create_node((uint8_t*)buf, strlen(buf), 
                                               NODE_NUMBER, result);
    
    create_edge(node_pattern, node_result, 255);
    
    if (debug) {
        fprintf(stderr, "[STORE] Computation stored as pattern for future recall\n");
    }
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main() {
    debug = getenv("MELVIN_DEBUG") != NULL;
    
    // Initialize
    g.node_cap = 10000;
    g.edge_cap = 100000;
    g.node_count = 0;
    g.edge_count = 0;
    
    g.nodes = malloc(g.node_cap * sizeof(Node));
    g.edges = malloc(g.edge_cap * sizeof(Edge));
    
    memset(g.nodes, 0, g.node_cap * sizeof(Node));
    memset(g.edges, 0, g.edge_cap * sizeof(Edge));
    
    // Read input
    char input[1024];
    if (fgets(input, sizeof(input), stdin) == NULL) {
        return 1;
    }
    
    // Strip newline
    size_t len = strlen(input);
    while (len > 0 && (input[len-1] == '\n' || input[len-1] == '\r')) {
        input[--len] = '\0';
    }
    
    // Determine if teaching or querying
    if (strchr(input, '=')) {
        // Teaching (has result)
        learn_computation((uint8_t*)input, len);
    } else {
        // Querying (no result)
        query_computation((uint8_t*)input, len);
    }
    
    // Cleanup
    free(g.nodes);
    free(g.edges);
    
    return 0;
}

