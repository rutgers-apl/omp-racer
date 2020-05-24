#ifndef __UTIL_H_
#define __UTIL_H_

#include <sys/stat.h>
#include <sys/types.h>
#include <iostream>
#include <vector>

template <typename T1>
std::ostream& operator << (std::ostream& strm,
const std::vector<T1>& v)
{
    if (v.empty()){
        return strm << "()";
    }
    for (auto it=v.cbegin(); it<v.cend()-1; ++it){
        strm << "(" << *it <<  "), ";
    }
    strm << "(" << *(v.cend()-1) << ")";
    return strm;
}

int createDir(const char* dir_name);
int dirExists(const char *path);


#endif /* __UTIL_H_ */