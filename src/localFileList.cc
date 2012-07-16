#include <iostream>
using namespace std;

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include "localFileList.h"

LocalFileList::LocalFileList() {
	this->paths = (char **) malloc(sizeof(char *) * 100);
	this->sizes = (uint64_t *) malloc(sizeof(uint64_t) * 100);
	this->allocCount = 100;
	this->count = 0;
}

LocalFileList::~LocalFileList() {
	for (uint32_t i=0;i<this->count;i++) {
		free(this->paths[i]);
	}

	free(this->sizes);
	free(this->paths);
}

void LocalFileList::add(char *path, uint64_t size) {
	this->paths[this->count] = path;
	this->sizes[this->count] = size;
	this->count++;
	if (this->count>=this->allocCount) {
		this->allocCount+=100;
		this->paths = (char **) realloc(this->paths, sizeof(char *) * this->allocCount);
		this->sizes = (uint64_t *) realloc(this->sizes, sizeof(uint64_t) * this->allocCount);
	}
}

void LocalFileList::recurseIn(char *path, char *prefix) {
	uint64_t len = strlen(prefix) + strlen(path)+2;
	char *toOpen = (char *) malloc(len);
	bzero(toOpen, len);
	strcat(toOpen, prefix);
	strcat(toOpen, path);

	DIR *dirp = opendir(toOpen);

  struct dirent *dp = NULL;
	while ((dp = readdir(dirp))) {
		if (dp->d_type == DT_REG) {
			uint64_t len = (strlen(path)+strlen(dp->d_name)+2);
			char *fullName = (char *) malloc((size_t)len);
			sprintf(fullName, "%s/%s", path, dp->d_name);

			char *statName = (char *) malloc(strlen(prefix)+strlen(fullName)+2);
			sprintf(statName, "%s%s", prefix, fullName);

			struct stat fileInfo;
			if (lstat(statName, &fileInfo)<0) {
				printf("[File List] WARNING: cannot stat file: %s\n", statName);
			} else {
				this->add(fullName, (uint64_t) fileInfo.st_size);
			}
			free(statName);

		} else if (dp->d_type == DT_DIR) {
			if (strcmp(dp->d_name, ".")!=0 && strcmp(dp->d_name, "..")!=0) {
				char *_path= (char *) malloc(strlen(path)+strlen(dp->d_name)+2);
				sprintf(_path, "%s/%s", path, dp->d_name);
				this->recurseIn(_path, prefix);
				free(_path);
			}
		}
	}

	free(toOpen);

	closedir(dirp);
}

uint64_t LocalFileList::calculateTotalSize() {
	uint64_t totalSize=0;
	for (uint32_t i=0;i<this->count;i++) {
		totalSize+=this->sizes[i];
	}
	return totalSize;
}

