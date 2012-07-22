#ifndef _CURLRESPONSE_H
#define _CURLRESPONSE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

struct CurlResponse {
	char *body;
	uint64_t bodySize;
	char *headers;
	uint64_t headersSize;
};

void CurlResponseInit(struct CurlResponse *curlResponse);
void CurlResponseFree(struct CurlResponse *curlResponse);

// CURL callbacks
size_t CurlResponseHeadersCallback(void *contents, size_t size, size_t nmemb, struct CurlResponse *curlResponse);
size_t CurlResponseBodyCallback(void *contents, size_t size, size_t nmemb, struct CurlResponse *curlResponse);

#endif
