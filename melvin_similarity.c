/*
 * MELVIN SIMILARITY - Associative not Sequential
 * 
 * The Rule: Connect to SIMILAR, not NEXT
 * 
 * When you see something, your brain asks:
 * "What's this LIKE?" not "What comes next?"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef struct {
    uint8_t token[64];
    uint8_t token_len;
    float state;
    float energy;
    uint32_t frequency;
} Node;

typedef struct {
    uint32_t src, dst;
    float similarity;  // NOT weight/frequency - SIMILARITY!
} Connection;

typedef struct {
    uint32_t node_count, node_cap, connection_count, connection_cap;
    uint64_t tick;
} GraphHeader;

typedef struct {
    Node *nodes;
    Connection *connections;
    uint32_t node_count, node_cap, connection_count, connection_cap;
    uint64_t tick;
    float similarity_threshold;  // How similar to connect
} Graph;

Graph g;
void *mmap_base = NULL;
int debug = 0;

/* Calculate similarity between two tokens */
float calculate_similarity(Node *a, Node *b) {
    if (a->token_len == 0 || b->token_len == 0) return 0.0f;
    
    // METHOD 1: Shared n-grams
    uint32_t shared = 0;
    uint32_t total = 0;
    
    // Compare all bytes
    for (uint32_t i = 0; i < a->token_len && i < b->token_len; i++) {
        if (a->token[i] == b->token[i]) shared++;
        total++;
    }
    
    // Longer token contributes more
    total = (a->token_len > b->token_len) ? a->token_len : b->token_len;
    
    if (total == 0) return 0.0f;
    return (float)shared / (float)total;
}

/* THE RULE: Connect to similar patterns! */
void connect_to_similar(uint32_t new_node_id) {
    Node *new_node = &g.nodes[new_node_id];
    
    // Find all similar existing nodes
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (i == new_node_id) continue;
        
        Node *existing = &g.nodes[i];
        float sim = calculate_similarity(new_node, existing);
        
        // If similar enough, connect!
        if (sim > g.similarity_threshold) {
            // Check if connection exists
            int exists = 0;
            for (uint32_t c = 0; c < g.connection_count; c++) {
                Connection *conn = &g.connections[c];
                if ((conn->src == new_node_id && conn->dst == i) ||
                    (conn->src == i && conn->dst == new_node_id)) {
                    conn->similarity += sim * 0.1f;  // Strengthen
                    exists = 1;
                    break;
                }
            }
            
            if (!exists && g.connection_count < g.connection_cap) {
                Connection *c = &g.connections[g.connection_count++];
                c->src = new_node_id;
                c->dst = i;
                c->similarity = sim;
                
                if (debug) {
                    printf("[SIMILAR] ");
                    for (uint32_t b = 0; b < new_node->token_len && b < 10; b++) {
                        printf("%c", new_node->token[b]);
                    }
                    printf(" ↔ ");
                    for (uint32_t b = 0; b < existing->token_len && b < 10; b++) {
                        printf("%c", existing->token[b]);
                    }
                    printf(" (sim=%.2f)\n", sim);
                }
            }
        }
    }
}

/* Sense input - find or create nodes */
void sense_input(uint8_t *bytes, uint32_t len) {
    // Simple: one node per word
    uint32_t start = 0;
    
    for (uint32_t i = 0; i <= len; i++) {
        uint8_t ch = (i < len) ? bytes[i] : ' ';
        
        if (ch == ' ' || ch == '\n' || i == len) {
            uint32_t word_len = i - start;
            if (word_len == 0) {
                start = i + 1;
                continue;
            }
            
            // Find or create node for this word
            uint32_t node_id = UINT32_MAX;
            
            for (uint32_t n = 0; n < g.node_count; n++) {
                if (g.nodes[n].token_len == word_len) {
                    int match = 1;
                    for (uint32_t b = 0; b < word_len; b++) {
                        if (g.nodes[n].token[b] != bytes[start + b]) {
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
                // CREATE
                if (g.node_count >= g.node_cap) {
                    start = i + 1;
                    continue;
                }
                node_id = g.node_count++;
                Node *n = &g.nodes[node_id];
                n->token_len = word_len;
                for (uint32_t b = 0; b < word_len && b < 64; b++) {
                    n->token[b] = bytes[start + b];
                }
                n->energy = 100.0f;
                n->frequency = 1;
                
                // NEW RULE: Connect to SIMILAR patterns!
                connect_to_similar(node_id);
            } else {
                // REUSE
                g.nodes[node_id].frequency++;
            }
            
            // Activate
            g.nodes[node_id].state = 1.0f;
            
            start = i + 1;
        }
    }
}

/* Spreading activation (not path following!) */
void propagate() {
    // Activation spreads through similar connections
    for (uint32_t i = 0; i < g.connection_count; i++) {
        Connection *c = &g.connections[i];
        Node *src = &g.nodes[c->src];
        Node *dst = &g.nodes[c->dst];
        
        // Spread activation proportional to similarity
        if (src->state > 0.1f) {
            dst->state += src->state * c->similarity * 0.5f;
        }
        if (dst->state > 0.1f) {
            src->state += dst->state * c->similarity * 0.5f;  // Bidirectional!
        }
    }
    
    // Decay
    for (uint32_t i = 0; i < g.node_count; i++) {
        g.nodes[i].state *= 0.9f;
    }
}

/* Output most activated (not strongest path!) */
void emit_output() {
    // Find top 3 most activated nodes
    for (int rank = 0; rank < 3; rank++) {
        float max_state = 0.0f;
        uint32_t max_id = UINT32_MAX;
        
        for (uint32_t i = 0; i < g.node_count; i++) {
            if (g.nodes[i].state > max_state) {
                max_state = g.nodes[i].state;
                max_id = i;
            }
        }
        
        if (max_id != UINT32_MAX && max_state > 0.5f) {
            Node *n = &g.nodes[max_id];
            for (uint32_t b = 0; b < n->token_len; b++) {
                write(STDOUT_FILENO, &n->token[b], 1);
            }
            write(STDOUT_FILENO, " ", 1);
            g.nodes[max_id].state = 0.0f;  // Don't output twice
        }
    }
    write(STDOUT_FILENO, "\n", 1);
}

int main() {
    if (getenv("MELVIN_DEBUG")) debug = 1;
    
    // Initialize (simplified)
    g.node_cap = 1000;
    g.connection_cap = 10000;
    g.similarity_threshold = 0.3f;  // 30% similar = connect
    
    size_t size = sizeof(GraphHeader) + 
                  g.node_cap * sizeof(Node) + 
                  g.connection_cap * sizeof(Connection);
    
    int fd = open("graph_similarity.mmap", O_RDWR | O_CREAT, 0644);
    ftruncate(fd, size);
    mmap_base = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    GraphHeader *h = (GraphHeader *)mmap_base;
    g.nodes = (Node *)((char *)mmap_base + sizeof(GraphHeader));
    g.connections = (Connection *)((char *)g.nodes + g.node_cap * sizeof(Node));
    
    if (h->tick == 0) {
        h->node_count = 0;
        h->connection_count = 0;
    }
    g.node_count = h->node_count;
    g.connection_count = h->connection_count;
    
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
        
        // Spread activation
        for (int i = 0; i < 5; i++) propagate();
        
        emit_output();
        
        g.tick++;
    }
    
    h->node_count = g.node_count;
    h->connection_count = g.connection_count;
    h->tick = g.tick;
    msync(mmap_base, size, MS_SYNC);
    munmap(mmap_base, size);
    close(fd);
    
    return 0;
}
