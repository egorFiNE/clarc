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
#include "md5.h"
#include "base64.h"
}

#include "deleter.h"
#include "amzHeaders.h"
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

size_t readFunctionForObjectDelete(void *ptr, size_t size, size_t nmemb, void *userdata)  {
	strcpy((char *)ptr, (char*) userdata);
	return strlen((char*)userdata);
}

int Deleter::performPostOnBucket(char *xml, uint32_t *statusCode, char *errorResult) {
	char method[5]="POST";

	char *date = getIsoDate(); 

	CURL *curl = curl_easy_init();

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
	asprintf(&stringToSign, "%s\n%s\n%s\n%s\n%s?delete", method, base64Md5, "", date, canonicalizedResource); 
	free(canonicalizedResource);

	char *authorization = this->amazonCredentials->createAuthorizationHeader(stringToSign); 
	free(stringToSign);

	if (authorization == NULL) {
		free(date);
		curl_easy_cleanup(curl);		
		strcpy(errorResult, "Error in auth module");
		return 0;
	}

	char *postUrl = this->amazonCredentials->generateUrlForObjectDelete(this->useSsl);

	struct CurlResponse curlResponse;
	CurlResponseInit(&curlResponse);

	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, NULL);

	curl_easy_setopt(curl, CURLOPT_READDATA, xml);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, &readFunctionForObjectDelete);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(xml));

	curl_easy_setopt(curl, CURLOPT_URL, postUrl);
	LOG(LOG_DBG, "[Delete] POST %s", postUrl);
	free(postUrl);

	struct curl_slist *slist = NULL;

	slist = AmzHeaders::addHeader(slist, "Date", date);
	free(date);

	slist = AmzHeaders::addHeader(slist, "Authorization", authorization);
	free(authorization);

	slist = curl_slist_append(slist, "Expect:");
	slist = curl_slist_append(slist, "Content-Type:");

	slist = AmzHeaders::addHeader(slist, "Content-MD5", (char *) base64Md5);

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, this->connectTimeout);
	curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, MAXCONNECTS);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, this->networkTimeout);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, LOW_SPEED_LIMIT);

	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&curlResponse);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlResponseBodyCallback);

	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, (void *)&curlResponse); 
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &CurlResponseHeadersCallback);

	CURLcode res = curl_easy_perform(curl);

	curl_slist_free_all(slist);

	*statusCode = 0;

	if (res==CURLE_OK) {
		long httpStatus = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
		*statusCode = (uint32_t) httpStatus;

		curl_easy_cleanup(curl);
		CurlResponseFree(&curlResponse);
		return 1;

	} else { 
		// now we are dealing with a weird curl or amazon bug. Sometimes we get CURLE_SEND_ERROR, 
		// but the request has been performed.
		if (strlen(curlResponse.headers)>1) {
			if (strstr(curlResponse.headers, "HTTP/1.1 200 OK")==curlResponse.headers) {
				curl_easy_cleanup(curl);
				CurlResponseFree(&curlResponse);
				*statusCode=200;
				return 1;
			}
		}

		strcpy(errorResult, "Error performing request");
		curl_easy_cleanup(curl);
		CurlResponseFree(&curlResponse);
		return 0;
	}
}

int Deleter::deleteBatch(char **batch, uint32_t batchCount, char *errorResult, uint32_t *statusCode) {
	LOG(LOG_INFO, "[Delete] Deleting batch of %d objects", batchCount);

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

#define BATCH_SIZE 3

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
			this->fileListStorage->storeDeletedBatch(batch, batchCount);
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
		this->fileListStorage->storeDeletedBatch(batch, batchCount);
		totalDeletedObjects+=batchCount;
	}

	if (totalDeletedObjects>0) {
		LOG(LOG_INFO, "[Delete] Successfully deleted %d objects", totalDeletedObjects);
	} else {
		LOG(LOG_INFO, "[Delete] Nothing to delete", totalDeletedObjects);
	}

	return 1;
}
