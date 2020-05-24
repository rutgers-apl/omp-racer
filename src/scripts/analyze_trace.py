#!/usr/bin/python
import csv
import os
from collections import OrderedDict 

trace_dir = "./trace"
trace_prefix = "tr_"
num_traces = 16
#key par_id, val: mem_dict
par_access_dict = OrderedDict()
#mem_dict : key: addr, vals: list freq

RD, WR = range(2)
read_count = 0
write_count = 0


def readTraceFiles():
    global num_traces
    global read_count
    global write_count

    for i in range(num_traces):
        trace_filename = trace_prefix + str(i) + ".csv"
        with open(os.path.join(trace_dir, trace_filename), 'rb') as trace_csv:
            reader = csv.reader(trace_csv, delimiter=',')
            count =0
            for m_access_line in reader:
                #if count == 10000:
                #    break
                #tid = i, address = m_access_line[0], type=[1], par_id=[2]
                tid = i
                addr_int = int(m_access_line[0],base=16)
                type_int = int(m_access_line[1])
                par_id_int = int(m_access_line[2])
                if type_int == RD:
                    read_count+=1
                else:
                    write_count+=1
                if par_id_int not in par_access_dict:
                    par_access_dict[par_id_int] = dict()
                
                #get dictionary for current parallel region
                mem_dict = par_access_dict[par_id_int]
                if addr_int not in mem_dict:
                    mem_dict[addr_int] = [0] * (num_traces*2)

                mem_dict[addr_int][type_int*16+tid]+=1 

                count+=1
        print(trace_filename + " read") 
    print('finished reading trace files')

def printDict():
    with open('trace_output.txt','w') as output, open('trace_summary.txt','w') as summary:
        all_accesses = read_count+write_count 
        sum_rd_contention = float(0)
        sum_wr_contention = float(0)
        par_id_count = 0
        for par_id, mem_dict in par_access_dict.items():
            output.write('parallel id ' + str(par_id) + ':\n')
            summary.write('parallel id ' + str(par_id) + ':\n')
            par_id_count+=1
            unique_addr = 0
            unique_read = 0
            unique_write = 0
            rd_contention = 0
            wr_contention = 0
            for addr, freq in mem_dict.items():
                output.write('addr: ' + hex(addr) + '\n')
                output.write('freq rd: ' + str(freq[0:16]) + '\n')
                output.write('freq wr: ' + str(freq[16:])  + '\n')
                count = 0
                for f in freq[0:16]:
                    if f>0:
                        count+=1
                        
                if count>0:
                    unique_read+=1
                rd_contention+=count

                count = 0
                for f in freq[16:]:
                    if f>0:
                        count+=1
                
                if count>0:
                    unique_write+=1
                wr_contention+=count
                unique_addr+=1
            summary.write('#addresses accessed:' + str(unique_addr) + '\n')
            summary.write('rd contention:' + str(float(rd_contention)/unique_read)+ '\n')
            summary.write('wr contention:' + str(float(wr_contention)/unique_write)+ '\n')
            sum_rd_contention+= float(rd_contention)/unique_read
            sum_wr_contention+= float(wr_contention)/unique_write
            print('parallel id ' + str(par_id) + ' analyzed')
        summary.write('------------aggregate results------------\n')
        summary.write('#instrument mem calls in all parallel regions:' + str(all_accesses) + '\n')
        summary.write('fraction of reads:' + str(float(read_count)/all_accesses) + '\n')
        summary.write('fraction of writes:' + str(float(write_count)/all_accesses) + '\n')
        summary.write('#parallel ids accessed:' + str(par_id_count)+ '\n')
        summary.write('avg rd contention:' + str(sum_rd_contention/par_id_count)+ '\n')
        summary.write('avg wr contention:' + str(sum_wr_contention/par_id_count)+ '\n')



def main():
    readTraceFiles()
    printDict()
    
    
    

if __name__ == '__main__':
    main()