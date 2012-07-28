#include <iostream>
using namespace std;

#include <libxml/xmlmemory.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <errno.h>
#include <math.h>

extern "C" {
#include "utils.h"
#include "base64.h"
#include "curlResponse.h"
#include "logger.h"
}

#include "deleter.h"
#include "amzHeaders.h"
#include "settings.h"

Deleter::Deleter(AmazonCredentials *amazonCredentials, LocalFileList *localFileList, FileListStorage *fileListStorage) {
	this->amazonCredentials = amazonCredentials;
	this->localFileList = localFileList;
	this->fileListStorage = fileListStorage;
}

Deleter::~Deleter() {
	this->amazonCredentials = NULL;
	this->localFileList = NULL;
	this->fileListStorage = NULL;
}

int Deleter::performDeletion() {
	printf("%s\n", this->localFileList->paths[0]);
	return 1;
}
