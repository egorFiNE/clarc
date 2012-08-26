#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "utils.h"
#include "base64.h"
#include "hmac.h"

char *getIsoDate() {
	time_t rawtime;
	time(&rawtime);
	struct tm *timeinfo;
	timeinfo = gmtime (&rawtime);
	char *date = (char *)malloc(128);
	date[0]=0;
	strftime(date, 128, "%a, %d %b %Y %H:%M:%S GMT", timeinfo);
	return date;
}

char *hrSize(uint64_t size) {
	char *result = NULL;
	if (size>=1024*1024*1024) { // gigabyte
		asprintf(&result, "%.3fGb", (double)size / 1024 / 1024 / 1024);
	} else if (size>=1024*1024) { // megabyte
		asprintf(&result, "%.2fMb", (double)size / 1024 / 1024);
	} else if (size>=1024) { // kilobyte
		asprintf(&result, "%.2fKb", (double)size / 1024);
	} else { 
		asprintf(&result, "%hub", (uint16_t) size);
	}
	return result;
}

char *guessContentType(char *filename) {
	char *dotPos = strrchr(filename, '.');
	if (dotPos==NULL) {
		return (char *)"application/octet-stream";
	}

	dotPos++;
	char *lowcaseExtension = (char *) malloc(strlen(filename));
	lowcaseExtension[0]=0;
	strcpy(lowcaseExtension, dotPos);

	uint32_t i;
	for(i = 0; i<strlen(lowcaseExtension); i++) {
		lowcaseExtension[i]=tolower(lowcaseExtension[i]);
	}

	// FIXME this is ugly
	char *result;
	if (strcmp(lowcaseExtension, "jpg")==0 || strcmp(lowcaseExtension, "jpeg")==0) {
		result = (char *)"image/jpeg";
	} else if (strcmp(lowcaseExtension, "png")==0) {
		result = (char *)"image/png";
	} else if (strcmp(lowcaseExtension, "gif")==0) {
		result = (char *)"image/gif";
	} else if (strcmp(lowcaseExtension, "bmp")==0) {
		result = (char *)"image/bmp";
	} else if (strcmp(lowcaseExtension, "tif")==0 || strcmp(lowcaseExtension, "tiff")==0) {
		result = (char *)"image/tif";

	} else if (strcmp(lowcaseExtension, "css")==0) {
		result = (char *)"text/css";
	} else if (strcmp(lowcaseExtension, "js")==0) {
		result = (char *)"text/javascript";
	} else if (strcmp(lowcaseExtension, "json")==0) {
		result = (char *)"text/json";
	} else if (strcmp(lowcaseExtension, "xml")==0) {
		result = (char *)"text/xml";
	} else if (strcmp(lowcaseExtension, "plist")==0) {
		result = (char *)"text/xml";
	} else if (strcmp(lowcaseExtension, "html")==0) {
		result = (char *)"text/html";
	} else if (strcmp(lowcaseExtension, "txt")==0) {
		result = (char *)"text/plain";

	} else if (strcmp(lowcaseExtension, "gz")==0) {
		result = (char *)"application/x-gzip";
	} else if (strcmp(lowcaseExtension, "bzip2")==0) {
		result = (char *)"application/x-bzip2";
	} else if (strcmp(lowcaseExtension, "tar")==0) {
		result = (char *)"application/x-tar";
	} else if (strcmp(lowcaseExtension, "zip")==0) {
		result = (char *)"application/zip";
		
	} else if (strcmp(lowcaseExtension, "pdf")==0) {
		result = (char *)"application/pdf";

	} else if (strcmp(lowcaseExtension, "jad")==0) {
		result = (char *)"text/vnd.sun.j2me.app-descriptor";
	} else if (strcmp(lowcaseExtension, "jar")==0) {
		result = (char *)"application/java-archive";

	} else { 
		result = (char *)"application/octet-stream";
	}
	
	free(lowcaseExtension);	

	return result;
}

