import pygraphviz as pgv
import random
import math
from pathlib import Path

def convert_pos(position_str):
    if len(position_str) == 0:
        return None, None
    pos = position_str.split(",")
    if position_str[-1] == "!":
        pos = position_str[:-1].split(",")
    return float(pos[0]), float(pos[1])

def distance(pos1, pos2):
    return math.sqrt((pos1[0] - pos2[0])**2 + (pos1[1] - pos2[1])**2)

def load_input(filename):
    G = pgv.AGraph(filename)
    edges = [(*edge, float(edge.attr["len"])) for edge in G.edges()]
    nodes = G.nodes()
    fixed_nodes = {}
    for node in nodes:
        if "!" in node.attr["pos"]:
            fixed_nodes[node] = node.attr["pos"]
    return nodes, edges, fixed_nodes

def layout_test(input_file, output_extension="png", modes=["sgd"], position_all_nodes=False, 
                fixed_nodes_positioning=False, independent_iterations=30, max_iter_layout=2, 
                save_layout_each_iter=True, save_final_layout=True, correct_position_scale=False, 
                seed=None):
    output_file_prefix = Path(input_file).stem
    for mode in modes:
        print(f"================{mode}================")
        G = pgv.AGraph(mode="sgd", normalize="true")

        nodes = dict()

        if seed != None:
            random.seed(seed)
        
        input_nodes, input_edges, fixed_nodes = load_input(input_file)

        for name in input_nodes:
            # pos = f"{random.random()},{random.random()}"
            G.add_node(name)
            nodes[name] = G.get_node(name)
        
        for fixed_node,pos in fixed_nodes.items():
            if correct_position_scale:
                nodes[fixed_node].attr["pos"] = pos
            else:
                x,y = convert_pos(pos)
                if x is None or y is None:
                    print(f"Error: cannot parse position \"{pos}\" of fixed node {fixed_node}! {x=};{y=}")
                    continue
                nodes[fixed_node].attr["pos"] = f"{x*72},{y*72}!"

        edges = list()
        
        for source,dest,length in input_edges:
            edges.append(G.add_edge(source, dest, len=length))

        for i in range(independent_iterations):
            print("-----------------------")
            print(f"iteration {i} start")
            print(nodes.keys())
            for node_to_position in random.sample(input_nodes, len(input_nodes)):
                # skip fixed node
                # if node_to_position in fixed_nodes:
                #     continue
                print(node_to_position)
                # create subset of the graph
                local_nodes = G.neighbors(node_to_position) + [node_to_position]
                filtered_edges = []
                for edge in input_edges:
                    if edge[0] in local_nodes and edge[1] in local_nodes:
                        filtered_edges.append(edge)
                print(filtered_edges)

                # create graph
                tmpG = pgv.AGraph(mode=mode, notranslate="true", maxiter=max_iter_layout, inputscale=72)

                # add nodes with current positions
                for node in local_nodes:
                    pin = node != node_to_position
                    if node in fixed_nodes:
                        pin = True
                    tmpG.add_node(node, pos=G.get_node(node).attr["pos"], pin="true" if pin else "false")
                
                # print positions
                for node in local_nodes:
                    pos = tmpG.get_node(node).attr['pos']
                    print(f"{node=} {pos=}")
                
                # add edges from node_to_position to neighbors
                for source,target,length in filtered_edges:
                    tmpG.add_edge(source, target, len=length)
                
                added_nodes = set()
                for source in local_nodes:
                    for target in set(nodes) - set(local_nodes):
                        pos1 = convert_pos(G.get_node(source).attr["pos"])
                        if pos1[0] is None or pos1[1] is None:
                            continue
                        pos2 = convert_pos(G.get_node(target).attr["pos"])
                        if pos2[0] is None or pos2[1] is None:
                            continue
                        print(f"{source=} {target=} {pos1=} {pos2=} {distance(pos1, pos2)=}")
                        if target not in added_nodes:
                            tmpG.add_node(target, pos=G.get_node(target).attr["pos"], pin="true")
                        dist = distance(pos1, pos2)
                        if abs(dist) <= 0.001:
                            print(f"skipping edge {source=},{target=} because {dist=} is 0")
                            continue
                        tmpG.add_edge(source, target, len=min(dist, 10))
                print("****")
                # run layout engine
                tmpG._layout("neato")

                # update positions
                for node in local_nodes:
                    pos = tmpG.get_node(node).attr['pos']
                    if node != node_to_position and pos != G.get_node(node).attr["pos"]:
                        print(f"pos moved for node {node} when positioning {node_to_position}! {G.get_node(node).attr['pos']=} {pos=}")
                    if "nan" in pos:
                        print(f"Skipping setting position to {node=} because contains nan {pos=}")
                        continue
                    if not fixed_nodes_positioning and node in fixed_nodes:
                        continue
                    if not position_all_nodes and node != node_to_position:
                        continue
                    
                    # set updated position
                    if correct_position_scale:
                        x,y = convert_pos(pos)
                        if x is None or y is None:
                            print(f"Could not set position for node {node} during positioning of {node_to_position}!! {pos=} {mode=} {i=}")
                            continue
                        x /= 72
                        y /= 72
                        G.get_node(node).attr["pos"] = f"{x},{y}{"!" if node in fixed_nodes else ""}"
                    else:
                        G.get_node(node).attr["pos"] = pos
                    print(f"new position: {node=} {pos=}")

            print("-----------------------")
            print(f"iteration={i}")
            for node in nodes.keys():
                print(f"{node}: {G.get_node(node).attr['pos']}")
            print("-----------------------")
            if save_layout_each_iter:
                tmpG = pgv.AGraph(mode="sgd")
                for node in nodes:
                    tmpG.add_node(node, pos=G.get_node(node).attr['pos'])
                for source,target,length in input_edges:
                    tmpG.add_edge(source,target,len=length)
                tmpG.layout("neato", args="-n2")
                tmpG.draw(f"{output_file_prefix}-independent-mode={mode}-iter{i}.{output_extension}")

        if save_final_layout:
            G.layout("neato", args="-n2")
            G.draw(f"{output_file_prefix}-independent-mode={mode}-final.{output_extension}")


input_files = ["../input2.dot", "../input3.dot" ]
modes = ["major", "KK", "sgd", "hier", "ipsep"]
output_extension = "svg"
position_all_nodes = False
fixed_nodes_positioning = True
independent_iterations = 100
max_iter_layout = 2
save_layout_each_iter = False
save_final_layout = True
correct_position_scale = False
seed = 99203

for input_file in input_files:
    print("========================================")
    print(f"{input_file=}")
    print("========================================")
    print("\n\n\n")
    layout_test(
        input_file=input_file,
        output_extension=output_extension,
        modes=modes,
        position_all_nodes=position_all_nodes,
        fixed_nodes_positioning=fixed_nodes_positioning,
        independent_iterations=independent_iterations,
        max_iter_layout=max_iter_layout,
        save_layout_each_iter=save_layout_each_iter,
        save_final_layout=save_final_layout,
        correct_position_scale=correct_position_scale,
        seed=seed)