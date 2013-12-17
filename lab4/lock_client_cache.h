// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"
#include <map>


// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.

class lock_release_user {
public:
    virtual void dorelease(lock_protocol::lockid_t) = 0;

    virtual ~lock_release_user() {
    };
};

class lock_client_cache : public lock_client {
private:
    class lock_release_user *lu;
    int rlock_port;
    std::string hostname;
    std::string id;

protected:

    enum lock_cache_state {
        NONE, FREE, LOCKED, ACQUIRING, RELEASING
    };
    typedef int l_cache_state;

    struct lock_cache_value {
        l_cache_state lock_cache_state; // State of the lock cache
        pthread_mutex_t client_lock_mutex; // To protect this structure
        pthread_cond_t client_lock_sign; // CV to be notified of state change
        pthread_cond_t client_revoke_sign; // CV to be notified if lock is revoked
    };

    typedef std::map<lock_protocol::lockid_t, lock_cache_value *> TLockCacheStateMap;
    TLockCacheStateMap clientcacheMap;
    pthread_mutex_t client_cache_mutex;
    pthread_cond_t client_cache_sign;
    pthread_mutex_t client_retry_mutex, client_releaser_mutex;
    pthread_cond_t client_retry_sign, client_releaser_sign;
    std::list<lock_protocol::lockid_t> retry_lock_list;
    std::list<lock_protocol::lockid_t> revoke_lock_list;
    lock_cache_value* get_lock_obj(lock_protocol::lockid_t lid);

public:
    static int last_port;
    lock_client_cache(std::string xdst, class lock_release_user *l = 0);

    virtual ~lock_client_cache() {
    };
    lock_protocol::status acquire(lock_protocol::lockid_t);
    lock_protocol::status release(lock_protocol::lockid_t);
    rlock_protocol::status revoke_handler(lock_protocol::lockid_t,
            int &);
    rlock_protocol::status retry_handler(lock_protocol::lockid_t,
            int &);

    void retryer(void);
    void releaser(void);
    int acquire_from_server(long long unsigned int, lock_client_cache::lock_cache_value*);
    void wait_to_acquire(long long unsigned int, lock_client_cache::lock_cache_value*);
};


#endif
