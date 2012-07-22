#ifndef _THREADS_H
#define _THREADS_H

#include <iostream>
using namespace std;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>

#define MAX_THREADS 100

class Threads
{
private:
	pthread_t threads[MAX_THREADS];
	int busyThreads[MAX_THREADS];
	int threadsCount;

public:
	Threads(int count);
	~Threads();

	int findFreeThread();
	void sleepTillAllThreadsFree();
	int sleepTillThreadFree();
	void markBusy(int threadNumber);
	void setThreadId(int threadNumber, pthread_t threadId);
	void markFree(int threadNumber);
};

#endif
