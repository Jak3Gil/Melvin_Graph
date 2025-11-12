#!/usr/bin/env python3
"""
Melvin Web Terminal - Browser-based UI
No dependencies needed - pure Python
"""

import http.server
import socketserver
import subprocess
import json
import urllib.parse
import os
from threading import Thread

PORT = 8888

HTML = """<!DOCTYPE html>
<html>
<head>
    <title>Melvin Terminal</title>
    <meta charset="UTF-8">
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Monaco', 'Consolas', monospace;
            background: #0d1117;
            color: #c9d1d9;
            height: 100vh;
            display: flex;
            flex-direction: column;
        }
        #header {
            background: #161b22;
            padding: 15px;
            border-bottom: 2px solid #30363d;
        }
        h1 {
            color: #58a6ff;
            font-size: 24px;
            margin-bottom: 10px;
        }
        #controls {
            display: flex;
            gap: 10px;
            flex-wrap: wrap;
        }
        button {
            background: #238636;
            color: white;
            border: none;
            padding: 8px 16px;
            border-radius: 6px;
            cursor: pointer;
            font-family: inherit;
            font-size: 14px;
        }
        button:hover { background: #2ea043; }
        button.secondary { background: #1f6feb; }
        button.secondary:hover { background: #388bfd; }
        button.danger { background: #da3633; }
        button.danger:hover { background: #f85149; }
        #main {
            display: flex;
            flex: 1;
            overflow: hidden;
        }
        #output {
            flex: 1;
            background: #0d1117;
            padding: 20px;
            overflow-y: auto;
            font-size: 13px;
            line-height: 1.6;
        }
        #sidebar {
            width: 300px;
            background: #161b22;
            border-left: 1px solid #30363d;
            padding: 15px;
            overflow-y: auto;
        }
        #input-area {
            background: #161b22;
            border-top: 2px solid #30363d;
            padding: 15px;
        }
        #input {
            width: 100%;
            background: #0d1117;
            color: #c9d1d9;
            border: 1px solid #30363d;
            border-radius: 6px;
            padding: 10px;
            font-family: inherit;
            font-size: 14px;
            resize: vertical;
            min-height: 60px;
        }
        #input:focus {
            outline: none;
            border-color: #58a6ff;
        }
        .log { margin: 5px 0; }
        .log.input { color: #58a6ff; }
        .log.output { color: #7ee787; font-weight: bold; }
        .log.debug { color: #8b949e; font-size: 12px; }
        .log.error { color: #f85149; }
        .log.info { color: #79c0ff; }
        #status {
            background: #21262d;
            padding: 8px 15px;
            border-top: 1px solid #30363d;
            font-size: 12px;
            color: #8b949e;
        }
        .param { 
            margin: 5px 0; 
            padding: 5px;
            background: #0d1117;
            border-radius: 3px;
            font-size: 12px;
        }
        .loading { animation: pulse 1.5s infinite; }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
    </style>
</head>
<body>
    <div id="header">
        <h1>⚡ MELVIN TERMINAL</h1>
        <div id="controls">
            <button onclick="send()" class="secondary">Send (Ctrl+Enter)</button>
            <button onclick="sendQuery()">Query with Debug</button>
            <button onclick="showState()" class="secondary">Show State</button>
            <button onclick="clearGraph()" class="danger">Clear Graph</button>
            <label><input type="checkbox" id="debug"> Debug Mode</label>
        </div>
    </div>
    <div id="main">
        <div id="output"></div>
        <div id="sidebar">
            <h3>Graph State</h3>
            <div id="state">Loading...</div>
        </div>
    </div>
    <div id="input-area">
        <textarea id="input" placeholder="Type your input here..."></textarea>
    </div>
    <div id="status">Ready</div>

    <script>
        const output = document.getElementById('output');
        const input = document.getElementById('input');
        const status = document.getElementById('status');
        const stateDiv = document.getElementById('state');

        function log(text, type='normal') {
            const div = document.createElement('div');
            div.className = 'log ' + type;
            div.textContent = text;
            output.appendChild(div);
            output.scrollTop = output.scrollHeight;
        }

        function setStatus(text) {
            status.textContent = text;
        }

        async function send() {
            const text = input.value.trim();
            if (!text) return;
            
            log('>>> ' + text, 'input');
            setStatus('Processing...');
            
            const debug = document.getElementById('debug').checked;
            
            try {
                const res = await fetch('/api/send', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({text, debug: false})
                });
                const data = await res.json();
                
                if (data.output) {
                    log('→ ' + data.output, 'output');
                } else {
                    log('(no output)', 'debug');
                }
                
                input.value = '';
                setStatus('Ready');
            } catch (e) {
                log('Error: ' + e, 'error');
                setStatus('Error');
            }
        }

        async function sendQuery() {
            const text = input.value.trim();
            if (!text) return;
            
            log('🔍 QUERY: ' + text, 'info');
            setStatus('Querying...');
            
            try {
                const res = await fetch('/api/send', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({text, debug: true})
                });
                const data = await res.json();
                
                if (data.debug) {
                    log('Debug:', 'debug');
                    data.debug.split('\\n').forEach(line => {
                        if (line.includes('[')) log('  ' + line, 'debug');
                    });
                }
                
                if (data.output) {
                    log('→ ' + data.output, 'output');
                } else {
                    log('(no output)', 'debug');
                }
                
                input.value = '';
                setStatus('Ready');
                showState();
            } catch (e) {
                log('Error: ' + e, 'error');
                setStatus('Error');
            }
        }

        async function showState() {
            try {
                const res = await fetch('/api/state');
                const data = await res.json();
                
                let html = '<div style="margin-top:10px">';
                
                if (data.params) {
                    html += '<strong>Parameters:</strong><br>';
                    data.params.forEach(p => {
                        html += `<div class="param">${p}</div>`;
                    });
                }
                
                if (data.top) {
                    html += '<br><strong>Top Activations:</strong><br>';
                    data.top.slice(0, 10).forEach(p => {
                        html += `<div class="param">${p}</div>`;
                    });
                }
                
                html += '</div>';
                stateDiv.innerHTML = html;
            } catch (e) {
                stateDiv.innerHTML = 'Error loading state';
            }
        }

        async function clearGraph() {
            if (!confirm('Clear graph.mmap?')) return;
            
            try {
                await fetch('/api/clear', {method: 'POST'});
                log('✓ Graph cleared', 'info');
                setStatus('Graph cleared');
                showState();
            } catch (e) {
                log('Error clearing graph', 'error');
            }
        }

        // Keyboard shortcuts
        input.addEventListener('keydown', (e) => {
            if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
                e.preventDefault();
                send();
            }
        });

        // Initial state load
        showState();
        setInterval(showState, 5000); // Auto-refresh every 5s
        
        log('=== Melvin Web Terminal Started ===', 'info');
        log('Type your input and press Send or Ctrl+Enter', 'info');
    </script>
</body>
</html>
"""

class MelvinHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/':
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            self.wfile.write(HTML.encode())
        elif self.path == '/api/state':
            self.send_json(self.get_state())
        else:
            self.send_error(404)
    
    def do_POST(self):
        if self.path == '/api/send':
            length = int(self.headers['Content-Length'])
            data = json.loads(self.rfile.read(length))
            result = self.send_to_melvin(data['text'], data.get('debug', False))
            self.send_json(result)
        elif self.path == '/api/clear':
            if os.path.exists('graph.mmap'):
                os.remove('graph.mmap')
            self.send_json({'status': 'ok'})
        else:
            self.send_error(404)
    
    def send_json(self, data):
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())
    
    def send_to_melvin(self, text, debug=False):
        env = os.environ.copy()
        if debug:
            env['MELVIN_DEBUG'] = '1'
        
        try:
            result = subprocess.run(
                ['./melvin'],
                input=text.encode('utf-8'),
                capture_output=True,
                env=env,
                timeout=10
            )
            return {
                'output': result.stdout.decode('utf-8', errors='replace').strip(),
                'debug': result.stderr.decode('utf-8', errors='replace') if debug else None
            }
        except Exception as e:
            return {'error': str(e)}
    
    def get_state(self):
        try:
            result = subprocess.run(
                ['./show_activations'],
                capture_output=True,
                timeout=2
            )
            lines = result.stdout.decode('utf-8', errors='replace').split('\n')
            
            params = [l.strip() for l in lines if l.strip() and l.strip().startswith('_')]
            top = [l.strip() for l in lines if l.strip() and ':' in l and not l.strip().startswith('_')]
            
            return {'params': params, 'top': top}
        except:
            return {'error': 'Could not read state'}
    
    def log_message(self, format, *args):
        pass  # Suppress logs

def main():
    with socketserver.TCPServer(("", PORT), MelvinHandler) as httpd:
        print(f"🚀 Melvin Web Terminal running at:")
        print(f"   http://localhost:{PORT}")
        print(f"\nOpen this URL in your browser!")
        print(f"Press Ctrl+C to stop\n")
        httpd.serve_forever()

if __name__ == "__main__":
    main()

