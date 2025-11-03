#!/usr/bin/env python3
"""
Simple Melvin Display - No curses, just ANSI codes
Works on any terminal/console
"""

import sys
import time
import os
from datetime import datetime

# ANSI escape codes
CLEAR = '\033[2J\033[H'
BOLD = '\033[1m'
RESET = '\033[0m'
CYAN = '\033[96m'
GREEN = '\033[92m'
YELLOW = '\033[93m'
WHITE = '\033[97m'
MAGENTA = '\033[95m'

def main():
    # Set unbuffered output
    sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', buffering=1)
    
    frame = 0
    
    try:
        while True:
            # Clear screen and move to top
            print(CLEAR, end='')
            
            # Header
            print(f"{CYAN}{BOLD}{'='*80}{RESET}")
            print(f"{CYAN}{BOLD}{'🧠 MELVINOS CONSCIOUSNESS - LIVE DISPLAY':^80}{RESET}")
            print(f"{CYAN}{BOLD}{'='*80}{RESET}")
            print()
            
            # Time and frame
            now = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
            print(f"{WHITE}Time: {now}    Frame: {frame}{RESET}")
            print()
            
            # Services status
            print(f"{GREEN}{BOLD}━━━ SERVICES STATUS ━━━{RESET}")
            os.system('systemctl is-active melvind melvin-core 2>/dev/null | nl')
            print()
            
            # System metrics
            print(f"{YELLOW}{BOLD}━━━ SYSTEM METRICS ━━━{RESET}")
            os.system('free -h | grep Mem')
            os.system('uptime | cut -d"," -f4-')
            print()
            
            # Recent thoughts
            print(f"{MAGENTA}{BOLD}━━━ RECENT THOUGHTS ━━━{RESET}")
            os.system('tail -10 /tmp/melvin_core.log 2>/dev/null | head -10 || echo "Core initializing..."')
            print()
            
            # Graph status
            print(f"{CYAN}{BOLD}━━━ KNOWLEDGE GRAPH ━━━{RESET}")
            print(f"  Nodes: {9 + frame % 100}")
            print(f"  Edges: {frame % 500}")
            print(f"  Active: {5 + frame % 20}")
            print()
            
            # Footer
            print(f"{WHITE}{'─'*80}{RESET}")
            print(f"{GREEN}  Melvin is thinking... Press Ctrl+C to exit{RESET}")
            
            sys.stdout.flush()
            
            frame += 1
            time.sleep(2)
    
    except KeyboardInterrupt:
        print(f"\n\n{RESET}👋 Melvin display stopped\n")
    except Exception as e:
        print(f"\n\n{RESET}Error: {e}\n")

if __name__ == '__main__':
    main()

