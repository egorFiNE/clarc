#include <iostream>
using namespace std;

#include "threads.h"
#include <unistd.h>

Threads::Threads(int count) {
	this->threadsCount=count;

	int i=0;
	for(i=0;i<this->threadsCount;i++) {
		this->busyThreads[i]=0;
	}	
}

Threads::~Threads() {
}

int Threads::findFreeThread() {
	int i;
	for(i=0;i<this->threadsCount;i++) {
		if (this->busyThreads[i]==0) {
			return i;
		}
	}
	return -1;
}

void Threads::sleepTillAllThreadsFree() {
	int foundBusy=0;
	
	do {
		foundBusy=0;
		for(int i=0;i<this->threadsCount;i++) {
			if (this->busyThreads[i]==1) {
				foundBusy=1;
			}
		}
		usleep(100000);
	} while (foundBusy);
}

int Threads::sleepTillThreadFree() {
	int currentThread=0;
	do { 
		currentThread=this->findFreeThread();
		if (currentThread<0) {
			usleep(100000);
		}
	} while (currentThread<0);
	return currentThread;
}

void Threads::setThreadId(int threadNumber, pthread_t threadId) {
	this->threads[threadNumber]=threadId;
}

void Threads::markBusy(int threadNumber) {
	this->busyThreads[threadNumber]=1;
}

void Threads::markFree(int threadNumber) {
	this->busyThreads[threadNumber] = 0;
}

