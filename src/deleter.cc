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
#include "logger.h"
#include "md5.h"
#include "base64.h"
}

#include "deleter.h"
#include "microCurl.h"
#include "settings.h"

Deleter::Deleter(
	AmazonCredentials *amazonCredentials, 
	LocalFileList *filesToDelete, 
	FileListStorage *fileListStorage, 
	char *databaseFilename
) {
	this->amazonCredentials = amazonCredentials;
	this->filesToDelete = filesToDelete;
	this->fileListStorage = fileListStorage;
	this->databaseFilename = databaseFilename;
	this->dryRun=0;
	this->useSsl=1;
	this->connectTimeout = CONNECT_TIMEOUT;
	this->networkTimeout = LOW_SPEED_TIME;
}

Deleter::~Deleter() {
	this->amazonCredentials = NULL;
	this->filesToDelete = NULL;
	this->fileListStorage = NULL;
	this->databaseFilename = NULL;
}


// size_t readFunctionForObjectDelete(void *ptr, size_t size, size_t nmemb, void *userdata)  {
// 	strcpy((char *)ptr, (char*) userdata);
// 	return strlen((char*)userdata);
// }

int Deleter::performPostOnBucket(char *xml, uint32_t *statusCode, char *errorResult) {
	MicroCurl *microCurl = new MicroCurl();
	microCurl->method=METHOD_POST;

	char *date = getIsoDate(); 

	//CURL *curl = curl_easy_init();

	char *canonicalizedResource;
	asprintf(&canonicalizedResource, "/%s/", this->amazonCredentials->bucket);

	md5_state_t state;
	md5_byte_t digest[16];
	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)xml, strlen(xml));
	md5_finish(&state, digest);

	char base64Md5[100];
	base64_encode((char *)digest, 16, (char*) base64Md5, 64);

	char *stringToSign;
	asprintf(&stringToSign, "POST\n%s\n%s\n%s\n%s?delete", base64Md5, "", date, canonicalizedResource); 
	free(canonicalizedResource);

	char *authorization = this->amazonCredentials->createAuthorizationHeader(stringToSign); 
	free(stringToSign);

	if (authorization == NULL) {
		free(date);
		delete microCurl;
		strcpy(errorResult, "Error in auth module");
		return 0;
	}

	char *postUrl = this->amazonCredentials->generateUrlForObjectDelete(this->useSsl);
	microCurl->url = strdup(postUrl);
	LOG(LOG_DBG, "[Delete] POST %s", postUrl);
	free(postUrl);

	microCurl->postData = strdup(xml);
	microCurl->postSize = strlen(xml);

	microCurl->addHeader("Date", date);
	free(date);

	microCurl->addHeader("Authorization", authorization);
	free(authorization);

	microCurl->addHeader("Expect", "");
	microCurl->addHeader("Content-Type", "");

	microCurl->addHeader("Content-MD5", (char *) base64Md5);

	microCurl->connectTimeout = this->connectTimeout;
	microCurl->maxConnects = MAXCONNECTS;
	microCurl->networkTimeout = this->networkTimeout;
	microCurl->lowSpeedLimit = LOW_SPEED_LIMIT;

	microCurl->prepare();
	CURLcode res = microCurl->go();

	*statusCode = microCurl->httpStatusCode;

	// now we are dealing with a weird curl or amazon bug. Sometimes we get CURLE_SEND_ERROR, 
	// but the request has been performed.
	if (res==CURLE_OK || microCurl->httpStatusCode==200) {
		delete microCurl;
		return 1;

	} else { 
		strcpy(errorResult, "Error performing request");
		delete microCurl;
		return 0;
	}
}

int Deleter::deleteBatch(char **batch, uint32_t batchCount, char *errorResult, uint32_t *statusCode) {
	LOG(LOG_INFO, "[Delete] %sDeleting batch of %d objects", this->dryRun ? "[dry] " : "", batchCount);

	uint32_t len = 0;
	for (uint32_t i=0;i<batchCount;i++) {
		len+=strlen(batch[i]);
	}

	len+=(batchCount*64) + 128;

	char *xml = (char*) malloc(len);
	xml[0]=0;

	strcat(xml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	strcat(xml, "<Delete>\n");
	strcat(xml, "<Quiet>true</Quiet>\n");
	for (uint32_t i=0;i<batchCount;i++) {
		strcat(xml, "\t<Object><Key>");
		strcat(xml, batch[i]); // FIXME escape 
		strcat(xml, "</Key></Object>\n");
		LOG(LOG_DBG, "[Delete] %s", batch[i]);
	}
	strcat(xml, "</Delete>\n");

	errorResult[0]=0;
	*statusCode=0;

	int res;
	if (this->dryRun) {
		*statusCode=200;
		res=1;
	} else { 
		res=this->performPostOnBucket(xml, statusCode, errorResult);
	}

	free(xml);

	return res;
}

#define BATCH_SIZE 999

int Deleter::performDeletion() {
	int failed=0;
	uint32_t totalDeletedObjects=0;
	char *journalFilename;
	asprintf(&journalFilename, "%s-journal", databaseFilename);

	char errorResult[1024*20];
	errorResult[0]=0;
	uint32_t statusCode;

	char *batch[BATCH_SIZE];

	uint32_t batchCount=0;
	for (uint32_t i=0; i<this->filesToDelete->count; i++) {
		char *path = this->filesToDelete->paths[i];

		if (strcmp(path, journalFilename)==0 || strcmp(path, databaseFilename)==0) {
			continue;
		}

		batch[batchCount]=path;

		batchCount++;
		if (batchCount>=BATCH_SIZE) {
			if (!this->deleteBatch(batch, batchCount, errorResult, &statusCode)) {
				LOG(LOG_FATAL, "[Delete] %s", errorResult);
				failed=1;
				break;
			} else if (statusCode!=200) {
				LOG(LOG_FATAL, "[Delete] HTTP Status Code = %d", statusCode);
				failed=1;
				break;
			}
			if (!this->dryRun) {
				this->fileListStorage->storeDeletedBatch(batch, batchCount);
			}
			totalDeletedObjects+=batchCount;
			batchCount=0;
		}
	}

	free(journalFilename);

	if (failed) {
		return 0;
	}

	if (batchCount>0) {
		if (!this->deleteBatch(batch, batchCount, errorResult, &statusCode)) {
			LOG(LOG_FATAL, "[Delete] %s", errorResult);
			failed=1;
			return 0;
		} else if (statusCode!=200) {
			LOG(LOG_FATAL, "[Delete] HTTP Status Code = %d", statusCode);
			failed=1;
			return 0;
		}
		if (!this->dryRun) {
			this->fileListStorage->storeDeletedBatch(batch, batchCount);
		}
		totalDeletedObjects+=batchCount;
	}

	if (totalDeletedObjects>0) {
		LOG(LOG_INFO, "[Delete] %sSuccessfully deleted %d objects", this->dryRun ? "[dry] " : "", totalDeletedObjects);
	} else {
		LOG(LOG_INFO, "[Delete] Nothing to delete", totalDeletedObjects);
	}

	return 1;
}
