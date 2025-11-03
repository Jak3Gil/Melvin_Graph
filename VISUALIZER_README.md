# 🧠 MelvinOS Visualization Interface v1.0

Advanced real-time cognition, brain graph, and performance metrics display for Melvin.

## Features

### 📺 Three-Panel Display

```
┌─────────────────────────────┬─────────────────────────────┐
│                             │   🧠 3D Brain Graph         │
│   💭 Mind Stream            │   Real-time neural activity │
│   Scrolling thought log     │   50+ nodes, connections    │
│   Color-coded by type:      │   Activation pulses         │
│   • 🧠 Thoughts (white)     │   Auto-rotating view        │
│   • 👁 Perception (cyan)    │                             │
│   • 🔁 Learning (yellow)    │   Category-based colors     │
│   • 🎯 Context (magenta)    │                             │
│                             ├─────────────────────────────┤
│                             │   📊 Performance Metrics    │
│                             │   CPU/GPU/RAM usage         │
│                             │   Tick rate, nodes, edges   │
│                             │   Mean error, latency       │
│                             │   Status indicator          │
└─────────────────────────────┴─────────────────────────────┘
```

## Installation

### On Jetson (from Mac):

```bash
cd /Users/jakegilbert/Desktop/single_mel_file

# Deploy visualizer
scp melvin_visualizer.py melvin-visualizer.service install_visualizer.sh requirements.txt \
    melvin@169.254.123.100:~/melvinos/

# Install on Jetson
ssh melvin@169.254.123.100
cd ~/melvinos
chmod +x install_visualizer.sh
./install_visualizer.sh
```

### Manual Install:

```bash
# Install dependencies
pip3 install dearpygui

# Copy files
cp melvin_visualizer.py /home/melvin/melvinos/
cp melvin-visualizer.service /etc/systemd/system/

# Enable and start
sudo systemctl daemon-reload
sudo systemctl enable melvin-visualizer
sudo systemctl start melvin-visualizer
```

## Usage

### Auto-Start

The visualizer automatically starts on boot and displays on the HDMI/DisplayPort monitor.

### Manual Control

```bash
# Start visualizer
sudo systemctl start melvin-visualizer

# Stop visualizer
sudo systemctl stop melvin-visualizer

# View logs
sudo journalctl -u melvin-visualizer -f

# Run standalone (for testing)
cd ~/melvinos
python3 melvin_visualizer.py
```

### Controls

- **Spacebar**: Toggle auto-rotation
- **Mouse Drag**: Rotate camera view (when implemented)
- **Scroll**: Zoom in/out (when implemented)
- **ESC**: Exit (standalone mode only)

## Data Flow

### Input: JSON Events

The visualizer connects to `/tmp/melvin_feed.sock` and receives JSON events:

```json
{"type": "thought", "text": "Analyzing scene", "context": "main", "timestamp": 1730565223}
{"type": "perception", "text": "Object detected: cup", "context": "vision", "timestamp": 1730565223}
{"type": "learning", "text": "Weights updated", "context": "training", "timestamp": 1730565224}
{"type": "graph_update", "node_id": 42, "activation": 0.87}
{"type": "metric", "cpu": 45, "gpu": 62, "ram": 58, "tick_rate": 18.5, "status": "ACTIVE"}
```

### Updating melvin_core

To emit events, modify `melvin_core.c` to write JSON to the socket:

```c
// At startup
int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
struct sockaddr_un addr = {.sun_family = AF_UNIX};
strncpy(addr.sun_path, "/tmp/melvin_feed.sock", sizeof(addr.sun_path) - 1);
unlink(addr.sun_path);
bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr));
listen(sock_fd, 5);

// When thinking
char json[512];
snprintf(json, sizeof(json), 
    "{\"type\":\"thought\",\"text\":\"%s\",\"timestamp\":%ld}\n",
    message, time(NULL));
write(client_fd, json, strlen(json));
```

## Demo Mode

If the visualizer can't connect to melvin_core, it runs in demo mode with simulated data:

- Random thought messages
- Animated node activations  
- Simulated performance metrics

This is useful for testing the interface without running the full system.

## Performance

- **Update Rate**: 30-60 FPS
- **Node Count**: Optimized for 50-200 nodes
- **Memory**: ~50MB RAM
- **CPU**: 5-15% on Jetson

## Visual Design

### Color Scheme

- Background: Dark neural (RGB 10,10,10)
- Nodes: Category-based HSV colors
- Edges: Blue-gray with activation glow
- Text: White, cyan, yellow, magenta

### Node Activation

- **Brightness**: Proportional to activation (0.3 - 1.0)
- **Pulse**: Expands when activation > 0.8
- **Glow**: Halo effect for highly active nodes

### Edge Rendering

- **Thickness**: Based on weight
- **Opacity**: Based on average node activation
- **Glow**: Bright when both nodes active

## Troubleshooting

### Visualizer Not Showing

```bash
# Check if service is running
sudo systemctl status melvin-visualizer

# Check if X display is available
echo $DISPLAY
xrandr -q

# Check logs
sudo journalctl -u melvin-visualizer -n 50
```

### Black Screen

```bash
# Restart GDM
sudo systemctl restart gdm

# Restart visualizer
sudo systemctl restart melvin-visualizer
```

### No Data

```bash
# Check if melvin-core is emitting data
sudo journalctl -u melvin-core -n 50

# Check socket
ls -la /tmp/melvin_feed.sock

# Run in demo mode for testing
python3 melvin_visualizer.py
```

### Python Errors

```bash
# Reinstall dependencies
pip3 install --upgrade dearpygui

# Check Python version (need 3.7+)
python3 --version
```

## Extending

### Add New Event Types

1. Define event in `melvin_core.c`
2. Add case in `process_events()` 
3. Create handler method
4. Update relevant panel

### Customize Appearance

Edit in `melvin_visualizer.py`:
- `get_color()` - Event colors
- `get_icon()` - Event icons
- `_get_category_color()` - Node colors
- Projection parameters in `project_to_screen()`

### Add Interactive Features

- Implement mouse handlers in `GraphPanel`
- Add keyboard shortcuts
- Create node selection/inspection
- Add timeline scrubbing

## Architecture

```
melvin_core.c 
    ↓ (emits JSON events)
/tmp/melvin_feed.sock
    ↓ (socket connection)
MelvinVisualizer
    ├─ TerminalPanel (thought stream)
    ├─ GraphPanel (3D visualization)
    └─ MetricsPanel (system stats)
    ↓ (DearPyGUI rendering)
HDMI/DisplayPort Monitor
```

## Future Enhancements

- [ ] OpenGL shader effects for nodes
- [ ] Audio visualization panel
- [ ] Motor command preview
- [ ] Timeline playback controls
- [ ] Node clustering visualization
- [ ] Real-time graph statistics
- [ ] Export video recording
- [ ] Multi-monitor support

---

**Melvin's consciousness, visualized in real-time! 🧠✨**

