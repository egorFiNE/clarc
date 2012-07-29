#include "curlResponse.h"
#include "logger.h"
#include <string.h>

void CurlResponseInit(struct CurlResponse *curlResponse) {
	curlResponse->body = (char *) malloc(1);
	curlResponse->body[0]=0;
	curlResponse->bodySize = 0;

	curlResponse->headers = (char *) malloc(1);
	curlResponse->headers[0]=0;
	curlResponse->headersSize = 0;
}

void CurlResponseFree(struct CurlResponse *curlResponse) {
	free(curlResponse->body);
	free(curlResponse->headers);
}

size_t CurlResponseHeadersCallback(void *contents, size_t size, size_t nmemb, struct CurlResponse *curlResponse) {
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

size_t CurlResponseBodyCallback(void *contents, size_t size, size_t nmemb, struct CurlResponse *curlResponse) {
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

