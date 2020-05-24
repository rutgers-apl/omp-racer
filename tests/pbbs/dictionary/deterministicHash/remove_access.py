import sys

inst_call = "call void @__omprace_instrument_access__"
loop_meta = "!llvm.mem.parallel_loop_access"
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print('usage, removes_access <ll file>')
        exit()
    file_name = sys.argv[1]    
    removals = 0
    lines = []
    with open(file_name, 'r') as f:
        lines = f.readlines()
        for i in range(1,len(lines)):
            if loop_meta in lines[i] and inst_call in lines[i-1]:
                lines[i-1] = ';' + lines[i-1]
                removals+=1

    with open(file_name+'.mod', 'w') as f:
        for line in lines:
            f.write(line)
    print('removes accesses = ' , removals)