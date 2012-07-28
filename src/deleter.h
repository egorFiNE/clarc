#ifndef _DELETER_H
#define _DELETER_H

#include <iostream>
using namespace std;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "amazonCredentials.h"
#include "fileListStorage.h"
#include "localFileList.h"

class Deleter
{
private:
	AmazonCredentials *amazonCredentials;
	LocalFileList *localFileList;
	FileListStorage *fileListStorage;

public:
	Deleter(AmazonCredentials *amazonCredentials, LocalFileList *localFileList, FileListStorage *fileListStorage);
	~Deleter();
	int performDeletion();
};

#endif
