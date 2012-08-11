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
#include "curlResponse.h"
#include "logger.h"
}

#include "microCurl.h"

MicroCurl::MicroCurl() {
	this->debug=0;
	this->method=0;
	this->connectTimeout=-1;
	this->networkTimeout=-1;
	this->maxConnects=-1;
	this->lowSpeedLimit=-1;
	this->url = NULL;

	this->body=NULL;
	this->reset();
}

MicroCurl::~MicroCurl() {
	free(this->url);
	if (this->body!=NULL) {
		free(this->body);
	}
}


void MicroCurl::addHeader(char *name, char *format, ...) {
	char *result;
	va_list args;
	va_start(args, format);
	vasprintf(&result, format, args);
	va_end(args);

	this->headerNamesList.push_back(name);
	this->headerValuesList.push_back(result);
	free(result);
}

void MicroCurl::reset() {
	if (this->body!=NULL) {
		free(this->body);
	}
	this->bodySize=0;
	this->httpStatusCode=0;

	resultingHeaderNamesList.clear();
	resultingHeaderValuesList.clear();
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

CURLcode MicroCurl::go() {
	this->reset();

	struct CurlResponse curlResponse;
	CurlResponseInit(&curlResponse);

	this->curl = curl_easy_init();

	if (this->debug) {
		curl_easy_setopt(this->curl, CURLOPT_VERBOSE, 1);
	}
	
	curl_easy_setopt(this->curl, CURLOPT_URL, this->url);

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
	curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, CurlResponseBodyCallback);

	curl_easy_setopt(this->curl, CURLOPT_HEADERFUNCTION, &CurlResponseHeadersCallback);
	curl_easy_setopt(this->curl, CURLOPT_WRITEHEADER, (void *)&curlResponse); 

	if (this->method==METHOD_HEAD) {
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
	}

	struct curl_slist *slist = NULL;

	for(std::vector<int>::size_type i = 0; i != this->headerNamesList.size(); i++) {
		char *header;
		asprintf(&header, "%s: %s", this->headerNamesList[i].c_str(), this->headerValuesList[i].c_str());
		slist = curl_slist_append(slist, header);
		free(header);
	}

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);

	CURLcode res = curl_easy_perform(this->curl);
	curl_slist_free_all(slist);

	curl_easy_getinfo(this->curl, CURLINFO_RESPONSE_CODE, &this->httpStatusCode);

	this->body = curlResponse.body;
	this->bodySize = curlResponse.bodySize;

	this->parseHeaders(curlResponse.headers, curlResponse.headersSize);

	curl_easy_cleanup(curl);
	free(curlResponse.headers);

	return res;
}
