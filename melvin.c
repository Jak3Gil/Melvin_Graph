/*
 * MELVIN - Pure Graph Structure
 * 
 * ONE node type. ONE edge type. Behavior emerges from structure.
 * No hardcoded types. No special cases. Just data and connections.
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
#include <time.h>

/* ONE NODE TYPE */
typedef struct __attribute__((packed)) {
    uint8_t token[16];
    uint16_t token_len;
    int32_t value;
} Node;

/* ONE EDGE TYPE */
typedef struct __attribute__((packed)) {
    uint32_t from;
    uint32_t to;
    uint8_t weight;
} Edge;

/* THE GRAPH */
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
int last_route_count = 0;  // Track routing success

/* Forward declarations */
uint32_t find_or_create_node(uint8_t *token, uint32_t len);
void create_edge(uint32_t from, uint32_t to, uint8_t weight);
void delete_edge(uint32_t from, uint32_t to);

/* Find or create node */
uint32_t find_or_create_node(uint8_t *token, uint32_t len) {
    // Find existing
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].token_len != len) continue;
        
        int match = 1;
        for (uint32_t b = 0; b < len && b < 16; b++) {
            if (g.nodes[i].token[b] != token[b]) { match = 0; break; }
        }
        if (match) return i;
    }
    
    // Create new
    if (g.node_count >= g.node_cap) return UINT32_MAX;
    
    uint32_t id = g.node_count++;
    memset(&g.nodes[id], 0, sizeof(Node));
    memcpy(g.nodes[id].token, token, (len < 16) ? len : 16);
    g.nodes[id].token_len = len;
    
    // If it's a number, store value AND create bit decomposition
    int is_num = 1;
    for (uint32_t i = 0; i < len; i++) {
        if (!isdigit(token[i]) && token[i] != '-') { is_num = 0; break; }
    }
    if (is_num && len > 0) {
        char buf[32];
        memcpy(buf, token, len);
        buf[len] = '\0';
        g.nodes[id].value = atoi(buf);
        
        // Create bit nodes and connect them
        if (g.nodes[id].value >= 0 && g.nodes[id].value < 256) {
            for (int bit = 0; bit < 8; bit++) {
                int bit_val = (g.nodes[id].value >> bit) & 1;
                char bit_name[16];
                snprintf(bit_name, 16, "%d_bit%d", g.nodes[id].value, bit);
                
                uint32_t bit_node = find_or_create_node((uint8_t*)bit_name, strlen(bit_name));
                if (bit_node != UINT32_MAX) {
                    g.nodes[bit_node].value = bit_val;
                    create_edge(id, bit_node, 200);  // Number connects to its bits
                    
                    if (debug) {
                        fprintf(stderr, "[DECOMPOSE] %d → bit%d=%d\n", 
                                g.nodes[id].value, bit, bit_val);
                    }
                }
            }
        }
    }
    
    return id;
}

/* Create edge */
void create_edge(uint32_t from, uint32_t to, uint8_t weight) {
    if (from >= g.node_count || to >= g.node_count || from == to) return;
    
    // Check if exists
    for (uint32_t i = 0; i < g.edge_count; i++) {
        if (g.edges[i].from == from && g.edges[i].to == to) {
            if (g.edges[i].weight < 245) g.edges[i].weight += 10;
            return;
        }
    }
    
    // Create new
    if (g.edge_count >= g.edge_cap) return;
    
    g.edges[g.edge_count].from = from;
    g.edges[g.edge_count].to = to;
    g.edges[g.edge_count].weight = weight;
    g.edge_count++;
}

/* Delete edge */
void delete_edge(uint32_t from, uint32_t to) {
    for (uint32_t i = 0; i < g.edge_count; i++) {
        if (g.edges[i].from == from && g.edges[i].to == to) {
            // Swap with last edge and decrease count
            g.edges[i] = g.edges[g.edge_count - 1];
            g.edge_count--;
            return;
        }
    }
}

/* Evolve: random mutations toward intelligence */
void evolve() {
    if (g.node_count <= 20) return;  // Need user nodes
    
    // Pick random nodes (skip core circuits 0-19)
    uint32_t range = g.node_count - 20;
    uint32_t a = 20 + (rand() % range);
    uint32_t b = 20 + (rand() % range);
    
    if (a == b || a >= g.node_count || b >= g.node_count) return;
    
    // Check if edge exists
    for (uint32_t i = 0; i < g.edge_count; i++) {
        if (g.edges[i].from == a && g.edges[i].to == b) return;
    }
    
    // Create random edge (mutation!)
    create_edge(a, b, 30);
    
    if (debug) {
        fprintf(stderr, "[EVOLVE] %u→%u (\"%.*s\"→\"%.*s\") edges=%u\n",
                a, b, g.nodes[a].token_len, g.nodes[a].token,
                g.nodes[b].token_len, g.nodes[b].token, g.edge_count);
    }
}

/* Create initial circuits in graph */
void init_circuits() {
    // Bit addition circuit
    const char *patterns[][2] = {
        {"0+0+0", "0,0"}, {"0+1+0", "1,0"}, {"1+0+0", "1,0"}, {"1+1+0", "0,1"},
        {"0+0+1", "1,0"}, {"0+1+1", "0,1"}, {"1+0+1", "0,1"}, {"1+1+1", "1,1"},
    };
    
    // Create pattern nodes and result edges
    for (int i = 0; i < 8; i++) {
        uint32_t p = find_or_create_node((uint8_t*)patterns[i][0], strlen(patterns[i][0]));
        uint32_t r = find_or_create_node((uint8_t*)patterns[i][1], strlen(patterns[i][1]));
        if (p != UINT32_MAX && r != UINT32_MAX) create_edge(p, r, 255);
    }
    
    // Build routing tree (route to patterns via edges, not scan)
    // Create bit value nodes: 0, 1
    uint32_t bit0 = find_or_create_node((uint8_t*)"0", 1);
    uint32_t bit1 = find_or_create_node((uint8_t*)"1", 1);
    
    // Create routing structure
    for (int i = 0; i < 8; i++) {
        uint32_t pattern = find_or_create_node((uint8_t*)patterns[i][0], strlen(patterns[i][0]));
        
        // Route based on pattern structure
        // E.g., "1+1+0" can be reached by: 1 → 1 → 0 path
        // This creates a decision tree in the graph!
        
        // For now, create direct edges from bit combinations
        char key[4];
        snprintf(key, 4, "%c%c%c", patterns[i][0][0], patterns[i][0][2], patterns[i][0][4]);
        uint32_t routing_key = find_or_create_node((uint8_t*)key, 3);
        if (routing_key != UINT32_MAX && pattern != UINT32_MAX) {
            create_edge(routing_key, pattern, 250);  // Route to full pattern
        }
    }
}

/* Route through graph - unified (no learn vs execute distinction) */
void route(uint8_t *input, uint32_t len) {
    uint32_t node_ids[100];
    uint32_t count = 0;
    uint32_t start = 0;
    
    // Tokenize
    for (uint32_t i = 0; i <= len && count < 100; i++) {
        uint8_t ch = (i < len) ? input[i] : ' ';
        
        if (ch == ' ' || ch == '\n' || ch == '\t' || i == len) {
            if (i > start) {
                uint32_t nid = find_or_create_node(&input[start], i - start);
                if (nid != UINT32_MAX) node_ids[count++] = nid;
            }
            start = i + 1;
        }
    }
    
    if (count == 0) return;
    
    // Create edges from sequence (if new nodes appeared together)
    for (uint32_t i = 0; i + 1 < count; i++) {
        create_edge(node_ids[i], node_ids[i+1], 100);
    }
    
    // Check for arithmetic patterns
    if (count == 3 && 
        g.nodes[node_ids[0]].value != 0 &&
        g.nodes[node_ids[2]].value != 0 &&
        g.nodes[node_ids[1]].token_len == 1) {
        
        uint32_t num_a = node_ids[0];
        uint32_t num_b = node_ids[2];
        uint32_t op_node = node_ids[1];
        char op = g.nodes[op_node].token[0];
        int result = 0;
        
        if (op == '+') {
            // ADDITION: Route through bit patterns
            if (debug) fprintf(stderr, "[ADD] %d + %d\n", g.nodes[num_a].value, g.nodes[num_b].value);
            
            int carry = 0;
            for (int bit = 0; bit < 32; bit++) {  // Support 32-bit now
                int bit_a = (g.nodes[num_a].value >> bit) & 1;
                int bit_b = (g.nodes[num_b].value >> bit) & 1;
                
                // Route to pattern via edges (like neurons!)
                char key[4];
                snprintf(key, 4, "%d%d%d", bit_a, bit_b, carry);
                
                // Find routing key node
                uint32_t routing_node = UINT32_MAX;
                for (uint32_t p = 0; p < g.node_count; p++) {
                    if (g.nodes[p].token_len == 3 &&
                        g.nodes[p].token[0] == key[0] &&
                        g.nodes[p].token[1] == key[1] &&
                        g.nodes[p].token[2] == key[2]) {
                        routing_node = p;
                        break;
                    }
                }
                
                // Follow routing edge to pattern
                if (routing_node != UINT32_MAX) {
                    for (uint32_t e = 0; e < g.edge_count; e++) {
                        if (g.edges[e].from == routing_node && g.edges[e].weight == 250) {
                            uint32_t pattern_node = g.edges[e].to;
                            
                            // Follow pattern edge to result
                            for (uint32_t e2 = 0; e2 < g.edge_count; e2++) {
                                if (g.edges[e2].from == pattern_node && g.edges[e2].weight == 255) {
                                    int sum = g.nodes[g.edges[e2].to].token[0] - '0';
                                    carry = g.nodes[g.edges[e2].to].token[2] - '0';
                                    result |= (sum << bit);
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
            }
        } 
        else if (op == '*') {
            // MULTIPLICATION: Loop through addition circuit
            if (debug) fprintf(stderr, "[MULTIPLY] %d * %d (via addition loops)\n", 
                    g.nodes[num_a].value, g.nodes[num_b].value);
            
            int accumulator = 0;
            int multiplier = g.nodes[num_b].value;
            int multiplicand = g.nodes[num_a].value;
            
            // For each bit of multiplier, add shifted multiplicand
            for (int bit = 0; bit < 32; bit++) {
                if ((multiplier >> bit) & 1) {
                    int shifted = multiplicand << bit;
                    
                    // Route through addition circuit
                    int carry = 0;
                    for (int b = 0; b < 32; b++) {
                        int bit_acc = (accumulator >> b) & 1;
                        int bit_shift = (shifted >> b) & 1;
                        
                        // Route to pattern (like neurons)
                        char key[4];
                        snprintf(key, 4, "%d%d%d", bit_acc, bit_shift, carry);
                        
                        uint32_t routing_node = UINT32_MAX;
                        for (uint32_t p = 0; p < g.node_count; p++) {
                            if (g.nodes[p].token_len == 3 &&
                                g.nodes[p].token[0] == key[0] &&
                                g.nodes[p].token[1] == key[1] &&
                                g.nodes[p].token[2] == key[2]) {
                                routing_node = p;
                                break;
                            }
                        }
                        
                        if (routing_node != UINT32_MAX) {
                            for (uint32_t e = 0; e < g.edge_count; e++) {
                                if (g.edges[e].from == routing_node && g.edges[e].weight == 250) {
                                    uint32_t pattern_node = g.edges[e].to;
                                    
                                    for (uint32_t e2 = 0; e2 < g.edge_count; e2++) {
                                        if (g.edges[e2].from == pattern_node && g.edges[e2].weight == 255) {
                                            int sum = g.nodes[g.edges[e2].to].token[0] - '0';
                                            carry = g.nodes[g.edges[e2].to].token[2] - '0';
                                            
                                            if (sum) accumulator |= (1 << b);
                                            else accumulator &= ~(1 << b);
                                            
                                            break;
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            result = accumulator;
        }
        
        // Create result node and connect
        if (result != 0 || (op == '+' || op == '*')) {
            char res_str[16];
            snprintf(res_str, 16, "%d", result);
            uint32_t result_node = find_or_create_node((uint8_t*)res_str, strlen(res_str));
            
            if (result_node != UINT32_MAX) {
                create_edge(op_node, result_node, 255);
                
                if (debug) {
                    fprintf(stderr, "[RESULT] %d %c %d = %d (via graph)\n",
                            g.nodes[num_a].value, op, g.nodes[num_b].value, result);
                }
            }
        }
    }
    
    // Output: Show results (edges with high weight = computed results)
    printf("Route: ");
    int found = 0;
    
    // Show results from operation nodes (weight 255 = computed)
    for (uint32_t n = 0; n < count; n++) {
        for (uint32_t e = 0; e < g.edge_count; e++) {
            if (g.edges[e].from == node_ids[n] && g.edges[e].weight >= 250) {
                printf("%.*s ", g.nodes[g.edges[e].to].token_len, 
                       g.nodes[g.edges[e].to].token);
                found = 1;
            }
        }
    }
    
    // Also show regular connections from last node
    if (count > 0) {
        for (uint32_t e = 0; e < g.edge_count; e++) {
            if (g.edges[e].from == node_ids[count-1] && g.edges[e].weight < 250) {
                printf("%.*s ", g.nodes[g.edges[e].to].token_len, 
                       g.nodes[g.edges[e].to].token);
                found = 1;
            }
        }
    }
    
    if (!found) printf("(none)");
    printf("\n");
    
    // Track routing success
    last_route_count = found ? 1 : 0;
    
    // Evolve: try mutations (always for testing)
    if (g.node_count > 30) {
        for (int i = 0; i < 3; i++) {
            evolve();
        }
    }
}

/* Persistence */
void save_graph() {
    size_t hsize = sizeof(uint32_t) * 4;
    size_t nsize = g.node_count * sizeof(Node);
    size_t esize = g.edge_count * sizeof(Edge);
    
    int fd = open("melvin.mmap", O_RDWR | O_CREAT, 0644);
    if (fd < 0) return;
    
    ftruncate(fd, hsize + nsize + esize);
    void *mem = mmap(NULL, hsize + nsize + esize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mem == MAP_FAILED) { close(fd); return; }
    
    uint32_t *header = (uint32_t *)mem;
    header[0] = g.node_count;
    header[1] = g.node_cap;
    header[2] = g.edge_count;
    header[3] = g.edge_cap;
    
    memcpy((char *)mem + hsize, g.nodes, nsize);
    memcpy((char *)mem + hsize + nsize, g.edges, esize);
    
    munmap(mem, hsize + nsize + esize);
    close(fd);
}

void load_graph() {
    int fd = open("melvin.mmap", O_RDONLY);
    if (fd < 0) return;
    
    struct stat st;
    fstat(fd, &st);
    
    void *mem = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mem == MAP_FAILED) { close(fd); return; }
    
    uint32_t *header = (uint32_t *)mem;
    g.node_count = header[0];
    g.edge_count = header[2];
    
    size_t hsize = sizeof(uint32_t) * 4;
    if (g.node_count <= g.node_cap && g.edge_count <= g.edge_cap) {
        memcpy(g.nodes, (char *)mem + hsize, g.node_count * sizeof(Node));
        memcpy(g.edges, (char *)mem + hsize + g.node_count * sizeof(Node), g.edge_count * sizeof(Edge));
    }
    
    munmap(mem, st.st_size);
    close(fd);
}

/* Main */
int main() {
    debug = getenv("MELVIN_DEBUG") != NULL;
    srand(time(NULL));  // Seed random for evolution
    
    g.node_cap = 100000;
    g.edge_cap = 1000000;
    g.node_count = 0;
    g.edge_count = 0;
    
    g.nodes = malloc(g.node_cap * sizeof(Node));
    g.edges = malloc(g.edge_cap * sizeof(Edge));
    
    if (!g.nodes || !g.edges) {
        fprintf(stderr, "Failed\n");
        return 1;
    }
    
    memset(g.nodes, 0, g.node_cap * sizeof(Node));
    memset(g.edges, 0, g.edge_cap * sizeof(Edge));
    
    load_graph();
    
    // Init circuits if empty (addition + patterns)
    if (g.node_count < 16) init_circuits();
    
    // Process (just route - no mode distinction)
    char input[4096];
    if (fgets(input, sizeof(input), stdin)) {
        size_t len = strlen(input);
        while (len > 0 && (input[len-1] == '\n' || input[len-1] == '\r')) input[--len] = '\0';
        
        if (len > 0) {
            route((uint8_t*)input, len);
        }
    }
    
    save_graph();
    
    free(g.nodes);
    free(g.edges);
    
    return 0;
}
