#ifndef _AMAZONCREDENTIALS_H
#define _AMAZONCREDENTIALS_H

#include <iostream>
using namespace std;

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>

class AmazonCredentials
{
private: 
	int sign(char *result, char *stringToSign);
	
public:
	AmazonCredentials(char *accessKeyId, char *secretAccessKey, char *bucket, char *endPoint);
	~AmazonCredentials();

	char *generateUrl(char *remotePath);
	char *createAuthorizationHeader(char *stringToSign);

	char *accessKeyId;
	char *secretAccessKey;
	char *bucket;
	char *endPoint;
};


#endif
