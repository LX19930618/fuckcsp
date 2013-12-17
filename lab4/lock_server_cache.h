#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>


#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"

class lock_server_cache {
private:
    int nacquire;

protected:

    enum lock_state {
        LOCKFREE, LOCKED, REVOKING, RETRYING
    };
    typedef int l_state;

    struct lock_cache_value {
        l_state lock_state;
        std::string owner_clientid_string; // used to send revoke
        std::string retrying_clientid_string; // used to match with incoming acquire request
        std::list<std::string> waiting_clientids_list; // need to send retry
    };
    typedef std::map<lock_protocol::lockid_t, lock_cache_value*> TLockStateMap;
    TLockStateMap tLockMap;

    struct client_info {
        std::string client_id_string;
        lock_protocol::lockid_t lid;
    };
    std::list<client_info> retry_client_list;
    std::list<client_info> revoke_client_list;
    lock_cache_value* get_lock_obj(lock_protocol::lockid_t lid);
    pthread_mutex_t lmap_mutex;
    pthread_cond_t lmap_state_sign;
    pthread_mutex_t retry_mutex;
    pthread_cond_t retry_sign;
    pthread_mutex_t releaser_mutex;
    pthread_cond_t releaser_sign;

public:
    lock_server_cache();
    ~lock_server_cache();
    lock_protocol::status stat(lock_protocol::lockid_t, int &);
    int acquire(lock_protocol::lockid_t, std::string id, int &);
    int release(lock_protocol::lockid_t, std::string id, int &);
    void retryer();
    void releaser();
};

#endif
