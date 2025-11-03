#!/usr/bin/env python3
"""
Analyze and visualize the Melvin graph state from binary files.
Reads nodes.bin and edges.bin and displays statistics.
"""

import struct
import sys
from collections import defaultdict

def read_nodes(filename):
    """Read nodes from binary file."""
    try:
        with open(filename, 'rb') as f:
            node_count = struct.unpack('I', f.read(4))[0]
            next_id = struct.unpack('I', f.read(4))[0]
            
            nodes = []
            # Node struct: id(4), a(1), a_prev(1), theta(2), in_deg(2), out_deg(2), 
            #              last_tick_seen(4), burst(2), sig_history(4), is_meta(1), 
            #              cluster_id(4), soma(4), hat(1), total_active_ticks(2)
            # Total: 33 bytes + padding
            
            node_size = 4 + 1 + 1 + 2 + 2 + 2 + 4 + 2 + 4 + 1 + 4 + 4 + 1 + 2
            
            for _ in range(node_count):
                data = f.read(node_size)
                if len(data) < node_size:
                    break
                
                # Parse basic fields
                node_id = struct.unpack('I', data[0:4])[0]
                a = data[4]
                a_prev = data[5]
                theta = struct.unpack('H', data[6:8])[0]
                in_deg = struct.unpack('H', data[8:10])[0]
                out_deg = struct.unpack('H', data[10:12])[0]
                last_tick = struct.unpack('I', data[12:16])[0]
                burst = struct.unpack('H', data[16:18])[0]
                
                nodes.append({
                    'id': node_id,
                    'a': a,
                    'a_prev': a_prev,
                    'theta': theta,
                    'in_deg': in_deg,
                    'out_deg': out_deg,
                    'last_tick': last_tick,
                    'burst': burst
                })
            
            return nodes, next_id
    except FileNotFoundError:
        print(f"Error: {filename} not found. Run melvin_core first.")
        return [], 0

def read_edges(filename):
    """Read edges from binary file."""
    try:
        with open(filename, 'rb') as f:
            edge_count = struct.unpack('I', f.read(4))[0]
            
            edges = []
            # Edge struct: src(4), dst(4), w_fast(1), w_slow(1), credit(2), 
            #              use_count(2), stale_ticks(2), eligibility(4), 
            #              C11(4), C10(4), avg_U(4), slow_update_countdown(2)
            # Total: 34 bytes + padding
            
            edge_size = 4 + 4 + 1 + 1 + 2 + 2 + 2 + 4 + 4 + 4 + 4 + 2
            
            for _ in range(edge_count):
                data = f.read(edge_size)
                if len(data) < edge_size:
                    break
                
                src = struct.unpack('I', data[0:4])[0]
                dst = struct.unpack('I', data[4:8])[0]
                w_fast = data[8]
                w_slow = data[9]
                credit = struct.unpack('h', data[10:12])[0]
                use_count = struct.unpack('H', data[12:14])[0]
                stale_ticks = struct.unpack('H', data[14:16])[0]
                
                edges.append({
                    'src': src,
                    'dst': dst,
                    'w_fast': w_fast,
                    'w_slow': w_slow,
                    'credit': credit,
                    'use_count': use_count,
                    'stale_ticks': stale_ticks
                })
            
            return edges
    except FileNotFoundError:
        print(f"Error: {filename} not found. Run melvin_core first.")
        return []

def analyze_graph(nodes, edges):
    """Compute and display graph statistics."""
    print("=" * 60)
    print("MELVIN GRAPH ANALYSIS")
    print("=" * 60)
    print()
    
    # Node statistics
    print(f"Total Nodes: {len(nodes)}")
    if nodes:
        active_nodes = sum(1 for n in nodes if n['a'] == 1)
        avg_theta = sum(n['theta'] for n in nodes) / len(nodes)
        avg_in = sum(n['in_deg'] for n in nodes) / len(nodes)
        avg_out = sum(n['out_deg'] for n in nodes) / len(nodes)
        
        print(f"  Active: {active_nodes} ({active_nodes/len(nodes)*100:.1f}%)")
        print(f"  Avg Threshold: {avg_theta:.1f}")
        print(f"  Avg In-Degree: {avg_in:.2f}")
        print(f"  Avg Out-Degree: {avg_out:.2f}")
        
        # Find highly connected nodes
        hub_nodes = sorted(nodes, key=lambda n: n['in_deg'] + n['out_deg'], reverse=True)[:5]
        print()
        print("Top 5 Hub Nodes:")
        for i, n in enumerate(hub_nodes, 1):
            total_deg = n['in_deg'] + n['out_deg']
            print(f"  {i}. Node {n['id']}: in={n['in_deg']}, out={n['out_deg']}, total={total_deg}")
    
    print()
    
    # Edge statistics
    print(f"Total Edges: {len(edges)}")
    if edges:
        avg_w_fast = sum(e['w_fast'] for e in edges) / len(edges)
        avg_w_slow = sum(e['w_slow'] for e in edges) / len(edges)
        avg_credit = sum(e['credit'] for e in edges) / len(edges)
        avg_use = sum(e['use_count'] for e in edges) / len(edges)
        
        print(f"  Avg Fast Weight: {avg_w_fast:.2f}")
        print(f"  Avg Slow Weight: {avg_w_slow:.2f}")
        print(f"  Avg Credit: {avg_credit:.2f}")
        print(f"  Avg Use Count: {avg_use:.2f}")
        
        # Find strongest edges
        strong_edges = sorted(edges, key=lambda e: e['w_fast'] + e['w_slow'], reverse=True)[:5]
        print()
        print("Top 5 Strongest Edges:")
        for i, e in enumerate(strong_edges, 1):
            w_total = e['w_fast'] + e['w_slow']
            print(f"  {i}. {e['src']} → {e['dst']}: w_fast={e['w_fast']}, w_slow={e['w_slow']}, "
                  f"credit={e['credit']}, uses={e['use_count']}")
        
        # Find high-credit edges
        credit_edges = sorted(edges, key=lambda e: e['credit'], reverse=True)[:5]
        print()
        print("Top 5 High-Credit Edges:")
        for i, e in enumerate(credit_edges, 1):
            print(f"  {i}. {e['src']} → {e['dst']}: credit={e['credit']}, "
                  f"w_fast={e['w_fast']}, w_slow={e['w_slow']}")
    
    print()
    
    # Connectivity analysis
    if nodes and edges:
        in_deg_dist = defaultdict(int)
        out_deg_dist = defaultdict(int)
        
        for n in nodes:
            in_deg_dist[n['in_deg']] += 1
            out_deg_dist[n['out_deg']] += 1
        
        print("Degree Distribution:")
        print("  In-Degree:")
        for deg in sorted(in_deg_dist.keys())[:10]:
            print(f"    {deg}: {in_deg_dist[deg]} nodes")
        
        print("  Out-Degree:")
        for deg in sorted(out_deg_dist.keys())[:10]:
            print(f"    {deg}: {out_deg_dist[deg]} nodes")
    
    print()
    print("=" * 60)

def main():
    nodes, next_id = read_nodes('nodes.bin')
    edges = read_edges('edges.bin')
    
    if not nodes and not edges:
        print("No graph data found. Run melvin_core to generate data.")
        sys.exit(1)
    
    analyze_graph(nodes, edges)
    
    print()
    print(f"Next Node ID: {next_id}")
    print()
    print("Note: Analysis assumes C struct packing. Results may vary by platform.")

if __name__ == '__main__':
    main()

