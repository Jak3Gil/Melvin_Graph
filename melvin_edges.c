/*
 * RICH EDGES - Position/context encoded in connections
 * 
 * User's insight: "Position awareness done with edges"
 * 
 * Instead of separate position tracking,
 * edges ENCODE the relationship:
 *   - Sequential edges (position offset)
 *   - Similarity edges (shared features)
 *   - Abstraction edges (hierarchy level)
 * 
 * Network of networks emerges from EDGE TYPES!
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

/* MINIMAL NODE - Just data */
typedef struct {
    uint8_t token[64];
    uint8_t token_len;
    float state;
    float energy;
} Node;

/* RICH EDGE - Encodes relationship */
typedef struct {
    uint32_t src, dst;
    float weight;
    
    // EDGE TYPE (what kind of relationship?)
    enum {
        EDGE_SEQUENTIAL,    // Appeared next to each other
        EDGE_SIMILAR,       // Share features (rhyme, prefix)
        EDGE_POSITIONAL,    // Same position in different contexts
        EDGE_ABSTRACTION    // One abstracts the other
    } type;
    
    // EDGE METADATA
    int8_t position_offset;   // For sequential: -1, +1, etc.
    float similarity_score;   // For similar: 0.0-1.0
    uint8_t shared_position;  // For positional: which position
    uint8_t level_difference; // For abstraction: how many levels
} Connection;

typedef struct {
    Node *nodes;
    Connection *connections;
    uint32_t node_count, node_cap, connection_count, connection_cap;
    uint32_t last_word_node;  // For sequential
    uint32_t current_position; // Track position in input
} Graph;

Graph g;
void *mmap_base = NULL;
int debug = 0;

/* Find or create node */
uint32_t find_or_create(uint8_t *token, uint32_t len) {
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].token_len == len) {
            int match = 1;
            for (uint32_t b = 0; b < len; b++) {
                if (g.nodes[i].token[b] != token[b]) {
                    match = 0;
                    break;
                }
            }
            if (match) return i;
        }
    }
    
    if (g.node_count >= g.node_cap) return UINT32_MAX;
    
    uint32_t id = g.node_count++;
    memset(&g.nodes[id], 0, sizeof(Node));
    g.nodes[id].token_len = len;
    for (uint32_t b = 0; b < len && b < 64; b++) {
        g.nodes[id].token[b] = token[b];
    }
    g.nodes[id].energy = 100.0f;
    return id;
}

/* Calculate similarity between tokens */
float token_similarity(Node *a, Node *b) {
    uint32_t shared = 0;
    uint32_t total = (a->token_len > b->token_len) ? a->token_len : b->token_len;
    
    for (uint32_t i = 0; i < a->token_len && i < b->token_len; i++) {
        if (a->token[i] == b->token[i]) shared++;
    }
    
    return (total > 0) ? (float)shared / (float)total : 0.0f;
}

/* Create connection with rich metadata */
void connect(uint32_t src, uint32_t dst, int type, float score, int8_t offset, uint8_t pos) {
    if (src >= g.node_count || dst >= g.node_count) return;
    if (g.connection_count >= g.connection_cap) return;
    
    Connection *c = &g.connections[g.connection_count++];
    c->src = src;
    c->dst = dst;
    c->weight = 1.0f;
    c->type = type;
    c->similarity_score = score;
    c->position_offset = offset;
    c->shared_position = pos;
    c->level_difference = 0;
    
    if (debug) {
        printf("[EDGE] %u → %u ", src, dst);
        switch(type) {
            case 0: printf("(sequential, offset=%d)", offset); break;
            case 1: printf("(similar, score=%.2f)", score); break;
            case 2: printf("(positional, pos=%u)", pos); break;
            case 3: printf("(abstraction)"); break;
        }
        printf("\n");
    }
}

/* Process input - create nodes and MULTIPLE types of edges */
void sense_input(uint8_t *bytes, uint32_t len) {
    uint32_t word_nodes[128];
    uint32_t word_positions[128];
    uint32_t word_count = 0;
    
    uint32_t word_start = 0;
    g.current_position = 0;
    
    // Parse words
    for (uint32_t i = 0; i <= len && word_count < 128; i++) {
        uint8_t ch = (i < len) ? bytes[i] : ' ';
        int is_boundary = (ch == ' ' || ch == '\n' || ch == '+' || ch == '=');
        
        if (i > word_start && (is_boundary || i == len)) {
            uint32_t node_id = find_or_create(&bytes[word_start], i - word_start);
            if (node_id != UINT32_MAX) {
                word_nodes[word_count] = node_id;
                word_positions[word_count] = g.current_position;
                word_count++;
                g.nodes[node_id].state = 1.0f;
                g.current_position++;
            }
            word_start = i + 1;
        }
        
        if (is_boundary && ch != ' ' && ch != '\n') {
            uint32_t node_id = find_or_create(&ch, 1);
            if (node_id != UINT32_MAX) {
                word_nodes[word_count] = node_id;
                word_positions[word_count] = g.current_position;
                word_count++;
                g.nodes[node_id].state = 1.0f;
                g.current_position++;
            }
            word_start = i + 1;
        }
    }
    
    // Create MULTIPLE EDGE TYPES for this input
    for (uint32_t i = 0; i < word_count; i++) {
        uint32_t node_id = word_nodes[i];
        uint32_t pos = word_positions[i];
        
        // 1. SEQUENTIAL edges (to previous/next)
        if (i > 0) {
            connect(word_nodes[i-1], node_id, 0, 0.0f, +1, 0);  // SEQUENTIAL
        }
        
        // 2. SIMILARITY edges (to similar nodes)
        for (uint32_t j = 0; j < g.node_count; j++) {
            if (j == node_id) continue;
            float sim = token_similarity(&g.nodes[node_id], &g.nodes[j]);
            if (sim > 0.4f) {
                connect(node_id, j, 1, sim, 0, 0);  // SIMILAR
            }
        }
        
        // 3. POSITIONAL edges (same position across contexts)
        // Find nodes that appeared at this position before
        for (uint32_t j = 0; j < word_count; j++) {
            if (i != j && word_positions[j] == pos) {
                connect(word_nodes[i], word_nodes[j], 2, 0.0f, 0, pos);  // POSITIONAL
            }
        }
    }
    
    if (debug) {
        printf("[NETWORK] Created %u nodes, %u edges across %u types\n",
               word_count, g.connection_count, 3);
    }
}

/* Propagate through different edge types */
void propagate() {
    for (uint32_t i = 0; i < g.connection_count; i++) {
        Connection *c = &g.connections[i];
        if (c->src >= g.node_count || c->dst >= g.node_count) continue;
        
        Node *src = &g.nodes[c->src];
        Node *dst = &g.nodes[c->dst];
        
        if (src->state > 0.01f) {
            float signal = 0.0f;
            
            switch(c->type) {
                case 0:  // Sequential
                    signal = src->state * c->weight * 0.3f;
                    break;
                case 1:  // Similar
                    signal = src->state * c->similarity_score * 0.5f;
                    break;
                case 2:  // Positional  
                    signal = src->state * c->weight * 0.8f;  // Strong!
                    break;
                case 3:  // Abstraction
                    signal = src->state * c->weight * 1.0f;  // Strongest!
                    break;
            }
            
            dst->state += signal;
        }
    }
}

/* Output most activated */
void emit_output() {
    for (int rank = 0; rank < 5; rank++) {
        float max_state = 0.0f;
        uint32_t max_id = UINT32_MAX;
        
        for (uint32_t i = 0; i < g.node_count; i++) {
            if (g.nodes[i].state > max_state) {
                max_state = g.nodes[i].state;
                max_id = i;
            }
        }
        
        if (max_id != UINT32_MAX && max_state > 0.3f) {
            for (uint32_t b = 0; b < g.nodes[max_id].token_len; b++) {
                write(STDOUT_FILENO, &g.nodes[max_id].token[b], 1);
            }
            write(STDOUT_FILENO, " ", 1);
            g.nodes[max_id].state = 0.0f;
        }
    }
    write(STDOUT_FILENO, "\n", 1);
}

int main() {
    if (getenv("MELVIN_DEBUG")) debug = 1;
    
    g.node_cap = 1000;
    g.connection_cap = 10000;
    
    size_t size = sizeof(Node) * g.node_cap + sizeof(Connection) * g.connection_cap;
    int fd = open("graph_edges.mmap", O_RDWR | O_CREAT, 0644);
    ftruncate(fd, size);
    mmap_base = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    g.nodes = (Node *)mmap_base;
    g.connections = (Connection *)((char *)mmap_base + g.node_cap * sizeof(Node));
    g.last_word_node = UINT32_MAX;
    
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
        
        for (int i = 0; i < 5; i++) propagate();
        emit_output();
    }
    
    msync(mmap_base, size, MS_SYNC);
    munmap(mmap_base, size);
    close(fd);
    return 0;
}

