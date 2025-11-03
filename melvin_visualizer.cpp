/*
 * melvin_visualizer.cpp - MelvinOS Visualization Interface v1.0
 * 
 * Full-screen 3-panel display:
 *   - Left:  Mind stream (scrolling thoughts)
 *   - Right Top: 3D brain graph with activation pulses
 *   - Right Bottom: Performance metrics with sparklines
 */

#include "melvin_visualizer.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ============================================================================
// Terminal Panel Implementation
// ============================================================================

TerminalPanel::TerminalPanel(int max_lines) 
    : max_lines_(max_lines)
    , scroll_position_(0.0f)
    , auto_scroll_(true) {
}

void TerminalPanel::AddEntry(const LogEntry& entry) {
    entries_.push_back(entry);
    if (entries_.size() > max_lines_) {
        entries_.pop_front();
    }
}

void TerminalPanel::Update(float delta_time) {
    // Update fade-in animations
    for (auto& entry : entries_) {
        if (entry.fade_alpha < 1.0f) {
            entry.fade_alpha += delta_time * 3.0f;
            if (entry.fade_alpha > 1.0f) entry.fade_alpha = 1.0f;
        }
    }
    
    // Auto-scroll
    if (auto_scroll_) {
        scroll_position_ = static_cast<float>(entries_.size());
    }
}

void TerminalPanel::Render() {
    ImGui::BeginChild("TerminalScroll", ImVec2(0, 0), true);
    
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
    
    for (const auto& entry : entries_) {
        // Format timestamp
        time_t t = static_cast<time_t>(entry.timestamp.time_since_epoch().count() / 1000000000);
        struct tm* tm_info = localtime(&t);
        char time_buf[32];
        strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);
        
        // Get color for type
        float r, g, b;
        GetTypeColor(entry.type, r, g, b);
        
        // Apply fade
        ImVec4 color(r, g, b, entry.fade_alpha);
        
        // Get icon
        const char* icon = "";
        switch (entry.type) {
            case EventType::THOUGHT: icon = "🧠"; break;
            case EventType::PERCEPTION: icon = "👁"; break;
            case EventType::LEARNING: icon = "🔁"; break;
            case EventType::CONTEXT: icon = "🎯"; break;
        }
        
        // Render line
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(time_buf);
        ImGui::SameLine();
        ImGui::Text(" %s %s | [%-10s] %s", 
                   icon,
                   entry.context_id.substr(0, 8).c_str(),
                   entry.type == EventType::THOUGHT ? "thought" :
                   entry.type == EventType::PERCEPTION ? "perception" :
                   entry.type == EventType::LEARNING ? "learning" : "context",
                   entry.message.c_str());
        ImGui::PopStyleColor();
    }
    
    ImGui::PopStyleVar();
    
    // Auto-scroll to bottom
    if (auto_scroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    
    ImGui::EndChild();
}

void TerminalPanel::Clear() {
    entries_.clear();
}

// ============================================================================
// Graph Panel Implementation
// ============================================================================

GraphPanel::GraphPanel()
    : camera_theta_(0.0f)
    , camera_phi_(M_PI / 4.0f)
    , camera_distance_(50.0f)
    , auto_rotate_(true)
    , mouse_dragging_(false)
    , last_mouse_x_(0.0f)
    , last_mouse_y_(0.0f)
    , hovered_node_id_(0) {
}

void GraphPanel::UpdateNode(uint32_t id, float activation) {
    if (nodes_.find(id) != nodes_.end()) {
        nodes_[id].activation = activation;
        if (activation > 0.8f) {
            nodes_[id].pulse_timer = 0.2f;
        }
    }
}

void GraphPanel::AddNode(const GraphNode& node) {
    nodes_[node.id] = node;
}

void GraphPanel::AddEdge(const GraphEdge& edge) {
    edges_.push_back(edge);
}

void GraphPanel::Update(float delta_time) {
    UpdateNodeBrightness(delta_time);
    UpdatePulseTimers(delta_time);
    
    if (auto_rotate_) {
        camera_theta_ += delta_time * 0.3f;
    }
}

void GraphPanel::UpdateNodeBrightness(float delta_time) {
    for (auto& pair : nodes_) {
        GraphNode& node = pair.second;
        // Smooth lerp to target activation
        node.brightness += (node.activation - node.brightness) * 0.15f * (delta_time * 60.0f);
    }
}

void GraphPanel::UpdatePulseTimers(float delta_time) {
    for (auto& pair : nodes_) {
        GraphNode& node = pair.second;
        if (node.pulse_timer > 0.0f) {
            node.pulse_timer -= delta_time;
        }
    }
}

void GraphPanel::ProjectToScreen(float x, float y, float z, float& sx, float& sy) {
    // Rotate around Y axis
    float cos_theta = std::cos(camera_theta_);
    float sin_theta = std::sin(camera_theta_);
    float x_rot = x * cos_theta - z * sin_theta;
    float z_rot = x * sin_theta + z * cos_theta;
    
    // Rotate around X axis
    float cos_phi = std::cos(camera_phi_);
    float sin_phi = std::sin(camera_phi_);
    float y_rot = y * cos_phi - z_rot * sin_phi;
    float z_final = y * sin_phi + z_rot * cos_phi;
    
    // Perspective projection
    float scale = camera_distance_ / (camera_distance_ + z_final);
    sx = x_rot * scale;
    sy = -y_rot * scale;
}

void GraphPanel::Render(float width, float height) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size(width, height);
    
    // Background
    draw_list->AddRectFilled(canvas_pos, 
                            ImVec2(canvas_pos.x + width, canvas_pos.y + height),
                            IM_COL32(10, 10, 10, 255));
    
    ImVec2 center(canvas_pos.x + width / 2, canvas_pos.y + height / 2);
    float scale = std::min(width, height) / 3.0f;
    
    // Render edges first (behind nodes)
    RenderEdges();
    
    // Render nodes
    RenderNodes();
    
    // Tooltip for hovered node
    if (hovered_node_id_ > 0) {
        RenderTooltip();
    }
    
    // Dummy for mouse interaction
    ImGui::InvisibleButton("graph_canvas", canvas_size);
    HandleInput();
}

void GraphPanel::RenderEdges() {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetItemRectSize();
    ImVec2 center(canvas_pos.x + canvas_size.x / 2, canvas_pos.y + canvas_size.y / 2);
    float scale = std::min(canvas_size.x, canvas_size.y) / 3.0f;
    
    for (const auto& edge : edges_) {
        if (nodes_.find(edge.from_id) == nodes_.end() || 
            nodes_.find(edge.to_id) == nodes_.end()) {
            continue;
        }
        
        const GraphNode& n1 = nodes_[edge.from_id];
        const GraphNode& n2 = nodes_[edge.to_id];
        
        float sx1, sy1, sx2, sy2;
        ProjectToScreen(n1.x, n1.y, n1.z, sx1, sy1);
        ProjectToScreen(n2.x, n2.y, n2.z, sx2, sy2);
        
        ImVec2 p1(center.x + sx1 * scale, center.y + sy1 * scale);
        ImVec2 p2(center.x + sx2 * scale, center.y + sy2 * scale);
        
        // Edge glow based on node activation
        float glow = (n1.brightness + n2.brightness) / 2.0f;
        int alpha = static_cast<int>(50 + glow * 150);
        
        draw_list->AddLine(p1, p2, IM_COL32(100, 100, 200, alpha), 1.5f);
    }
}

void GraphPanel::RenderNodes() {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetItemRectSize();
    ImVec2 center(canvas_pos.x + canvas_size.x / 2, canvas_pos.y + canvas_size.y / 2);
    float scale = std::min(canvas_size.x, canvas_size.y) / 3.0f;
    
    // Sort by Z for proper depth
    std::vector<std::pair<uint32_t, const GraphNode*>> sorted_nodes;
    for (const auto& pair : nodes_) {
        sorted_nodes.push_back({pair.first, &pair.second});
    }
    std::sort(sorted_nodes.begin(), sorted_nodes.end(),
             [](const auto& a, const auto& b) { return a.second->z > b.second->z; });
    
    for (const auto& [id, node_ptr] : sorted_nodes) {
        const GraphNode& node = *node_ptr;
        
        float sx, sy;
        ProjectToScreen(node.x, node.y, node.z, sx, sy);
        
        ImVec2 pos(center.x + sx * scale, center.y + sy * scale);
        
        // Base radius with activation scaling
        float base_radius = 5.0f;
        if (node.pulse_timer > 0.0f) {
            base_radius *= (1.0f + node.pulse_timer * 3.0f);
        }
        float radius = base_radius * (0.5f + node.brightness * 0.5f);
        
        // Node color with brightness multiplier
        float brightness_mult = 0.3f + node.brightness * 0.7f;
        ImU32 color = IM_COL32(
            static_cast<int>(node.r * brightness_mult * 255),
            static_cast<int>(node.g * brightness_mult * 255),
            static_cast<int>(node.b * brightness_mult * 255),
            255
        );
        
        // Glow effect for high activation
        if (node.brightness > 0.7f) {
            float glow_radius = radius * 2.0f;
            int glow_alpha = static_cast<int>(node.brightness * 100);
            ImU32 glow_color = IM_COL32(
                static_cast<int>(node.r * 255),
                static_cast<int>(node.g * 255),
                static_cast<int>(node.b * 255),
                glow_alpha
            );
            draw_list->AddCircleFilled(pos, glow_radius, glow_color, 16);
        }
        
        // Main node
        draw_list->AddCircleFilled(pos, radius, color, 12);
        
        // Check if mouse hovering
        ImVec2 mouse_pos = ImGui::GetMousePos();
        float dist = std::sqrt((mouse_pos.x - pos.x) * (mouse_pos.x - pos.x) +
                              (mouse_pos.y - pos.y) * (mouse_pos.y - pos.y));
        if (dist < radius) {
            hovered_node_id_ = id;
        }
    }
}

void GraphPanel::RenderTooltip() {
    if (nodes_.find(hovered_node_id_) == nodes_.end()) {
        return;
    }
    
    const GraphNode& node = nodes_[hovered_node_id_];
    
    ImGui::BeginTooltip();
    ImGui::Text("ID: %u", node.id);
    ImGui::Text("Label: %s", node.label.c_str());
    ImGui::Text("Activation: %.2f", node.activation);
    ImGui::Text("Edges: %d", node.edge_count);
    ImGui::Text("Context: %s", node.context.c_str());
    ImGui::EndTooltip();
}

void GraphPanel::HandleInput() {
    ImGuiIO& io = ImGui::GetIO();
    
    if (ImGui::IsItemHovered()) {
        // Mouse drag to rotate
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            if (!mouse_dragging_) {
                last_mouse_x_ = io.MousePos.x;
                last_mouse_y_ = io.MousePos.y;
                mouse_dragging_ = true;
                auto_rotate_ = false;
            } else {
                float dx = io.MousePos.x - last_mouse_x_;
                float dy = io.MousePos.y - last_mouse_y_;
                camera_theta_ += dx * 0.01f;
                camera_phi_ += dy * 0.01f;
                camera_phi_ = std::clamp(camera_phi_, 0.1f, float(M_PI) - 0.1f);
                last_mouse_x_ = io.MousePos.x;
                last_mouse_y_ = io.MousePos.y;
            }
        } else {
            mouse_dragging_ = false;
        }
        
        // Mouse wheel to zoom
        if (io.MouseWheel != 0.0f) {
            camera_distance_ -= io.MouseWheel * 5.0f;
            camera_distance_ = std::clamp(camera_distance_, 20.0f, 100.0f);
        }
    }
    
    // Spacebar to toggle auto-rotate
    if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
        auto_rotate_ = !auto_rotate_;
    }
}

// ============================================================================
// Metrics Panel Implementation
// ============================================================================

MetricsPanel::MetricsPanel(int sparkline_points)
    : sparkline_points_(sparkline_points) {
}

void MetricsPanel::UpdateMetrics(const SystemMetrics& metrics) {
    current_metrics_ = metrics;
}

void MetricsPanel::Update(float delta_time) {
    // Update handled externally via UpdateMetrics
}

void MetricsPanel::Render(float width, float height) {
    RenderStatusIndicator();
    
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::Text("Performance:");
    RenderMetricRow("CPU", current_metrics_.cpu_usage, "%%", 
                   current_metrics_.cpu_history, 90.0f);
    RenderMetricRow("GPU", current_metrics_.gpu_usage, "%%", 
                   current_metrics_.gpu_history, 90.0f);
    RenderMetricRow("RAM", current_metrics_.ram_usage, "%%", 
                   current_metrics_.ram_history, 95.0f);
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::Text("Cognition:");
    RenderMetricRow("Tick Rate", current_metrics_.tick_rate, " Hz", 
                   current_metrics_.tick_rate_history, -1.0f);
    ImGui::Text("  Active Nodes:    %6d", current_metrics_.active_nodes);
    ImGui::Text("  Total Edges:     %6d", current_metrics_.total_edges);
    RenderMetricRow("Mean Error", current_metrics_.mean_error, "", 
                   current_metrics_.error_history, 0.1f);
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::Text("Motor:");
    ImGui::Text("  Latency:         %6.2f ms", current_metrics_.motor_latency);
}

void MetricsPanel::RenderStatusIndicator() {
    const char* status_icon = "⚫";
    ImVec4 status_color(0.5f, 0.5f, 0.5f, 1.0f);
    
    if (current_metrics_.status == "ACTIVE") {
        status_icon = "🟢";
        status_color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
    } else if (current_metrics_.status == "LEARNING") {
        status_icon = "🟡";
        status_color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
    } else if (current_metrics_.status == "IDLE") {
        status_icon = "⚪";
        status_color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    }
    
    ImGui::PushStyleColor(ImGuiCol_Text, status_color);
    ImGui::Text("%s System Status: %s", status_icon, current_metrics_.status.c_str());
    ImGui::PopStyleColor();
}

void MetricsPanel::RenderMetricRow(const char* label, float value, const char* unit,
                                  const std::deque<float>& history, float threshold) {
    ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    if (threshold > 0 && value > threshold) {
        color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // Red if over threshold
    }
    
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::Text("  %-12s %6.2f%s", label, value, unit);
    ImGui::PopStyleColor();
    
    if (!history.empty()) {
        ImGui::SameLine();
        RenderSparkline(history, threshold > 0 ? threshold * 1.2f : 100.0f);
    }
}

void MetricsPanel::RenderSparkline(const std::deque<float>& data, float max_val) {
    if (data.empty()) return;
    
    ImVec2 size(80, 20);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    
    draw_list->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), 
                      IM_COL32(100, 100, 100, 255));
    
    // Draw sparkline
    std::vector<ImVec2> points;
    for (size_t i = 0; i < data.size(); ++i) {
        float x = pos.x + (i / static_cast<float>(data.size())) * size.x;
        float normalized = std::clamp(data[i] / max_val, 0.0f, 1.0f);
        float y = pos.y + size.y - (normalized * size.y);
        points.push_back(ImVec2(x, y));
    }
    
    if (points.size() >= 2) {
        draw_list->AddPolyline(points.data(), points.size(), 
                              IM_COL32(0, 255, 100, 255), 0, 1.5f);
    }
    
    ImGui::Dummy(size);
}

// ============================================================================
// Main Visualizer Implementation
// ============================================================================

MelvinVisualizer::MelvinVisualizer()
    : socket_fd_(-1)
    , connected_(false)
    , running_(false)
    , delta_time_(0.0f) {
    
    terminal_panel_ = std::make_unique<TerminalPanel>();
    graph_panel_ = std::make_unique<GraphPanel>();
    metrics_panel_ = std::make_unique<MetricsPanel>();
}

MelvinVisualizer::~MelvinVisualizer() {
    Shutdown();
}

bool MelvinVisualizer::Initialize() {
    if (!InitializeWindow()) {
        return false;
    }
    
    if (!InitializeSocket()) {
        std::cerr << "Warning: Could not connect to Melvin core, running in demo mode\n";
    }
    
    return true;
}

bool MelvinVisualizer::InitializeWindow() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }
    
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    
    // Create fullscreen window
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, 
                                         "MelvinOS Visualization", 
                                         monitor, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return false;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // Enable vsync
    
    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Setup style (dark neural theme)
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.0f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.02f, 0.02f, 0.02f, 1.0f);
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    return true;
}

bool MelvinVisualizer::InitializeSocket() {
    socket_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        return false;
    }
    
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/melvin_feed.sock", sizeof(addr.sun_path) - 1);
    
    if (connect(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    // Set non-blocking
    fcntl(socket_fd_, F_SETFL, O_NONBLOCK);
    connected_ = true;
    
    return true;
}

void MelvinVisualizer::Run() {
    running_ = true;
    last_frame_time_ = std::chrono::steady_clock::now();
    
    GLFWwindow* window = glfwGetCurrentContext();
    
    while (!glfwWindowShouldClose(window) && running_) {
        glfwPollEvents();
        
        auto current_time = std::chrono::steady_clock::now();
        delta_time_ = std::chrono::duration<float>(current_time - last_frame_time_).count();
        last_frame_time_ = current_time;
        
        ProcessEvents();
        UpdatePanels(delta_time_);
        RenderUI();
        
        glfwSwapBuffers(window);
    }
}

void MelvinVisualizer::ProcessEvents() {
    if (socket_fd_ >= 0) {
        char buffer[4096];
        ssize_t n = read(socket_fd_, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            std::string data(buffer);
            
            // Parse newline-delimited JSON
            size_t pos = 0;
            while ((pos = data.find('\n')) != std::string::npos) {
                std::string line = data.substr(0, pos);
                ParseJsonEvent(line);
                data.erase(0, pos + 1);
            }
        }
    }
}

void MelvinVisualizer::ParseJsonEvent(const std::string& json_line) {
    try {
        auto j = json::parse(json_line);
        std::string type = j.value("type", "");
        
        if (type == "thought" || type == "perception" || 
            type == "learning" || type == "context") {
            
            EventType event_type = EventType::THOUGHT;
            if (type == "perception") event_type = EventType::PERCEPTION;
            else if (type == "learning") event_type = EventType::LEARNING;
            else if (type == "context") event_type = EventType::CONTEXT;
            
            HandleThoughtEvent(
                j.value("text", ""),
                type,
                j.value("context", "main"),
                j.value("timestamp", 0)
            );
        }
        else if (type == "graph_update") {
            HandleGraphUpdate(
                j.value("node_id", 0),
                j.value("activation", 0.0f)
            );
        }
        else if (type == "metric") {
            SystemMetrics metrics;
            metrics.cpu_usage = j.value("cpu", 0.0f);
            metrics.gpu_usage = j.value("gpu", 0.0f);
            metrics.ram_usage = j.value("ram", 0.0f);
            metrics.tick_rate = j.value("tick_rate", 0.0f);
            metrics.active_nodes = j.value("active_nodes", 0);
            metrics.total_edges = j.value("total_edges", 0);
            metrics.mean_error = j.value("mean_error", 0.0f);
            metrics.motor_latency = j.value("motor_latency", 0.0f);
            metrics.status = j.value("status", "IDLE");
            HandleMetricUpdate(metrics);
        }
    }
    catch (const json::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }
}

void MelvinVisualizer::HandleThoughtEvent(const std::string& text, 
                                         const std::string& type,
                                         const std::string& context, 
                                         uint64_t timestamp) {
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.context_id = context;
    entry.message = text;
    entry.fade_alpha = 0.0f;  // Start faded, will fade in
    
    if (type == "thought") entry.type = EventType::THOUGHT;
    else if (type == "perception") entry.type = EventType::PERCEPTION;
    else if (type == "learning") entry.type = EventType::LEARNING;
    else if (type == "context") entry.type = EventType::CONTEXT;
    
    terminal_panel_->AddEntry(entry);
}

void MelvinVisualizer::HandleGraphUpdate(uint32_t node_id, float activation) {
    graph_panel_->UpdateNode(node_id, activation);
}

void MelvinVisualizer::HandleMetricUpdate(const SystemMetrics& metrics) {
    metrics_panel_->UpdateMetrics(metrics);
}

void MelvinVisualizer::UpdatePanels(float dt) {
    terminal_panel_->Update(dt);
    graph_panel_->Update(dt);
    metrics_panel_->Update(dt);
}

void MelvinVisualizer::RenderUI() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    // Fullscreen window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | 
                                    ImGuiWindowFlags_NoMove | 
                                    ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus;
    
    ImGui::Begin("MelvinOS", nullptr, window_flags);
    
    // Header
    ImGui::Text("🧠 MELVINOS VISUALIZATION v1.0");
    ImGui::SameLine(ImGui::GetWindowWidth() - 200);
    ImGui::Text("Press ESC to exit");
    ImGui::Separator();
    
    // Get available space
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float left_width = avail.x * 0.5f;
    float right_width = avail.x - left_width - 10;
    float top_height = avail.y * 0.5f;
    float bottom_height = avail.y - top_height - 10;
    
    // Left panel - Terminal
    ImGui::BeginChild("TerminalPanel", ImVec2(left_width, avail.y), true);
    ImGui::Text("🧠 MELVIN'S MIND STREAM");
    ImGui::Separator();
    terminal_panel_->Render();
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    // Right column
    ImGui::BeginGroup();
    
    // Top right - Graph
    ImGui::BeginChild("GraphPanel", ImVec2(right_width, top_height), true);
    ImGui::Text("🧠 BRAIN ACTIVITY MAP");
    ImGui::Separator();
    graph_panel_->Render(right_width - 20, top_height - 40);
    ImGui::EndChild();
    
    // Bottom right - Metrics
    ImGui::BeginChild("MetricsPanel", ImVec2(right_width, bottom_height), true);
    ImGui::Text("📊 PERFORMANCE METRICS");
    ImGui::Separator();
    metrics_panel_->Render(right_width, bottom_height);
    ImGui::EndChild();
    
    ImGui::EndGroup();
    
    ImGui::End();
    
    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.04f, 0.04f, 0.04f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void MelvinVisualizer::Shutdown() {
    running_ = false;
    
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwTerminate();
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    std::cout << "╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║     MELVINOS VISUALIZATION v1.0                      ║\n";
    std::cout << "╚══════════════════════════════════════════════════════╝\n";
    std::cout << "\nInitializing...\n";
    
    MelvinVisualizer visualizer;
    
    if (!visualizer.Initialize()) {
        std::cerr << "Failed to initialize visualizer\n";
        return 1;
    }
    
    std::cout << "✅ Initialized\n";
    std::cout << "Starting visualization...\n\n";
    
    visualizer.Run();
    visualizer.Shutdown();
    
    return 0;
}

