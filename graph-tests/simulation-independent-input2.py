import pygraphviz as pgv
import random

seed = 99203

# for mode in ["major", "KK", "sgd", "hier", "ipsep"]:
for mode in ["sgd"]:
    G = pgv.AGraph(mode="sgd", normalize="true")

    nodes = dict()

    input_nodes = [
        ("a", "0,0!"), 
        ("b", "1,1"), 
        ("c", "2,2"), 
        ("d", "3,3"), 
        ("e", "4,4"), 
        ("f", "6,5"), 
        ("g", "6,6"), 
        ("h", "7,7"),
        ]
    
    # random.seed(seed)

    for name,pos in input_nodes:
        # pos = f"{random.random()},{random.random()}"
        G.add_node(name)
        nodes[name] = G.get_node(name)
    
    nodes["a"].attr["pos"] = "0,0!"
    # nodes["g"].attr["pos"] = "0,-2!"

    edges = list()
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

    for i in range(30):
        print("-----------------------")
        print(f"iteration {i} start")
        print(nodes.keys())
        for node_to_position in nodes:
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

            # add nodes with current positions
            for node in neighbors:
                pin = node != node_to_position
                if node == "a":
                    pin = True
                # if node_to_position == "a":
                #     pin = not pin
                tmpG.add_node(node, pos=G.get_node(node).attr["pos"], pin="true" if pin else "false")
            
            # get the positions
            for node in neighbors:
                pos = tmpG.get_node(node).attr['pos']
                print(f"{node=} {pos=}")
            
            # add relevant edges
            for source,target,length in filtered_edges:
                tmpG.add_edge(source, target, len=length)
            # add edges to all from all neighbors to all other nodes
            # for source in neighbors:
            #     for target in set(nodes) - set(neighbors):
            #         tmpG.add_edge(source, target, minlen=1.4)

            # run layout engine
            tmpG._layout("neato")

            # get the positions
            for node in neighbors:
                pos = tmpG.get_node(node).attr['pos']
                if node != node_to_position and pos != G.get_node(node).attr["pos"]:
                    print(f"pos mismatch! {G.get_node(node).attr['pos']=} {pos=}")
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

    for start in range(10):
        for i in range(0,15+1):
            tmpG = pgv.AGraph(mode=mode, notranslate="true", inputscale=72, maxiter=i, start=start)
            
            random.seed(seed)
            for name,pos in input_nodes:
                pos = f"{random.random()},{random.random()}"
                tmpG.add_node(name, pos=pos)
            for source,dest,length in input_edges:
                tmpG.add_edge(source, dest, len=length)
            tmpG.get_node("a").attr["pos"] = "0,0!"
            tmpG.layout("neato")
            tmpG._draw(f"test-global-mode={mode}-start{start}-iter{i}.png")
