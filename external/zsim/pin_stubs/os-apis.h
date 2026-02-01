/* Stub os-apis.h - uses pthread instead of Pin CRT */
#ifndef OS_APIS_STUB_H
#define OS_APIS_STUB_H

#include <pthread.h>
#include <time.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Mutex types */
typedef pthread_mutex_t OS_MUTEX_T;
typedef pthread_mutex_t OS_MUTEX_TYPE;

/* RW Lock types */
typedef pthread_rwlock_t OS_RWLOCK_T;
typedef pthread_rwlock_t OS_APIS_RW_LOCK_T;

/* Mutex functions */
static inline void OS_MutexInit(OS_MUTEX_T* m) { pthread_mutex_init(m, NULL); }
static inline void OS_MutexDestroy(OS_MUTEX_T* m) { pthread_mutex_destroy(m); }
static inline void OS_MutexLock(OS_MUTEX_T* m) { pthread_mutex_lock(m); }
static inline void OS_MutexUnlock(OS_MUTEX_T* m) { pthread_mutex_unlock(m); }
static inline int OS_MutexTryLock(OS_MUTEX_T* m) { return pthread_mutex_trylock(m) == 0; }
static inline int OS_MutexIsLocked(OS_MUTEX_T* m) {
    if (pthread_mutex_trylock(m) == 0) {
        pthread_mutex_unlock(m);
        return 0;  /* Not locked */
    }
    return 1;  /* Locked */
}
static inline int OS_MutexTimedLock(OS_MUTEX_T* m, unsigned int timeout_ms) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    return pthread_mutex_timedlock(m, &ts) == 0;
}

/* RW Lock functions */
static inline void OS_RWLockInit(OS_RWLOCK_T* l) { pthread_rwlock_init(l, NULL); }
static inline void OS_RWLockInitialize(OS_RWLOCK_T* l) { pthread_rwlock_init(l, NULL); }
static inline void OS_RWLockDestroy(OS_RWLOCK_T* l) { pthread_rwlock_destroy(l); }
static inline void OS_RWLockAcquireRead(OS_RWLOCK_T* l) { pthread_rwlock_rdlock(l); }
static inline void OS_RWLockAcquireWrite(OS_RWLOCK_T* l) { pthread_rwlock_wrlock(l); }
static inline void OS_RWLockRelease(OS_RWLOCK_T* l) { pthread_rwlock_unlock(l); }
static inline int OS_RWLockTryAcquireRead(OS_RWLOCK_T* l) { return pthread_rwlock_tryrdlock(l) == 0; }
static inline int OS_RWLockTryAcquireWrite(OS_RWLOCK_T* l) { return pthread_rwlock_trywrlock(l) == 0; }

#ifdef __cplusplus
}
#endif

#endif /* OS_APIS_STUB_H */
