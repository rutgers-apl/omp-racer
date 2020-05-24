#ifndef TASK_DEPENDENCY_H_
#define TASK_DEPENDENCY_H_

#include <iostream>
#include <omp-tools.h>
#include <omp.h>
/// various types of dependency
/*enum class DependenceType{
    NO_TYPE= 0,
    OUT = 1,
    IN = 2,
    IN_OUT = 3
};
*/


/*---------------------
 * dependences type from ompt
 *---------------------*/
/*from omp-tools.h

typedef struct ompt_dependence_t {
  ompt_data_t variable;
  ompt_dependence_type_t dependence_type;
} ompt_dependence_t;


typedef enum ompt_dependence_type_t {
  ompt_dependence_type_in              = 1,
  ompt_dependence_type_out             = 2,
  ompt_dependence_type_inout           = 3,
  ompt_dependence_type_mutexinoutset   = 4,
  ompt_dependence_type_source          = 5,
  ompt_dependence_type_sink            = 6
} ompt_dependence_type_t;

*/

/// Represents a single dependence
class TaskDependence{
public:
    uint64_t addr;//TODO make sure this is correct
    ompt_dependence_type_t dep_type; 
    TaskDependence(uint64_t, ompt_dependence_type_t);
    friend std::ostream& operator<<(std::ostream& os, const TaskDependence& td);
};

#endif // TASK_DEPENDENCY_H_