// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"
#include <set>

class lock_server {

 protected:
  int nacquire;
  std::map<lock_protocol::lockid_t, pthread_mutex_t> lock_mutex_map;
  std::map<lock_protocol::lockid_t, pthread_cond_t> lock_cond_map;
  std::map<lock_protocol::lockid_t, int> lock_clt_map;
  std::set<lock_protocol::lockid_t> active_lock;
  pthread_mutex_t rwlock;
  void new_lock(int clt, lock_protocol::lockid_t lid);
  std::vector<pthread_mutex_t> mtx_pool;
  std::vector<pthread_cond_t> cond_pool;
  
 public:
  lock_server();
  ~lock_server() {};
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 







