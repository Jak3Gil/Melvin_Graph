/*
 * MELVIN MINIMAL - Stripped down to essentials
 * 
 * The 3 Rules That Breed Intelligence:
 * 1. REUSE before CREATE (generalization)
 * 2. CONNECT sequentially (pattern learning)
 * 3. STRENGTHEN with use (Hebbian learning)
 *
 * Everything else is emergent!
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>

/* ========================================================================
 * MINIMAL STRUCTURES
 * ======================================================================== */

typedef struct {
    uint8_t token[64];       // Variable-length token (increased from 16!)
    uint8_t token_len;
    float state;             // Current activation
    float energy;            // Survival
    uint32_t frequency;      // Reuse count = intelligence!
    uint32_t last_active_tick;
} Node;

typedef struct {
    uint32_t src;
    uint32_t dst;
    float weight;            // THE MEMORY
} Connection;

typedef struct {
    uint32_t node_count, node_cap, connection_count, connection_cap;
    uint64_t tick;
    uint32_t magic;
} GraphHeader;

typedef struct {
    Node *nodes;
    Connection *connections;
    uint32_t node_count, node_cap, connection_count, connection_cap;
    uint64_t tick;
    
    uint32_t last_word_node;  // For sequential wiring
    
    // Intelligence metric
    uint32_t patterns_reused;
    uint32_t patterns_created;
    
    // DYNAMIC parameters (adapt, not hardcoded!)
    float learning_rate;
    float decay_rate;
    float metabolism;
    uint32_t ngram_max;      // Adaptive max token length
    uint32_t prop_hops;      // Adaptive propagation depth
} Graph;

Graph g_graph;
void *g_mmap_base = NULL;
size_t g_mmap_size = 0;
int g_debug = 0;

/* ========================================================================
 * MMAP (simplified)
 * ======================================================================== */

void mmap_init(const char *filename) {
    uint32_t initial_node_cap = 1000;      // Start small (was 10K)
    uint32_t initial_conn_cap = 10000;     // Start small (was 100K)
    
    size_t header_size = sizeof(GraphHeader);
    g_mmap_size = header_size + 
                  initial_node_cap * sizeof(Node) + 
                  initial_conn_cap * sizeof(Connection);
    
    int fd = open(filename, O_RDWR | O_CREAT, 0644);
    struct stat st;
    int file_exists = (fstat(fd, &st) == 0 && st.st_size > 0);
    
    if (!file_exists) {
        ftruncate(fd, g_mmap_size);
    } else {
        g_mmap_size = st.st_size;
    }
    
    g_mmap_base = mmap(NULL, g_mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    GraphHeader *header = (GraphHeader *)g_mmap_base;
    
    if (file_exists && header->magic == 0xE3E6E01) {
        // Restore
        g_graph.node_count = header->node_count;
        g_graph.node_cap = header->node_cap;
        g_graph.connection_count = header->connection_count;
        g_graph.connection_cap = header->connection_cap;
        g_graph.tick = header->tick;
    } else {
        // New
        header->magic = 0xE3E6E01;
        header->node_count = 0;
        header->node_cap = initial_node_cap;
        header->connection_count = 0;
        header->connection_cap = initial_conn_cap;
        g_graph.node_count = 0;
        g_graph.node_cap = initial_node_cap;
        g_graph.connection_count = 0;
        g_graph.connection_cap = initial_conn_cap;
    }
    
    g_graph.nodes = (Node *)((char *)g_mmap_base + header_size);
    g_graph.connections = (Connection *)((char *)g_graph.nodes + header->node_cap * sizeof(Node));
    
    close(fd);
}

void mmap_sync() {
    if (!g_mmap_base) return;
    GraphHeader *header = (GraphHeader *)g_mmap_base;
    header->node_count = g_graph.node_count;
    header->node_cap = g_graph.node_cap;
    header->connection_count = g_graph.connection_count;
    header->connection_cap = g_graph.connection_cap;
    header->tick = g_graph.tick;
    msync(g_mmap_base, g_mmap_size, MS_SYNC);
}

/* ========================================================================
 * NODE/CONNECTION OPS
 * ======================================================================== */

uint32_t node_create() {
    if (g_graph.node_count >= g_graph.node_cap) {
        fprintf(stderr, "Node capacity exceeded\n");
        return UINT32_MAX;
    }
    
    uint32_t idx = g_graph.node_count++;
    Node *n = &g_graph.nodes[idx];
    memset(n, 0, sizeof(Node));
    n->energy = 100.0f;
    return idx;
}

Connection *find_connection(uint32_t src, uint32_t dst) {
    for (uint32_t i = 0; i < g_graph.connection_count; i++) {
        Connection *c = &g_graph.connections[i];
        if (c->src == src && c->dst == dst) return c;
    }
    return NULL;
}

uint32_t connection_create(uint32_t src, uint32_t dst, float weight) {
    if (src >= g_graph.node_count || dst >= g_graph.node_count) return UINT32_MAX;
    if (src == dst) return UINT32_MAX;
    
    Connection *existing = find_connection(src, dst);
    if (existing) {
        existing->weight += weight * 0.1f;
        if (existing->weight > 10.0f) existing->weight = 10.0f;
        return UINT32_MAX;
    }
    
    if (g_graph.connection_count >= g_graph.connection_cap) {
        fprintf(stderr, "Connection capacity exceeded\n");
        return UINT32_MAX;
    }
    
    uint32_t idx = g_graph.connection_count++;
    Connection *c = &g_graph.connections[idx];
    c->src = src;
    c->dst = dst;
    c->weight = weight;
    return idx;
}

/* ========================================================================
 * RULE 1: REUSE before CREATE (generalization emerges!)
 * ======================================================================== */

void sense_input(const uint8_t *bytes, uint32_t len) {
    // Parse into words
    uint32_t word_starts[128];
    uint32_t word_lens[128];
    uint32_t word_count = 0;
    uint32_t word_start = 0;
    int in_word = 0;
    
    for (uint32_t i = 0; i <= len && word_count < 128; i++) {
        uint8_t ch = (i < len) ? bytes[i] : ' ';
        int is_boundary = (ch == ' ' || ch == '\n' || ch == '\r');
        
        if (!in_word && !is_boundary) {
            word_start = i;
            in_word = 1;
        } else if (in_word && is_boundary) {
            word_starts[word_count] = word_start;
            word_lens[word_count] = i - word_start;
            word_count++;
            in_word = 0;
        }
    }
    
    // Process each word: create all n-grams
    for (uint32_t w = 0; w < word_count; w++) {
        uint32_t start = word_starts[w];
        uint32_t word_len = word_lens[w];
        uint32_t longest_node = UINT32_MAX;
        
        // ADAPTIVE ngram length (not hardcoded!)
        uint32_t max_ngram = (word_len < g_graph.ngram_max) ? word_len : g_graph.ngram_max;
        
        for (uint32_t ngram_len = 1; ngram_len <= max_ngram; ngram_len++) {
            for (uint32_t offset = 0; offset + ngram_len <= word_len; offset++) {
                uint32_t pos = start + offset;
                
                // RULE 1: Try to REUSE existing node
                uint32_t node_id = UINT32_MAX;
                for (uint32_t n = 0; n < g_graph.node_count; n++) {
                    Node *node = &g_graph.nodes[n];
                    if (node->token_len == ngram_len) {
                        int match = 1;
                        for (uint32_t b = 0; b < ngram_len; b++) {
                            if (node->token[b] != bytes[pos + b]) {
                                match = 0;
                                break;
                            }
                        }
                        if (match) {
                            node_id = n;
                            break;
                        }
                    }
                }
                
                if (node_id == UINT32_MAX) {
                    // CREATE new pattern
                    node_id = node_create();
                    if (node_id != UINT32_MAX) {
                        Node *new_node = &g_graph.nodes[node_id];
                        new_node->token_len = ngram_len;
                        for (uint32_t b = 0; b < ngram_len; b++) {
                            new_node->token[b] = bytes[pos + b];
                        }
                        new_node->frequency = 1;
                        g_graph.patterns_created++;
                    }
                } else {
                    // REUSED! Intelligence!
                    g_graph.nodes[node_id].frequency++;
                    g_graph.patterns_reused++;
                }
                
                if (node_id != UINT32_MAX) {
                    g_graph.nodes[node_id].state = (float)ngram_len;
                    g_graph.nodes[node_id].energy += (float)ngram_len;
                    g_graph.nodes[node_id].last_active_tick = g_graph.tick;
                    
                    if (offset == 0 && ngram_len == word_len) {
                        longest_node = node_id;
                    }
                }
            }
        }
        
        // RULE 2: WIRE sequentially (longest tokens only)
        if (longest_node != UINT32_MAX && g_graph.last_word_node != UINT32_MAX) {
            uint32_t conn_id = connection_create(g_graph.last_word_node, longest_node, 1.0f);
            if (g_debug && conn_id != UINT32_MAX) {
                fprintf(stderr, "[CONNECT] %u → %u\n", g_graph.last_word_node, longest_node);
            }
        }
        g_graph.last_word_node = longest_node;
    }
}

/* ========================================================================
 * DYNAMIC PROPAGATION (until convergence, not fixed hops!)
 * ======================================================================== */

void propagate() {
    // Decay first
    for (uint32_t i = 0; i < g_graph.node_count; i++) {
        g_graph.nodes[i].state *= g_graph.decay_rate;
    }
    
    // Transmit through connections
    for (uint32_t i = 0; i < g_graph.connection_count; i++) {
        Connection *c = &g_graph.connections[i];
        if (c->src >= g_graph.node_count || c->dst >= g_graph.node_count) continue;
        
        Node *src = &g_graph.nodes[c->src];
        Node *dst = &g_graph.nodes[c->dst];
        
        if (src->state > 0.01f) {
            float signal = src->state * c->weight * 0.1f;
            dst->state += signal;
            src->energy -= signal * 0.01f;
        }
    }
}

/* ========================================================================
 * RULE 3: STRENGTHEN with use (Hebbian)
 * ======================================================================== */

void learn() {
    for (uint32_t i = 0; i < g_graph.connection_count; i++) {
        Connection *c = &g_graph.connections[i];
        if (c->src >= g_graph.node_count || c->dst >= g_graph.node_count) continue;
        
        Node *src = &g_graph.nodes[c->src];
        Node *dst = &g_graph.nodes[c->dst];
        
        // Both active? Strengthen!
        if (src->last_active_tick == g_graph.tick && dst->last_active_tick == g_graph.tick) {
            c->weight += g_graph.learning_rate;
            if (c->weight > 10.0f) c->weight = 10.0f;
        } else {
            // Decay unused
            c->weight -= g_graph.learning_rate * 0.01f;
            if (c->weight < 0.0f) c->weight = 0.0f;
        }
    }
}

/* ========================================================================
 * OUTPUT: Follow rules (graph execution)
 * ======================================================================== */

void emit_output() {
    uint8_t output[1024];
    uint32_t output_len = 0;
    
    // Start from last input node, follow strongest connections
    uint32_t current = g_graph.last_word_node;
    
    if (g_debug) {
        fprintf(stderr, "[OUTPUT] Starting from node %u\n", current);
    }
    
    for (uint32_t step = 0; step < 5 && output_len < 1000; step++) {
        if (current >= g_graph.node_count) {
            if (g_debug) fprintf(stderr, "[OUTPUT] Invalid node %u\n", current);
            break;
        }
        
        Node *node = &g_graph.nodes[current];
        
        // Output this token
        for (uint32_t b = 0; b < node->token_len && output_len < 1024; b++) {
            output[output_len++] = node->token[b];
        }
        if (output_len < 1023) output[output_len++] = ' ';
        
        if (g_debug) {
            fprintf(stderr, "[OUTPUT] Step %u: node %u (", step, current);
            for (uint32_t b = 0; b < node->token_len && b < 10; b++) {
                fprintf(stderr, "%c", node->token[b]);
            }
            fprintf(stderr, ")\n");
        }
        
        // Find strongest outgoing connection
        float strongest = 0.0f;
        uint32_t next = UINT32_MAX;
        
        for (uint32_t i = 0; i < g_graph.connection_count; i++) {
            if (g_graph.connections[i].src == current && 
                g_graph.connections[i].weight > strongest) {
                strongest = g_graph.connections[i].weight;
                next = g_graph.connections[i].dst;
            }
        }
        
        if (g_debug) {
            fprintf(stderr, "[OUTPUT] Strongest conn: %u→%u (weight=%.2f)\n", 
                    current, next, strongest);
        }
        
        if (next == UINT32_MAX || strongest < 0.5f) break;
        current = next;
    }
    
    if (output_len > 0) {
        write(STDOUT_FILENO, output, output_len);
        write(STDOUT_FILENO, "\n", 1);
    } else if (g_debug) {
        fprintf(stderr, "[OUTPUT] No output generated\n");
    }
}

/* ========================================================================
 * METABOLISM: Death from energy depletion
 * ======================================================================== */

void apply_metabolism() {
    for (uint32_t i = 0; i < g_graph.node_count; i++) {
        Node *n = &g_graph.nodes[i];
        n->energy -= g_graph.metabolism;
        
        if (n->energy <= 0.0f) {
            n->state = 0.0f;
            n->energy = 0.0f;
            n->frequency = 0;  // Dead
        }
    }
}

/* ========================================================================
 * ADAPT PARAMETERS (make dynamic, not hardcoded!)
 * ======================================================================== */

void adapt_parameters() {
    // Adapt learning rate based on reuse ratio (intelligence)
    if (g_graph.patterns_created + g_graph.patterns_reused > 100) {
        float reuse_ratio = (float)g_graph.patterns_reused / 
                           (float)(g_graph.patterns_created + g_graph.patterns_reused);
        
        // Low reuse (< 50%) → increase learning to find better patterns
        if (reuse_ratio < 0.5f) {
            g_graph.learning_rate += 0.01f;
            if (g_graph.learning_rate > 0.5f) g_graph.learning_rate = 0.5f;
        } else {
            // High reuse → decrease learning (patterns are good)
            g_graph.learning_rate -= 0.01f;
            if (g_graph.learning_rate < 0.05f) g_graph.learning_rate = 0.05f;
        }
        
        // Reset counters
        g_graph.patterns_created = 0;
        g_graph.patterns_reused = 0;
    }
    
    // Adapt ngram_max based on node count
    // More nodes → can handle longer tokens
    g_graph.ngram_max = 10 + (g_graph.node_count / 100);
    if (g_graph.ngram_max > 64) g_graph.ngram_max = 64;
    
    // Adapt propagation depth based on graph density
    float density = (g_graph.node_count > 0) ? 
                   (float)g_graph.connection_count / (float)g_graph.node_count : 1.0f;
    g_graph.prop_hops = (uint32_t)(density) + 3;
    if (g_graph.prop_hops > 10) g_graph.prop_hops = 10;
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main() {
    if (getenv("MELVIN_DEBUG")) g_debug = 1;
    
    memset(&g_graph, 0, sizeof(Graph));
    mmap_init("graph_minimal.mmap");
    
    // Initialize dynamic parameters
    if (g_graph.tick == 0) {
        g_graph.learning_rate = 0.1f;
        g_graph.decay_rate = 0.95f;
        g_graph.metabolism = 0.1f;
        g_graph.ngram_max = 10;
        g_graph.prop_hops = 5;
        g_graph.last_word_node = UINT32_MAX;
    }
    
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
    
    uint8_t input[4096];
    int idle = 0;
    
    while (idle < 10) {
        ssize_t n = read(STDIN_FILENO, input, sizeof(input));
        
        if (n > 0) {
            sense_input(input, n);
            idle = 0;
        } else {
            idle++;
            usleep(10000);
        }
        
        // DYNAMIC propagation (not hardcoded 5 hops!)
        for (uint32_t hop = 0; hop < g_graph.prop_hops; hop++) {
            propagate();
        }
        
        learn();
        emit_output();
        apply_metabolism();
        
        // Adapt parameters every 200 ticks
        if (g_graph.tick % 200 == 0 && g_graph.tick > 0) {
            adapt_parameters();
            
            if (g_debug) {
                float reuse = (g_graph.patterns_created + g_graph.patterns_reused > 0) ?
                    (float)g_graph.patterns_reused / (float)(g_graph.patterns_created + g_graph.patterns_reused) * 100.0f : 0.0f;
                fprintf(stderr, "[TICK %llu] Nodes:%u Conns:%u Intelligence:%.1f%% LR:%.3f PropHops:%u\n",
                        (unsigned long long)g_graph.tick, g_graph.node_count, g_graph.connection_count,
                        reuse, g_graph.learning_rate, g_graph.prop_hops);
            }
        }
        
        g_graph.tick++;
        
        if (g_graph.tick % 100 == 0) mmap_sync();
    }
    
    mmap_sync();
    munmap(g_mmap_base, g_mmap_size);
    return 0;
}

