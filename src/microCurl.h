#ifndef _MICROCURL_H
#define _MICROCURL_H

#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

#include <curl/curl.h>

#include "amazonCredentials.h"

#define METHOD_GET  1
#define METHOD_POST 2
#define METHOD_PUT  3
#define METHOD_HEAD 4

class MicroCurl
{
private: 
	CURL *curl;
	char *headerContentType;
	char *headerContentMd5;
	char *headerDate;
	AmazonCredentials *amazonCredentials;

	std::vector<std::string> headerNamesList;
	std::vector<std::string> headerValuesList;

	std::vector<std::string> resultingHeaderNamesList;
	std::vector<std::string> resultingHeaderValuesList;

	void reset();
	void parseHeaders(char *headers, uint32_t headersSize);
	const char *getMethodName();

public:
	int debug;
	int method;
	char *url;
	char *canonicalizedResource;
	int connectTimeout;
	int networkTimeout;
	int maxConnects;
	int lowSpeedLimit;

	char *postData;
	uint32_t postSize;

	FILE *fileIn;
	uint64_t fileSize;

	char *curlErrors;
	int httpStatusCode;

	char *body;
	uint32_t bodySize;

	MicroCurl(AmazonCredentials *amazonCredentials);
	~MicroCurl();

	void addHeader(char *name, char *value);
	void addHeaderFormat(char *name, char *format, ...);
	char *serializeAmzHeadersIntoStringToSign();
	char *getStringToSign();

	CURL *prepare();
	CURLcode go();

	char *getHeader(char *name);
	char *escapePath(char *path);

};

#endif
