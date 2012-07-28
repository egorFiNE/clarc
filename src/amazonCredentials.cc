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

int AmazonCredentials::isValidRegionForBucketCreate(char *region) {
	return (
		strcmp(region, "") == 0 || 
		strcmp(region, "US") == 0 || 
		strcmp(region, "EU") == 0 || 
		strcmp(region, "eu-west-1") == 0 || 
		strcmp(region, "us-west-1") == 0 || 
		strcmp(region, "us-west-2") == 0 || 
		strcmp(region, "ap-southeast-1") == 0 || 
		strcmp(region, "ap-northeast-1") == 0 || 
		strcmp(region, "sa-east-1") == 0
	);
}

char *AmazonCredentials::generateUrlForBucketCreate(int useSsl) {
	char *resultUrl;
	asprintf(&resultUrl, "%s://%s.s3.amazonaws.com/", useSsl?"https":"http", this->bucket);
	return resultUrl;
}

char *AmazonCredentials::generateUrl(char *remotePath, int useSsl) {
	if (remotePath==NULL) {
		return NULL;
	}

	char *remotePathToUse = remotePath;
	if (remotePathToUse[0]=='/') {
		remotePathToUse++;
	}

	char *resultUrl;
	asprintf(&resultUrl, "%s://%s.%s/%s", useSsl?"https":"http", this->bucket, this->endPoint, remotePathToUse);
	return resultUrl;
}

int AmazonCredentials::sign(char *result, char *stringToSign) {
	// char *key = strdup(this->secretAccessKey); was needed for something
	char resultBinary[64];
	int res = hmac_sha1(this->secretAccessKey, strlen(this->secretAccessKey), stringToSign, strlen(stringToSign), resultBinary);
	if (res==0) {
		base64_encode((char *)resultBinary, 20, result, 64);
		return 1;
	} else {
		return 0;
	}
}

 char *AmazonCredentials::createAuthorizationHeader(char *stringToSign) {
	char signature[1024] = "";

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
