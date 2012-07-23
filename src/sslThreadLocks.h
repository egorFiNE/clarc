#include <stdlib.h>
#include <stdint.h>
#include <openssl/crypto.h>
#include <pthread.h>

#define OPENSSL_THREAD_DEFINES
#include <openssl/opensslconf.h>
#if defined(OPENSSL_THREADS)
#else
#error I need OpenSSL compiled with threads support!
#endif


static pthread_mutex_t *sslLocksArray;

static unsigned long sslThreadId(void) {
	return (unsigned long)pthread_self();
}

static void sslLockCallback(int mode, int type, const char *file, int line) {
	(void)file;
	(void)line;
	
	if (mode & CRYPTO_LOCK) {
		pthread_mutex_lock(&(sslLocksArray[type]));
	} else {
		pthread_mutex_unlock(&(sslLocksArray[type]));
	}
}

static void sslInitLocks(void) {
	sslLocksArray=(pthread_mutex_t *)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));

	int i;
	for (i=0; i<CRYPTO_num_locks(); i++) {
		pthread_mutex_init(&(sslLocksArray[i]),NULL);
	}

	CRYPTO_set_id_callback((unsigned long (*)())sslThreadId);
	CRYPTO_set_locking_callback(&sslLockCallback);
}
