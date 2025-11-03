#!/usr/bin/env python3
"""
MelvinOS Terminal Visualization Interface
Real-time cognition display using curses (no external dependencies)
"""

import curses
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

@dataclass
class GraphNode:
    id: int
    x: float
    y: float
    z: float
    activation: float = 0.0
    brightness: float = 0.0
    pulse_timer: float = 0.0

@dataclass
class SystemMetrics:
    cpu_usage: float = 0.0
    gpu_usage: float = 0.0
    ram_usage: float = 0.0
    tick_rate: float = 0.0
    active_nodes: int = 0
    total_edges: int = 0
    mean_error: float = 0.0
    status: str = "IDLE"

# ============================================================================
# Visualizer
# ============================================================================

class MelvinVisualizerCurses:
    def __init__(self):
        self.entries = deque(maxlen=500)
        self.nodes: Dict[int, GraphNode] = {}
        self.metrics = SystemMetrics()
        self.event_queue = queue.Queue()
        self.running = False
        self.camera_theta = 0.0
        
        # Initialize sample nodes
        self._init_nodes()
    
    def _init_nodes(self):
        """Create initial graph structure"""
        num_nodes = 50
        for i in range(num_nodes):
            theta = (i / num_nodes) * 2 * math.pi
            phi = math.acos(2 * (i / num_nodes) - 1)
            
            r = 1.0
            x = r * math.sin(phi) * math.cos(theta)
            y = r * math.sin(phi) * math.sin(theta)
            z = r * math.cos(phi)
            
            self.nodes[i] = GraphNode(id=i, x=x, y=y, z=z)
    
    def _get_color_pair(self, event_type: str) -> int:
        """Get curses color pair for event type"""
        colors = {
            "thought": 1,      # White
            "perception": 2,   # Cyan
            "learning": 3,     # Yellow
            "context": 4,      # Magenta
        }
        return colors.get(event_type, 0)
    
    def _get_icon(self, event_type: str) -> str:
        """Get icon for event type"""
        icons = {
            "thought": "🧠",
            "perception": "👁",
            "learning": "🔁",
            "context": "🎯",
        }
        return icons.get(event_type, "•")
    
    def connect_to_socket(self):
        """Connect to Melvin core or run demo mode"""
        try:
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock.connect("/tmp/melvin_feed.sock")
            
            while self.running:
                try:
                    data = sock.recv(4096).decode('utf-8')
                    if not data:
                        break
                    
                    for line in data.strip().split('\n'):
                        if line:
                            try:
                                event = json.loads(line)
                                self.event_queue.put(event)
                            except json.JSONDecodeError:
                                pass
                
                except Exception:
                    time.sleep(1)
            
            sock.close()
        except Exception:
            # Run in demo mode
            self._run_demo_mode()
    
    def _run_demo_mode(self):
        """Generate simulated events"""
        import random
        
        messages = [
            "Analyzing visual scene",
            "Detecting object in field",
            "Updating synaptic weights", 
            "Context shift detected",
            "Motor command generated",
            "Prediction error computed",
            "Learning rate adapted",
            "Attention focus updated",
            "Memory consolidation active",
            "Pattern recognition engaged"
        ]
        
        types = ["thought", "perception", "learning", "context"]
        
        while self.running:
            time.sleep(0.3 + random.random() * 0.5)
            
            # Thought event
            event = {
                "type": random.choice(types),
                "text": random.choice(messages),
                "context": "main",
                "timestamp": time.time()
            }
            self.event_queue.put(event)
            
            # Graph update
            node_id = random.randint(0, 49)
            activation = random.random()
            self.event_queue.put({
                "type": "graph_update",
                "node_id": node_id,
                "activation": activation
            })
            
            # Metrics
            if random.random() < 0.3:
                self.event_queue.put({
                    "type": "metric",
                    "cpu": 30 + random.random() * 40,
                    "gpu": 40 + random.random() * 30,
                    "ram": 50 + random.random() * 20,
                    "tick_rate": 15 + random.random() * 10,
                    "active_nodes": random.randint(20, 50),
                    "total_edges": random.randint(100, 200),
                    "mean_error": random.random() * 0.1,
                    "status": random.choice(["ACTIVE", "LEARNING", "IDLE"])
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
                    self.entries.append(entry)
                
                elif event_type == "graph_update":
                    node_id = event.get("node_id", 0)
                    activation = event.get("activation", 0.0)
                    if node_id in self.nodes:
                        node = self.nodes[node_id]
                        node.activation = activation
                        if activation > 0.8:
                            node.pulse_timer = 0.2
                
                elif event_type == "metric":
                    self.metrics = SystemMetrics(
                        cpu_usage=event.get("cpu", 0),
                        gpu_usage=event.get("gpu", 0),
                        ram_usage=event.get("ram", 0),
                        tick_rate=event.get("tick_rate", 0),
                        active_nodes=event.get("active_nodes", 0),
                        total_edges=event.get("total_edges", 0),
                        mean_error=event.get("mean_error", 0),
                        status=event.get("status", "IDLE")
                    )
            
            except queue.Empty:
                break
    
    def update_nodes(self, delta_time: float):
        """Update node states"""
        for node in self.nodes.values():
            # Smooth brightness
            target = node.activation
            node.brightness += (target - node.brightness) * 0.15
            
            # Update pulse timer
            if node.pulse_timer > 0:
                node.pulse_timer -= delta_time
        
        # Rotate camera
        self.camera_theta += delta_time * 0.3
    
    def project_node(self, node: GraphNode, width: int, height: int) -> Tuple[int, int, float]:
        """Project 3D node to 2D screen position"""
        # Rotate around Y axis
        cos_t = math.cos(self.camera_theta)
        sin_t = math.sin(self.camera_theta)
        
        x_rot = node.x * cos_t - node.z * sin_t
        z_rot = node.x * sin_t + node.z * cos_t
        
        # Perspective projection
        distance = 3.0
        scale = distance / (distance + z_rot)
        
        screen_x = int(width / 2 + x_rot * scale * width / 4)
        screen_y = int(height / 2 - node.y * scale * height / 3)
        
        return screen_x, screen_y, scale
    
    def render_terminal(self, stdscr, start_row, start_col, height, width):
        """Render terminal panel"""
        stdscr.addstr(start_row, start_col, "=" * width, curses.color_pair(0) | curses.A_BOLD)
        stdscr.addstr(start_row + 1, start_col + 2, "🧠 MELVIN'S MIND STREAM", 
                     curses.color_pair(2) | curses.A_BOLD)
        stdscr.addstr(start_row + 2, start_col, "=" * width, curses.color_pair(0) | curses.A_BOLD)
        
        # Show recent entries
        display_lines = min(height - 4, len(self.entries))
        start_idx = max(0, len(self.entries) - display_lines)
        
        for i, entry in enumerate(list(self.entries)[start_idx:]):
            row = start_row + 3 + i
            if row >= start_row + height:
                break
            
            timestamp = datetime.fromtimestamp(entry.timestamp).strftime("%H:%M:%S")
            icon = self._get_icon(entry.event_type)
            color = self._get_color_pair(entry.event_type)
            
            # Truncate message to fit
            max_msg_len = width - 25
            msg = entry.message[:max_msg_len]
            
            try:
                stdscr.addstr(row, start_col, f"{timestamp} {icon} ", curses.color_pair(color))
                stdscr.addstr(row, start_col + 12, msg, curses.color_pair(0))
            except curses.error:
                pass  # Skip if can't render
    
    def render_graph(self, stdscr, start_row, start_col, height, width):
        """Render 3D brain graph"""
        stdscr.addstr(start_row, start_col, "=" * width, curses.color_pair(0) | curses.A_BOLD)
        stdscr.addstr(start_row + 1, start_col + 2, "🧠 BRAIN ACTIVITY MAP", 
                     curses.color_pair(2) | curses.A_BOLD)
        stdscr.addstr(start_row + 2, start_col, "=" * width, curses.color_pair(0) | curses.A_BOLD)
        
        # Draw nodes
        drawn_positions = set()
        for node in sorted(self.nodes.values(), key=lambda n: n.z):
            x, y, scale = self.project_node(node, width, height - 3)
            
            if scale < 0:  # Behind camera
                continue
            
            if 0 <= x < width and 3 <= y < height:
                pos = (start_row + y, start_col + x)
                if pos in drawn_positions:
                    continue
                drawn_positions.add(pos)
                
                # Choose character based on activation
                if node.pulse_timer > 0:
                    char = "●"
                    attr = curses.A_BOLD
                elif node.brightness > 0.7:
                    char = "◉"
                    attr = curses.A_BOLD
                elif node.brightness > 0.4:
                    char = "◎"
                    attr = curses.A_NORMAL
                else:
                    char = "○"
                    attr = curses.A_DIM
                
                try:
                    stdscr.addstr(pos[0], pos[1], char, curses.color_pair(1) | attr)
                except curses.error:
                    pass
    
    def render_metrics(self, stdscr, start_row, start_col, height, width):
        """Render metrics panel"""
        stdscr.addstr(start_row, start_col, "=" * width, curses.color_pair(0) | curses.A_BOLD)
        stdscr.addstr(start_row + 1, start_col + 2, "📊 PERFORMANCE METRICS", 
                     curses.color_pair(2) | curses.A_BOLD)
        stdscr.addstr(start_row + 2, start_col, "=" * width, curses.color_pair(0) | curses.A_BOLD)
        
        # Status indicator
        status_icon = {"ACTIVE": "🟢", "LEARNING": "🟡", "IDLE": "⚪"}.get(self.metrics.status, "⚫")
        stdscr.addstr(start_row + 3, start_col + 2, 
                     f"{status_icon} System: {self.metrics.status}", 
                     curses.color_pair(3) | curses.A_BOLD)
        
        # Metrics
        row = start_row + 5
        metrics_text = [
            f"CPU:        {self.metrics.cpu_usage:>6.1f}%",
            f"GPU:        {self.metrics.gpu_usage:>6.1f}%",
            f"RAM:        {self.metrics.ram_usage:>6.1f}%",
            f"",
            f"Tick Rate:  {self.metrics.tick_rate:>6.2f} Hz",
            f"Nodes:      {self.metrics.active_nodes:>6d}",
            f"Edges:      {self.metrics.total_edges:>6d}",
            f"Error:      {self.metrics.mean_error:>6.4f}",
        ]
        
        for i, text in enumerate(metrics_text):
            try:
                stdscr.addstr(row + i, start_col + 2, text, curses.color_pair(0))
            except curses.error:
                pass
    
    def run(self, stdscr):
        """Main loop"""
        # Setup colors
        curses.start_color()
        curses.use_default_colors()
        curses.init_pair(1, curses.COLOR_WHITE, -1)
        curses.init_pair(2, curses.COLOR_CYAN, -1)
        curses.init_pair(3, curses.COLOR_YELLOW, -1)
        curses.init_pair(4, curses.COLOR_MAGENTA, -1)
        
        # Hide cursor
        curses.curs_set(0)
        stdscr.nodelay(1)
        stdscr.timeout(50)
        
        # Clear screen
        stdscr.clear()
        stdscr.refresh()
        
        # Start socket thread
        self.running = True
        socket_thread = threading.Thread(target=self.connect_to_socket, daemon=True)
        socket_thread.start()
        
        last_time = time.time()
        
        while self.running:
            current_time = time.time()
            delta_time = current_time - last_time
            last_time = current_time
            
            # Get terminal size
            max_y, max_x = stdscr.getmaxyx()
            
            # Process events
            self.process_events()
            self.update_nodes(delta_time)
            
            # Clear screen
            stdscr.clear()
            
            # Calculate layout
            left_width = max_x // 2
            right_width = max_x - left_width
            top_height = (max_y - 1) // 2
            bottom_height = max_y - top_height - 1
            
            # Render panels
            self.render_terminal(stdscr, 0, 0, max_y, left_width)
            self.render_graph(stdscr, 0, left_width, top_height, right_width)
            self.render_metrics(stdscr, top_height, left_width, bottom_height, right_width)
            
            # Header
            header = " MELVINOS VISUALIZATION v1.0 - Press 'q' to quit "
            stdscr.addstr(0, (max_x - len(header)) // 2, header, 
                         curses.color_pair(2) | curses.A_BOLD)
            
            # Refresh
            stdscr.refresh()
            
            # Check for quit
            try:
                key = stdscr.getch()
                if key == ord('q') or key == ord('Q'):
                    self.running = False
            except:
                pass

def main():
    visualizer = MelvinVisualizerCurses()
    try:
        curses.wrapper(visualizer.run)
    except KeyboardInterrupt:
        pass

if __name__ == "__main__":
    main()

