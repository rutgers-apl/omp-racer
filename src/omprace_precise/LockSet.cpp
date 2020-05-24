#include "LockSet.h"

pthread_mutex_t lockmap_lock;

int LockSet::lock_count = 0;
std::map<uint64_t, size_t> LockSet::lockid_map;

//create an empty lockset
LockSet::LockSet() {
  lockset = 0xffffffffffffffff;
}

//fast path to get current lockset
size_t LockSet::getLockset() {
  return lockset;
}

void LockSet::addLockToStack(uint64_t lockId) {
  locks.push(lockId);
}

void LockSet::addLockToLockset(uint64_t lock_held) {
  size_t lockId = getLockId(lock_held);
  addLockToStack(lockId);
  lockset = lockset & lockId;
}

void LockSet::removeLockFromLockset() {
  size_t lockId = locks.top();
  lockset = lockset | ~(lockId);
  locks.pop();
}

size_t LockSet::getLockId(uint64_t lock_held) {
  size_t lockId;
  pthread_mutex_lock(&lockmap_lock);
  if (lockid_map.count(lock_held) == 0) {
    //why?
    lockId = ~((size_t)1 << lock_count++);
    lockid_map.insert(std::pair<uint64_t, size_t>(lock_held, lockId));
  } else {
    lockId = lockid_map.at(lock_held);
  }
  pthread_mutex_unlock(&lockmap_lock);
  return lockId;
}
