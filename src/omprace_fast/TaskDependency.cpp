#include "TaskDependency.h"

TaskDependence::TaskDependence(uint64_t addr, ompt_dependence_type_t dep_type):
addr(addr), dep_type(dep_type)
{}

std::ostream& operator<<(std::ostream& os, const TaskDependence& td){
    os << std::hex << td.addr << std::dec << "," << static_cast<int>(td.dep_type);
    return os;
}
