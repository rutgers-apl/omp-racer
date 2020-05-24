#ifndef __TRACE_READER_H_
#define __TRACE_READER_H_

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>

//static const size_t appBase = 0x7f0000000000ull;
//static const size_t appEnd =  0x7fffffffffffull;

class TraceReader{
    public:
        TraceReader(std::string _trace_directory,std::string _file_prefix, int _trace_num)
        : trace_directory(_trace_directory),file_prefix(_file_prefix),trace_num(_trace_num)
        {
            mem_accessed = new std::vector<size_t>[trace_num];
        }

        void ReadTraceFiles(){
            for (int i=0; i<trace_num; i++){
                std::string file_name = trace_directory + "/" + file_prefix + "_" + std::to_string(i) + ".csv";
                ReadTraceFile(file_name, i);
            }
            return;
        }
        std::vector<size_t> *GetAccessVecotrs(){
            return mem_accessed;
        }
    private:
        void ReadTraceFile(std::string file_name, int tid){
            std::ifstream f(file_name);
            if (!f.good()){
                std::cerr << "unable to open trace file: " << file_name << ". Exiting program" << std::endl;
                exit(1);
            }
            std::string line;
            while (std::getline(f,line)){
                std::stringstream *sstream= new std::stringstream(line);
                std::string addr_str;
                std::getline(*sstream, addr_str, ',');            
                auto addr = std::stoul(addr_str, nullptr, 16);
                assert(addr >= 0x7f0000000000ull);
                assert(addr <= 0x7fffffffffffull);
                mem_accessed[tid].push_back(addr);
                //TODO read memory R/W from csv as well
                delete sstream;
            }
        }
        std::string trace_directory;
        std::string file_prefix;
        int trace_num;
        std::vector<size_t> *mem_accessed;
        
};

#endif