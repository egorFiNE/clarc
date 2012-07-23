#ifndef _DESTINATION_H
#define _DESTINATION_H

#include <iostream>
using namespace std;

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

class Destination
{
private:
	void parse(char *destination);

public:
	Destination(char *destination);
	~Destination();

	int isValid();
	char *absoluteString();

	char *bucket;
	char *endPoint;
	char *folder;
};

#endif
