#ifndef _UPLOAD_H
#define _UPLOAD_H

#include <iostream>
using namespace std;

#include <dispatch/dispatch.h>
#include "fileListStorage.h"
#include "filePattern.h"
#include "threads.h"

#define UPLOAD_SUCCESS 1
#define UPLOAD_FAILED 0

#define MAX_S3_FILE_SIZE 5315022020 // almost 5 gigabytes

class Uploader
{
private: 
	AmazonCredentials *amazonCredentials;
	time_t lastProgressUpdate;
	dispatch_queue_t systemQueryQueue;
	FilePattern *excludeFilePattern;
	int failed;

	int uploadFileWithRetry(
		char *localPath, 
		char *remotePath, 
		char *contentType, 
		struct stat *fileInfo,
		uint32_t *httpStatusCode, 
		char *md5,
		char *errorResult
	);
	CURLcode uploadFile(
		char *localPath, 
		char *remotePath, 
		char *url,
		char *contentType, 
		struct stat *fileInfo,
		uint32_t *httpStatusCode, 
		char *md5,
		char *errorResult
	);
	int uploadFileWithRetryAndStore(
		FileListStorage *fileListStorage, 
		char *localPath, 
		char *remotePath, 
		char *contentType, 
		struct stat *fileInfo,
		//dispatch_queue_t sqlQueue,
		char *errorResult
	);

	static void extractMD5FromETagHeaders(char *headers, char *md5);
	static void extractLocationFromHeaders(char *headers, char *locationResult);
	static char *createRealLocalPath(char *prefix, char *path);

public:
	Threads *threads;
	Uploader(AmazonCredentials *amazonCredentials, FilePattern *excludeFilePattern);
	~Uploader();

	uint64_t totalSize;
	uint64_t uploadedSize;
	int showProgress;
	int useRrs;
	int makeAllPublic;
	int useSsl;
	int dryRun;
	int connectTimeout;
	int networkTimeout;
	int uploadThreads;

	void progress(char *path, double uploadedBytes, double ulnow, double ultotal);
	int uploadFiles(FileListStorage *fileListStorage, char *prefix);
	int uploadDatabase(char *databasePath, char *databaseFilename);
	void runOverThread(uint8_t threadNumber, FileListStorage *fileListStorage, char *realLocalPath, char *path, char *contentType, struct stat *fileInfo);
};

#endif
