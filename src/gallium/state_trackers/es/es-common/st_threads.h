#ifndef ST_THREADS_H
#define ST_THREADS_H

/* Macro access to thread-local storage.
 *
 * Variables holding keys should be of type TLS_KEY_TYPE.
 * To allocate a new key, use TLS_ALLOC_KEY(), which will return
 * true if the key was allocated successfully:
 *     TLS_KEY_TYPE key;
 *     if (!TLS_ALLOC_KEY(key, NULL)) {
 *         error("Couldn't allocate a key");
 *         return;
 *     }
 * TLS_DELETE_KEY() releases a key, returning true if successful.
 * TLS_SET_VALUE(key, value) sets the value of the key, returning true if successful.
 * TLS_GET_VALUE(key) returns the value of a key.
 */
#if (defined(WIN32) || defined(_WIN32_WCE))
#include <windows.h>
#define TLS_KEY_TYPE DWORD
#define TLS_ALLOC_KEY(key, destructor) ((key = TlsAlloc()) != 0xffffffff)
#define TLS_DELETE_KEY(key) (TlsFree(key) != 0)
#define TLS_SET_VALUE(key,value) (TlsSetValue(key,(void *)value) != 0)
#define TLS_GET_VALUE(key) TlsGetValue(key)
#else
#include <pthread.h>
#define TLS_KEY_TYPE pthread_key_t
#define TLS_ALLOC_KEY(key, destructor) (pthread_key_create(&key, destructor) == 0)
#define TLS_DELETE_KEY(key) (pthread_key_delete(key) == 0)
#define TLS_SET_VALUE(key,value) (pthread_setspecific(key,(void *)value) == 0)
#define TLS_GET_VALUE(key) pthread_getspecific(key)
#endif

#endif /* ST_THREADS_H */
