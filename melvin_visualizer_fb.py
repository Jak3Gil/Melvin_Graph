#!/usr/bin/env python3
"""
MelvinOS Framebuffer Visualizer - Direct Hardware Rendering
No X11, No Desktop - Pure Melvin Display
"""

import os
import sys
import mmap
import struct
import array
import fcntl
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

# Framebuffer IOCTLs
FBIOGET_VSCREENINFO = 0x4600
FBIOGET_FSCREENINFO = 0x4602

@dataclass
class FramebufferInfo:
    width: int
    height: int
    bpp: int  # bits per pixel
    line_length: int

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

class FramebufferDisplay:
    """Direct framebuffer access for hardware rendering"""
    
    def __init__(self, device='/dev/fb0'):
        self.device = device
        self.fb_fd = None
        self.fb_mem = None
        self.info = None
        
    def open(self):
        """Open and map framebuffer"""
        try:
            self.fb_fd = os.open(self.device, os.O_RDWR)
            
            # Get screen info
            vinfo = array.array('I', [0] * 40)
            fcntl.ioctl(self.fb_fd, FBIOGET_VSCREENINFO, vinfo)
            
            width = vinfo[0]
            height = vinfo[1]
            bpp = vinfo[6]
            
            finfo = array.array('I', [0] * 12)
            fcntl.ioctl(self.fb_fd, FBIOGET_FSCREENINFO, finfo)
            line_length = finfo[7]
            
            self.info = FramebufferInfo(width, height, bpp, line_length)
            
            # Memory map the framebuffer
            fb_size = self.info.line_length * self.info.height
            self.fb_mem = mmap.mmap(self.fb_fd, fb_size, 
                                   mmap.MAP_SHARED, 
                                   mmap.PROT_READ | mmap.PROT_WRITE)
            
            print(f"✅ Framebuffer opened: {width}x{height} @ {bpp}bpp")
            return True
            
        except Exception as e:
            print(f"❌ Failed to open framebuffer: {e}")
            return False
    
    def close(self):
        """Close framebuffer"""
        if self.fb_mem:
            self.fb_mem.close()
        if self.fb_fd:
            os.close(self.fb_fd)
    
    def clear(self, color=(0, 0, 0)):
        """Clear screen to color"""
        if not self.fb_mem:
            return
        
        r, g, b = color
        # BGRA format for most framebuffers
        pixel = struct.pack('BBBB', b, g, r, 255)
        
        self.fb_mem.seek(0)
        for _ in range(self.info.width * self.info.height):
            self.fb_mem.write(pixel)
    
    def draw_pixel(self, x, y, color):
        """Draw single pixel"""
        if not self.fb_mem or x < 0 or y < 0 or x >= self.info.width or y >= self.info.height:
            return
        
        r, g, b = color
        offset = y * self.info.line_length + x * 4
        
        self.fb_mem.seek(offset)
        self.fb_mem.write(struct.pack('BBBB', b, g, r, 255))
    
    def draw_rect(self, x, y, w, h, color, fill=False):
        """Draw rectangle"""
        if fill:
            for py in range(y, min(y + h, self.info.height)):
                for px in range(x, min(x + w, self.info.width)):
                    self.draw_pixel(px, py, color)
        else:
            # Top and bottom
            for px in range(x, min(x + w, self.info.width)):
                self.draw_pixel(px, y, color)
                self.draw_pixel(px, y + h - 1, color)
            # Left and right
            for py in range(y, min(y + h, self.info.height)):
                self.draw_pixel(x, py, color)
                self.draw_pixel(x + w - 1, py, color)
    
    def draw_circle(self, cx, cy, radius, color, fill=False):
        """Draw circle"""
        for angle in range(0, 360, 5 if not fill else 1):
            rad = math.radians(angle)
            if fill:
                for r in range(0, radius, 2):
                    x = int(cx + r * math.cos(rad))
                    y = int(cy + r * math.sin(rad))
                    self.draw_pixel(x, y, color)
            else:
                x = int(cx + radius * math.cos(rad))
                y = int(cy + radius * math.sin(rad))
                self.draw_pixel(x, y, color)
    
    def draw_line(self, x1, y1, x2, y2, color):
        """Draw line using Bresenham's algorithm"""
        dx = abs(x2 - x1)
        dy = abs(y2 - y1)
        sx = 1 if x1 < x2 else -1
        sy = 1 if y1 < y2 else -1
        err = dx - dy
        
        x, y = x1, y1
        while True:
            self.draw_pixel(x, y, color)
            if x == x2 and y == y2:
                break
            e2 = 2 * err
            if e2 > -dy:
                err -= dy
                x += sx
            if e2 < dx:
                err += dx
                y += sy
    
    def draw_text(self, x, y, text, color, size=1):
        """Draw text (bitmap font 8x8)"""
        # Simple bitmap font rendering
        for i, char in enumerate(text):
            char_x = x + i * 8 * size
            if char_x >= self.info.width:
                break
            self._draw_char(char_x, y, char, color, size)
    
    def _draw_char(self, x, y, char, color, size):
        """Draw single character using simple bitmap"""
        # Very simple 8x8 bitmap font (just dots for demo)
        # In production, load a proper bitmap font
        if ord(char) >= 32 and ord(char) <= 126:
            # Just draw a simple pattern for now
            for py in range(8 * size):
                for px in range(8 * size):
                    if (px // size) % 2 == 0 and (py // size) % 2 == 0:
                        self.draw_pixel(x + px, y + py, color)

class MelvinVisualizerFB:
    """Melvin visualizer using direct framebuffer"""
    
    def __init__(self):
        self.fb = FramebufferDisplay()
        self.entries = deque(maxlen=500)
        self.nodes: Dict[int, GraphNode] = {}
        self.metrics = SystemMetrics()
        self.event_queue = queue.Queue()
        self.running = False
        self.camera_theta = 0.0
        
        # Colors
        self.COLOR_BG = (10, 10, 10)
        self.COLOR_TEXT = (200, 200, 200)
        self.COLOR_HEADER = (0, 200, 255)
        self.COLOR_NODE = (0, 255, 100)
        self.COLOR_EDGE = (100, 100, 200)
        
        self._init_nodes()
    
    def _init_nodes(self):
        """Initialize graph nodes"""
        num_nodes = 50
        for i in range(num_nodes):
            theta = (i / num_nodes) * 2 * math.pi
            phi = math.acos(2 * (i / num_nodes) - 1)
            
            r = 1.0
            x = r * math.sin(phi) * math.cos(theta)
            y = r * math.sin(phi) * math.sin(theta)
            z = r * math.cos(phi)
            
            self.nodes[i] = GraphNode(id=i, x=x, y=y, z=z)
    
    def connect_to_socket(self):
        """Connect to Melvin or run demo"""
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
            self._run_demo_mode()
    
    def _run_demo_mode(self):
        """Generate demo data"""
        import random
        
        messages = [
            "Analyzing visual input",
            "Processing sensory data",
            "Updating neural weights",
            "Context shift: environment",
            "Motor planning active",
            "Prediction computed",
            "Learning adaptation",
            "Attention reoriented",
            "Memory consolidation",
            "Pattern recognized"
        ]
        
        types = ["thought", "perception", "learning", "context"]
        
        while self.running:
            time.sleep(0.4 + random.random() * 0.4)
            
            self.event_queue.put({
                "type": random.choice(types),
                "text": random.choice(messages),
                "context": "main",
                "timestamp": time.time()
            })
            
            node_id = random.randint(0, 49)
            self.event_queue.put({
                "type": "graph_update",
                "node_id": node_id,
                "activation": random.random()
            })
            
            if random.random() < 0.3:
                self.event_queue.put({
                    "type": "metric",
                    "cpu": 30 + random.random() * 40,
                    "tick_rate": 15 + random.random() * 10,
                    "active_nodes": random.randint(20, 50),
                    "total_edges": random.randint(100, 200),
                    "mean_error": random.random() * 0.1,
                    "status": random.choice(["ACTIVE", "LEARNING"])
                })
    
    def process_events(self):
        """Process event queue"""
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
                    if node_id in self.nodes:
                        node = self.nodes[node_id]
                        node.activation = event.get("activation", 0.0)
                        if node.activation > 0.8:
                            node.pulse_timer = 0.2
                
                elif event_type == "metric":
                    self.metrics = SystemMetrics(
                        cpu_usage=event.get("cpu", 0),
                        tick_rate=event.get("tick_rate", 0),
                        active_nodes=event.get("active_nodes", 0),
                        total_edges=event.get("total_edges", 0),
                        mean_error=event.get("mean_error", 0),
                        status=event.get("status", "IDLE")
                    )
            except queue.Empty:
                break
    
    def update_nodes(self, delta_time):
        """Update node states"""
        for node in self.nodes.values():
            node.brightness += (node.activation - node.brightness) * 0.15
            if node.pulse_timer > 0:
                node.pulse_timer -= delta_time
        
        self.camera_theta += delta_time * 0.3
    
    def project_node(self, node, width, height):
        """Project 3D to 2D"""
        cos_t = math.cos(self.camera_theta)
        sin_t = math.sin(self.camera_theta)
        
        x_rot = node.x * cos_t - node.z * sin_t
        z_rot = node.x * sin_t + node.z * cos_t
        
        distance = 3.0
        scale = distance / (distance + z_rot)
        
        screen_x = int(width / 2 + x_rot * scale * width / 4)
        screen_y = int(height / 2 - node.y * scale * height / 3)
        
        return screen_x, screen_y, scale
    
    def render_frame(self):
        """Render complete frame"""
        if not self.fb.fb_mem:
            return
        
        width = self.fb.info.width
        height = self.fb.info.height
        
        # Clear to black
        self.fb.clear(self.COLOR_BG)
        
        # Header
        self.fb.draw_text(width // 2 - 200, 20, 
                         "MELVINOS CONSCIOUSNESS v1.0", 
                         self.COLOR_HEADER, 2)
        
        # Layout
        left_width = width // 2
        right_width = width - left_width
        top_height = height // 2
        
        # Terminal panel (left)
        self.render_terminal(10, 60, left_width - 20, height - 70)
        
        # Graph panel (top right)
        self.render_graph(left_width + 10, 60, right_width - 20, top_height - 70)
        
        # Metrics panel (bottom right)
        self.render_metrics(left_width + 10, 60 + top_height, right_width - 20, height - top_height - 70)
    
    def render_terminal(self, x, y, w, h):
        """Render thought stream"""
        self.fb.draw_rect(x, y, w, h, self.COLOR_TEXT, fill=False)
        self.fb.draw_text(x + 10, y + 10, "MIND STREAM", self.COLOR_HEADER)
        
        line_y = y + 40
        for entry in list(self.entries)[-15:]:
            if line_y >= y + h - 20:
                break
            
            timestamp = datetime.fromtimestamp(entry.timestamp).strftime("%H:%M:%S")
            text = f"{timestamp} {entry.message[:40]}"
            self.fb.draw_text(x + 10, line_y, text, self.COLOR_TEXT)
            line_y += 20
    
    def render_graph(self, x, y, w, h):
        """Render brain graph"""
        self.fb.draw_rect(x, y, w, h, self.COLOR_TEXT, fill=False)
        self.fb.draw_text(x + 10, y + 10, "BRAIN ACTIVITY", self.COLOR_HEADER)
        
        center_x = x + w // 2
        center_y = y + h // 2
        
        # Draw nodes
        for node in sorted(self.nodes.values(), key=lambda n: n.z):
            nx, ny, scale = self.project_node(node, w, h)
            nx += x
            ny += y
            
            if scale < 0 or nx < x or ny < y or nx >= x + w or ny >= y + h:
                continue
            
            radius = max(2, int(3 * scale * (0.5 + node.brightness * 0.5)))
            
            brightness = int(node.brightness * 255)
            color = (0, brightness, int(brightness * 0.5))
            
            self.fb.draw_circle(nx, ny, radius, color, fill=True)
    
    def render_metrics(self, x, y, w, h):
        """Render metrics"""
        self.fb.draw_rect(x, y, w, h, self.COLOR_TEXT, fill=False)
        self.fb.draw_text(x + 10, y + 10, "PERFORMANCE", self.COLOR_HEADER)
        
        metrics_y = y + 40
        self.fb.draw_text(x + 10, metrics_y, f"Status: {self.metrics.status}", self.COLOR_TEXT)
        metrics_y += 25
        self.fb.draw_text(x + 10, metrics_y, f"CPU: {self.metrics.cpu_usage:.1f}%", self.COLOR_TEXT)
        metrics_y += 25
        self.fb.draw_text(x + 10, metrics_y, f"Tick: {self.metrics.tick_rate:.1f} Hz", self.COLOR_TEXT)
        metrics_y += 25
        self.fb.draw_text(x + 10, metrics_y, f"Nodes: {self.metrics.active_nodes}", self.COLOR_TEXT)
        metrics_y += 25
        self.fb.draw_text(x + 10, metrics_y, f"Error: {self.metrics.mean_error:.4f}", self.COLOR_TEXT)
    
    def run(self):
        """Main loop"""
        if not self.fb.open():
            print("Failed to open framebuffer!")
            return
        
        print("✅ Starting framebuffer visualizer...")
        print(f"   Resolution: {self.fb.info.width}x{self.fb.info.height}")
        print(f"   Press Ctrl+C to exit")
        
        self.running = True
        
        # Start socket thread
        socket_thread = threading.Thread(target=self.connect_to_socket, daemon=True)
        socket_thread.start()
        
        last_time = time.time()
        frame_count = 0
        
        try:
            while self.running:
                current_time = time.time()
                delta_time = current_time - last_time
                last_time = current_time
                
                self.process_events()
                self.update_nodes(delta_time)
                self.render_frame()
                
                frame_count += 1
                if frame_count % 30 == 0:
                    print(f"✅ Rendering at {1.0/delta_time:.1f} FPS")
                
                time.sleep(0.033)  # ~30 FPS
        
        except KeyboardInterrupt:
            print("\n👋 Shutting down...")
        finally:
            self.running = False
            self.fb.close()

def main():
    print("╔══════════════════════════════════════════════════════╗")
    print("║     MELVINOS FRAMEBUFFER VISUALIZER v1.0            ║")
    print("╚══════════════════════════════════════════════════════╝")
    print()
    
    visualizer = MelvinVisualizerFB()
    visualizer.run()

if __name__ == "__main__":
    main()

