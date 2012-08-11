#ifndef _MICROCURL_H
#define _MICROCURL_H

#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

#include "amzHeaders.h"
#include <curl/curl.h>

#define METHOD_GET  1
#define METHOD_POST 2
#define METHOD_PUT  3
#define METHOD_HEAD 4

class MicroCurl
{
private: 
	CURL *curl;
	std::vector<std::string> headerNamesList;
	std::vector<std::string> headerValuesList;

	std::vector<std::string> resultingHeaderNamesList;
	std::vector<std::string> resultingHeaderValuesList;

	void reset();
	void parseHeaders(char *headers, uint32_t headersSize);

public:
	int method;
	char *url;
	int debug;
	int connectTimeout;
	int networkTimeout;
	int maxConnects;
	int lowSpeedLimit;

	int httpStatusCode;
	char *body;
	uint32_t bodySize;

	MicroCurl();
	~MicroCurl();

	void addHeader(char *name, char *format, ...);
	CURLcode go();
	char *getHeader(char *name);
	char *escapePath(char *path);
};

#endif
