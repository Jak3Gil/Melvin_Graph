/*
 * MELVIN - The Graph Is Everything
 * 
 * C code: Load, route, save. That's it.
 * Graph: All data, all logic, all intelligence.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef struct __attribute__((packed)) {
    uint8_t token[16];
    uint16_t token_len;
    int32_t value;
} Node;

typedef struct __attribute__((packed)) {
    uint32_t from, to;
    uint8_t weight;
} Edge;

typedef struct {
    Node *nodes;
    Edge *edges;
    uint32_t node_count, node_cap;
    uint32_t edge_count, edge_cap;
} Graph;

Graph g;

/* Find or create node */
uint32_t find_or_create(uint8_t *token, uint32_t len) {
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].token_len == len && 
            memcmp(g.nodes[i].token, token, len) == 0) return i;
    }
    
    if (g.node_count >= g.node_cap) return UINT32_MAX;
    
    uint32_t id = g.node_count++;
    memset(&g.nodes[id], 0, sizeof(Node));
    memcpy(g.nodes[id].token, token, len < 16 ? len : 16);
    g.nodes[id].token_len = len;
    
    // Parse numeric value
    int is_num = 1;
    for (uint32_t i = 0; i < len; i++) {
        if (token[i] < '0' || token[i] > '9') { is_num = 0; break; }
    }
    if (is_num && len > 0) {
        char buf[32];
        memcpy(buf, token, len);
        buf[len] = '\0';
        g.nodes[id].value = atoi(buf);
    }
    
    return id;
}

/* Create edge */
void create_edge(uint32_t from, uint32_t to, uint8_t weight) {
    if (from >= g.node_count || to >= g.node_count || from == to) return;
    
    for (uint32_t i = 0; i < g.edge_count; i++) {
        if (g.edges[i].from == from && g.edges[i].to == to) {
            g.edges[i].weight = (g.edges[i].weight < 240) ? g.edges[i].weight + 15 : 255;
            return;
        }
    }
    
    if (g.edge_count >= g.edge_cap) return;
    g.edges[g.edge_count++] = (Edge){from, to, weight};
}

/* Init: create bit patterns in graph */
void init() {
    const char *patterns[][2] = {
        {"0+0+0", "0,0"}, {"0+1+0", "1,0"}, {"1+0+0", "1,0"}, {"1+1+0", "0,1"},
        {"0+0+1", "1,0"}, {"0+1+1", "0,1"}, {"1+0+1", "0,1"}, {"1+1+1", "1,1"},
    };
    
    for (int i = 0; i < 8; i++) {
        uint32_t p = find_or_create((uint8_t*)patterns[i][0], strlen(patterns[i][0]));
        uint32_t r = find_or_create((uint8_t*)patterns[i][1], strlen(patterns[i][1]));
        create_edge(p, r, 255);
    }
}

/* Route: follow edges, depth-first to find all reachable nodes */
void route(char *input) {
    uint32_t nodes[100], count = 0, start = 0;
    size_t len = strlen(input);
    
    // Tokenize
    for (size_t i = 0; i <= len; i++) {
        if (i == len || input[i] == ' ' || input[i] == '\n') {
            if (i > start) {
                uint32_t nid = find_or_create((uint8_t*)&input[start], i - start);
                if (nid != UINT32_MAX) nodes[count++] = nid;
            }
            start = i + 1;
        }
    }
    
    if (count == 0) return;
    
    // Connect sequence
    for (uint32_t i = 0; i + 1 < count; i++) {
        create_edge(nodes[i], nodes[i+1], 100);
    }
    
    // Check if graph has routing rules for this pattern
    // Look for rule nodes that match input structure
    if (count == 3) {
        // Check for routing rule: "rule_3tokens" or similar
        uint32_t rule = UINT32_MAX;
        for (uint32_t i = 0; i < g.node_count; i++) {
            if (g.nodes[i].token_len == 11 && 
                memcmp(g.nodes[i].token, "rule_3token", 11) == 0) {
                rule = i;
                break;
            }
        }
        
        // If rule exists, follow its edges to execute
        if (rule != UINT32_MAX) {
            for (uint32_t e = 0; e < g.edge_count; e++) {
                if (g.edges[e].from == rule) {
                    // Rule says what to do! (could point to circuits)
                    // For now, just note it exists
                    break;
                }
            }
        }
    }
    
    // Route through graph: follow ALL edges until exhausted
    uint32_t visited[10000];
    int visit_count = 0;
    uint32_t queue[10000];
    int q_start = 0, q_end = 0;
    
    // Start from last node
    queue[q_end++] = nodes[count-1];
    visited[visit_count++] = nodes[count-1];
    
    // Follow edges until no more new nodes found (unlimited hops)
    while (q_start < q_end && visit_count < 10000) {
        uint32_t current = queue[q_start++];
        
        for (uint32_t e = 0; e < g.edge_count; e++) {
            if (g.edges[e].from == current) {
                uint32_t target = g.edges[e].to;
                
                // Check if visited
                int seen = 0;
                for (int v = 0; v < visit_count; v++) {
                    if (visited[v] == target) { seen = 1; break; }
                }
                
                if (!seen) {
                    visited[visit_count++] = target;
                    queue[q_end++] = target;
                }
            }
        }
    }
    
    // Output all reachable nodes
    printf("%.*s → ", g.nodes[nodes[count-1]].token_len, g.nodes[nodes[count-1]].token);
    for (int v = 1; v < visit_count && v < 20; v++) {
        printf("%.*s ", g.nodes[visited[v]].token_len, g.nodes[visited[v]].token);
    }
    printf("\n");
}

/* Save graph */
void save() {
    int fd = open("melvin.mmap", O_RDWR | O_CREAT, 0644);
    if (fd < 0) return;
    
    size_t size = sizeof(uint32_t)*4 + g.node_count*sizeof(Node) + g.edge_count*sizeof(Edge);
    ftruncate(fd, size);
    
    void *mem = mmap(NULL, size, PROT_WRITE, MAP_SHARED, fd, 0);
    if (mem != MAP_FAILED) {
        uint32_t *h = mem;
        h[0] = g.node_count; h[1] = g.node_cap; h[2] = g.edge_count; h[3] = g.edge_cap;
        memcpy(h+4, g.nodes, g.node_count * sizeof(Node));
        memcpy((char*)(h+4) + g.node_count*sizeof(Node), g.edges, g.edge_count*sizeof(Edge));
        munmap(mem, size);
    }
    close(fd);
}

/* Load graph */
void load() {
    int fd = open("melvin.mmap", O_RDONLY);
    if (fd < 0) return;
    
    struct stat st;
    fstat(fd, &st);
    
    void *mem = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mem != MAP_FAILED) {
        uint32_t *h = mem;
        g.node_count = h[0]; g.edge_count = h[2];
        memcpy(g.nodes, h+4, g.node_count * sizeof(Node));
        memcpy(g.edges, (char*)(h+4) + g.node_count*sizeof(Node), g.edge_count*sizeof(Edge));
        munmap(mem, st.st_size);
    }
    close(fd);
}

int main() {
    g.node_cap = 100000;
    g.edge_cap = 1000000;
    g.nodes = calloc(g.node_cap, sizeof(Node));
    g.edges = calloc(g.edge_cap, sizeof(Edge));
    
    if (!g.nodes || !g.edges) return 1;
    
    load();
    if (g.node_count == 0) init();
    
    char input[4096];
    if (fgets(input, sizeof(input), stdin)) {
        route(input);
    }
    
    save();
    free(g.nodes);
    free(g.edges);
    return 0;
}
