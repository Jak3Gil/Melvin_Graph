/*
 * STRUCTURAL PATTERN RECOGNITION
 * 
 * Key insight: Track WHERE nodes appear, not just WHAT they are
 * "1+1=2" and "2+2=4" share "+" and "=" in SAME POSITIONS
 * = Same structural pattern!
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

typedef struct {
    uint8_t token[64];
    uint8_t token_len;
    float state, energy;
    uint32_t frequency;
} Node;

// POSITION-AWARE CONNECTION
typedef struct {
    uint32_t node_id;        // Which node
    uint32_t position;       // At what position in sequence
    uint32_t context_id;     // In which input context
} NodeOccurrence;

typedef struct {
    // Track all positions where this node appeared
    NodeOccurrence occurrences[256];
    uint32_t occurrence_count;
} PositionTracker;

typedef struct {
    Node *nodes;
    PositionTracker *positions;
    uint32_t node_count, node_cap;
    uint64_t context_id;  // Increments each input
    float structural_similarity_threshold;
} Graph;

Graph g;
void *mmap_base = NULL;
int debug = 0;

/* Find or create node for token */
uint32_t find_or_create_node(uint8_t *token, uint32_t len) {
    // Try to find
    for (uint32_t i = 0; i < g.node_count; i++) {
        Node *n = &g.nodes[i];
        if (n->token_len == len) {
            int match = 1;
            for (uint32_t b = 0; b < len; b++) {
                if (n->token[b] != token[b]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                n->frequency++;  // REUSE!
                return i;
            }
        }
    }
    
    // Create
    if (g.node_count >= g.node_cap) return UINT32_MAX;
    
    uint32_t id = g.node_count++;
    Node *n = &g.nodes[id];
    memset(n, 0, sizeof(Node));
    n->token_len = len;
    for (uint32_t b = 0; b < len && b < 64; b++) {
        n->token[b] = token[b];
    }
    n->energy = 100.0f;
    n->frequency = 1;
    
    return id;
}

/* Record that node appeared at position in current context */
void record_position(uint32_t node_id, uint32_t position) {
    if (node_id >= g.node_cap) return;
    
    PositionTracker *pt = &g.positions[node_id];
    if (pt->occurrence_count >= 256) return;
    
    pt->occurrences[pt->occurrence_count].node_id = node_id;
    pt->occurrences[pt->occurrence_count].position = position;
    pt->occurrences[pt->occurrence_count].context_id = g.context_id;
    pt->occurrence_count++;
}

/* Calculate STRUCTURAL similarity (same nodes in same positions?) */
float calculate_structural_similarity(uint32_t ctx1, uint32_t ctx2) {
    uint32_t shared_positions = 0;
    uint32_t total_positions = 0;
    
    if (debug) {
        printf("    Checking ctx %u vs ctx %u:\n", ctx1, ctx2);
    }
    
    for (uint32_t i = 0; i < g.node_count; i++) {
        PositionTracker *pt = &g.positions[i];
        
        uint32_t pos1 = UINT32_MAX;
        for (uint32_t o = 0; o < pt->occurrence_count; o++) {
            if (pt->occurrences[o].context_id == ctx1) {
                pos1 = pt->occurrences[o].position;
                break;
            }
        }
        
        uint32_t pos2 = UINT32_MAX;
        for (uint32_t o = 0; o < pt->occurrence_count; o++) {
            if (pt->occurrences[o].context_id == ctx2) {
                pos2 = pt->occurrences[o].position;
                break;
            }
        }
        
        if (pos1 != UINT32_MAX && pos2 != UINT32_MAX) {
            total_positions++;
            if (pos1 == pos2) {
                shared_positions++;
                if (debug) {
                    Node *n = &g.nodes[i];
                    printf("      Match at pos %u: '", pos1);
                    for (uint32_t b = 0; b < n->token_len && b < 5; b++) {
                        printf("%c", n->token[b]);
                    }
                    printf("'\n");
                }
            }
        }
    }
    
    if (total_positions == 0) return 0.0f;
    float sim = (float)shared_positions / (float)total_positions;
    
    if (debug) {
        printf("    → %u/%u positions match = %.1f%%\n", 
               shared_positions, total_positions, sim * 100.0f);
    }
    
    return sim;
}

/* Process input - tokenize and record positions */
void sense_input(uint8_t *bytes, uint32_t len) {
    g.context_id++;  // New input context
    
    uint32_t position = 0;
    uint32_t word_start = 0;
    
    for (uint32_t i = 0; i <= len; i++) {
        uint8_t ch = (i < len) ? bytes[i] : ' ';
        
        if (ch == ' ' || ch == '\n' || ch == '+' || ch == '=' || i == len) {
            // Process word before this
            if (i > word_start) {
                uint32_t node_id = find_or_create_node(&bytes[word_start], i - word_start);
                if (node_id != UINT32_MAX) {
                    record_position(node_id, position);
                    g.nodes[node_id].state = 1.0f;
                    
                    if (debug) {
                        printf("[POS %u] ", position);
                        for (uint32_t b = 0; b < g.nodes[node_id].token_len; b++) {
                            printf("%c", g.nodes[node_id].token[b]);
                        }
                        printf("\n");
                    }
                    
                    position++;
                }
            }
            
            // Process operator as its own node
            if (ch == '+' || ch == '=') {
                uint32_t node_id = find_or_create_node(&ch, 1);
                if (node_id != UINT32_MAX) {
                    record_position(node_id, position);
                    g.nodes[node_id].state = 1.0f;
                    
                    if (debug) {
                        printf("[POS %u] %c\n", position, ch);
                    }
                    
                    position++;
                }
            }
            
            word_start = i + 1;
        }
    }
    
    // Check structural similarity to ALL previous contexts
    if (g.context_id > 1 && debug) {
        printf("[STRUCTURE] Context %llu - checking against previous:\n", 
               (unsigned long long)g.context_id);
        
        float max_sim = 0.0f;
        uint32_t best_match = 0;
        
        for (uint32_t prev = 1; prev < g.context_id; prev++) {
            float sim = calculate_structural_similarity(g.context_id, prev);
            if (sim > max_sim) {
                max_sim = sim;
                best_match = prev;
            }
        }
        
        if (max_sim > 0.0f) {
            printf("  → Best match: context %u (%.1f%% similar)", 
                   best_match, max_sim * 100.0f);
            if (max_sim > 0.6f) printf(" ← STRONG PATTERN!");
            printf("\n");
        } else {
            printf("  → No structural matches\n");
        }
    }
}

void emit_output() {
    // Output active nodes in position order
    for (uint32_t pos = 0; pos < 10; pos++) {
        for (uint32_t i = 0; i < g.node_count; i++) {
            PositionTracker *pt = &g.positions[i];
            
            // Did this node appear at this position in current context?
            for (uint32_t o = 0; o < pt->occurrence_count; o++) {
                if (pt->occurrences[o].context_id == g.context_id &&
                    pt->occurrences[o].position == pos &&
                    g.nodes[i].state > 0.5f) {
                    
                    Node *n = &g.nodes[i];
                    for (uint32_t b = 0; b < n->token_len; b++) {
                        write(STDOUT_FILENO, &n->token[b], 1);
                    }
                    write(STDOUT_FILENO, " ", 1);
                }
            }
        }
    }
    write(STDOUT_FILENO, "\n", 1);
}

int main() {
    if (getenv("MELVIN_DEBUG")) debug = 1;
    
    g.node_cap = 1000;
    g.structural_similarity_threshold = 0.5f;
    
    size_t size = sizeof(Node) * g.node_cap + 
                  sizeof(PositionTracker) * g.node_cap;
    
    int fd = open("graph_structural.mmap", O_RDWR | O_CREAT, 0644);
    ftruncate(fd, size);
    mmap_base = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    g.nodes = (Node *)mmap_base;
    g.positions = (PositionTracker *)((char *)mmap_base + g.node_cap * sizeof(Node));
    
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
        
        emit_output();
    }
    
    msync(mmap_base, size, MS_SYNC);
    munmap(mmap_base, size);
    close(fd);
    return 0;
}
