# Test node positioning

Nodes are devices spread across wide area. Each node can measure distance to other nodes that are close enough that WiFi signal reaches. In this project there are examples of using already known algorithms for graph layout to estimate the relative position of all devices. 

In the graph, nodes will be graph nodes (points) and edges between nodes will be the distances measured between the nodes.

## Global algorithms

Using script `compare-attrs.sh`, we can test graphviz's layout algorithms for this problem. It generates several images with different combination of parameters.

From my testing SGD algorithm works the best. At least for well defined layouts. When there is a node that can measure distance to only one other node the position cannot be precisely determined since it can be anywhere on a circle with radius of the distance between the nodes. We could use knowledge of how the distances are measured to narrow it down to small area but it would require to modify the algorithms (that if we did not measure distance to some node it should be further than some distance).

## Local algorithm

In `simulation-independent.py` we are testing an alternative approach of this problem where we globally know the distances between nodes, only nodes know the distances to its neighbors. The only globally known information is the position of nodes. So for each node we create a graph of its local area from which we compute how to change its position given that other nodes are fixed. If we iterate through all nodes several times we get similar results to global algorithm. Only nodes with only one distance to other nodes has completely wrong position (inside or close to other nodes). That can be expected since the position of such node cannot be accurately determined and when we are positioning the node we only know about one neighboring node.

The algorithm could be modified such each node would be repelled from other nodes. This way the positions could be improved but that is yet to be tested.