#!/usr/bin/env python3
"""
Melvin GUI Terminal - Standalone App
A proper terminal interface for interacting with Melvin
"""

import tkinter as tk
from tkinter import ttk, scrolledtext, filedialog, messagebox
import subprocess
import os
import threading
from pathlib import Path

class MelvinGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Melvin Terminal")
        self.root.geometry("1000x700")
        
        # Configure style
        style = ttk.Style()
        style.theme_use('default')
        
        self.melvin_path = "./melvin"
        self.debug_mode = tk.BooleanVar(value=False)
        self.setup_ui()
        
    def setup_ui(self):
        # Main container
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(0, weight=1)
        main_frame.rowconfigure(2, weight=1)
        
        # Title
        title = ttk.Label(main_frame, text="MELVIN TERMINAL", 
                         font=('Monaco', 16, 'bold'))
        title.grid(row=0, column=0, pady=10)
        
        # Control panel
        control_frame = ttk.LabelFrame(main_frame, text="Controls", padding="10")
        control_frame.grid(row=1, column=0, sticky=(tk.W, tk.E), pady=5)
        
        # Buttons
        btn_frame = ttk.Frame(control_frame)
        btn_frame.grid(row=0, column=0, columnspan=3, sticky=(tk.W, tk.E))
        
        ttk.Button(btn_frame, text="Train from File", 
                  command=self.train_from_file).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="Show Graph State", 
                  command=self.show_state).pack(side=tk.LEFT, padx=5)
        ttk.Button(btn_frame, text="Clear Graph", 
                  command=self.clear_graph).pack(side=tk.LEFT, padx=5)
        ttk.Checkbutton(btn_frame, text="Debug Mode", 
                       variable=self.debug_mode).pack(side=tk.LEFT, padx=5)
        
        # Output display (main terminal area)
        output_frame = ttk.LabelFrame(main_frame, text="Output", padding="5")
        output_frame.grid(row=2, column=0, sticky=(tk.W, tk.E, tk.N, tk.S), pady=5)
        output_frame.columnconfigure(0, weight=1)
        output_frame.rowconfigure(0, weight=1)
        
        self.output_text = scrolledtext.ScrolledText(
            output_frame, 
            wrap=tk.WORD,
            font=('Monaco', 11),
            bg='#1e1e1e',
            fg='#d4d4d4',
            insertbackground='white',
            height=20
        )
        self.output_text.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Input area
        input_frame = ttk.LabelFrame(main_frame, text="Input", padding="5")
        input_frame.grid(row=3, column=0, sticky=(tk.W, tk.E), pady=5)
        input_frame.columnconfigure(0, weight=1)
        
        self.input_text = tk.Text(
            input_frame,
            height=3,
            font=('Monaco', 11),
            bg='#2d2d2d',
            fg='#d4d4d4',
            insertbackground='white'
        )
        self.input_text.grid(row=0, column=0, sticky=(tk.W, tk.E), padx=5, pady=5)
        
        # Send button
        send_frame = ttk.Frame(input_frame)
        send_frame.grid(row=1, column=0, sticky=(tk.W, tk.E))
        
        ttk.Button(send_frame, text="Send (⌘+Enter)", 
                  command=self.send_input).pack(side=tk.LEFT, padx=5)
        ttk.Button(send_frame, text="Send as Query (Debug)", 
                  command=self.send_query).pack(side=tk.LEFT, padx=5)
        
        # Status bar
        self.status_var = tk.StringVar(value="Ready")
        status_bar = ttk.Label(main_frame, textvariable=self.status_var, 
                              relief=tk.SUNKEN, anchor=tk.W)
        status_bar.grid(row=4, column=0, sticky=(tk.W, tk.E), pady=5)
        
        # Keyboard bindings
        self.input_text.bind('<Command-Return>', lambda e: self.send_input())
        self.input_text.bind('<Control-Return>', lambda e: self.send_input())
        
        # Initial message
        self.write_output("=== Melvin Terminal Started ===\n", "info")
        self.write_output("Type your input below and press Send\n", "info")
        self.write_output("Or use buttons to train from file, show state, etc.\n\n", "info")
        
    def write_output(self, text, tag="normal"):
        """Write text to output with color coding"""
        self.output_text.insert(tk.END, text)
        
        # Configure tags for colors
        self.output_text.tag_config("info", foreground="#6a9fb5")
        self.output_text.tag_config("success", foreground="#90c978")
        self.output_text.tag_config("error", foreground="#f44747")
        self.output_text.tag_config("debug", foreground="#858585")
        self.output_text.tag_config("result", foreground="#dcdcaa", font=('Monaco', 12, 'bold'))
        
        # Apply tag to last line
        last_line = self.output_text.index(tk.END)
        start = f"{last_line.split('.')[0]}.0"
        self.output_text.tag_add(tag, start, tk.END)
        
        # Auto-scroll
        self.output_text.see(tk.END)
        self.output_text.update()
    
    def send_to_melvin(self, byte_data, show_debug=False):
        """Send bytes to Melvin"""
        env = os.environ.copy()
        if show_debug or self.debug_mode.get():
            env['MELVIN_DEBUG'] = '1'
        
        try:
            result = subprocess.run(
                [self.melvin_path],
                input=byte_data,
                capture_output=True,
                env=env,
                timeout=10
            )
            return result.stdout.decode('utf-8', errors='replace'), \
                   result.stderr.decode('utf-8', errors='replace')
        except subprocess.TimeoutExpired:
            return None, "Timeout: Melvin took too long"
        except Exception as e:
            return None, f"Error: {str(e)}"
    
    def send_input(self):
        """Send input text to Melvin"""
        text = self.input_text.get("1.0", tk.END).strip()
        if not text:
            return
        
        self.write_output(f"\n>>> {text}\n", "info")
        self.status_var.set("Processing...")
        self.root.update()
        
        # Run in thread to not block UI
        thread = threading.Thread(target=self._send_input_thread, args=(text,))
        thread.daemon = True
        thread.start()
    
    def _send_input_thread(self, text):
        """Background thread for sending input"""
        byte_data = text.encode('utf-8')
        stdout, stderr = self.send_to_melvin(byte_data, show_debug=False)
        
        # Update UI from main thread
        self.root.after(0, self._handle_response, stdout, stderr, False)
    
    def send_query(self):
        """Send as query with debug info"""
        text = self.input_text.get("1.0", tk.END).strip()
        if not text:
            return
        
        self.write_output(f"\n🔍 QUERY: {text}\n", "info")
        self.status_var.set("Querying with debug...")
        self.root.update()
        
        thread = threading.Thread(target=self._send_query_thread, args=(text,))
        thread.daemon = True
        thread.start()
    
    def _send_query_thread(self, text):
        """Background thread for query"""
        byte_data = text.encode('utf-8')
        stdout, stderr = self.send_to_melvin(byte_data, show_debug=True)
        self.root.after(0, self._handle_response, stdout, stderr, True)
    
    def _handle_response(self, stdout, stderr, is_query):
        """Handle response in main thread"""
        if stderr and is_query:
            self.write_output("\nDebug Info:\n", "debug")
            for line in stderr.split('\n'):
                if any(tag in line for tag in ['[SPREAD]', '[PARAMS]', '[ADJUST]', '[LEARN]', '[CONTROL]']):
                    self.write_output(f"  {line}\n", "debug")
        
        if stdout and stdout.strip():
            self.write_output("\n→ ", "success")
            self.write_output(f"{stdout.strip()}\n", "result")
        else:
            self.write_output("(no output)\n", "debug")
        
        self.status_var.set("Ready")
        
        # Clear input
        self.input_text.delete("1.0", tk.END)
    
    def train_from_file(self):
        """Train Melvin from a file"""
        filepath = filedialog.askopenfilename(
            title="Select Training File",
            filetypes=[("Text files", "*.txt"), ("All files", "*.*")]
        )
        
        if not filepath:
            return
        
        self.write_output(f"\n📚 TRAINING FROM: {filepath}\n", "info")
        self.status_var.set("Training...")
        self.root.update()
        
        thread = threading.Thread(target=self._train_thread, args=(filepath,))
        thread.daemon = True
        thread.start()
    
    def _train_thread(self, filepath):
        """Background training thread"""
        try:
            with open(filepath, 'r') as f:
                lines = [line.strip() for line in f if line.strip() and not line.startswith('#')]
            
            total = len(lines)
            self.root.after(0, self.write_output, f"Training on {total} patterns...\n\n", "info")
            
            for i, line in enumerate(lines, 1):
                byte_data = line.encode('utf-8')
                self.send_to_melvin(byte_data, show_debug=False)
                
                if i % 5 == 0 or i == total:
                    msg = f"  [{i}/{total}] {line[:60]}{'...' if len(line) > 60 else ''}\n"
                    self.root.after(0, self.write_output, msg, "debug")
                    self.root.after(0, self.status_var.set, f"Training... {i}/{total}")
            
            self.root.after(0, self.write_output, "\n✓ Training complete!\n", "success")
            self.root.after(0, self.status_var.set, "Training complete")
            
        except Exception as e:
            self.root.after(0, self.write_output, f"\n✗ Error: {e}\n", "error")
            self.root.after(0, self.status_var.set, "Error")
    
    def show_state(self):
        """Show current graph state"""
        self.write_output("\n📊 GRAPH STATE:\n", "info")
        self.status_var.set("Reading graph state...")
        
        try:
            result = subprocess.run(
                ["./show_activations"],
                capture_output=True,
                timeout=2
            )
            output = result.stdout.decode('utf-8', errors='replace')
            
            # Parse and display nicely
            lines = output.split('\n')
            non_control = [l for l in lines if l.strip() and not '_' in l.split(':')[0] if ':' in l]
            control = [l for l in lines if '_' in l.split(':')[0] if ':' in l]
            
            if control:
                self.write_output("\nControl Parameters:\n", "info")
                for line in control[:10]:
                    self.write_output(f"  {line}\n", "debug")
            
            if non_control:
                self.write_output("\nTop Activations:\n", "info")
                for line in non_control[:15]:
                    self.write_output(f"  {line}\n", "normal")
            
            self.status_var.set("Ready")
            
        except Exception as e:
            self.write_output(f"✗ Could not read graph: {e}\n", "error")
            self.status_var.set("Error")
    
    def clear_graph(self):
        """Clear the graph"""
        if messagebox.askyesno("Clear Graph", "Are you sure you want to clear graph.mmap?"):
            try:
                if os.path.exists("graph.mmap"):
                    os.remove("graph.mmap")
                self.write_output("\n✓ Graph cleared\n", "success")
                self.status_var.set("Graph cleared")
            except Exception as e:
                self.write_output(f"\n✗ Error clearing graph: {e}\n", "error")

def main():
    root = tk.Tk()
    app = MelvinGUI(root)
    root.mainloop()

if __name__ == "__main__":
    main()

