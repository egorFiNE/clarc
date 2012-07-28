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
#include "filePattern.h"

extern "C" {
#include "logger.h"
}

LocalFileList::LocalFileList(FilePattern *excludeFilePattern) {
	this->paths = (char **) malloc(sizeof(char *) * 100);
	this->sizes = (uint64_t *) malloc(sizeof(uint64_t) * 100);
	this->allocCount = 100;
	this->count = 0;
	this->excludeFilePattern = excludeFilePattern;
}

LocalFileList::~LocalFileList() {
	for (uint32_t i=0;i<this->count;i++) {
		free(this->paths[i]);
	}

	free(this->sizes);
	free(this->paths);

	this->excludeFilePattern = NULL;
}

void LocalFileList::add(char *path, uint64_t size) {
	if (this->excludeFilePattern && this->excludeFilePattern->matches(path)) {
		LOG(LOG_DBG, "[File List] Excluded %s", path);
		return;
	}

	this->paths[this->count] = strdup(path);
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
	toOpen[0]=0;
	strcat(toOpen, prefix);
	strcat(toOpen, path);

	DIR *dirp = opendir(toOpen);

	struct dirent *dp = NULL;
	while ((dp = readdir(dirp))) {
		if (dp->d_type == DT_REG || dp->d_type == DT_LNK) {
			uint64_t len = (strlen(path)+strlen(dp->d_name)+2);
			char *fullName = (char *) malloc((size_t)len);
			fullName[0]=0;
			strcat(fullName, path);
			strcat(fullName, "/");
			strcat(fullName, dp->d_name);

			char *statName = (char *) malloc(strlen(prefix)+strlen(fullName)+2);
			statName[0]=0;
			strcat(statName, prefix);
			strcat(statName, fullName);

			struct stat fileInfo;
			if (lstat(statName, &fileInfo)<0) {
				LOG(LOG_WARN, "[File List] WARNING: cannot stat file: %s", statName);
			} else {
				this->add(fullName, (uint64_t) fileInfo.st_size);
			}
			free(fullName);
			free(statName);

		} else if (dp->d_type == DT_DIR) {
			if (strcmp(dp->d_name, ".")!=0 && strcmp(dp->d_name, "..")!=0) {
				char *_path= (char *) malloc(strlen(path)+strlen(dp->d_name)+2);
				_path[0]=0;
				strcat(_path, path);
				strcat(_path, "/");
				strcat(_path, dp->d_name);
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

