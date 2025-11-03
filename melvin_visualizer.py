#!/usr/bin/env python3
"""
MelvinOS Visualization Interface v1.0
Real-time cognition, brain graph, and performance metrics display
"""

import dearpygui.dearpygui as dpg
import socket
import json
import threading
import queue
import time
import math
from collections import deque
from dataclasses import dataclass
from typing import Dict, List, Tuple
from datetime import datetime

# ============================================================================
# Data Structures
# ============================================================================

@dataclass
class LogEntry:
    timestamp: float
    context_id: str
    event_type: str
    message: str
    fade_alpha: float = 1.0

@dataclass
class GraphNode:
    id: int
    label: str
    x: float
    y: float
    z: float
    activation: float = 0.0
    brightness: float = 0.0
    pulse_timer: float = 0.0
    category: str = "default"
    context: str = ""
    edge_count: int = 0
    color: Tuple[float, float, float] = (0.5, 0.5, 0.5)

@dataclass
class GraphEdge:
    from_id: int
    to_id: int
    weight: float = 1.0
    glow_intensity: float = 0.0

@dataclass
class SystemMetrics:
    cpu_usage: float = 0.0
    gpu_usage: float = 0.0
    ram_usage: float = 0.0
    vram_usage: float = 0.0
    tick_rate: float = 0.0
    active_nodes: int = 0
    total_edges: int = 0
    mean_error: float = 0.0
    motor_latency: float = 0.0
    status: str = "IDLE"

# ============================================================================
# Terminal Panel
# ============================================================================

class TerminalPanel:
    def __init__(self, max_lines=1000):
        self.entries = deque(maxlen=max_lines)
        self.max_lines = max_lines
        self.auto_scroll = True
        
    def add_entry(self, entry: LogEntry):
        self.entries.append(entry)
        
    def get_color(self, event_type: str) -> Tuple[int, int, int]:
        colors = {
            "thought": (255, 255, 255),      # White
            "perception": (0, 255, 255),     # Cyan
            "learning": (255, 255, 0),       # Yellow
            "context": (255, 0, 255),        # Magenta
        }
        return colors.get(event_type, (180, 180, 180))
    
    def get_icon(self, event_type: str) -> str:
        icons = {
            "thought": "🧠",
            "perception": "👁",
            "learning": "🔁",
            "context": "🎯",
        }
        return icons.get(event_type, "•")
    
    def render(self, parent):
        """Render terminal panel"""
        if not dpg.does_item_exist("terminal_window"):
            with dpg.child_window(tag="terminal_window", parent=parent, 
                                 width=-1, height=-1):
                dpg.add_text("", tag="terminal_text", wrap=0)
        
        # Update text
        lines = []
        for entry in list(self.entries)[-100:]:  # Show last 100 lines
            timestamp = datetime.fromtimestamp(entry.timestamp).strftime("%H:%M:%S")
            icon = self.get_icon(entry.event_type)
            line = f"{timestamp} | {entry.context_id[:8]} | {icon} [{entry.event_type}] {entry.message}"
            lines.append(line)
        
        if dpg.does_item_exist("terminal_text"):
            dpg.set_value("terminal_text", "\n".join(lines))

# ============================================================================
# Graph Panel (3D Brain Visualization)
# ============================================================================

class GraphPanel:
    def __init__(self):
        self.nodes: Dict[int, GraphNode] = {}
        self.edges: List[GraphEdge] = []
        self.camera_theta = 0.0
        self.camera_phi = math.pi / 4
        self.camera_distance = 50.0
        self.auto_rotate = True
        self.hovered_node_id = None
        
    def update_node(self, node_id: int, activation: float):
        if node_id in self.nodes:
            node = self.nodes[node_id]
            node.activation = activation
            if activation > 0.8:
                node.pulse_timer = 0.2
    
    def add_node(self, node: GraphNode):
        self.nodes[node.id] = node
    
    def add_edge(self, edge: GraphEdge):
        self.edges.append(edge)
    
    def update(self, delta_time: float):
        # Update node brightness (smooth lerp)
        for node in self.nodes.values():
            target = node.activation
            node.brightness += (target - node.brightness) * 0.15
            
            # Update pulse timer
            if node.pulse_timer > 0:
                node.pulse_timer -= delta_time
        
        # Auto-rotate camera
        if self.auto_rotate:
            self.camera_theta += delta_time * 0.2
    
    def project_to_screen(self, x, y, z, width, height):
        """Simple 3D to 2D projection"""
        # Rotate around camera
        cos_theta = math.cos(self.camera_theta)
        sin_theta = math.sin(self.camera_theta)
        cos_phi = math.cos(self.camera_phi)
        sin_phi = math.sin(self.camera_phi)
        
        # Rotate Y
        x_rot = x * cos_theta - z * sin_theta
        z_rot = x * sin_theta + z * cos_theta
        
        # Rotate X
        y_rot = y * cos_phi - z_rot * sin_phi
        z_final = y * sin_phi + z_rot * cos_phi
        
        # Perspective projection
        scale = self.camera_distance / (self.camera_distance + z_final)
        screen_x = width / 2 + x_rot * scale * 10
        screen_y = height / 2 - y_rot * scale * 10
        
        return screen_x, screen_y, scale
    
    def render(self, width, height):
        """Render 3D brain graph"""
        if not dpg.does_item_exist("graph_drawlist"):
            return
        
        dpg.draw_rectangle([0, 0], [width, height], 
                          color=(10, 10, 10), fill=(10, 10, 10),
                          parent="graph_drawlist")
        
        # Sort nodes by Z for proper depth
        sorted_nodes = sorted(self.nodes.values(), 
                            key=lambda n: n.z, reverse=True)
        
        # Render edges first
        for edge in self.edges:
            if edge.from_id in self.nodes and edge.to_id in self.nodes:
                n1 = self.nodes[edge.from_id]
                n2 = self.nodes[edge.to_id]
                
                x1, y1, scale1 = self.project_to_screen(n1.x, n1.y, n1.z, width, height)
                x2, y2, scale2 = self.project_to_screen(n2.x, n2.y, n2.z, width, height)
                
                # Edge glow based on node activation
                glow = (n1.brightness + n2.brightness) / 2
                alpha = int(50 + glow * 150)
                
                dpg.draw_line([x1, y1], [x2, y2],
                             color=(100, 100, 150, alpha),
                             thickness=1,
                             parent="graph_drawlist")
        
        # Render nodes
        for node in sorted_nodes:
            x, y, scale = self.project_to_screen(node.x, node.y, node.z, width, height)
            
            # Skip if behind camera
            if scale < 0:
                continue
            
            # Node size based on activation and distance
            base_radius = 3 * scale
            if node.pulse_timer > 0:
                base_radius *= (1.0 + node.pulse_timer * 2)
            
            radius = base_radius * (0.5 + node.brightness * 0.5)
            
            # Node color with brightness
            r, g, b = node.color
            brightness_mult = 0.3 + node.brightness * 0.7
            color = (int(r * brightness_mult * 255), 
                    int(g * brightness_mult * 255), 
                    int(b * brightness_mult * 255))
            
            # Glow effect for high activation
            if node.brightness > 0.7:
                glow_radius = radius * 1.5
                glow_alpha = int(node.brightness * 100)
                dpg.draw_circle([x, y], glow_radius,
                               color=(*color, glow_alpha),
                               fill=(*color, glow_alpha // 2),
                               parent="graph_drawlist")
            
            # Main node
            dpg.draw_circle([x, y], radius,
                           color=color,
                           fill=color,
                           parent="graph_drawlist")

# ============================================================================
# Metrics Panel
# ============================================================================

class MetricsPanel:
    def __init__(self, sparkline_points=100):
        self.metrics = SystemMetrics()
        self.sparkline_points = sparkline_points
        self.cpu_history = deque(maxlen=sparkline_points)
        self.tick_rate_history = deque(maxlen=sparkline_points)
        self.error_history = deque(maxlen=sparkline_points)
        
    def update_metrics(self, metrics: SystemMetrics):
        self.metrics = metrics
        self.cpu_history.append(metrics.cpu_usage)
        self.tick_rate_history.append(metrics.tick_rate)
        self.error_history.append(metrics.mean_error)
    
    def render(self, parent):
        """Render metrics dashboard"""
        if not dpg.does_item_exist("metrics_window"):
            with dpg.child_window(tag="metrics_window", parent=parent,
                                 width=-1, height=-1):
                dpg.add_text("", tag="metrics_text")
        
        # Format metrics
        status_color = {
            "ACTIVE": "🟢",
            "LEARNING": "🟡",
            "IDLE": "⚪"
        }.get(self.metrics.status, "⚫")
        
        text = f"""
{status_color} System Status: {self.metrics.status}

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

📊 Performance Metrics:
  CPU:        {self.metrics.cpu_usage:>6.1f}%
  GPU:        {self.metrics.gpu_usage:>6.1f}%
  RAM:        {self.metrics.ram_usage:>6.1f}%
  VRAM:       {self.metrics.vram_usage:>6.1f}%

⚡ Cognition:
  Tick Rate:  {self.metrics.tick_rate:>6.2f} Hz
  Nodes:      {self.metrics.active_nodes:>6d}
  Edges:      {self.metrics.total_edges:>6d}
  Error:      {self.metrics.mean_error:>6.4f}

🤖 Motor Control:
  Latency:    {self.metrics.motor_latency:>6.2f} ms
"""
        
        if dpg.does_item_exist("metrics_text"):
            dpg.set_value("metrics_text", text)

# ============================================================================
# Main Visualizer
# ============================================================================

class MelvinVisualizer:
    def __init__(self):
        self.terminal_panel = TerminalPanel()
        self.graph_panel = GraphPanel()
        self.metrics_panel = MetricsPanel()
        
        self.event_queue = queue.Queue()
        self.running = False
        self.socket_thread = None
        
        # Initialize some sample nodes for demonstration
        self._init_sample_graph()
    
    def _init_sample_graph(self):
        """Create initial graph structure"""
        # Create nodes in a sphere layout
        num_nodes = 50
        for i in range(num_nodes):
            theta = (i / num_nodes) * 2 * math.pi
            phi = math.acos(2 * (i / num_nodes) - 1)
            
            r = 20.0
            x = r * math.sin(phi) * math.cos(theta)
            y = r * math.sin(phi) * math.sin(theta)
            z = r * math.cos(phi)
            
            category = ["perception", "motor", "memory", "reasoning"][i % 4]
            color = self._get_category_color(category)
            
            node = GraphNode(
                id=i,
                label=f"node_{i}",
                x=x, y=y, z=z,
                category=category,
                color=color
            )
            self.graph_panel.add_node(node)
        
        # Create some edges
        for i in range(num_nodes):
            for j in range(i+1, min(i+4, num_nodes)):
                edge = GraphEdge(from_id=i, to_id=j)
                self.graph_panel.add_edge(edge)
    
    def _get_category_color(self, category: str) -> Tuple[float, float, float]:
        colors = {
            "perception": (0.2, 0.8, 1.0),
            "motor": (1.0, 0.4, 0.2),
            "memory": (0.8, 0.2, 1.0),
            "reasoning": (0.2, 1.0, 0.4)
        }
        return colors.get(category, (0.5, 0.5, 0.5))
    
    def connect_to_socket(self):
        """Connect to Melvin core data feed"""
        try:
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock.connect("/tmp/melvin_feed.sock")
            
            while self.running:
                try:
                    data = sock.recv(4096).decode('utf-8')
                    if not data:
                        break
                    
                    # Parse JSON events (newline delimited)
                    for line in data.strip().split('\n'):
                        if line:
                            try:
                                event = json.loads(line)
                                self.event_queue.put(event)
                            except json.JSONDecodeError:
                                pass
                
                except Exception as e:
                    print(f"Socket error: {e}")
                    time.sleep(1)
            
            sock.close()
        except Exception as e:
            print(f"Could not connect to Melvin core: {e}")
            print("Running in demo mode with simulated data")
            self._run_demo_mode()
    
    def _run_demo_mode(self):
        """Generate simulated events for demonstration"""
        import random
        
        messages = [
            "Analyzing visual scene",
            "Detecting object: cup",
            "Updating synaptic weights",
            "Context shift: kitchen_scene",
            "Motor command: reach_forward",
            "Prediction error: 0.023",
            "Learning rate adapted",
            "Attention focus updated"
        ]
        
        types = ["thought", "perception", "learning", "context"]
        
        while self.running:
            time.sleep(0.5 + random.random())
            
            # Generate thought event
            event = {
                "type": random.choice(types),
                "text": random.choice(messages),
                "context": "main",
                "timestamp": time.time()
            }
            self.event_queue.put(event)
            
            # Generate graph update
            node_id = random.randint(0, 49)
            activation = random.random()
            self.event_queue.put({
                "type": "graph_update",
                "node_id": node_id,
                "activation": activation
            })
            
            # Generate metrics
            if random.random() < 0.2:
                self.event_queue.put({
                    "type": "metric",
                    "cpu": 30 + random.random() * 40,
                    "gpu": 40 + random.random() * 30,
                    "ram": 50 + random.random() * 20,
                    "tick_rate": 15 + random.random() * 10,
                    "active_nodes": random.randint(20, 50),
                    "total_edges": random.randint(100, 200),
                    "mean_error": random.random() * 0.1,
                    "status": random.choice(["ACTIVE", "LEARNING"])
                })
    
    def process_events(self):
        """Process queued events"""
        while not self.event_queue.empty():
            try:
                event = self.event_queue.get_nowait()
                event_type = event.get("type", "")
                
                if event_type in ["thought", "perception", "learning", "context"]:
                    entry = LogEntry(
                        timestamp=event.get("timestamp", time.time()),
                        context_id=event.get("context", "main"),
                        event_type=event_type,
                        message=event.get("text", "")
                    )
                    self.terminal_panel.add_entry(entry)
                
                elif event_type == "graph_update":
                    node_id = event.get("node_id", 0)
                    activation = event.get("activation", 0.0)
                    self.graph_panel.update_node(node_id, activation)
                
                elif event_type == "metric":
                    metrics = SystemMetrics(
                        cpu_usage=event.get("cpu", 0),
                        gpu_usage=event.get("gpu", 0),
                        ram_usage=event.get("ram", 0),
                        tick_rate=event.get("tick_rate", 0),
                        active_nodes=event.get("active_nodes", 0),
                        total_edges=event.get("total_edges", 0),
                        mean_error=event.get("mean_error", 0),
                        motor_latency=event.get("motor_latency", 0),
                        status=event.get("status", "IDLE")
                    )
                    self.metrics_panel.update_metrics(metrics)
            
            except queue.Empty:
                break
    
    def initialize(self):
        """Initialize DearPyGUI"""
        dpg.create_context()
        dpg.create_viewport(title="MelvinOS Visualization", 
                           width=1920, height=1080,
                           decorated=True)
        dpg.setup_dearpygui()
        
        # Main window
        with dpg.window(label="Melvin", tag="main_window"):
            with dpg.group(horizontal=True):
                # Left half - Terminal
                with dpg.child_window(width=960, height=1000, tag="terminal_panel"):
                    dpg.add_text("Melvin's Mind Stream", color=(100, 200, 255))
                    dpg.add_separator()
                
                # Right half - Split vertical
                with dpg.group():
                    # Top - Brain Graph
                    with dpg.child_window(width=940, height=500, tag="graph_panel"):
                        dpg.add_text("Brain Activity", color=(100, 200, 255))
                        dpg.add_drawlist(width=920, height=460, tag="graph_drawlist")
                    
                    # Bottom - Metrics
                    with dpg.child_window(width=940, height=480, tag="metrics_panel"):
                        dpg.add_text("Performance Metrics", color=(100, 200, 255))
                        dpg.add_separator()
        
        dpg.set_primary_window("main_window", True)
    
    def run(self):
        """Main loop"""
        self.running = True
        
        # Start socket connection thread
        self.socket_thread = threading.Thread(target=self.connect_to_socket, daemon=True)
        self.socket_thread.start()
        
        dpg.show_viewport()
        
        last_time = time.time()
        
        while dpg.is_dearpygui_running():
            current_time = time.time()
            delta_time = current_time - last_time
            last_time = current_time
            
            # Process events from Melvin core
            self.process_events()
            
            # Update panels
            self.graph_panel.update(delta_time)
            
            # Render
            self.terminal_panel.render("terminal_panel")
            self.graph_panel.render(920, 460)
            self.metrics_panel.render("metrics_panel")
            
            dpg.render_dearpygui_frame()
        
        self.running = False
        dpg.destroy_context()

# ============================================================================
# Main Entry Point
# ============================================================================

def main():
    visualizer = MelvinVisualizer()
    visualizer.initialize()
    visualizer.run()

if __name__ == "__main__":
    main()

