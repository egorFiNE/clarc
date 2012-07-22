#include <iostream>
using namespace std;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "amazonCredentials.h"

extern "C" {
#include "base64.h"
#include "hmac.h"
#include "utils.h"
}

AmazonCredentials::AmazonCredentials(char *accessKeyId, char *secretAccessKey, char *bucket, char *endPoint) {
	this->accessKeyId = accessKeyId;
	this->secretAccessKey = secretAccessKey;
	this->bucket = bucket;
	this->endPoint = endPoint;
}

AmazonCredentials::~AmazonCredentials() {
	this->accessKeyId=NULL;
	this->secretAccessKey=NULL;
	this->bucket=NULL;
	this->endPoint=NULL;
}

char *AmazonCredentials::generateUrl(char *remotePath, int useSsl) {
	if (remotePath==NULL) {
		return NULL;
	}

	char *remotePathToUse = remotePath;
	if (remotePathToUse[0]=='/') {
		remotePathToUse++;
	}

	char *resultUrl = (char *) malloc(strlen(this->bucket) + strlen(this->endPoint) + strlen(remotePathToUse) + 32);
	resultUrl[0]=0; 
  sprintf(resultUrl, "%s://%s.%s/%s", useSsl?"https":"http", this->bucket, this->endPoint, remotePathToUse);
  return resultUrl;
}

int AmazonCredentials::sign(char *result, char *stringToSign) {
	// char *key = strdup(this->secretAccessKey); was needed for something
	char resultBinary[64];
	int res =  hmac_sha1(this->secretAccessKey, strlen(this->secretAccessKey), stringToSign, strlen(stringToSign), resultBinary);
	if (res==0) {
		base64_encode((char *)resultBinary, 20, result, 64);
		return 1;
	} else {
		return 0;
	}
}

 char *AmazonCredentials::createAuthorizationHeader(char *stringToSign) {
	char signature[128] = "";

	int res = this->sign(signature, stringToSign);
	if (res) {
		char *authorization = (char*) malloc(strlen(accessKeyId) + strlen(signature) + 10);
		authorization[0]=0;
		strcat(authorization, "AWS ");
		strcat(authorization, accessKeyId);
		strcat(authorization, ":");
		strcat(authorization, signature);
		return authorization;
	}

	return NULL;
}
