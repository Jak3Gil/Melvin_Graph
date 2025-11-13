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

/* Forward declarations */
uint32_t find_or_create_node(uint8_t *token, uint32_t len);
void create_edge(uint32_t from, uint32_t to, uint8_t weight);

/* Find or create node - NO TYPE CHECKING */
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
    
    // If it's a number, store value
    int is_num = 1;
    for (uint32_t i = 0; i < len; i++) {
        if (!isdigit(token[i]) && token[i] != '-') { is_num = 0; break; }
    }
    if (is_num && len > 0) {
        char buf[32];
        memcpy(buf, token, len);
        buf[len] = '\0';
        g.nodes[id].value = atoi(buf);
    }
    
    return id;
}

/* Create edge - just connects nodes */
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

/* Create initial circuits in graph */
void init_circuits() {
    // Bit addition circuit
    const char *patterns[][2] = {
        {"0+0+0", "0,0"}, {"0+1+0", "1,0"}, {"1+0+0", "1,0"}, {"1+1+0", "0,1"},
        {"0+0+1", "1,0"}, {"0+1+1", "0,1"}, {"1+0+1", "0,1"}, {"1+1+1", "1,1"},
    };
    
    for (int i = 0; i < 8; i++) {
        uint32_t p = find_or_create_node((uint8_t*)patterns[i][0], strlen(patterns[i][0]));
        uint32_t r = find_or_create_node((uint8_t*)patterns[i][1], strlen(patterns[i][1]));
        if (p != UINT32_MAX && r != UINT32_MAX) create_edge(p, r, 255);
    }
}

/* Learn - tokenize and create edges */
void learn(uint8_t *input, uint32_t len) {
    uint32_t nodes[100];
    uint32_t count = 0;
    uint32_t start = 0;
    
    // Tokenize
    for (uint32_t i = 0; i <= len && count < 100; i++) {
        uint8_t ch = (i < len) ? input[i] : ' ';
        
        if (ch == ' ' || ch == '\n' || ch == '\t' || i == len) {
            if (i > start) {
                uint32_t nid = find_or_create_node(&input[start], i - start);
                if (nid != UINT32_MAX) nodes[count++] = nid;
            }
            start = i + 1;
        }
    }
    
    if (count == 0) return;
    
    // Create edges from sequence (edges = sequence)
    for (uint32_t i = 0; i + 1 < count; i++) {
        create_edge(nodes[i], nodes[i+1], 100);
    }
}

/* Execute - follow edges */
void execute(uint8_t *input, uint32_t len) {
    // Find node
    uint32_t match = UINT32_MAX;
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].token_len != len) continue;
        
        int exact = 1;
        for (uint32_t b = 0; b < len && b < 16; b++) {
            if (g.nodes[i].token[b] != input[b]) { exact = 0; break; }
        }
        if (exact) { match = i; break; }
    }
    
    if (match == UINT32_MAX) return;
    
    // Follow edges
    printf("Execute: ");
    int found = 0;
    for (uint32_t e = 0; e < g.edge_count; e++) {
        if (g.edges[e].from == match) {
            uint32_t target = g.edges[e].to;
            printf("%.*s ", g.nodes[target].token_len, g.nodes[target].token);
            found = 1;
        }
    }
    if (!found) printf("(none)");
    printf("\n");
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
    
    // Process
    char input[4096];
    if (fgets(input, sizeof(input), stdin)) {
        size_t len = strlen(input);
        while (len > 0 && (input[len-1] == '\n' || input[len-1] == '\r')) input[--len] = '\0';
        
        if (len > 0) {
            int is_query = 1;
            for (size_t i = 0; i < len; i++) {
                if (input[i] == ' ') { is_query = 0; break; }
            }
            
            if (is_query) {
                execute((uint8_t*)input, len);
            } else {
                learn((uint8_t*)input, len);
            }
        }
    }
    
    save_graph();
    
    free(g.nodes);
    free(g.edges);
    
    return 0;
}
