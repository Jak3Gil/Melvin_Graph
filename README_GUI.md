# Melvin GUI Terminal

**A standalone terminal app for interacting with Melvin**

## Quick Start

```bash
python3 melvin_gui.py
```

This opens a **separate GUI window** with a full terminal interface.

## Features

### 🎨 Visual Interface
- **Dark theme** for comfortable viewing
- **Color-coded output** (info, success, error, debug)
- **Syntax highlighting** for different message types
- **Auto-scrolling** output

### 📝 Input Methods
1. **Type and Send** - Direct text input
2. **Query Mode** - Send with full debug info
3. **Train from File** - Batch training from `.txt` files
4. **File Upload** - Send any file as bytes (future feature)

### 🎛️ Controls

| Button | Function |
|--------|----------|
| **Train from File** | Load training data from text file |
| **Show Graph State** | Display current activations and parameters |
| **Clear Graph** | Reset Melvin (delete `graph.mmap`) |
| **Debug Mode** | Toggle debug output for all operations |

### ⌨️ Keyboard Shortcuts
- **⌘+Enter** (Mac) or **Ctrl+Enter** (Windows/Linux) - Send input
- Type in the text box and click "Send"

## Usage Examples

### 1. Simple Query
```
Type: cat sat on the mat
Click: Send
```

### 2. Debug Query
```
Type: cat
Click: Send as Query (Debug)
→ Shows full propagation info
```

### 3. Batch Training
```
Click: Train from File
Select: training_arithmetic.txt
→ Processes all lines automatically
```

### 4. Check Graph State
```
Click: Show Graph State
→ Displays:
  - Control parameters (_decay, _saturation, etc.)
  - Top activated nodes
  - Current activation values
```

## Training File Format

Create `.txt` files with one pattern per line:

```txt
# Comments start with #
1 + 1 = 2
2 + 2 = 4

# Meta-patterns
when uncertain decompose
break large problems into small ones

# Any text works!
the cat sat on the mat
```

## Color Coding

- **Blue** - Info messages
- **Green** - Success/Results  
- **Red** - Errors
- **Gray** - Debug info
- **Yellow** - Melvin's output (bold)

## What Makes This Special

### Universal Input
- **Text** - Normal sentences
- **Files** - Any file converted to bytes
- **Images** - Binary data (future)
- **Code** - Source files as training data

### Dense Information
You can feed complex, multi-line, rich data:
```
Train from file with:
- 1000 lines of arithmetic
- Natural language patterns
- Code snippets
- Any structured data
```

### Real-time Feedback
- See graph adjusting parameters
- Watch activation spreading
- Monitor learning progress
- Inspect state at any time

## Advanced: Programmatic Use

The GUI can be extended:

```python
from melvin_gui import MelvinGUI
import tkinter as tk

root = tk.Tk()
app = MelvinGUI(root)

# Add custom buttons
app.add_custom_feature()

root.mainloop()
```

## Troubleshooting

**GUI doesn't open?**
```bash
# Make sure tkinter is installed (comes with Python)
python3 -m tkinter
# Should open a test window
```

**Melvin not responding?**
- Check that `./melvin` exists
- Check that `./show_activations` exists
- Run `make` to rebuild if needed

**Graph state not showing?**
- Make sure `show_activations` is compiled
- Run: `gcc -o show_activations show_activations.c`

## Next Steps

1. **Try the demo training file**: `training_arithmetic.txt`
2. **Create your own training data**
3. **Experiment with different patterns**
4. **Watch the graph learn and self-adjust**

---

**The GUI is YOUR interface to Melvin's self-programming brain!**

