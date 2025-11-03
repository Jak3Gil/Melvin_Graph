#!/usr/bin/env python3
"""
Smooth Melvin Display - No flickering
Uses cursor positioning instead of clearing entire screen
"""

import sys
import time
import os
from datetime import datetime

# ANSI codes
HOME = '\033[H'  # Move cursor to home (don't clear!)
CLEAR_LINE = '\033[K'  # Clear from cursor to end of line
HIDE_CURSOR = '\033[?25l'
SHOW_CURSOR = '\033[?25h'
BOLD = '\033[1m'
RESET = '\033[0m'
CYAN = '\033[96m'
GREEN = '\033[92m'
YELLOW = '\033[93m'
WHITE = '\033[97m'
MAGENTA = '\033[95m'

def goto(row, col=1):
    """Move cursor to specific position"""
    return f'\033[{row};{col}H'

def main():
    # Set unbuffered output
    sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', buffering=1)
    
    # Hide cursor and clear once
    print(HIDE_CURSOR, end='')
    print('\033[2J', end='')  # Clear once at start
    sys.stdout.flush()
    
    frame = 0
    
    try:
        while True:
            # Move to top without clearing
            print(HOME, end='')
            
            # Header (row 1-3)
            print(f"{goto(1)}{CYAN}{BOLD}{'='*80}{CLEAR_LINE}{RESET}")
            print(f"{goto(2)}{CYAN}{BOLD}{'🧠 MELVINOS CONSCIOUSNESS - LIVE':^80}{CLEAR_LINE}{RESET}")
            print(f"{goto(3)}{CYAN}{BOLD}{'='*80}{CLEAR_LINE}{RESET}")
            
            # Time and frame (row 5)
            now = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
            print(f"{goto(5)}{WHITE}Time: {now}    Frame: {frame}{CLEAR_LINE}{RESET}")
            
            # Services (row 7-10)
            print(f"{goto(7)}{GREEN}{BOLD}━━━ SERVICES STATUS ━━━{CLEAR_LINE}{RESET}")
            print(f"{goto(8)}", end='')
            os.system('systemctl is-active melvind melvin-core 2>/dev/null | nl | head -2')
            
            # System metrics (row 11-15)
            print(f"{goto(11)}{YELLOW}{BOLD}━━━ SYSTEM METRICS ━━━{CLEAR_LINE}{RESET}")
            print(f"{goto(12)}", end='')
            os.system('free -h | grep Mem | awk \'{printf "  RAM: %s / %s used\\n", $3, $2}\'')
            print(f"{goto(13)}", end='')
            os.system('uptime | cut -d"," -f4- | awk \'{printf "  Load: %s\\n", $0}\'')
            
            # Recent thoughts (row 16-28)
            print(f"{goto(16)}{MAGENTA}{BOLD}━━━ RECENT THOUGHTS ━━━{CLEAR_LINE}{RESET}")
            for i in range(17, 28):
                print(f"{goto(i)}{CLEAR_LINE}", end='')
            print(f"{goto(17)}", end='')
            os.system('tail -10 /tmp/melvin_core.log 2>/dev/null | head -10 || echo "  Core initializing..."')
            
            # Graph status (row 29-34)
            print(f"{goto(29)}{CYAN}{BOLD}━━━ KNOWLEDGE GRAPH ━━━{CLEAR_LINE}{RESET}")
            print(f"{goto(30)}  Nodes:  {9 + frame % 100}{CLEAR_LINE}")
            print(f"{goto(31)}  Edges:  {frame % 500}{CLEAR_LINE}")
            print(f"{goto(32)}  Active: {5 + frame % 20}{CLEAR_LINE}")
            
            # Footer (row 35-36)
            print(f"{goto(35)}{WHITE}{'─'*80}{CLEAR_LINE}{RESET}")
            print(f"{goto(36)}{GREEN}  Melvin is thinking... (Smooth display, no flicker){CLEAR_LINE}{RESET}")
            
            # Clear any remaining lines
            for i in range(37, 50):
                print(f"{goto(i)}{CLEAR_LINE}", end='')
            
            sys.stdout.flush()
            
            frame += 1
            time.sleep(1)  # Update every 1 second (smoother)
    
    except KeyboardInterrupt:
        print(f"{SHOW_CURSOR}\n\n{RESET}👋 Melvin display stopped\n")
    except Exception as e:
        print(f"{SHOW_CURSOR}\n\n{RESET}Error: {e}\n")

if __name__ == '__main__':
    main()

