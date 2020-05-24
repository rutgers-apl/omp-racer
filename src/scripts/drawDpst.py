#!/usr/bin/python
import csv
import os
import pydot
import pprint
from sets import Set

LogFolder = "./log"
internalNodeFilePrefix = "tree_"
stepNodeFilePrefix = "step_work_"
graph = pydot.Dot(graph_type='graph')
NO_TYPE, ASYNC, FINISH, STEP = range(4)
node_str_dict = {}#key = node_id, val = list of str_vals in csv logs
node_dict = {}#key = node_id in str, val = pydot.Node
total_nodes = 0
step_set = Set()#set of step node_id

def createGraphNode(node_id,node_type):
    color = "blue"
    if (node_type == ASYNC): 
        color="cyan"
    elif (node_type == FINISH): 
        color="red"
    elif (node_type == STEP): 
        color="green"
    tree_node = pydot.Node(node_id, style="filled", fillcolor=color)
    graph.add_node(tree_node)
    node_dict[node_id] = tree_node
    return tree_node
            

def readInternalNodes(num_logs):
    global total_nodes    
    for i in range(num_logs):
        #print(i)
        node_filename = internalNodeFilePrefix + str(i) + ".csv"
        with open(os.path.join(LogFolder, node_filename), 'rb') as nodeCsvFile:
            reader = csv. reader(nodeCsvFile, delimiter=',')
            for internal_node_str in reader:
                #print internal_node_str        
                #print ("node id:%s, parent_id:%s, type: %s" % (internal_node_str[0],internal_node_str[5], internal_node_str[4]))
                current_node_id = internal_node_str[0].strip()
                total_nodes+=1
                node_str_dict[int(current_node_id)] = internal_node_str

                
    return 

def readStepNodes(num_logs):
    global total_nodes
    global step_set

    for i in range(num_logs):
        step_filename = stepNodeFilePrefix + str(i) + ".csv"
        print(step_filename)
        with open(os.path.join(LogFolder, step_filename), 'rb') as stepCsvFile:
            reader = csv. reader(stepCsvFile, delimiter=',')
            for step_node_str in reader:
                #print(step_node_str)
                current_step_id = step_node_str[0].strip()
                total_nodes+=1
                node_str_dict[int(current_step_id)] = step_node_str
                step_set.add(int(current_step_id))
    return

def buildTree():
    #global total_nodes
    #print total_nodes
    #print len(node_str_dict.keys())
    assert(total_nodes == len(node_str_dict.keys()))

    root_node = pydot.Node("0", style="filled", fillcolor="blue")
    graph.add_node(root_node)
    node_dict["0"] = root_node
    print('step set length = %d' % len(step_set))
    for i in range(1,total_nodes+1):
        #print (i)
        if i not in node_str_dict:
			print(str(i) + " node does not exist")
			continue
        #print(i)
        #if len(node_str_dict[i]) == 11: #step node
        if int(node_str_dict[i][0].strip()) in step_set: #step node
            step_node_str = node_str_dict[i]
            current_step_id = step_node_str[0].strip()
            parent_step_id = step_node_str[1].strip()
            #hard coded but it should be fine for now
            #print ("step id:%s, parent_id:%s, cycles: %s" % (current_step_id,parent_step_id, current_step_cycles))
            current_step = createGraphNode(current_step_id, STEP)
            current_edge = pydot.Edge(node_dict[parent_step_id], current_step)
            graph.add_edge(current_edge)
        
        else:
            internal_node_str = node_str_dict[i]
            current_node_id = internal_node_str[0].strip()
            node_str_dict[int(current_node_id)] = internal_node_str
            parent_node_id = internal_node_str[2].strip()
            node_type = int(internal_node_str[1].strip())
            current_node = createGraphNode(current_node_id, node_type)
            current_edge = pydot.Edge(node_dict[parent_node_id], current_node)
            graph.add_edge(current_edge)

        #print(len(node_str_dict[i]))
        #print(node_str_dict[i])
    

def main():
    readInternalNodes(1)
    readStepNodes(1)
    buildTree()
    graph.write_png('tree.png')
    #print graph.to_string()

if __name__ == '__main__':
    main()
