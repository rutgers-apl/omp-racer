/*
* taskgroup test case based on openmp5.0 examples
*/

#include "util.h"
#include <stdio.h>
void start_background_work(void){
    serialwork(100000000);
    printf("background work done\n");
}

void check_step(int i){
    printf("check step for i=%d\n", i);
}


struct tree_node
{
   struct tree_node *left{nullptr};
   struct tree_node *right{nullptr};
   int val{0};
};
typedef struct tree_node* tree_type;

void print_results(tree_type root){
    printf("root val=%d, root.left val=%d, root.right val=%d\n", root->val, root->left->val, root->right->val);
}

//create a tree
tree_type init_tree(void){
    tree_type root = new struct tree_node;
    root->val = 1;
    tree_type left = new struct tree_node;
    left->val = 2;
    tree_type right = new struct tree_node;
    right->val = 3;
    root->left = left;
    root->right = right;
    return root;
}

#define max_steps 2

void compute_something(tree_type tree)
{
   // some computation
   serialwork(10000000);
   tree->val +=1;
}
void compute_tree(tree_type tree)
{
   if (tree->left)
   {
     #pragma omp task
       compute_tree(tree->left);
   }
   if (tree->right)
   {
     #pragma omp task
       compute_tree(tree->right);
   }
   #pragma omp task
   compute_something(tree);
}

int main()
{
  int i;
  tree_type tree;
  tree = init_tree();
  #pragma omp parallel
  #pragma omp single
  {
    #pragma omp task
      start_background_work();
    for (i = 0; i < max_steps; i++)
    {
        #pragma omp taskgroup
        {
           #pragma omp task
             compute_tree(tree);
        } // wait on tree traversal in this step
        check_step(i);
    }
  } // only now is background work required to be complete
  print_results(tree);
  return 0;
}