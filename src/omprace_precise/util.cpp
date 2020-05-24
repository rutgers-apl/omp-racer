#include "util.h"

int createDir(const char* dir_name){
  const int dir_err = mkdir(dir_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (-1 == dir_err)
  {
    std::cout << "Error creating directory!" << std::endl;
  }
  return dir_err;
}

/** @brief check if directory exists. utility function
 *  @param path to the directory
 *  @return 0 if directory exists. 1 otherwise
 */
int dirExists(const char *path){
    struct stat info;
    if(stat( path, &info ) != 0)
        return 0;
    else if(info.st_mode & S_IFDIR)
        return 1;
    else
        return 0;
}