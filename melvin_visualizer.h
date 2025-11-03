/*
 * melvin_visualizer.h - MelvinOS Visualization Interface
 * Real-time cognition, brain graph, and performance metrics display
 */

#ifndef MELVIN_VISUALIZER_H
#define MELVIN_VISUALIZER_H

#include <string>
#include <vector>
#include <deque>
#include <map>
#include <memory>
#include <chrono>

// ============================================================================
// Data Structures
// ============================================================================

enum class EventType {
    THOUGHT,
    PERCEPTION,
    LEARNING,
    CONTEXT,
    GRAPH_UPDATE,
    METRIC
};

struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    std::string context_id;
    EventType type;
    std::string message;
    float fade_alpha = 1.0f;  // For fade-in animation
};

struct GraphNode {
    uint32_t id;
    std::string label;
    float x, y, z;  // Position in 3D space
    float activation;
    float brightness;  // Smoothed activation for rendering
    float pulse_timer;
    std::string category;
    std::string context;
    int edge_count;
    
    // Color based on category
    float r, g, b;
};

struct GraphEdge {
    uint32_t from_id;
    uint32_t to_id;
    float weight;
    float glow_intensity;
};

struct SystemMetrics {
    float cpu_usage;
    float gpu_usage;
    float ram_usage;
    float vram_usage;
    float tick_rate;
    int active_nodes;
    int total_edges;
    float mean_error;
    float motor_latency;
    std::string status;  // "ACTIVE", "LEARNING", "IDLE"
    
    std::deque<float> cpu_history;
    std::deque<float> tick_rate_history;
    std::deque<float> error_history;
};

// ============================================================================
// Panel Classes
// ============================================================================

class TerminalPanel {
public:
    TerminalPanel(int max_lines = 1000);
    
    void AddEntry(const LogEntry& entry);
    void Update(float delta_time);
    void Render();
    void Clear();
    
private:
    std::deque<LogEntry> entries_;
    int max_lines_;
    float scroll_position_;
    bool auto_scroll_;
};

class GraphPanel {
public:
    GraphPanel();
    
    void UpdateNode(uint32_t id, float activation);
    void AddNode(const GraphNode& node);
    void AddEdge(const GraphEdge& edge);
    void Update(float delta_time);
    void Render(float width, float height);
    
    void SetAutoRotate(bool enable) { auto_rotate_ = enable; }
    void HandleInput();  // Mouse drag, zoom
    
private:
    std::map<uint32_t, GraphNode> nodes_;
    std::vector<GraphEdge> edges_;
    
    // Camera control
    float camera_theta_;
    float camera_phi_;
    float camera_distance_;
    bool auto_rotate_;
    bool mouse_dragging_;
    float last_mouse_x_;
    float last_mouse_y_;
    
    // Rendering state
    uint32_t hovered_node_id_;
    
    void UpdateNodeBrightness(float delta_time);
    void UpdatePulseTimers(float delta_time);
    void RenderNodes();
    void RenderEdges();
    void RenderTooltip();
    void ProjectToScreen(float x, float y, float z, float& sx, float& sy);
};

class MetricsPanel {
public:
    MetricsPanel(int sparkline_points = 100);
    
    void UpdateMetrics(const SystemMetrics& metrics);
    void Update(float delta_time);
    void Render(float width, float height);
    
private:
    SystemMetrics current_metrics_;
    int sparkline_points_;
    
    void RenderMetricRow(const char* label, float value, const char* unit, 
                        const std::deque<float>& history, float threshold = -1.0f);
    void RenderSparkline(const std::deque<float>& data, float max_val);
    void RenderStatusIndicator();
};

// ============================================================================
// Main Visualizer
// ============================================================================

class MelvinVisualizer {
public:
    MelvinVisualizer();
    ~MelvinVisualizer();
    
    bool Initialize();
    void Run();
    void Shutdown();
    
private:
    // Panels
    std::unique_ptr<TerminalPanel> terminal_panel_;
    std::unique_ptr<GraphPanel> graph_panel_;
    std::unique_ptr<MetricsPanel> metrics_panel_;
    
    // Data connection
    int socket_fd_;
    bool connected_;
    
    // Main loop
    bool running_;
    float delta_time_;
    std::chrono::steady_clock::time_point last_frame_time_;
    
    // Methods
    bool InitializeWindow();
    bool InitializeSocket();
    bool ConnectToMelvinCore();
    
    void ProcessEvents();
    void ParseJsonEvent(const std::string& json_line);
    void UpdatePanels(float dt);
    void RenderUI();
    
    void HandleThoughtEvent(const std::string& text, const std::string& type, 
                           const std::string& context, uint64_t timestamp);
    void HandleGraphUpdate(uint32_t node_id, float activation);
    void HandleMetricUpdate(const SystemMetrics& metrics);
};

// ============================================================================
// Color Utilities
// ============================================================================

inline void GetTypeColor(EventType type, float& r, float& g, float& b) {
    switch (type) {
        case EventType::THOUGHT:
            r = 1.0f; g = 1.0f; b = 1.0f;  // White
            break;
        case EventType::PERCEPTION:
            r = 0.0f; g = 1.0f; b = 1.0f;  // Cyan
            break;
        case EventType::LEARNING:
            r = 1.0f; g = 1.0f; b = 0.0f;  // Yellow
            break;
        case EventType::CONTEXT:
            r = 1.0f; g = 0.0f; b = 1.0f;  // Magenta
            break;
        default:
            r = 0.7f; g = 0.7f; b = 0.7f;  // Gray
            break;
    }
}

inline void GetCategoryColor(const std::string& category, float& r, float& g, float& b) {
    // Hash category string to consistent color
    uint32_t hash = 0;
    for (char c : category) {
        hash = hash * 31 + c;
    }
    
    // Generate pleasing colors in HSV space
    float hue = (hash % 360) / 360.0f;
    float sat = 0.7f;
    float val = 0.9f;
    
    // Simple HSV to RGB conversion
    float h = hue * 6.0f;
    float c = val * sat;
    float x = c * (1.0f - std::abs(std::fmod(h, 2.0f) - 1.0f));
    float m = val - c;
    
    int hi = (int)h;
    switch (hi) {
        case 0: r = c; g = x; b = 0; break;
        case 1: r = x; g = c; b = 0; break;
        case 2: r = 0; g = c; b = x; break;
        case 3: r = 0; g = x; b = c; break;
        case 4: r = x; g = 0; b = c; break;
        default: r = c; g = 0; b = x; break;
    }
    r += m; g += m; b += m;
}

#endif // MELVIN_VISUALIZER_H

