#include <iostream>
using namespace std;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "filePattern.h"
#include "re2/re2.h"

FilePattern::FilePattern() {
	this->count=0;
	this->size=1000;
	this->patterns =  (RE2 **) malloc(sizeof(RE2 *)*this->size); 
}

FilePattern::~FilePattern() {
	for (int i=0;i<this->count;i++) {
		delete this->patterns[i];
	}
	free(this->patterns);
}

int FilePattern::readFile(char *filename) {
	if (FILE *fp = fopen(filename, "r")) {
		char line[1024*100];
		while (fgets(line, 1024*100, fp)) {
			if (line[strlen(line)-1]=='\n') {
				line[strlen(line)-1]=0;
			}
			if (!this->add(line)) {
				fclose(fp);
				return 0;
			}
		}
		fclose(fp);
		return 1;
	}
	return 0;
}

int FilePattern::add(char *pattern) {
	if (this->count==this->size) {
	 	this->size+=100;
		this->patterns =  (RE2 **) realloc(this->patterns, sizeof(RE2 *)*this->size); 
	}

	RE2 *re = new RE2(pattern);
	if (re->ok()) {
		this->patterns[this->count] = re;
		this->count++;
		return 1;
	} else {
		return 0;
	}
}

int FilePattern::matches(char *path) {
	for (int i=0;i<this->count;i++) {
		RE2 *re = this->patterns[i];
		if (RE2::FullMatch(path, *re)) {
			return 1;
		}
	}
	return 0;
}
