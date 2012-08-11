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

#include "remoteListOfFiles.h"
#include "amazonCredentials.h"
#include "amzHeaders.h"
#include "threads.h"
#include "settings.h"

#include "microCurl.h"

RemoteListOfFiles::RemoteListOfFiles(AmazonCredentials *amazonCredentials) {
	this->amazonCredentials = amazonCredentials;
	this->paths = (char **) malloc(sizeof(char *) * 100);
	this->md5s = (char **) malloc(sizeof(char *) * 100);
	this->mtimes = (uint32_t *) malloc(sizeof(uint32_t) * 100);
	this->count = 0;
	this->allocCount = 100;
	this->failed=0;

	this->showProgress=0;
	this->useSsl=1;
	this->connectTimeout = CONNECT_TIMEOUT;
	this->networkTimeout = LOW_SPEED_TIME;
}

RemoteListOfFiles::~RemoteListOfFiles() {
	for (uint32_t i=0;i<this->count;i++) {
		free(this->paths[i]);
		free(this->md5s[i]);
	}

	free(this->mtimes);
	free(this->paths);
	free(this->md5s);

	this->amazonCredentials = NULL;
}

uint32_t RemoteListOfFiles::extractMtimeFromMicroCurl(MicroCurl *microCurl) {
	char *header = microCurl->getHeader("x-amz-meta-mtime");
	if (header==NULL) {
		return 0;
	}

	return (uint32_t) atoll(header);
}

char *RemoteListOfFiles::extractMd5FromEtag(char *etag) {
	if (!etag) {
		return NULL;
	}

	if (strlen(etag)<3) {
		// can etag be one char?... 
		return NULL;
	}

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
	*errorResult=0;
	*lastKey=0;
	*isTruncated=0;

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
			char *md5=NULL; int keyFound=0;

			xmlNodePtr cur2 = cur->xmlChildrenNode;
			while (cur2 != NULL) {
				if ((!xmlStrcmp(cur2->name, (const xmlChar *)"Key"))) {
					xmlChar *key = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
					strcpy(lastKey, (char *)key);
					keyFound=1;
					xmlFree(key);

				} else if ((!xmlStrcmp(cur2->name, (const xmlChar *)"ETag"))) {
					xmlChar *key = xmlNodeListGetString(doc, cur2->xmlChildrenNode, 1);
					md5 = RemoteListOfFiles::extractMd5FromEtag((char *)key);
					xmlFree(key);
				}

				cur2 = cur2->next;
			}

			if (md5!=NULL) {
				if (keyFound) {
					this->add(lastKey, md5);
				}
				free(md5);
			}

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

int RemoteListOfFiles::performGetOnBucket(char *url, char *marker, int setLocationHeader, char *body, uint64_t *bodySize, uint32_t *statusCode, char *errorResult) {
	MicroCurl *microCurl = new MicroCurl();
	microCurl->method = METHOD_GET;

	char *date = getIsoDate();
	microCurl->addHeader("Date", date);

	char *canonicalizedResource;
	asprintf(&canonicalizedResource, "/%s/%s", amazonCredentials->bucket, setLocationHeader ? "?location" : "");

	char *stringToSign;
	asprintf(&stringToSign, "GET\n\n%s\n%s\n%s", "", date, canonicalizedResource);
	free(canonicalizedResource);

	char *authorization = amazonCredentials->createAuthorizationHeader(stringToSign); 
	free(stringToSign);

	if (authorization == NULL) {
		free(date);
		delete microCurl;
		strcpy(errorResult, "Error in auth module");
		return LIST_FAILED;
	}

	char *postUrl;
	if (url) {
		postUrl = strdup(url);
	} else {
		postUrl = amazonCredentials->generateUrl((char *) "", this->useSsl);
	}

	if (marker && strlen(marker) > 0) {
		if (strstr(postUrl, marker)==NULL) {
			postUrl = (char*) realloc(postUrl, strlen(postUrl) + strlen(marker) + 16);
			strcat(postUrl, "?marker=");
			strcat(postUrl, marker);
		}

	} else if (setLocationHeader) {
		if (strstr(postUrl, "?location")==NULL) {
			postUrl = (char*) realloc(postUrl, strlen(postUrl) + 16);
			strcat(postUrl, "?location");
		}
	}

	microCurl->url = strdup(postUrl);

	LOG(LOG_DBG, "[File list] GET %s", postUrl);
	free(postUrl);

	microCurl->addHeader("Authorization", authorization);
	free(authorization);

	microCurl->networkTimeout = this->networkTimeout;
	microCurl->connectTimeout = this->connectTimeout;
	microCurl->maxConnects = MAXCONNECTS;
	microCurl->lowSpeedLimit = LOW_SPEED_LIMIT;

	CURLcode res = microCurl->go();

	*statusCode = microCurl->httpStatusCode;

	if (res==CURLE_OK) {
		if (microCurl->httpStatusCode==307) {
			char *location = microCurl->getHeader("location");
			strcpy(errorResult, location);
			free(location);
			*bodySize=0;
		} else { 
			memcpy(body, microCurl->body, microCurl->bodySize);
			*bodySize = microCurl->bodySize;
		}

		delete microCurl;

		return LIST_SUCCESS;

	} else { 
		strcpy(errorResult, "Error performing request");

		delete microCurl;
		return LIST_FAILED;
	}
}

/*
int RemoteListOfFiles::performGetOnBucket(char *url, char *marker, int setLocationHeader, char *body, uint64_t *bodySize, uint32_t *statusCode, char *errorResult) {
	char method[4]="GET";

	char *date = getIsoDate(); 

	CURL *curl = curl_easy_init();

	char *canonicalizedResource;
	asprintf(&canonicalizedResource, "/%s/%s", amazonCredentials->bucket, setLocationHeader ? "?location" : "");

	char *stringToSign;
	asprintf(&stringToSign, "%s\n\n%s\n%s\n%s", method, "", date, canonicalizedResource);
	free(canonicalizedResource);

	char *authorization = amazonCredentials->createAuthorizationHeader(stringToSign); 
	free(stringToSign);

	if (authorization == NULL) {
		free(date);
		curl_easy_cleanup(curl);		
		strcpy(errorResult, "Error in auth module");
		return LIST_FAILED;
	}

	char *postUrl;
	if (url) {
		postUrl = strdup(url);
	} else {
		postUrl = amazonCredentials->generateUrl((char *) "", this->useSsl);
	}

	if (marker && strlen(marker) > 0) {
		if (strstr(postUrl, marker)==NULL) {
			postUrl = (char*) realloc(postUrl, strlen(postUrl) + strlen(marker) + 16);
			strcat(postUrl, "?marker=");
			strcat(postUrl, marker);
		}

	} else if (setLocationHeader) {
		if (strstr(postUrl, "?location")==NULL) {
			postUrl = (char*) realloc(postUrl, strlen(postUrl) + 16);
			strcat(postUrl, "?location");
		}
	}

	struct CurlResponse curlResponse;
	CurlResponseInit(&curlResponse);

	curl_easy_setopt(curl, CURLOPT_URL, postUrl);
	LOG(LOG_DBG, "[File list] GET %s", postUrl);
	free(postUrl);

	struct curl_slist *slist = NULL;

	slist = AmzHeaders::addHeader(slist, "Date", date);
	free(date);

	slist = AmzHeaders::addHeader(slist, "Authorization", authorization);
	free(authorization);

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, this->connectTimeout);
	curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, MAXCONNECTS);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, this->networkTimeout);
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

		if (httpStatus==307) {
			extractLocationFromHeaders(curlResponse.headers, errorResult);
			*bodySize=0;
		} else { 
			memcpy(body, curlResponse.body, curlResponse.bodySize);
			*bodySize = (uint64_t) curlResponse.bodySize;
		}

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
*/

size_t readFunctionForRegionCreate(void *ptr, size_t size, size_t nmemb, void *userdata)  {
	strcpy((char *)ptr, (char*) userdata);
	return strlen((char*)userdata);
}

int RemoteListOfFiles::performPutOnBucket(char *url, char *region, uint32_t *statusCode, char *errorResult) {
	char method[4]="PUT";

	char *date = getIsoDate(); 

	CURL *curl = curl_easy_init();

	char *canonicalizedResource;
	asprintf(&canonicalizedResource, "/%s/", amazonCredentials->bucket);

	AmzHeaders *amzHeaders = new AmzHeaders();
	amzHeaders->add((char *) "x-amz-acl", (char *) "private");
	char *amzHeadersToSign = amzHeaders->serializeIntoStringToSign();

	char *stringToSign;
	asprintf(&stringToSign, "%s\n\n%s\n%s\n%s%s", method, "", date, amzHeadersToSign, canonicalizedResource); 
	free(canonicalizedResource);
	free(amzHeadersToSign);

	char *authorization = amazonCredentials->createAuthorizationHeader(stringToSign); 
	free(stringToSign);

	if (authorization == NULL) {
		free(date);
		curl_easy_cleanup(curl);		
		strcpy(errorResult, "Error in auth module");
		return LIST_FAILED;
	}

	char *postUrl;
	if (url) {
		postUrl = strdup(url);
	} else {
		postUrl = amazonCredentials->generateUrlForBucketCreate(this->useSsl);
	}

	struct CurlResponse curlResponse;
	CurlResponseInit(&curlResponse);

	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

	char *createBucketData = NULL;
	if (strcmp(region, "")==0 || strcmp(region, "US")==0) {
		curl_easy_setopt(curl, CURLOPT_INFILESIZE, 0);
	} else { 
		asprintf(&createBucketData, "<CreateBucketConfiguration xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">" \
			"<LocationConstraint>%s</LocationConstraint></CreateBucketConfiguration>\n", region);

		curl_easy_setopt(curl, CURLOPT_READDATA, createBucketData);
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, &readFunctionForRegionCreate);
		curl_easy_setopt(curl, CURLOPT_INFILESIZE, strlen(createBucketData));
	}

	curl_easy_setopt(curl, CURLOPT_URL, postUrl);
	LOG(LOG_DBG, "[Create Bucket] PUT %s", postUrl);
	free(postUrl);

	struct curl_slist *slist = NULL;

	slist = AmzHeaders::addHeader(slist, "Date", date);
	free(date);

	slist = AmzHeaders::addHeader(slist, "Authorization", authorization);
	free(authorization);

	slist = curl_slist_append(slist, "Expect:");

	slist = amzHeaders->serializeIntoCurl(slist);
	delete amzHeaders;

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

	if (createBucketData!=NULL) {
		free(createBucketData);
	}

	curl_slist_free_all(slist);

	*statusCode = 0;

	if (res==CURLE_OK) {
		long httpStatus = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
		*statusCode = (uint32_t) httpStatus;

		if (httpStatus==307) {
			extractLocationFromHeaders(curlResponse.headers, errorResult);
		}

		curl_easy_cleanup(curl);
		CurlResponseFree(&curlResponse);

		return CREATE_SUCCESS;

	} else { 
		strcpy(errorResult, "Error performing request");
		curl_easy_cleanup(curl);
		CurlResponseFree(&curlResponse);
		return CREATE_FAILED;
	}
}

int RemoteListOfFiles::createBucket(char *region) {
	char *url = NULL;
	do {
		char errorResult[1024*100];
		errorResult[0]=0;
		uint32_t statusCode=0;

		int res = this->performPutOnBucket(url, region, &statusCode, errorResult);

		if (url!=NULL) {
			free(url);
			url=NULL;
		}

		if (res==CREATE_FAILED) {
			LOG(LOG_FATAL, "[BucketCreate] FAIL: %s", errorResult);
			return CREATE_FAILED;
		}

		if (statusCode==307) {
			url = strdup(errorResult); 
			LOG(LOG_INFO, "[BucketCreate] Retrying: Amazon asked to repeat to: %s", url);
			continue;

		} else if (statusCode!=200) {
			LOG(LOG_FATAL, "[BucketCreate] FAIL: HTTP status=%d", statusCode);
			return CREATE_FAILED;

		} else if (statusCode==200) {
			return CREATE_SUCCESS;
		}
	} while (true);
}

int RemoteListOfFiles::downloadList() {
	uint32_t statusCode = 0;
	char lastKey[1024*100] = "";
	uint8_t isTruncated=0;

	char *url = NULL;

	do {
		char errorResult[1024*100] = "";

		if (strlen(lastKey)>0) {
			LOG(LOG_INFO, "[List] Getting 1000 files after %s", lastKey);
		} else { 
			LOG(LOG_INFO, "[List] Getting first 1000 files");
		}

		char *body = (char *) malloc(1024*1024*3); // 3mb should be enough?
		body[0]=0;
		uint64_t bodySize=0;

		int res = this->performGetOnBucket(url, lastKey, 0, body, &bodySize, &statusCode, errorResult); 
		if (url!=NULL) {
			free(url);
			url=NULL;
		}

		if (res==LIST_FAILED) {
			LOG(LOG_FATAL, "[List] FAIL GET: %s", errorResult);
			free(body);
			return LIST_FAILED;
		}

		if (statusCode==404) {
			LOG(LOG_FATAL, "[List] FAIL GET: Bucket doesn't exists");
			free(body);
			return LIST_FAILED;

		} else if (statusCode==307) {
			LOG(LOG_INFO, "[List] Retrying: Amazon asked to repeat to: %s", errorResult);
			url = strdup(errorResult); 
			free(body);
			continue;
		}

		if (statusCode!=200) {
			LOG(LOG_FATAL, "[List] FAIL GET: HTTP status code = %d", statusCode);
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
			LOG(LOG_FATAL, "[List] FAIL Parsing XML: %s", errorResult);
			return LIST_FAILED;
		}

	} while (isTruncated);

	if (url!=NULL) {  // satisfy clang --analyze
		free(url);
	}

	return LIST_SUCCESS;
}

int RemoteListOfFiles::performHeadOnFile(char *url, char *remotePath, uint32_t *remoteMtime, uint32_t *statusCode, char *errorResult) {
	MicroCurl *microCurl = new MicroCurl();
	microCurl->method=METHOD_HEAD;

	char *escapedRemotePath=microCurl->escapePath(remotePath);
	
	char *canonicalizedResource;
	asprintf(&canonicalizedResource, "/%s/%s", amazonCredentials->bucket, escapedRemotePath);

	char *date = getIsoDate(); 

	char *stringToSign;
	asprintf(&stringToSign, "HEAD\n\n%s\n%s\n%s", "", date, canonicalizedResource);
	free(canonicalizedResource);

	char *authorization = amazonCredentials->createAuthorizationHeader(stringToSign);
	free(stringToSign);

	if (authorization == NULL) {
		strcpy(errorResult, "Error in auth module");
		free(date);
		free(escapedRemotePath);
		delete microCurl;
		return HEAD_FAILED;
	}

	char *headUrl;
	if (url) {
		headUrl = strdup(url);
	} else {
		headUrl = amazonCredentials->generateUrl(escapedRemotePath, this->useSsl); 
	}

	free(escapedRemotePath);

	microCurl->url = strdup(headUrl);
	free(headUrl);

	microCurl->addHeader("Date", date);
	free(date);

	microCurl->addHeader("Authorization", authorization);
	free(authorization);

	microCurl->connectTimeout = this->connectTimeout;
	microCurl->maxConnects = MAXCONNECTS;
	microCurl->networkTimeout = this->networkTimeout;
	microCurl->lowSpeedLimit = LOW_SPEED_LIMIT;

	CURLcode res = microCurl->go();

	*statusCode = microCurl->httpStatusCode;

	if (res==CURLE_OK) {
		if (microCurl->httpStatusCode==307) {
			char *location = microCurl->getHeader("location");
			strcpy(errorResult, location);
			free(location);
		}

		*remoteMtime = RemoteListOfFiles::extractMtimeFromMicroCurl(microCurl);

		delete microCurl;

		return HEAD_SUCCESS;

	} else { 
		strcpy(errorResult, "Error performing request");

		delete microCurl;

		return HEAD_FAILED;
	}
}

struct ThreadCommand {
	RemoteListOfFiles *self;
	int threadNumber;
	int pos;
};

void *remoteListOfFiles_runOverThreadFunc(void *arg) {
	struct ThreadCommand *threadCommand = (struct ThreadCommand *)arg;
	threadCommand->self->runOverThread(threadCommand->threadNumber, threadCommand->pos);
	free(arg); 
	pthread_exit(NULL);
}

void RemoteListOfFiles::runOverThread(int threadNumber, int pos) {
	if (this->failed) {
		this->threads->markFree(threadNumber);
		return;
	}

	char *url = NULL;
	do {
		char errorResult[1024*100];
		errorResult[0]=0;
		uint32_t mtime=0, statusCode=0;

		int res = this->performHeadOnFile(url, this->paths[pos], &mtime, &statusCode, errorResult);

		if (url!=NULL) {
			free(url);
			url=NULL;
		}

		if (res==HEAD_FAILED) {
			LOG(LOG_FATAL, "[MetaUpdate] FAIL %s: %s", this->paths[pos], errorResult);
			this->failed=1;
			this->threads->markFree(threadNumber);
			return;
		}

		if (statusCode==307) {
			url = strdup(errorResult); 
			LOG(LOG_INFO, "[MetaUpdate] Retrying: Amazon asked to repeat to: %s", url);
			continue;

		} else if (statusCode!=200) {
			LOG(LOG_FATAL, "[MetaUpdate] FAIL %s: HTTP status=%d", this->paths[pos], statusCode);
			this->failed=1;
			this->threads->markFree(threadNumber);
			return;

		} else if (statusCode==200) {
			this->mtimes[pos] = mtime;
			double percent = (double) pos / (double) this->count;

			if (this->showProgress) {
				printf("\r[MetaUpdate] Updated %.1f%% (%u files out of %u)     \r", percent*100, (uint32_t) pos, this->count);
			}

			LOG(LOG_INFO, "[MetaUpdate] updated %s (%d)                                             ", 
				this->paths[pos], this->mtimes[pos]);
			this->threads->markFree(threadNumber);
			return;
		}
	} while (true);
}

int RemoteListOfFiles::resolveMtimes() {
	uint32_t i;
	for (i=0;i<this->count;i++) {
		(this->mtimes)[i]=0;
	}
	
	this->threads = new Threads(50);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	for (i=0;i<this->count; i++) {
		int threadNumber = threads->sleepTillThreadFree();
		threads->markBusy(threadNumber);
		
		struct ThreadCommand *threadCommand = (struct ThreadCommand *) malloc(sizeof(struct ThreadCommand));
		threadCommand->threadNumber = threadNumber;
		threadCommand->pos = i;
		threadCommand->self = this;

		pthread_t threadId;
		int rc = pthread_create(&threadId, &attr, remoteListOfFiles_runOverThreadFunc, (void *)threadCommand);
		threads->setThreadId(threadNumber, threadId);
		if (rc) {
			LOG(LOG_FATAL, "Return code from pthread_create() is %d, exit", rc);
			exit(-1);
		}

		if (this->failed) {
			break;
		}
	}

	threads->sleepTillAllThreadsFree();
	delete this->threads;

	pthread_attr_destroy(&attr);

	if (!this->failed) {
		LOG(LOG_INFO, "[MetaUpdate] All %d files updated", i);
	}
	
	return this->failed ? LIST_FAILED : LIST_SUCCESS;
}

int RemoteListOfFiles::checkAuth() {
	uint32_t statusCode;
	char *errorResult = (char *) malloc(1024*100);
	errorResult[0]=0;
	char *body = (char*)malloc(1024*1024*3); // 3mb should be enough?
	body[0]=0;

	uint64_t bodySize=0;
	int res = this->performGetOnBucket(NULL, NULL, 1, body, &bodySize, &statusCode, errorResult); 
	free(body);

	if (res == LIST_FAILED) {
		LOG(LOG_DBG, "[Auth] GET Failed: %s", errorResult);
		free(errorResult);
		return AUTH_FAILED;
	}

	free(errorResult);

	if (statusCode==200) {
		return AUTH_SUCCESS;
	} else if (statusCode==404) {
		return AUTH_FAILED_BUCKET_DOESNT_EXISTS;
	} else {
		return AUTH_FAILED;
	}
}
