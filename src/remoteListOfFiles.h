#ifndef _REMOTELISTOFFILES_H
#define _REMOTELISTOFFILES_H

#include <iostream>
using namespace std;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "amazonCredentials.h"
#include "threads.h"
#include "microCurl.h"


class RemoteListOfFiles
{
private:
	AmazonCredentials *amazonCredentials;
	uint32_t allocCount;
	Threads *threads;
	int failed;
	time_t lastProgressUpdate;

	static char *extractMd5FromEtag(char *etag);
	uint32_t extractMtimeFromMicroCurl(MicroCurl *microCurl);

	int parseListOfFiles(char *body, uint64_t bodySize, uint8_t *isTruncated, char *lastKey, char *errorResult);
	int performGetOnBucket(char *url, char *marker, int setLocationHeader, char *body, uint64_t *bodySize, uint32_t *statusCode, char *errorResult);
	CURLcode performHeadOnFileWithRetry(char *url, char *remotePath, uint32_t *remoteMtime, uint32_t *statusCode, char *errorResult);
	CURLcode performHeadOnFile(char *url, char *remotePath, uint32_t *remoteMtime, uint32_t *statusCode, char *errorResult);
	int performPutOnBucket(char *url, char *region, uint32_t *statusCode, char *errorResult);

public:
	RemoteListOfFiles(AmazonCredentials *amazonCredentials);
	~RemoteListOfFiles();
	
	void add(char *path, char *md5);
	int downloadList();
	int resolveMtimes();

	int checkAuth();
	int createBucket(char *region);

	void runOverThread(int threadNumber, int pos);

	char **paths;
	char **md5s;
	uint32_t *mtimes;
	uint32_t count;
	int showProgress;
	int useSsl;
	int connectTimeout;
	int networkTimeout;
};

#define LIST_FAILED 0
#define LIST_SUCCESS 1

#define CREATE_FAILED 0
#define CREATE_SUCCESS 1

#define AUTH_FAILED 0
#define AUTH_FAILED_BUCKET_DOESNT_EXISTS -1
#define AUTH_SUCCESS 1

#endif
