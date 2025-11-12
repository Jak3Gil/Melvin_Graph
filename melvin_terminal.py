#!/usr/bin/env python3
"""
Melvin Terminal - Universal Interface
Converts ANY data (text, images, files) to bytes for Melvin
"""

import sys
import subprocess
import os
from pathlib import Path

class MelvinTerminal:
    def __init__(self):
        self.melvin_path = "./melvin"
        self.debug = False
        
    def to_bytes(self, data, data_type="text"):
        """Convert any data type to bytes for Melvin"""
        if data_type == "text":
            return data.encode('utf-8')
        elif data_type == "file":
            with open(data, 'rb') as f:
                return f.read()
        elif data_type == "hex":
            return bytes.fromhex(data)
        else:
            return data.encode('utf-8')
    
    def send_to_melvin(self, byte_data, show_debug=False):
        """Send bytes to Melvin and get response"""
        env = os.environ.copy()
        if show_debug or self.debug:
            env['MELVIN_DEBUG'] = '1'
        
        try:
            result = subprocess.run(
                [self.melvin_path],
                input=byte_data,
                capture_output=True,
                env=env,
                timeout=5
            )
            return result.stdout.decode('utf-8', errors='replace'), result.stderr.decode('utf-8', errors='replace')
        except subprocess.TimeoutExpired:
            return None, "Timeout: Melvin took too long"
        except Exception as e:
            return None, f"Error: {str(e)}"
    
    def show_graph_state(self):
        """Show current graph statistics"""
        try:
            result = subprocess.run(
                ["./show_activations"],
                capture_output=True,
                timeout=2
            )
            return result.stdout.decode('utf-8', errors='replace')
        except:
            return "Could not read graph state"
    
    def train_mode(self, data_file):
        """Batch training from file"""
        print(f"\n=== TRAINING MODE: {data_file} ===\n")
        
        if not os.path.exists(data_file):
            print(f"Error: File {data_file} not found")
            return
        
        with open(data_file, 'r') as f:
            lines = [line.strip() for line in f if line.strip()]
        
        print(f"Training on {len(lines)} patterns...\n")
        
        for i, line in enumerate(lines, 1):
            byte_data = self.to_bytes(line)
            _, stderr = self.send_to_melvin(byte_data, show_debug=False)
            
            if i % 10 == 0 or i == len(lines):
                print(f"  [{i}/{len(lines)}] Processed: {line[:50]}{'...' if len(line) > 50 else ''}")
        
        print("\n✓ Training complete!")
        print("\nGraph state:")
        print(self.show_graph_state()[:500])
    
    def query_mode(self, query):
        """Single query with detailed output"""
        print(f"\n=== QUERY: {query} ===\n")
        
        byte_data = self.to_bytes(query)
        stdout, stderr = self.send_to_melvin(byte_data, show_debug=True)
        
        print("Debug info:")
        for line in stderr.split('\n'):
            if any(tag in line for tag in ['[SPREAD]', '[PARAMS]', '[ADJUST]', '[LEARN]']):
                print(f"  {line}")
        
        print(f"\nOutput: '{stdout.strip()}'")
        
        print("\nTop activations:")
        state = self.show_graph_state()
        lines = [l for l in state.split('\n') if l.strip() and not l.strip().startswith('_')]
        for line in lines[1:11]:  # Top 10
            print(f"  {line}")
    
    def interactive_mode(self):
        """Interactive terminal"""
        print("\n" + "="*60)
        print("MELVIN TERMINAL - Universal Interface")
        print("="*60)
        print("\nCommands:")
        print("  text: <message>     - Send text to Melvin")
        print("  file: <path>        - Send file contents as bytes")
        print("  train: <file>       - Batch training from file (one pattern per line)")
        print("  query: <question>   - Query with debug info")
        print("  state               - Show graph state")
        print("  debug on/off        - Toggle debug mode")
        print("  reset               - Clear graph")
        print("  quit                - Exit")
        print("="*60 + "\n")
        
        while True:
            try:
                cmd = input("melvin> ").strip()
                
                if not cmd:
                    continue
                
                if cmd == "quit":
                    print("\nGoodbye!")
                    break
                
                elif cmd == "state":
                    print("\n" + self.show_graph_state())
                
                elif cmd == "reset":
                    if input("Clear graph.mmap? (yes/no): ").lower() == 'yes':
                        if os.path.exists("graph.mmap"):
                            os.remove("graph.mmap")
                        print("✓ Graph cleared")
                
                elif cmd.startswith("debug "):
                    mode = cmd.split()[1]
                    self.debug = (mode == "on")
                    print(f"Debug mode: {'ON' if self.debug else 'OFF'}")
                
                elif cmd.startswith("text: "):
                    text = cmd[6:]
                    byte_data = self.to_bytes(text)
                    stdout, _ = self.send_to_melvin(byte_data)
                    print(f"→ {stdout.strip()}")
                
                elif cmd.startswith("file: "):
                    filepath = cmd[6:].strip()
                    if os.path.exists(filepath):
                        byte_data = self.to_bytes(filepath, "file")
                        print(f"Sending {len(byte_data)} bytes from {filepath}...")
                        stdout, _ = self.send_to_melvin(byte_data)
                        print(f"→ {stdout.strip()}")
                    else:
                        print(f"Error: File not found: {filepath}")
                
                elif cmd.startswith("train: "):
                    filepath = cmd[7:].strip()
                    self.train_mode(filepath)
                
                elif cmd.startswith("query: "):
                    query = cmd[7:]
                    self.query_mode(query)
                
                else:
                    # Default: treat as text query
                    byte_data = self.to_bytes(cmd)
                    stdout, _ = self.send_to_melvin(byte_data)
                    if stdout.strip():
                        print(f"→ {stdout.strip()}")
            
            except KeyboardInterrupt:
                print("\n\nInterrupted. Type 'quit' to exit.")
            except Exception as e:
                print(f"Error: {e}")

def main():
    terminal = MelvinTerminal()
    
    if len(sys.argv) == 1:
        # Interactive mode
        terminal.interactive_mode()
    elif len(sys.argv) == 2:
        # Quick query mode
        terminal.query_mode(sys.argv[1])
    elif len(sys.argv) == 3 and sys.argv[1] == "train":
        # Batch training mode
        terminal.train_mode(sys.argv[2])
    else:
        print("Usage:")
        print("  python3 melvin_terminal.py              # Interactive mode")
        print("  python3 melvin_terminal.py 'query'      # Quick query")
        print("  python3 melvin_terminal.py train file   # Batch training")

if __name__ == "__main__":
    main()

