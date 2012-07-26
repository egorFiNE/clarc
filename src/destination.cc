#include <iostream>
using namespace std;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "destination.h"

extern "C" {
#include "base64.h"
#include "hmac.h"
#include "utils.h"
}

static char *endPoints[] = {
	(char *) "s3.amazonaws.com",
	(char *) "s3-us-west-2.amazonaws.com",
	(char *) "s3-us-west-1.amazonaws.com",
	(char *) "s3-eu-west-1.amazonaws.com",
	(char *) "s3-ap-southeast-1.amazonaws.com",
	(char *) "s3-ap-northeast-1.amazonaws.com",
	(char *) "s3-sa-east-1.amazonaws.com",
	NULL
};

Destination::Destination(char *destination) {
	this->bucket = NULL;
	this->endPoint = strdup((char *) "s3.amazonaws.com");
	this->folder = NULL;

	this->parse(destination);
}

Destination::~Destination() {
	if (this->bucket!=NULL) {
		free(this->bucket);
	}

	if (this->folder!=NULL) {
		free(this->folder);
	}

	free(this->endPoint);
}

void Destination::parse(char *destination) {
	if (destination==NULL) {
		return;
	}

	if (strstr(destination, "s3://")!=destination) {
		return;
	}

	char *path = strdup(destination+5);

	char *slash = strchr(path, '/');
	if (slash!=NULL) {
		*slash=0;
		this->folder = strdup(slash+1);
	} else { 
		this->folder = strdup("");
	}

	if (this->folder[strlen(this->folder)-1]=='/') {
		this->folder[strlen(this->folder)-1]=0;
	}

	if (strchr(this->folder, '/')!=NULL) {
		return; 
	}

	int i=0;
	while (char *e=endPoints[i++]) {
		int len = strlen(e);
		char *r = strstr(path, e);
		if (r!=NULL) {
			if (*(r+len)==0) {
				this->endPoint = strdup(e);
				*(r-1)=0;
				this->bucket = strdup(path);
				break;
			}
		}
	}

	if (this->bucket==NULL) {
		this->bucket = strdup(path);
	}

	free(path);
}

int Destination::isValid() {
	return (this->bucket!=NULL && strlen(this->bucket)>0 && this->endPoint!=NULL && this->folder!=NULL) ? 1 : 0;
}

char *Destination::absoluteString() {
	int len = strlen(this->endPoint) + strlen(this->bucket) + 16;
	if (folder) {
		 len+=strlen(this->folder);
	}
	
	char *result = (char*) malloc(len);
	result[0]=0;
	strcat(result, "s3://");
	strcat(result, this->bucket);
	strcat(result, ".");
	strcat(result, this->endPoint);
	strcat(result, "/");
	if (this->folder && strcmp(this->folder, "")!=0) {
		strcat(result, this->folder);
		strcat(result, "/");
	}
	return result;
}

