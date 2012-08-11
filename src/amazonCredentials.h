#ifndef _AMAZONCREDENTIALS_H
#define _AMAZONCREDENTIALS_H

#include <iostream>
using namespace std;

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

class AmazonCredentials
{
private: 
	int signString(char *result, char *stringToSign);
	
public:
	AmazonCredentials(char *accessKeyId, char *secretAccessKey, char *bucket, char *endPoint);
	~AmazonCredentials();

	char *generateUrl(char *remotePath, int useSsl);
	char *generateUrlForBucketCreate(int useSsl);
	char *generateUrlForObjectDelete(int useSsl);
	static int isValidRegionForBucketCreate(char *region);

	char *sign(char *stringToSign);

	char *accessKeyId;
	char *secretAccessKey;
	char *bucket;
	char *endPoint;
};

#endif
