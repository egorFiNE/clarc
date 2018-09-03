#include <iostream>
using namespace std;

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <errno.h>
#include <sqlite3.h>
#include <time.h>

extern "C" {
#include "utils.h"
#include "base64.h"
#include "logger.h"
}

#include "microCurl.h"

	struct ReadFunctionData {
		char *userData;
		char *currentPtr;
	};

MicroCurl::MicroCurl(AmazonCredentials *amazonCredentials) {
	this->amazonCredentials = amazonCredentials;

	this->method=0;
	this->url = NULL;

	this->connectTimeout=-1;
	this->networkTimeout=-1;
	this->maxConnects=-1;
	this->lowSpeedLimit=-1;
	this->insecureSsl=0;

	this->canonicalizedResource = NULL;
	this->postData = NULL;
	this->postSize = 0;
	this->fileIn = NULL;

	this->bodySize=0;
	this->httpStatusCode=0;
	this->body=NULL;
	this->curlErrors = NULL;
	this->headerContentType = NULL;
	this->headerContentMd5 = NULL;
	this->headerDate	 = NULL;
}

MicroCurl::~MicroCurl() {
	free(this->url);

	if (this->headerContentType!=NULL) {
		free(this->headerContentType);
	}

	if (this->headerContentMd5!=NULL) {
		free(this->headerContentMd5);
	}

	if (this->headerDate!=NULL) {
		free(this->headerDate);
	}

	if (this->canonicalizedResource!=NULL) {
		free(this->canonicalizedResource);
	}

	if (this->body!=NULL) {
		free(this->body);
	}

	if (this->postData!=NULL) {
		free(this->postData);
	}

	if (this->curlErrors!=NULL) {
		free(this->curlErrors);
	}
}

void MicroCurl::reset() {
	if (this->body!=NULL) {
		free(this->body);
	}

	if (this->curlErrors!=NULL) {
		free(this->curlErrors);
	}
	this->bodySize=0;
	this->httpStatusCode=0;

	resultingHeaderNamesList.clear();
	resultingHeaderValuesList.clear();
}

void MicroCurl::addHeader(char *name, char *value) {
	this->headerNamesList.push_back(name);
	this->headerValuesList.push_back(value);

	if (strcasecmp(name, "content-type")==0) {
		this->headerContentType = strdup(value);
	} else if (strcasecmp(name, "content-md5")==0) {
		this->headerContentMd5 = strdup(value);
	} else if (strcasecmp(name, "date")==0) {
		this->headerDate = strdup(value);
	}
}

void MicroCurl::addHeaderFormat(char *name, char *format, ...) {
	char *result;
	va_list args;
	va_start(args, format);
	vasprintf(&result, format, args);
	va_end(args);

	this->addHeader(name, result);

	free(result);
}

void MicroCurl::parseHeaders(char *headers, uint32_t headersSize) {
	char *token;

	while ((token = strsep(&headers, "\n")) != NULL) {
		if (strlen(token)<2) {
			continue;
		}

		if (strcmp(token, "\n")==0) {
			continue;
		}

		if (strncmp(token, "HTTP/1", 6)==0) {
			continue;
		}

		token[strlen(token)-1]=0;

		char *header = strsep(&token, ":");
		if (header) {
			token++;

			for (int i = 0; header[i]; i++) header[i] = tolower(header[i]);

			resultingHeaderNamesList.push_back(header);
			resultingHeaderValuesList.push_back(token);
		}
	}
}

char *MicroCurl::escapePath(char *path) {
	CURL *s = curl_easy_init();
	char *escapedPath=curl_easy_escape(s, path, 0);
	curl_easy_cleanup(s);
	return escapedPath;
}

char *MicroCurl::getHeader(char *name) {
	for(std::vector<int>::size_type i = 0; i != this->resultingHeaderNamesList.size(); i++) {
		if (strcmp(name, this->resultingHeaderNamesList[i].c_str())==0) {
			const char *candidate = this->resultingHeaderValuesList[i].c_str();
			if (candidate != NULL) {
				return strdup(candidate);
			}
		}
	}
	return NULL;
}

size_t readFunctionForMicroCurl(void *ptr, size_t size, size_t nmemb, void *userdata)  {
	size_t totalBytes = size*nmemb;
	struct ReadFunctionData *readFunctionData = (struct ReadFunctionData *)userdata;
	strncpy((char *)ptr, (char*) readFunctionData->currentPtr, totalBytes);
	size_t l = strlen((char*)ptr);
	readFunctionData->currentPtr+=l;
	return l;
}

struct CurlResponse {
	char *body;
	uint64_t bodySize;
	char *headers;
	uint64_t headersSize;
};

size_t curlResponseHeadersCallback(void *contents, size_t size, size_t nmemb, struct CurlResponse *curlResponse) {
	size_t realsize = size * nmemb;
	curlResponse->headers = (char *) realloc(curlResponse->headers, curlResponse->headersSize + realsize + 1);
	if (curlResponse->headers == NULL) {
		LOG(LOG_FATAL, "[curl] Out of memory");
		exit(1);
	}

	memcpy(&(curlResponse->headers[curlResponse->headersSize]), contents, realsize);
	curlResponse->headersSize += realsize;
	curlResponse->headers[curlResponse->headersSize] = 0;

	return realsize;
}

size_t curlResponseBodyCallback(void *contents, size_t size, size_t nmemb, struct CurlResponse *curlResponse) {
	size_t realsize = size * nmemb;

	curlResponse->body = (char *) realloc(curlResponse->body, curlResponse->bodySize + realsize + 1);
	if (curlResponse->body == NULL) {
		LOG(LOG_FATAL, "[curl] Out of memory");
		exit(1);
	}

	memcpy(&(curlResponse->body[curlResponse->bodySize]), contents, realsize);
	curlResponse->bodySize += realsize;
	curlResponse->body[curlResponse->bodySize] = 0;

	return realsize;
}

CURL *MicroCurl::prepare() {
	this->reset();
	this->curl = curl_easy_init();

	char *date = getIsoDate();
	this->addHeader("Date", date);
	free(date);

	char *stringToSign = this->getStringToSign();

	char *authorization = this->amazonCredentials->sign(stringToSign);
	free(stringToSign);
	if (authorization==NULL) {
		return NULL;
	}
	this->addHeader("Authorization", authorization);

	free(authorization);

	return this->curl;
}

CURLcode MicroCurl::go() {
	struct CurlResponse curlResponse;

	curlResponse.body = (char *) malloc(1);
	curlResponse.body[0]=0;
	curlResponse.bodySize = 0;

	curlResponse.headers = (char *) malloc(1);
	curlResponse.headers[0]=0;
	curlResponse.headersSize = 0;

	if (this->debug) {
		curl_easy_setopt(this->curl, CURLOPT_VERBOSE, 1);
	}

	curl_easy_setopt(this->curl, CURLOPT_URL, this->url);

	if (this->insecureSsl) {
		curl_easy_setopt(this->curl, CURLOPT_SSL_VERIFYHOST, 0);
	}

	curl_easy_setopt(this->curl, CURLOPT_NOSIGNAL, 1);

	if (this->connectTimeout>=0) {
		curl_easy_setopt(this->curl, CURLOPT_CONNECTTIMEOUT, this->connectTimeout);
	}
	if (this->maxConnects>=0) {
		curl_easy_setopt(this->curl, CURLOPT_MAXCONNECTS, this->maxConnects);
	}
	if (this->networkTimeout>=0) {
		curl_easy_setopt(this->curl, CURLOPT_LOW_SPEED_TIME, this->networkTimeout);
	}
	if (this->lowSpeedLimit>=0) {
		curl_easy_setopt(this->curl, CURLOPT_LOW_SPEED_LIMIT, this->lowSpeedLimit);
	}

	curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, (void *)&curlResponse);
	curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, curlResponseBodyCallback);

	curl_easy_setopt(this->curl, CURLOPT_WRITEHEADER, (void *)&curlResponse);
	curl_easy_setopt(this->curl, CURLOPT_HEADERFUNCTION, &curlResponseHeadersCallback);

	struct ReadFunctionData readFunctionData;
	readFunctionData.userData = NULL;

	if (this->method==METHOD_HEAD) {
		curl_easy_setopt(this->curl, CURLOPT_NOBODY, 1);

	} else if (this->method==METHOD_PUT) {
		curl_easy_setopt(this->curl, CURLOPT_UPLOAD, 1L);
		if (this->fileIn!=NULL) {
			curl_easy_setopt(this->curl, CURLOPT_READDATA, this->fileIn);
			curl_easy_setopt(this->curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t) this->fileSize);
		} else if (this->postSize==0) {
			curl_easy_setopt(this->curl, CURLOPT_INFILESIZE, 0);
		} else {
			readFunctionData.userData  = this->postData;
			readFunctionData.currentPtr = this->postData;

			curl_easy_setopt(this->curl, CURLOPT_READDATA, &readFunctionData);
			curl_easy_setopt(this->curl, CURLOPT_READFUNCTION, &readFunctionForMicroCurl);
			curl_easy_setopt(this->curl, CURLOPT_INFILESIZE, this->postSize);
		}

	} else if (this->method==METHOD_POST) {

		curl_easy_setopt(this->curl, CURLOPT_POST, 1L);
		curl_easy_setopt(this->curl, CURLOPT_POSTFIELDS, NULL);

		readFunctionData.userData  = this->postData;
		readFunctionData.currentPtr = this->postData;

		curl_easy_setopt(this->curl, CURLOPT_READDATA, &readFunctionData);
		curl_easy_setopt(this->curl, CURLOPT_READFUNCTION, &readFunctionForMicroCurl);
		curl_easy_setopt(this->curl, CURLOPT_POSTFIELDSIZE, this->postSize);
	}

	struct curl_slist *slist = NULL;

	for(std::vector<int>::size_type i = 0; i != this->headerNamesList.size(); i++) {
		char *header;
		asprintf(&header, "%s: %s", this->headerNamesList[i].c_str(), this->headerValuesList[i].c_str());
		slist = curl_slist_append(slist, header);
		free(header);
	}

	curl_easy_setopt(this->curl, CURLOPT_HTTPHEADER, slist);

	this->curlErrors = (char *) malloc(CURL_ERROR_SIZE);
	this->curlErrors[0]=0;
	curl_easy_setopt(this->curl, CURLOPT_ERRORBUFFER, this->curlErrors);

	CURLcode res = curl_easy_perform(this->curl);

	curl_slist_free_all(slist);

	curl_easy_getinfo(this->curl, CURLINFO_RESPONSE_CODE, &this->httpStatusCode);

	this->body = curlResponse.body;
	this->bodySize = (uint32_t) curlResponse.bodySize;

	this->parseHeaders(curlResponse.headers, (uint32_t) curlResponse.headersSize);

	// now we are dealing with a weird curl or amazon bug. Sometimes we get CURLE_SEND_ERROR,
	// but the request has been performed.
	if (strlen(curlResponse.headers)>1) {
		if (strstr(curlResponse.headers, "HTTP/1.1 200 OK")==curlResponse.headers) {
			this->httpStatusCode=200;
		}
	}

	curl_easy_cleanup(this->curl);
	free(curlResponse.headers);

	return res;
}

char *MicroCurl::serializeAmzHeadersIntoStringToSign() {
	int c=0;

	std::vector<std::string> headersList;
	for(std::vector<int>::size_type i = 0; i < this->headerNamesList.size(); i++) {
		if (strncmp(this->headerNamesList[i].c_str(), "x-amz", 5)==0) {
			c++;
			char *header;
			asprintf(&header, "%s:%s\n", this->headerNamesList[i].c_str(), this->headerValuesList[i].c_str());
			headersList.push_back(header);
			free(header);
		}
	}

	if (c==0) {
		return strdup("");
	}

	std::sort(headersList.begin(), headersList.end());

	std::string result;

	for(std::vector<int>::size_type i = 0; i < headersList.size(); i++) {
		result.append(headersList[i]);
	}

	return strdup((char *) result.c_str());
}

const char *MicroCurl::getMethodName() {
	switch(this->method) {
		case METHOD_POST:
			return "POST";
		case METHOD_HEAD:
			return "HEAD";
		case METHOD_PUT:
			return "PUT";
		default:
			return "GET";
	}
}

char *MicroCurl::getStringToSign() {
	char *amzHeaders = this->serializeAmzHeadersIntoStringToSign();

	char *stringToSign;
	asprintf(&stringToSign,
		"%s\n"  // HTTP-Verb + "\n" +
		"%s\n"  // Content-MD5 + "\n" +
		"%s\n"	// Content-Type + "\n" +
		"%s\n"  // Date + "\n" +
		"%s"    // CanonicalizedAmzHeaders +
		"%s",   // CanonicalizedResource


		this->getMethodName(), // HTTP-Verb + "\n" +
		this->headerContentMd5 == NULL ? "" : this->headerContentMd5, // Content-MD5 + "\n" +
		this->headerContentType == NULL ? "" : this->headerContentType, // Content-Type + "\n" +
		this->headerDate, // Date + "\n" +
		amzHeaders==NULL ? "" : amzHeaders, // CanonicalizedAmzHeaders +
		this->canonicalizedResource
	);

	if (amzHeaders!=NULL) {
		free(amzHeaders);
	}

	return stringToSign;
}
