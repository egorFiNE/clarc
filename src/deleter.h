#ifndef _DELETER_H
#define _DELETER_H

#include <iostream>
using namespace std;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "amazonCredentials.h"
#include "fileListStorage.h"
#include "localFileList.h"

class Deleter
{
private:
	AmazonCredentials *amazonCredentials;
	LocalFileList *filesToDelete;
	FileListStorage *fileListStorage;
	char *databaseFilename;
	int deleteBatch(char **batch, uint32_t batchCount, char *errorResult, uint32_t *statusCode);
	int performPostOnBucket(char *xml, uint32_t *statusCode, char *errorResult);
	static char *xmlEscape(char *src);

public:
	int dryRun;
	int useSsl;
	int connectTimeout;
	int networkTimeout;

	Deleter(AmazonCredentials *amazonCredentials, LocalFileList *filesToDelete, FileListStorage *fileListStorage, char *databaseFilename);
	~Deleter();
	int performDeletion();
};

#endif
