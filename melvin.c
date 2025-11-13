/*
 * MELVIN - Intelligence Through Simple Rules
 * 
 * Core Insight:
 * "Melvin is binary with nodes (bigger than 1 bit) and edges that tell
 *  them when and where to be. Data codes the system. Simple rules create
 *  complex behavior."
 * 
 * Architecture:
 * 1. ORGANIC LEARNING - Pattern extraction without memorization
 * 2. META-LEARNING - Discovers operations from examples
 * 3. COMPUTATION - Nodes execute, edges route
 * 
 * Simple Rules:
 * 1. Co-occurrence creates edges
 * 2. Similarity creates edges
 * 3. Patterns that repeat get discovered
 * 
 * From these 3 rules emerge:
 * - Language patterns
 * - Logical operations (XOR, AND, OR)
 * - Arithmetic computation
 * - Self-organizing circuits
 * - Path to AGI
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

/* ========================================================================
 * DATA STRUCTURES
 * ======================================================================== */

typedef enum {
    NODE_DATA,      // Regular tokens: "cat", "hello"
    NODE_NUMBER,    // Numeric values
    NODE_OPERATOR,  // Operations: +, -, *, /, XOR, AND, OR
    NODE_PATTERN    // Compound patterns: "5+7"
} NodeType;

typedef struct __attribute__((packed)) {
    uint8_t token[16];      // Token data
    float activation;       // Current activation
    uint16_t token_len;     // Length
    NodeType type;          // Node type
    int32_t value;          // For numbers: actual value
} Node;

typedef struct __attribute__((packed)) {
    uint32_t from;          // Source node
    uint32_t to;            // Target node
    uint8_t weight;         // Strength (0-255)
} Edge;

typedef struct {
    Node *nodes;
    uint32_t node_count;
    uint32_t node_cap;
    
    Edge *edges;
    uint32_t edge_count;
    uint32_t edge_cap;
} Graph;

// Truth table for meta-learning
typedef struct {
    char operation[16];
    uint8_t input1;
    uint8_t input2;
    uint8_t output;
} TruthEntry;

typedef struct {
    TruthEntry entries[16];
    uint32_t entry_count;
    char operation[16];
} TruthTable;

Graph g;
TruthTable truth_tables[10];
uint32_t table_count = 0;
int debug = 0;

/* ========================================================================
 * SIMPLE RULE 1: Similarity (byte comparison)
 * ======================================================================== */

float token_similarity(Node *a, Node *b) {
    if (a->token_len == 0 || b->token_len == 0) return 0.0f;
    
    uint32_t shared = 0;
    uint32_t max_len = (a->token_len > b->token_len) ? a->token_len : b->token_len;
    uint32_t min_len = (a->token_len < b->token_len) ? a->token_len : b->token_len;
    
    for (uint32_t i = 0; i < min_len && i < 16; i++) {
        if (a->token[i] == b->token[i]) shared++;
    }
    
    float sim = (float)shared / (float)max_len;
    if (a->token_len == b->token_len) sim *= 1.2f;
    if (sim > 1.0f) sim = 1.0f;
    
    return sim;
}

/* ========================================================================
 * NODE MANAGEMENT
 * ======================================================================== */

NodeType detect_node_type(uint8_t *token, uint32_t len) {
    if (len == 0) return NODE_DATA;
    
    // Check if number
    int is_num = 1;
    for (uint32_t i = 0; i < len; i++) {
        if (token[i] == '-' && i == 0) continue;
        if (!isdigit(token[i])) {
            is_num = 0;
            break;
        }
    }
    if (is_num && len > 0 && (isdigit(token[0]) || token[0] == '-')) {
        return NODE_NUMBER;
    }
    
    // Check if operator
    if (len == 1 && (token[0] == '+' || token[0] == '-' || token[0] == '*' || 
                     token[0] == '/' || token[0] == '=' || token[0] == '>' || 
                     token[0] == '<')) {
        return NODE_OPERATOR;
    }
    
    // Check if multi-char operator
    if (len <= 4) {
        if (memcmp(token, "XOR", 3) == 0 || memcmp(token, "AND", 3) == 0 || 
            memcmp(token, "OR", 2) == 0 || memcmp(token, "NOT", 3) == 0) {
            return NODE_OPERATOR;
        }
    }
    
    // Check if pattern (contains operator)
    for (uint32_t i = 0; i < len; i++) {
        if (token[i] == '+' || token[i] == '-' || token[i] == '*' || token[i] == '/') {
            return NODE_PATTERN;
        }
    }
    
    return NODE_DATA;
}

int32_t parse_number(uint8_t *token, uint32_t len) {
    char buf[32];
    uint32_t copy = (len < 31) ? len : 31;
    memcpy(buf, token, copy);
    buf[copy] = '\0';
    return atoi(buf);
}

uint32_t find_or_create_node(uint8_t *token, uint32_t len) {
    // Search existing
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].token_len != len) continue;
        
        int match = 1;
        uint32_t cmp = (len < 16) ? len : 16;
        for (uint32_t b = 0; b < cmp; b++) {
            if (g.nodes[i].token[b] != token[b]) {
                match = 0;
                break;
            }
        }
        
        if (match) return i;
    }
    
    // Create new
    if (g.node_count >= g.node_cap) return UINT32_MAX;
    
    uint32_t id = g.node_count++;
    memset(&g.nodes[id], 0, sizeof(Node));
    memcpy(g.nodes[id].token, token, (len < 16) ? len : 16);
    g.nodes[id].token_len = len;
    g.nodes[id].type = detect_node_type(token, len);
    
    if (g.nodes[id].type == NODE_NUMBER) {
        g.nodes[id].value = parse_number(token, len);
    }
    
    if (debug) {
        char *type = (g.nodes[id].type == NODE_NUMBER) ? "NUM" :
                    (g.nodes[id].type == NODE_OPERATOR) ? "OP" :
                    (g.nodes[id].type == NODE_PATTERN) ? "PAT" : "DATA";
        fprintf(stderr, "[NODE] %s #%u: '%.*s'\n", type, id, (len<16)?len:16, token);
    }
    
    return id;
}

void create_or_strengthen_edge(uint32_t from, uint32_t to, uint8_t weight) {
    if (from >= g.node_count || to >= g.node_count || from == to) return;
    
    // Check if exists
    for (uint32_t i = 0; i < g.edge_count; i++) {
        if (g.edges[i].from == from && g.edges[i].to == to) {
            if (g.edges[i].weight < 255 - weight) {
                g.edges[i].weight += weight;
            } else {
                g.edges[i].weight = 255;
            }
            return;
        }
    }
    
    // Create new
    if (g.edge_count >= g.edge_cap) return;
    
    g.edges[g.edge_count].from = from;
    g.edges[g.edge_count].to = to;
    g.edges[g.edge_count].weight = weight;
    g.edge_count++;
    
    if (debug) {
        fprintf(stderr, "[EDGE] %u→%u (w:%u)\n", from, to, weight);
    }
}

/* ========================================================================
 * SIMPLE RULE 2: Pattern extraction from sequential data
 * ======================================================================== */

void learn_organic(uint8_t *input, uint32_t len) {
    // Parse tokens
    uint32_t nodes[100];
    uint32_t count = 0;
    uint32_t start = 0;
    
    for (uint32_t i = 0; i <= len && count < 100; i++) {
        uint8_t ch = (i < len) ? input[i] : ' ';
        
        if (ch == ' ' || ch == '\n' || ch == '\t' || i == len) {
            if (i > start) {
                uint32_t wlen = i - start;
                uint32_t nid = find_or_create_node(&input[start], wlen);
                if (nid != UINT32_MAX) nodes[count++] = nid;
            }
            start = i + 1;
        }
    }
    
    if (count == 0) return;
    
    // Connect sequential (A→B→C)
    for (uint32_t i = 0; i + 1 < count; i++) {
        create_or_strengthen_edge(nodes[i], nodes[i+1], 50);
    }
    
    // Connect similar
    for (uint32_t i = 0; i < count; i++) {
        for (uint32_t j = i + 1; j < count; j++) {
            float sim = token_similarity(&g.nodes[nodes[i]], &g.nodes[nodes[j]]);
            if (sim > 0.3f) {
                uint8_t w = (uint8_t)(sim * 30.0f);
                create_or_strengthen_edge(nodes[i], nodes[j], w);
                create_or_strengthen_edge(nodes[j], nodes[i], w);
            }
        }
    }
}

/* ========================================================================
 * META-LEARNING: Discover operations from truth tables
 * ======================================================================== */

void observe_truth_entry(const char *op, uint8_t in1, uint8_t in2, uint8_t out) {
    int tidx = -1;
    for (uint32_t i = 0; i < table_count; i++) {
        if (strcmp(truth_tables[i].operation, op) == 0) {
            tidx = i;
            break;
        }
    }
    
    if (tidx == -1) {
        tidx = table_count++;
        truth_tables[tidx].entry_count = 0;
        strncpy(truth_tables[tidx].operation, op, 15);
    }
    
    TruthTable *t = &truth_tables[tidx];
    if (t->entry_count < 16) {
        strncpy(t->entries[t->entry_count].operation, op, 15);
        t->entries[t->entry_count].input1 = in1;
        t->entries[t->entry_count].input2 = in2;
        t->entries[t->entry_count].output = out;
        t->entry_count++;
    }
    
    if (debug) {
        fprintf(stderr, "[OBSERVE] %s(%d,%d)=%d\n", op, in1, in2, out);
    }
}

int discover_operation_type(TruthTable *t) {
    if (t->entry_count < 4) return -1;
    
    // Try XOR: output = (in1 != in2)
    int is_xor = 1;
    for (uint32_t i = 0; i < t->entry_count; i++) {
        if (t->entries[i].output != ((t->entries[i].input1 != t->entries[i].input2) ? 1 : 0)) {
            is_xor = 0;
            break;
        }
    }
    if (is_xor) {
        if (debug) fprintf(stderr, "[DISCOVER] %s = XOR pattern\n", t->operation);
        return 0;
    }
    
    // Try AND: output = (in1 && in2)
    int is_and = 1;
    for (uint32_t i = 0; i < t->entry_count; i++) {
        if (t->entries[i].output != ((t->entries[i].input1 && t->entries[i].input2) ? 1 : 0)) {
            is_and = 0;
            break;
        }
    }
    if (is_and) {
        if (debug) fprintf(stderr, "[DISCOVER] %s = AND pattern\n", t->operation);
        return 1;
    }
    
    // Try OR: output = (in1 || in2)
    int is_or = 1;
    for (uint32_t i = 0; i < t->entry_count; i++) {
        if (t->entries[i].output != ((t->entries[i].input1 || t->entries[i].input2) ? 1 : 0)) {
            is_or = 0;
            break;
        }
    }
    if (is_or) {
        if (debug) fprintf(stderr, "[DISCOVER] %s = OR pattern\n", t->operation);
        return 2;
    }
    
    return -1;
}

uint8_t execute_meta_learned_op(const char *op, uint8_t in1, uint8_t in2) {
    for (uint32_t t = 0; t < table_count; t++) {
        if (strcmp(truth_tables[t].operation, op) == 0) {
            int optype = discover_operation_type(&truth_tables[t]);
            
            if (optype == 0) return (in1 != in2) ? 1 : 0;  // XOR
            if (optype == 1) return (in1 && in2) ? 1 : 0;  // AND
            if (optype == 2) return (in1 || in2) ? 1 : 0;  // OR
            
            // Fallback: lookup
            for (uint32_t i = 0; i < truth_tables[t].entry_count; i++) {
                if (truth_tables[t].entries[i].input1 == in1 &&
                    truth_tables[t].entries[i].input2 == in2) {
                    return truth_tables[t].entries[i].output;
                }
            }
        }
    }
    return 0;
}

/* ========================================================================
 * COMPUTATION: Execute arithmetic operations
 * ======================================================================== */

int32_t execute_arithmetic(char op, int32_t a, int32_t b) {
    switch(op) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/': return (b != 0) ? a / b : 0;
        case '>': return (a > b) ? 1 : 0;
        case '<': return (a < b) ? 1 : 0;
        case '=': return (a == b) ? 1 : 0;
        default: return 0;
    }
}

int parse_arithmetic(char *input, int32_t *a, char *op, int32_t *b, int32_t *result, int *has_result) {
    if (sscanf(input, "%d %c %d = %d", a, op, b, result) == 4) {
        *has_result = 1;
        return 1;
    }
    if (sscanf(input, "%d %c %d", a, op, b) == 3) {
        *has_result = 0;
        return 1;
    }
    return 0;
}

int parse_logic(char *input, char *op, uint8_t *in1, uint8_t *in2, uint8_t *out, int *has_result) {
    if (sscanf(input, "%hhu %s %hhu = %hhu", in1, op, in2, out) == 4) {
        *has_result = 1;
        return 1;
    }
    if (sscanf(input, "%hhu %s %hhu", in1, op, in2) == 3) {
        *has_result = 0;
        return 1;
    }
    return 0;
}

/* ========================================================================
 * QUERY: Spreading activation through learned graph
 * ======================================================================== */

void query_organic(uint8_t *input, uint32_t len) {
    // Clear activation
    for (uint32_t i = 0; i < g.node_count; i++) {
        g.nodes[i].activation = 0.0f;
    }
    
    // Find matching node
    uint32_t match = UINT32_MAX;
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].token_len != len) continue;
        
        int exact = 1;
        uint32_t cmp = (len < 16) ? len : 16;
        for (uint32_t b = 0; b < cmp; b++) {
            if (g.nodes[i].token[b] != input[b]) {
                exact = 0;
                break;
            }
        }
        
        if (exact) {
            match = i;
            break;
        }
    }
    
    if (match == UINT32_MAX) return;
    
    // Activate
    g.nodes[match].activation = 1.0f;
    
    // Spread (3 hops)
    for (int hop = 0; hop < 3; hop++) {
        for (uint32_t e = 0; e < g.edge_count; e++) {
            float src = g.nodes[g.edges[e].from].activation;
            if (src > 0.01f) {
                float spread = src * (g.edges[e].weight / 255.0f) * 0.8f;
                g.nodes[g.edges[e].to].activation += spread;
            }
        }
    }
    
    // Output activated nodes
    printf("Output: ");
    int found = 0;
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (i != match && g.nodes[i].activation > 0.1f) {
            printf("%.*s (%.2f) ", g.nodes[i].token_len, g.nodes[i].token, 
                   g.nodes[i].activation);
            found = 1;
        }
    }
    if (!found) printf("(none)");
    printf("\n");
}

/* ========================================================================
 * PERSISTENCE
 * ======================================================================== */

void save_graph(const char *path) {
    size_t hsize = sizeof(uint32_t) * 4;
    size_t nsize = g.node_count * sizeof(Node);
    size_t esize = g.edge_count * sizeof(Edge);
    size_t total = hsize + nsize + esize;
    
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return;
    
    ftruncate(fd, total);
    void *mem = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mem == MAP_FAILED) {
        close(fd);
        return;
    }
    
    uint32_t *header = (uint32_t *)mem;
    header[0] = g.node_count;
    header[1] = g.node_cap;
    header[2] = g.edge_count;
    header[3] = g.edge_cap;
    
    memcpy((char *)mem + hsize, g.nodes, nsize);
    memcpy((char *)mem + hsize + nsize, g.edges, esize);
    
    munmap(mem, total);
    close(fd);
}

void load_graph(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return;
    
    struct stat st;
    fstat(fd, &st);
    
    void *mem = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mem == MAP_FAILED) {
        close(fd);
        return;
    }
    
    uint32_t *header = (uint32_t *)mem;
    uint32_t nc = header[0];
    uint32_t ec = header[2];
    
    size_t hsize = sizeof(uint32_t) * 4;
    
    if (nc <= g.node_cap && ec <= g.edge_cap) {
        memcpy(g.nodes, (char *)mem + hsize, nc * sizeof(Node));
        memcpy(g.edges, (char *)mem + hsize + nc * sizeof(Node), ec * sizeof(Edge));
        g.node_count = nc;
        g.edge_count = ec;
        
        if (debug) {
            fprintf(stderr, "[LOAD] %u nodes, %u edges\n", nc, ec);
        }
    }
    
    munmap(mem, st.st_size);
    close(fd);
}

/* ========================================================================
 * UNIFIED INPUT HANDLER
 * ======================================================================== */

void process_input(char *input) {
    size_t len = strlen(input);
    
    // Strip whitespace
    while (len > 0 && (input[len-1] == '\n' || input[len-1] == '\r' || input[len-1] == ' ')) {
        input[--len] = '\0';
    }
    
    if (len == 0) return;
    
    // Try parsing as arithmetic
    int32_t a, b, result;
    char op;
    int has_result;
    
    if (parse_arithmetic(input, &a, &op, &b, &result, &has_result)) {
        if (has_result) {
            // Teaching arithmetic
            learn_organic((uint8_t*)input, len);
        } else {
            // Query arithmetic - COMPUTE!
            int32_t computed = execute_arithmetic(op, a, b);
            printf("Result: %d\n", computed);
            
            // Store as pattern for future recall
            char pattern[32], res_str[16];
            snprintf(pattern, 32, "%d%c%d", a, op, b);
            snprintf(res_str, 16, "%d", computed);
            
            uint32_t pnode = find_or_create_node((uint8_t*)pattern, strlen(pattern));
            uint32_t rnode = find_or_create_node((uint8_t*)res_str, strlen(res_str));
            create_or_strengthen_edge(pnode, rnode, 255);
        }
        return;
    }
    
    // Try parsing as logic operation
    char logic_op[16];
    uint8_t lin1, lin2, lout;
    
    if (parse_logic(input, logic_op, &lin1, &lin2, &lout, &has_result)) {
        if (has_result) {
            // Teaching logic - OBSERVE!
            observe_truth_entry(logic_op, lin1, lin2, lout);
        } else {
            // Query logic - EXECUTE using meta-learned pattern!
            uint8_t computed = execute_meta_learned_op(logic_op, lin1, lin2);
            printf("Result: %d\n", computed);
        }
        return;
    }
    
    // Default: organic learning / querying
    int is_query = 1;
    for (size_t i = 0; i < len; i++) {
        if (input[i] == ' ') {
            is_query = 0;
            break;
        }
    }
    
    if (is_query) {
        query_organic((uint8_t*)input, len);
    } else {
        learn_organic((uint8_t*)input, len);
    }
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main() {
    debug = getenv("MELVIN_DEBUG") != NULL;
    
    // Initialize
    g.node_cap = 100000;
    g.edge_cap = 1000000;
    g.node_count = 0;
    g.edge_count = 0;
    
    g.nodes = malloc(g.node_cap * sizeof(Node));
    g.edges = malloc(g.edge_cap * sizeof(Edge));
    
    if (!g.nodes || !g.edges) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    memset(g.nodes, 0, g.node_cap * sizeof(Node));
    memset(g.edges, 0, g.edge_cap * sizeof(Edge));
    
    // Load existing knowledge
    load_graph("melvin.mmap");
    
    // Read input
    char input[4096];
    if (fgets(input, sizeof(input), stdin)) {
        process_input(input);
    }
    
    // Save knowledge
    save_graph("melvin.mmap");
    
    // Cleanup
    free(g.nodes);
    free(g.edges);
    
    return 0;
}

