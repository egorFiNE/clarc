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

bool AmazonCredentials::isValidRegionForBucketCreate(std::string region) {
	return
		(region == "us-east-2") ||
		(region == "us-east-1") ||
		(region == "us-west-2") ||
		(region == "us-west-1") ||
		(region == "ca-central-1") ||
		(region == "ap-south-1") ||
		(region == "ap-northeast-2") ||
		(region == "ap-northeast-3") ||
		(region == "ap-southeast-1") ||
		(region == "ap-southeast-2") ||
		(region == "ap-northeast-1") ||
		(region == "cn-north-1") ||
		(region == "eu-central-1") ||
		(region == "eu-west-1") ||
		(region == "eu-west-2") ||
		(region == "eu-west-3") ||
		(region == "sa-east-1");
}

char *AmazonCredentials::generateUrlForObjectDelete(int useSsl) {
	char *resultUrl;
	asprintf(&resultUrl, "%s://%s.s3.amazonaws.com/?delete", useSsl?"https":"http", this->bucket);
	return resultUrl;
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

int AmazonCredentials::signString(char *result, char *stringToSign) {
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

 char *AmazonCredentials::sign(char *stringToSign) {
	char signature[1024] = "";

	int res = this->signString(signature, stringToSign);
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
