#ifndef LOCK_SET_H_
#define LOCK_SET_H_

#include <map>
#include <stack>
#include <pthread.h>

struct LockSet {
private:
  //number of locks occupying bitmap
  static int lock_count;
  //mapping from lock addr to index in bitmap
  static std::map<uint64_t, size_t> lockid_map;
  //bitmap representing lockset
  size_t lockset;
  //stack representing all lock ids held by lockset
  std::stack<uint64_t> locks;

  void addLockToStack (uint64_t lockId);

public:
  LockSet();
  size_t getLockset();
  void addLockToLockset(uint64_t lock);
  void removeLockFromLockset();
  static size_t getLockId(uint64_t lock);
};

#endif //LOCK_SET_H_