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

	char result[10240];
	va_list args;
	va_start(args, format);
	vsprintf(result, format, args);
	va_end(args);

	this->values[this->count]=strndup(result, strlen(result));

	this->namesLength+=strlen(name);
	this->valuesLength+=strlen(result);

	this->count++;
}

char *AmzHeaders::serializeIntoStringToSign() {
	// add 2 chars for each entry because each entry is joined by ":" and ended with \n
	int len = this->namesLength + this->valuesLength + (this->count * 2) + 2;
	char *result = (char *) malloc(len);
	result[0]=0;
	for (int i=0;i<this->count;i++) {
		strcat(result, this->names[i]);
		strcat(result, ":");
		strcat(result, this->values[i]);
		strcat(result, "\n");
	}

	return result;
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
