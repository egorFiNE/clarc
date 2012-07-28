#include <vector>
#include <algorithm>
#include <iostream>
using namespace std;

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "amzHeaders.h"


AmzHeaders::AmzHeaders() {
	this->count=0;
	this->namesLength=0;
	this->valuesLength=0;
}

AmzHeaders::~AmzHeaders() {
	for (int i=0;i<this->count;i++) {
		this->names[i] = NULL;
		free(this->values[i]);
	}
}

void AmzHeaders::add(char *name, char *format, ...) {
	this->names[this->count]=name;

	char *result;
	va_list args;
	va_start(args, format);
	vasprintf(&result, format, args);
	va_end(args);

	this->values[this->count]=result;

	this->namesLength+=strlen(name);
	this->valuesLength+=strlen(result);

	this->count++;
}

char *AmzHeaders::serializeIntoStringToSign() {
	// okay here I cheat and use C++. Please forgive me or rewrite in plain C.
	std::vector<std::string> headersList;
	for (int i=0;i<this->count;i++) {
		char *header;
		asprintf(&header, "%s:%s\n", this->names[i], this->values[i]);
		headersList.push_back(header);
		free(header);
	}

	std::sort(headersList.begin(), headersList.end());

	std::string result;

	for (int i=0;i<this->count;i++) {
		result.append(headersList[i]);
	}

	return strdup((char *) result.c_str());
}

struct curl_slist *AmzHeaders::serializeIntoCurl(struct curl_slist *slist) {
	struct curl_slist *slist2 = slist;
	char value[1024*10];
	for (int i=0;i<this->count;i++) {
		sprintf(value, "%s: %s", this->names[i], this->values[i]);
		slist2 = curl_slist_append(slist2, value);
	}

	return slist2;
}
