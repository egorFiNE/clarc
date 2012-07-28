#ifndef _AMZHEADERS_H
#define _AMZHEADERS_H

#include <iostream>
using namespace std;
#include <curl/curl.h>
#include <stdint.h>

class AmzHeaders
{
private:
	char *names[50];
	char *values[50];
	uint32_t namesLength;
	uint32_t valuesLength;
	int count;


public:
	AmzHeaders();
	~AmzHeaders();

	void add(char *name, char *format, ...);
	char *serializeIntoStringToSign();
	struct curl_slist *serializeIntoCurl(struct curl_slist *slist);

	// it's not a good design to pack these *functions* here.
	static struct curl_slist *addHeader(struct curl_slist *slist, char *name, char *value);
};

#endif
