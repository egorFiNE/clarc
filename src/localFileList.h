#ifndef _LOCALFILELIST_H
#define _LOCALFILELIST_H

#include <iostream>
using namespace std;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

class LocalFileList
{
private:
	uint32_t allocCount;

public:
	LocalFileList();
	~LocalFileList();

	void add(char *path, uint64_t size);
	void recurseIn(char *path, char *prefix);
	uint64_t calculateTotalSize();
	
	char **paths;
	uint64_t *sizes;
	uint32_t count;
};


#endif
