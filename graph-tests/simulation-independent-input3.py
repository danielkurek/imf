import pygraphviz as pgv
import random
import math

def convert_pos(position_str):
    if len(position_str) == 0:
        return None, None
    pos = position_str.split(",")
    if position_str[-1] == "!":
        pos = position_str[:-1].split(",")
    return float(pos[0]), float(pos[1])

def distance(pos1, pos2):
    return math.sqrt((pos1[0] - pos2[0])**2 + (pos1[1] - pos2[1])**2)

seed = 99203

for mode in ["major", "KK", "sgd", "hier", "ipsep"]:
# for mode in ["sgd"]:
    print(f"================{mode}================")
    G = pgv.AGraph(mode="sgd", normalize="true")

    nodes = dict()
    
    random.seed(seed)
    # input_nodes = "ABCDEFGHIJKLMN"
    input_nodes = "abcdefgh"

    for name in input_nodes:
        # pos = f"{random.random()},{random.random()}"
        G.add_node(name)
        nodes[name] = G.get_node(name)
    
    fixed_node = "a"
    nodes[fixed_node].attr["pos"] = "0,0!"
    # fixed_node = "D"
    # nodes[fixed_node].attr["pos"] = "-1,5!"

    edges = list()
    # input_edges = [
    #     ("A", "B", 2.82),
    #     ("A", "C", 3.6),
    #     ("A", "D", 2.23),
    #     ("A", "E", 2.23),
    #     # ("A", "F", 3.6),
    #     # ("A", "G", 4),
    #     ("B", "E", 4.12),
    #     ("B", "F", 4.12),
    #     ("B", "J", 3.16),
    #     ("B", "M", 3.6),
    #     ("C", "D", 4),
    #     ("C", "I", 3.6),
    #     ("C", "K", 3),
    #     ("C", "J", 3.6),
    #     ("D", "E", 3.16),
    #     ("D", "G", 3.6),
    #     ("D", "H", 3),
    #     ("D", "I", 3.6),
    #     ("E", "F", 2),
    #     ("E", "G", 2.23),
    #     ("G", "H", 2),
    #     ("J", "K", 3.16),
    #     ("J", "L", 3.16),
    #     ("J", "M", 3),
    #     ("K", "L", 2.82),
    #     ("M", "N", 2.23),]
    input_edges = [
        ("a", "b", 2),
        ("a", "d", 1.4),
        ("a", "e", 1.4),
        ("a", "f", 1.4),
        ("a", "h", 1.4),
        ("b", "d", 1.4),
        ("b", "h", 1.4),
        ("b", "c", 2),
        ("d", "e", 2),
        ("e", "f", 2),
        ("f", "g", 1.4),
        ("f", "h", 2),
        ("g", "h", 1.4)]
    for source,dest,length in input_edges:
        edges.append(G.add_edge(source, dest, len=length))

    for i in range(100):
        print("-----------------------")
        print(f"iteration {i} start")
        print(nodes.keys())
        for node_to_position in random.sample(input_nodes, len(input_nodes)):
            # skip fixed node
            # if node_to_position == "a":
            #     continue
            print(node_to_position)
            # create subset of the graph
            neighbors = G.neighbors(node_to_position) + [node_to_position]
            filtered_edges = []
            for edge in input_edges:
                if edge[0] in neighbors and edge[1] in neighbors:
                    filtered_edges.append(edge)
            print(filtered_edges)

            # create graph
            tmpG = pgv.AGraph(mode=mode, notranslate="true", maxiter=2, inputscale=72)
            # tmpG = pgv.AGraph(mode="sgd")

            # TODO: add repel mechanism - node_to_position should be repelled from non-neighboring nodes

            # add nodes with current positions
            for node in neighbors:
                pin = node != node_to_position
                if node == fixed_node:
                    pin = True
                # if node_to_position == "a":
                #     pin = not pin
                tmpG.add_node(node, pos=G.get_node(node).attr["pos"], pin="true" if pin else "false")
            
            # get the positions
            for node in neighbors:
                pos = tmpG.get_node(node).attr['pos']
                print(f"{node=} {pos=}")
            
            # add edges from node_to_position to neighbors
            for source,target,length in filtered_edges:
                tmpG.add_edge(source, target, len=length)
            

            added_nodes = set()
            for source in neighbors:
                for target in set(nodes) - set(neighbors):
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
                    if dist == 0.0:
                        print(f"skipping edge {source=},{target=} because {dist=} is 0")
                        continue
                    tmpG.add_edge(source, target, len=min(dist, 10))
            print("****")
            # run layout engine
            tmpG._layout("neato")

            # get the positions
            for node in neighbors:
                pos = tmpG.get_node(node).attr['pos']
                if node != node_to_position and pos != G.get_node(node).attr["pos"]:
                    print(f"pos mismatch! {G.get_node(node).attr['pos']=} {pos=}")
                if "nan" in pos:
                    print(f"Skipping setting position to {node=} because contains nan {pos=}")
                    continue
                if node != node_to_position:
                    continue
                G.get_node(node).attr["pos"] = pos
                print(f"{node=} {pos=}")
            # G.get_node(node_to_position).attr["pos"] = tmpG.get_node(node_to_position).attr['pos']
            
            # tmpG.draw(f"test-iter{i}-position_{node_to_position.name}.png")

        print("-----------------------")
        print(f"iteration={i}")
        for node in nodes.keys():
            print(f"{node}: {G.get_node(node).attr['pos']}")
        print("-----------------------")
        tmpG = pgv.AGraph(mode="sgd")
        for node in nodes:
            tmpG.add_node(node, pos=G.get_node(node).attr['pos'])
        for source,target,length in input_edges:
            tmpG.add_edge(source,target,len=length)
        tmpG.layout("neato", args="-n2")

        tmpG.draw(f"test-independent-mode={mode}-iter{i}.png")

    G.layout("neato", args="-n2")

    G.draw(f"test-independent-mode={mode}-final.png")

    # for start in range(15):
    #     for i in range(0,15+1):
    #         tmpG = pgv.AGraph(mode=mode, notranslate="true", inputscale=72, maxiter=i, start=start)
            
    #         random.seed(seed)
    #         for name in input_nodes:
    #             pos = f"{random.random()},{random.random()}"
    #             tmpG.add_node(name, pos=pos)
    #         for source,dest,length in input_edges:
    #             tmpG.add_edge(source, dest, len=length)
    #         tmpG.get_node("A").attr["pos"] = "-3,6!"
    #         tmpG.layout("neato")
    #         tmpG._draw(f"test-global-mode={mode}-random-start{start}-iter{i}.png")
