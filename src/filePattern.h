#ifndef _FILEPATTERN_H
#define _FILEPATTERN_H

#include <iostream>
using namespace std;

#include <string.h>
#include <stdlib.h>
#include "re2/re2.h"

class FilePattern
{
private:
	RE2 **patterns;
	uint32_t size;

public:
	uint32_t count;

	FilePattern();
	~FilePattern();

	int add(char *pattern);
	void addDatabase(char *databaseFilename);
	int readFile(char *filename);

	int matches(char *path);
};

#endif
