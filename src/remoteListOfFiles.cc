#include <iostream>
using namespace std;

#include <libxml/xmlmemory.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <curl/curl.h>
#include <errno.h>
#include <dispatch/dispatch.h>
#include <math.h>

extern "C" {
#include "utils.h"
#include "base64.h"
#include "curlResponse.h"
}

#include "remoteListOfFiles.h"
#include "amazonCredentials.h"
#include "settings.h"

RemoteListOfFiles::RemoteListOfFiles(AmazonCredentials *amazonCredentials) {
	this->amazonCredentials = amazonCredentials;
	this->paths = (char **) malloc(sizeof(char *) * 100);
	this->md5s = (char **) malloc(sizeof(char *) * 100);
	this->mtimes = (uint32_t *) malloc(sizeof(uint32_t) * 100);
	this->count = 0;

	this->allocCount = 100;
}

RemoteListOfFiles::~RemoteListOfFiles() {
	for (int i=0;i<this->count;i++) {
		free(this->paths[i]);
		free(this->md5s[i]);
	}

	free(this->mtimes);
	free(this->paths);
	free(this->md5s);

	this->amazonCredentials = NULL;
}

void RemoteListOfFiles::extractMtimeFromHeaders(char *headers, uint32_t *mtime) {
	*mtime = 0;
	char *str = strstr(headers, "x-amz-meta-mtime: ");
	if (str) { 
		char another[11];
		bzero(another, 11);
		strncpy(another, (str+18), 10);
		*mtime = (uint32_t) atoll(another);
	}
}

char *RemoteListOfFiles::extractMd5FromEtag(char *etag) {
	size_t len = strlen(etag)-1;
	char *md5 = (char *) malloc(len);
	bzero(md5, len);
	strncpy(md5, (etag+1), len-1);
	return md5;
}


void RemoteListOfFiles::add(char *path, char *md5) { 
	this->paths[this->count] = strdup(path);
	this->md5s[this->count] = strdup(md5);

	this->count++;

	if (this->count>=this->allocCount) {
		this->allocCount+=100;
		this->md5s = (char **) realloc(this->md5s, sizeof(char *) * this->allocCount);
		this->paths = (char **) realloc(this->paths, sizeof(char *) * this->allocCount);
		this->mtimes = (uint32_t *) realloc(this->mtimes, sizeof(uint32_t) * this->allocCount);
	}
}

int RemoteListOfFiles::parseListOfFiles(char *body, uint64_t bodySize, uint8_t *isTruncated, char *lastKey, char *errorResult) {
	xmlDocPtr doc;
	xmlNodePtr cur;

	doc = xmlParseMemory(body, (int) bodySize);

	if (doc == NULL ) {
		strcpy(errorResult, "Document not parsed successfully");
		return 0;
	}

	cur = xmlDocGetRootElement(doc);

	if (cur == NULL) {
		strcpy(errorResult, "Empty document");
		xmlFreeDoc(doc);
		return 0;
	}

	if (xmlStrcmp(cur->name, (const xmlChar *) "ListBucketResult")) {
		strcpy(errorResult, "Document of the wrong type, root node != ListBucketResult");
		xmlFreeDoc(doc);
		return 0;
	}

	*isTruncated = 0;

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"Contents"))){
			char *md5=NULL; 

			xmlNodePtr cur2 = cur->xmlChildrenNode;
			while (cur2 != NULL) {
				if ((!xmlStrcmp(cur2->name, (const xmlChar *)"Key"))) {
					xmlChar *key = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
					strcpy(lastKey, (char *)key);
					xmlFree(key);

				} else if ((!xmlStrcmp(cur2->name, (const xmlChar *)"ETag"))) {
					xmlChar *key = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
					md5 = this->extractMd5FromEtag((char *)key);
					xmlFree(key);
				}
				cur2 = cur2->next;
			}

			this->add(lastKey, md5);
			free(md5);

		} else if ((!xmlStrcmp(cur->name, (const xmlChar *)"IsTruncated"))) {
			xmlChar *key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			if (strcmp((char*)key, "true")==0) {
				*isTruncated = 1;
			}
			xmlFree(key);
		}

		cur = cur->next;
	}
	
	xmlFreeDoc(doc);

	return 1;
}

int RemoteListOfFiles::performGetOnBucket(char *marker, int setLocationHeader, char *body, uint64_t *bodySize, uint32_t *statusCode, char *errorResult) {
	char method[4]="GET";

	char *date = getIsoDate(); 

  CURL *curl = curl_easy_init();

	char canonicalizedResource[102400];
	sprintf(canonicalizedResource, "/%s/", amazonCredentials->bucket);
	if (setLocationHeader) {
		strcat(canonicalizedResource, "?location");
	}

	char stringToSign[102400];
	sprintf(stringToSign, "%s\n\n%s\n%s\n%s", method, "", date, canonicalizedResource);

	char *authorization = amazonCredentials->createAuthorizationHeader(stringToSign); 
	if (authorization == NULL) {
		free(date);
	  curl_easy_cleanup(curl);		
		strcpy(errorResult, "Error in auth module.");
		return LIST_FAILED;
	}

  char *url = amazonCredentials->generateUrl((char *) "");
  if (marker && strlen(marker) > 0) {
  	strcat(url, "?marker=");
  	strcat(url, marker);
  } else if (setLocationHeader) {
  	strcat(url, "?location");
  }

  struct CurlResponse curlResponse;
  CurlResponseInit(&curlResponse);

  curl_easy_setopt(curl, CURLOPT_URL, url);
  free(url);

  struct curl_slist *slist = NULL;

	char dateHeader[1024];
	sprintf(dateHeader, "Date: %s", date);
  slist = curl_slist_append(slist, dateHeader);
  free(date);

  char authorizationHeader[1024];
  sprintf(authorizationHeader, "Authorization: %s", authorization);
  slist = curl_slist_append(slist, authorizationHeader);
  free(authorization);

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
  
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT);
	curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, MAXCONNECTS);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, LOW_SPEED_TIME);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, LOW_SPEED_LIMIT);

  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&curlResponse);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlResponseBodyCallback);

	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &CurlResponseHeadersCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, (void *)&curlResponse); 

	CURLcode res = curl_easy_perform(curl);

  curl_slist_free_all(slist);

	*statusCode = 0;

	if (res==CURLE_OK) {
		long httpStatus = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
	  *statusCode = (uint32_t) httpStatus;

		memcpy(body, curlResponse.body, curlResponse.bodySize);
		*bodySize = (uint64_t) curlResponse.bodySize;

	  curl_easy_cleanup(curl);
		CurlResponseFree(&curlResponse);

		return LIST_SUCCESS;

	} else { 
		strcpy(errorResult, "Error performing request");
	  curl_easy_cleanup(curl);
	  CurlResponseFree(&curlResponse);
	  return LIST_FAILED;
	}
}

int RemoteListOfFiles::downloadList() {
	uint32_t statusCode = 0;
	char lastKey[1024*100] = "";
	uint8_t isTruncated=0;

	do {
		char errorResult[1024*100] = "";

		if (strlen(lastKey)>0) {
			printf("[List] Getting 1000 files after %s\n", lastKey);
		} else { 
			printf("[List] Getting first 1000 files\n");
		}

		char *body = (char *) malloc(1024*1024*3); // 3mb should be enough?
		body[0]=0;
		uint64_t bodySize=0;
		int res = this->performGetOnBucket(lastKey, 0, body, &bodySize, &statusCode, errorResult); 
		if (res==LIST_FAILED) {
			printf("[List] FAIL GET: %s\n", errorResult);
			free(body);
			return LIST_FAILED;
		}

		if (statusCode==404) {
			printf("[List] FAIL GET: Bucket doesn't exists\n");
			free(body);
			return LIST_FAILED;
		}

		if (statusCode!=200) {
			printf("[List] FAIL GET: HTTP status code = %d\n", statusCode);
			free(body);
			return LIST_FAILED;
		}

		res = this->parseListOfFiles(
			body,
			bodySize,
			&isTruncated,
			lastKey, 
			errorResult
		);

		free(body);

		if (!res) {
			printf("[List] FAIL Parsing XML: %s\n", errorResult);
			return LIST_FAILED;
		}

	} while (isTruncated);

	return LIST_SUCCESS;
}

int RemoteListOfFiles::performHeadOnFile(char *remotePath, uint32_t *remoteMtime, uint32_t *statusCode, char *errorResult) {
	char method[5]="HEAD";

	char *date = getIsoDate(); 

  CURL *curl = curl_easy_init();

	char *escapedRemotePath=curl_easy_escape(curl, remotePath, 0);
	char canonicalizedResource[102400];
	sprintf(canonicalizedResource, "/%s/%s", amazonCredentials->bucket, escapedRemotePath);

	char stringToSign[102400];
	sprintf(stringToSign, "%s\n\n%s\n%s\n%s", method, "", date, canonicalizedResource);

	char *authorization = amazonCredentials->createAuthorizationHeader(stringToSign);
	if (authorization == NULL) {
		strcpy(errorResult, "Error in auth module");
		free(date);
		free(escapedRemotePath);
	  curl_easy_cleanup(curl);
		return HEAD_FAILED;
	}

  char *url = amazonCredentials->generateUrl(escapedRemotePath); 

  struct CurlResponse curlResponse;
  CurlResponseInit(&curlResponse);

  curl_easy_setopt(curl, CURLOPT_URL, url);
  free(url);

  struct curl_slist *slist = NULL;

	char dateHeader[1024];
	sprintf(dateHeader, "Date: %s", date);
  slist = curl_slist_append(slist, dateHeader);
  free(date);

  char authorizationHeader[1024];
  sprintf(authorizationHeader, "Authorization: %s", authorization);
  slist = curl_slist_append(slist, authorizationHeader);
  free(authorization);

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
  
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT);
	curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, MAXCONNECTS);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, LOW_SPEED_TIME);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, LOW_SPEED_LIMIT);

  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&curlResponse);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlResponseBodyCallback);

	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &CurlResponseHeadersCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, (void *)&curlResponse); 

	CURLcode res = curl_easy_perform(curl);

  curl_slist_free_all(slist);

	*statusCode = 0;

	if (res==CURLE_OK) {
		long httpStatus = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
	  *statusCode = (uint32_t) httpStatus;

		this->extractMtimeFromHeaders(curlResponse.headers, remoteMtime);

	  curl_easy_cleanup(curl);
		CurlResponseFree(&curlResponse);

		return HEAD_SUCCESS;

	} else { 
		strcpy(errorResult, "Error performing request");

	  curl_easy_cleanup(curl);
	  CurlResponseFree(&curlResponse);

	  return HEAD_FAILED;
	}
}


int RemoteListOfFiles::resolveMtimes() {
	uint32_t i;
	for (i=0;i<this->count;i++) {
		(this->mtimes)[i]=0;
	}
	
	dispatch_queue_t globalQueue = dispatch_get_global_queue(0, 0);
	dispatch_group_t group = dispatch_group_create();
	
	dispatch_semaphore_t threadsCount = dispatch_semaphore_create(50);
	
	__block int failed = 0;

	for (i=0;i<this->count; i++) {
		dispatch_semaphore_wait(threadsCount, DISPATCH_TIME_FOREVER);
		
		dispatch_group_async(group, globalQueue, ^{
			if (!failed) {
				char errorResult[1024*100] = "";
				uint32_t mtime=0, statusCode=0;

				int res = this->performHeadOnFile(this->paths[i], &mtime, &statusCode, errorResult);
				if (res==HEAD_FAILED) {
					printf("\n[MetaUpdate] FAIL %s: %s\n", this->paths[i], errorResult);
					failed=1;
					dispatch_semaphore_signal(threadsCount);							
					return;
				}

				if (statusCode!=200) {
					printf("\n[MetaUpdate] FAIL %s: HTTP status=%d\n", this->paths[i], statusCode);
					failed=1;
					dispatch_semaphore_signal(threadsCount);							
					return;
				}

				this->mtimes[i] = mtime;
				double percent = (double) i / (double) this->count;
				printf("\r[MetaUpdate] Updated %.1f%% (%u files out of %u)     \r", percent*100, (uint32_t) i, this->count);
			}

			dispatch_semaphore_signal(threadsCount);							
		});

		if (failed) {
			break;
		}
	}

	dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

	if (failed) {
		printf("\n[MetaUpdate] Some failed\n");
	} else { 
		printf("\n[MetaUpdate] All %d files updated\n", i);
	}
	
	fflush(stdout);

	dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

	return failed ? LIST_FAILED : LIST_SUCCESS;
}

int RemoteListOfFiles::checkAuth() {
	uint32_t statusCode;
	char errorResult[1024*100] = "";
	char body[1024*1024*3] = ""; // 3mb should be enough?
	uint64_t bodySize=0;
	int res = this->performGetOnBucket(NULL, 1, body, &bodySize, &statusCode, errorResult); 

	if (res == LIST_FAILED) {
		printf("[Auth] GET Failed: %s\n", errorResult);
		return AUTH_FAILED;
	}

	if (statusCode==200) {
		return AUTH_SUCCESS;
	} else if (statusCode==404) {
		return AUTH_FAILED_BUCKET_DOESNT_EXISTS;
	} else {
		return AUTH_FAILED;
	}
}
