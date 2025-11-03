/*
 * melvin_display.c — Real-time Melvin consciousness display
 * 
 * Shows Melvin's thoughts, learning, memory, and actions on DisplayPort
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <signal.h>

#define MAX_LOG_LINES 50
#define MAX_DETECTIONS 20
#define MAX_ACTIONS 10

static volatile int running = 1;

typedef struct {
    char text[256];
    time_t timestamp;
} LogEntry;

typedef struct {
    LogEntry logs[MAX_LOG_LINES];
    int log_count;
    
    char recent_detections[MAX_DETECTIONS][128];
    int detection_count;
    
    char recent_actions[MAX_ACTIONS][128];
    int action_count;
    
    // Graph state
    unsigned int nodes;
    unsigned int edges;
    unsigned int active_nodes;
    float error;
    unsigned int tick;
    
    // System stats
    float cpu_usage;
    float memory_usage;
    float temperature;
    unsigned int rx_frames;
    unsigned int tx_frames;
} MelvinState;

MelvinState g_state = {0};

void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

void get_terminal_size(int *rows, int *cols) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    *rows = w.ws_row;
    *cols = w.ws_col;
}

void clear_screen(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
}

void move_cursor(int row, int col) {
    printf("\033[%d;%dH", row, col);
}

void set_color(int fg, int bg, int bold) {
    if (bold) {
        printf("\033[1;%d;%dm", 30 + fg, 40 + bg);
    } else {
        printf("\033[0;%d;%dm", 30 + fg, 40 + bg);
    }
}

void reset_color(void) {
    printf("\033[0m");
}

void draw_box(int row, int col, int width, int height, const char *title) {
    move_cursor(row, col);
    
    // Top border
    printf("╔");
    for (int i = 0; i < width - 2; i++) printf("═");
    printf("╗");
    
    // Title
    if (title) {
        int title_len = strlen(title);
        int title_pos = (width - title_len - 2) / 2;
        move_cursor(row, col + title_pos);
        printf("║ %s ║", title);
    }
    
    // Sides
    for (int i = 1; i < height - 1; i++) {
        move_cursor(row + i, col);
        printf("║");
        move_cursor(row + i, col + width - 1);
        printf("║");
    }
    
    // Bottom border
    move_cursor(row + height - 1, col);
    printf("╚");
    for (int i = 0; i < width - 2; i++) printf("═");
    printf("╝");
}

void draw_header(int cols) {
    set_color(7, 0, 1);  // White on black, bold
    move_cursor(1, 1);
    
    int padding = (cols - 30) / 2;
    for (int i = 0; i < padding; i++) printf(" ");
    printf("🧠 MELVIN'S CONSCIOUSNESS 🧠");
    for (int i = 0; i < padding; i++) printf(" ");
    
    reset_color();
}

void draw_vital_signs(int start_row, int cols) {
    set_color(2, 0, 1);  // Green bold
    move_cursor(start_row, 2);
    printf("⚡ TICK: %u", g_state.tick);
    
    move_cursor(start_row, cols/4);
    printf("🧬 NODES: %u", g_state.nodes);
    
    move_cursor(start_row, cols/2);
    printf("🔗 EDGES: %u", g_state.edges);
    
    move_cursor(start_row, 3*cols/4);
    printf("💡 ACTIVE: %u", g_state.active_nodes);
    
    reset_color();
    
    // Progress bar for error
    move_cursor(start_row + 1, 2);
    printf("📊 ERROR: ");
    
    int bar_width = 30;
    float error_normalized = g_state.error > 1.0f ? 1.0f : g_state.error;
    int filled = (int)(error_normalized * bar_width);
    
    set_color(1, 0, 0);  // Red
    printf("[");
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) {
            printf("█");
        } else {
            printf("░");
        }
    }
    printf("]");
    reset_color();
    printf(" %.3f", g_state.error);
}

void draw_system_stats(int start_row, int start_col, int width) {
    set_color(6, 0, 1);  // Cyan bold
    move_cursor(start_row, start_col + 2);
    printf("SYSTEM STATUS");
    reset_color();
    
    move_cursor(start_row + 2, start_col + 2);
    printf("CPU:  %.1f%%", g_state.cpu_usage);
    
    move_cursor(start_row + 3, start_col + 2);
    printf("MEM:  %.1f%%", g_state.memory_usage);
    
    move_cursor(start_row + 4, start_col + 2);
    printf("TEMP: %.1f°C", g_state.temperature);
    
    move_cursor(start_row + 6, start_col + 2);
    set_color(3, 0, 0);  // Yellow
    printf("RX:   %u frames", g_state.rx_frames);
    
    move_cursor(start_row + 7, start_col + 2);
    printf("TX:   %u frames", g_state.tx_frames);
    reset_color();
}

void draw_detections(int start_row, int start_col, int width, int height) {
    set_color(5, 0, 1);  // Magenta bold
    move_cursor(start_row, start_col + 2);
    printf("RECENT DETECTIONS");
    reset_color();
    
    int display_count = height - 4;
    if (display_count > g_state.detection_count) {
        display_count = g_state.detection_count;
    }
    
    for (int i = 0; i < display_count; i++) {
        int idx = (g_state.detection_count - display_count + i) % MAX_DETECTIONS;
        move_cursor(start_row + 2 + i, start_col + 2);
        
        set_color(2, 0, 0);  // Green
        printf("➤ ");
        reset_color();
        
        printf("%.60s", g_state.recent_detections[idx]);
    }
}

void draw_actions(int start_row, int start_col, int width, int height) {
    set_color(4, 0, 1);  // Blue bold
    move_cursor(start_row, start_col + 2);
    printf("ACTIONS TAKEN");
    reset_color();
    
    int display_count = height - 4;
    if (display_count > g_state.action_count) {
        display_count = g_state.action_count;
    }
    
    for (int i = 0; i < display_count; i++) {
        int idx = (g_state.action_count - display_count + i) % MAX_ACTIONS;
        move_cursor(start_row + 2 + i, start_col + 2);
        
        set_color(3, 0, 0);  // Yellow
        printf("⚡ ");
        reset_color();
        
        printf("%.60s", g_state.recent_actions[idx]);
    }
}

void draw_thought_stream(int start_row, int start_col, int width, int height) {
    set_color(7, 0, 1);  // White bold
    move_cursor(start_row, start_col + 2);
    printf("THOUGHT STREAM");
    reset_color();
    
    int display_count = height - 4;
    if (display_count > g_state.log_count) {
        display_count = g_state.log_count;
    }
    
    for (int i = 0; i < display_count; i++) {
        int idx = (g_state.log_count - display_count + i) % MAX_LOG_LINES;
        move_cursor(start_row + 2 + i, start_col + 2);
        
        time_t now = time(NULL);
        int age = now - g_state.logs[idx].timestamp;
        
        if (age < 2) {
            set_color(7, 0, 1);  // Bright white
        } else if (age < 5) {
            set_color(7, 0, 0);  // Normal white
        } else {
            set_color(7, 0, 0);  // Dimmed
        }
        
        printf("%.70s", g_state.logs[idx].text);
        reset_color();
    }
}

void add_log(const char *text) {
    int idx = g_state.log_count % MAX_LOG_LINES;
    strncpy(g_state.logs[idx].text, text, 255);
    g_state.logs[idx].text[255] = '\0';
    g_state.logs[idx].timestamp = time(NULL);
    g_state.log_count++;
}

void add_detection(const char *text) {
    int idx = g_state.detection_count % MAX_DETECTIONS;
    strncpy(g_state.recent_detections[idx], text, 127);
    g_state.recent_detections[idx][127] = '\0';
    g_state.detection_count++;
}

void add_action(const char *text) {
    int idx = g_state.action_count % MAX_ACTIONS;
    strncpy(g_state.recent_actions[idx], text, 127);
    g_state.recent_actions[idx][127] = '\0';
    g_state.action_count++;
}

void update_display(void) {
    int rows, cols;
    get_terminal_size(&rows, &cols);
    
    clear_screen();
    
    // Header
    draw_header(cols);
    
    // Vital signs
    draw_vital_signs(3, cols);
    
    int mid_row = rows / 2;
    int mid_col = cols / 2;
    
    // Top section (system status + detections)
    draw_box(6, 2, cols/2 - 2, mid_row - 5, NULL);
    draw_system_stats(6, 2, cols/2 - 2);
    
    draw_box(6, cols/2 + 1, cols/2 - 2, mid_row - 5, NULL);
    draw_detections(6, cols/2 + 1, cols/2 - 2, mid_row - 5);
    
    // Middle section (actions)
    draw_box(mid_row + 1, 2, cols/2 - 2, mid_row - 5, NULL);
    draw_actions(mid_row + 1, 2, cols/2 - 2, mid_row - 5);
    
    // Thought stream (large right panel)
    draw_box(mid_row + 1, cols/2 + 1, cols/2 - 2, mid_row - 5, NULL);
    draw_thought_stream(mid_row + 1, cols/2 + 1, cols/2 - 2, mid_row - 5);
    
    // Footer
    move_cursor(rows - 1, 2);
    set_color(6, 0, 0);  // Cyan
    printf("Press Ctrl+C to exit");
    reset_color();
    
    fflush(stdout);
}

void parse_melvin_output(const char *line) {
    // Parse tick information
    if (strstr(line, "[TICK")) {
        sscanf(line, "[TICK %u] nodes=%u edges=%u active=%u err=%f",
               &g_state.tick, &g_state.nodes, &g_state.edges,
               &g_state.active_nodes, &g_state.error);
        
        char log[256];
        snprintf(log, sizeof(log), "Learning cycle complete. Graph: %u nodes, %u edges",
                 g_state.nodes, g_state.edges);
        add_log(log);
    }
    
    // Parse daemon stats
    if (strstr(line, "RX buffer:")) {
        sscanf(line, "[MELVIND] RX buffer: %u frames | TX buffer: %u frames",
               &g_state.rx_frames, &g_state.tx_frames);
    }
    
    // Parse detections
    if (strstr(line, "proc:cpu") || strstr(line, "sys:temp") || 
        strstr(line, "net:") || strstr(line, "can:")) {
        add_detection(line);
    }
    
    // General log
    if (line[0] == '[') {
        add_log(line);
    }
}

void simulate_activity(void) {
    // Read from melvin_core and daemon logs
    FILE *fp = popen("ssh melvin@169.254.123.100 'tail -f /tmp/melvin_core.log 2>/dev/null & sudo journalctl -u melvind -f --no-pager' 2>/dev/null", "r");
    
    if (!fp) {
        add_log("Waiting for Melvin to start...");
        return;
    }
    
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        if (strlen(line) > 0) {
            parse_melvin_output(line);
        }
    }
    
    pclose(fp);
}

int main(void) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Disable input buffering
    struct termios old_tio, new_tio;
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= (~ICANON & ~ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
    
    // Hide cursor
    printf("\033[?25l");
    fflush(stdout);
    
    // Initial state
    add_log("Melvin consciousness initialized...");
    add_log("Connecting to Jetson neural substrate...");
    add_log("Loading graph from memory...");
    add_detection("System boot detected");
    add_action("Observing environment");
    
    g_state.nodes = 9;
    g_state.edges = 0;
    g_state.active_nodes = 0;
    g_state.error = 0.0f;
    
    // Main display loop
    int update_counter = 0;
    while (running) {
        update_display();
        
        // Update state every few cycles
        if (update_counter++ % 5 == 0) {
            // Simulate some activity for testing
            g_state.tick++;
            g_state.cpu_usage = 5.0f + (rand() % 30) / 10.0f;
            g_state.memory_usage = 45.0f + (rand() % 100) / 10.0f;
            g_state.temperature = 55.0f + (rand() % 80) / 10.0f;
            
            // Could read actual data from files here
        }
        
        usleep(200000);  // 200ms refresh
    }
    
    // Restore terminal
    printf("\033[?25h");  // Show cursor
    clear_screen();
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
    
    return 0;
}

