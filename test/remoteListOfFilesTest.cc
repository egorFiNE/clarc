#include <iostream>
using namespace std;

extern "C" {
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <errno.h>
#include <sqlite3.h>
#include <getopt.h>
#include <check.h>
#include "test.h"
}

#define private public
#include "remoteListOfFiles.h"
#include "amazonCredentials.h"

RemoteListOfFiles *remoteListOfFiles;
AmazonCredentials *remoteListOfFiles_amazonCredentials;

void RemoteListOfFiles_setup(void) {
	remoteListOfFiles_amazonCredentials = new AmazonCredentials(
		"AKIAIOSFODNN7EXAMPLE", 
		"wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY",
		"bucket", "end-point"
	);
	remoteListOfFiles = new RemoteListOfFiles(remoteListOfFiles_amazonCredentials);
}

void RemoteListOfFiles_teardown(void) {
	delete remoteListOfFiles_amazonCredentials;
	delete remoteListOfFiles;
}

START_TEST(RemoteListOfFiles_basicAdd) {
	fail_unless(remoteListOfFiles->count==0);
	remoteListOfFiles->add("/vasya", "68b329da9893e34099c7d8ad5cb9c940");
	remoteListOfFiles->add("/petya", "68b222222222224099c7d8ad5cb9c940");
	fail_unless(remoteListOfFiles->count==2);
	fail_unless(strcmp(remoteListOfFiles->paths[1], "/petya")==0);

	remoteListOfFiles->mtimes[0]=1342551900;
	remoteListOfFiles->mtimes[1]=1342551901;
} END_TEST


START_TEST(RemoteListOfFiles_extractMd5FromEtag) {
	fail_unless(strcmp(RemoteListOfFiles::extractMd5FromEtag("\"68b329da9893e34099c7d8ad5cb9c940\""), "68b329da9893e34099c7d8ad5cb9c940")==0);
	fail_unless(RemoteListOfFiles::extractMd5FromEtag(NULL)==NULL);
	fail_unless(RemoteListOfFiles::extractMd5FromEtag("")==NULL);
	fail_unless(RemoteListOfFiles::extractMd5FromEtag("1")==NULL);
	fail_unless(RemoteListOfFiles::extractMd5FromEtag("12")==NULL);
} END_TEST

START_TEST(RemoteListOfFiles_extractMtimeFromHeaders) {
	fail_unless(RemoteListOfFiles::extractMtimeFromHeaders("x-amz-meta-mtime: 1342552434")==1342552434);
	fail_unless(RemoteListOfFiles::extractMtimeFromHeaders("\nsdfsdf: 234\nx-amz-meta-mtime: 1342552434\n")==1342552434);
	fail_unless(RemoteListOfFiles::extractMtimeFromHeaders("\nsdfsdf: 234\r\nx-amz-meta-mtime: 1342552434\r\n")==1342552434);
	fail_unless(RemoteListOfFiles::extractMtimeFromHeaders("\nsdfsdf: 234\nx-amz-meta-mtime: 3\nsirko: vasya\n")==3);
	fail_unless(RemoteListOfFiles::extractMtimeFromHeaders("\nsdfsdf: 234\r\nx-amz-meta-mtime: 3\r\nsirko: vasya\r\n")==3);
	fail_unless(RemoteListOfFiles::extractMtimeFromHeaders("\nsdfsdf: 234\nx-amz-meta-mtime: ")==0);
	fail_unless(RemoteListOfFiles::extractMtimeFromHeaders("\nsdfsdf: 234\nx-amz-meta-mtime: \r\n")==0);
	fail_unless(RemoteListOfFiles::extractMtimeFromHeaders("\nsdfsdf: 234\nx-amz-nothing: nothing")==0);
	fail_unless(RemoteListOfFiles::extractMtimeFromHeaders("")==0);
	fail_unless(RemoteListOfFiles::extractMtimeFromHeaders(NULL)==0);
} END_TEST

char *readXml(int num) {
	char *path;
	asprintf(&path, "./data/list/list-%d.xml", num);

	FILE *f = fopen(path, "r");
	if (!f) {
		free(path);
		return NULL;
	}
	free(path);

	uint32_t len = 2*1024*1024;

	char *sirko = (char *)malloc(len); 
	bzero(sirko, len);
	size_t realBytes = fread(sirko, 1, len, f);
	sirko = (char *)realloc(sirko, realBytes);

	fclose(f);

	return sirko;
}

START_TEST(RemoteListOfFiles_parseFirstXML) {
	char *xml = readXml(0);
	fail_unless(strlen(xml)>0);

	char lastKey[1024]="";
	char errorResult[1024]="";
	uint8_t isTruncated=0;
	int res;

	res=remoteListOfFiles->parseListOfFiles(xml, strlen(xml), &isTruncated, lastKey, errorResult);
	fail_unless(res==1);
	fail_unless(isTruncated==1);
	fail_unless(strcmp(lastKey, "_usr_bin_tfmtodit")==0);
	fail_unless(strcmp(errorResult, "")==0);

	fail_unless(remoteListOfFiles->count==1000);
	fail_unless(strcmp(remoteListOfFiles->paths[0], ".clarc.sqlite3")==0);
	fail_unless(strcmp(remoteListOfFiles->md5s[0], "2538c9b8ac602d3fb92772d62fdcaa54")==0);
	fail_unless(strcmp(remoteListOfFiles->paths[999], "_usr_bin_tfmtodit")==0);
	fail_unless(strcmp(remoteListOfFiles->md5s[999], "d9cca721a735dac4efe709e0f3518373")==0);
} END_TEST

START_TEST(RemoteListOfFiles_parseSecondXML) {
	char lastKey[10240]="";
	char errorResult[10240]="";
	uint8_t isTruncated=0;
	int res;

	remoteListOfFiles_amazonCredentials = new AmazonCredentials(
		"AKIAIOSFODNN7EXAMPLE", 
		"wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY",
		"bucket", "end-point"
	);
	remoteListOfFiles = new RemoteListOfFiles(remoteListOfFiles_amazonCredentials);

	char *xml = readXml(0);
	fail_unless(strlen(xml)>0);
	res=remoteListOfFiles->parseListOfFiles(xml, strlen(xml), &isTruncated, lastKey, errorResult);
	fail_unless(res==1);
	fail_unless(isTruncated==1);

	xml = readXml(1);
	fail_unless(strlen(xml)>0);
	res=remoteListOfFiles->parseListOfFiles(xml, strlen(xml), &isTruncated, lastKey, errorResult);
	fail_unless(res==1);
	fail_unless(isTruncated==0);
	fail_unless(strcmp(lastKey, "wireless-regdb-2011.04.28.tar.bz9")==0);
	fail_unless(strcmp(errorResult, "")==0);

	fail_unless(remoteListOfFiles->count==1430);
	fail_unless(strcmp(remoteListOfFiles->paths[0], ".clarc.sqlite3")==0);
	fail_unless(strcmp(remoteListOfFiles->md5s[0], "2538c9b8ac602d3fb92772d62fdcaa54")==0);
	fail_unless(strcmp(remoteListOfFiles->paths[1429], "wireless-regdb-2011.04.28.tar.bz9")==0);
	fail_unless(strcmp(remoteListOfFiles->md5s[1429], "16b7fabd4d7761ccf206702a3f18cce9")==0);
	
	// delete remoteListOfFiles_amazonCredentials;
	// delete remoteListOfFiles;
} END_TEST

START_TEST(RemoteListOfFiles_parseBrokenXML) {
	fail_unless(remoteListOfFiles->count==0);

	char lastKey[1024]="";
	char errorResult[1024]="";
	uint8_t isTruncated=0;
	int res;
	char xml[1024*10]="";

	res=remoteListOfFiles->parseListOfFiles(xml, strlen(xml), &isTruncated, lastKey, errorResult);
	fail_unless(res==0);
	fail_unless(strlen(errorResult)>0);

	strcpy(xml, "broken");
	res=remoteListOfFiles->parseListOfFiles(xml, strlen(xml), &isTruncated, lastKey, errorResult);
	fail_unless(res==0);
	fail_unless(strlen(errorResult)>0);

	strcpy(xml, "<empty/>");
	res=remoteListOfFiles->parseListOfFiles(xml, strlen(xml), &isTruncated, lastKey, errorResult);
	fail_unless(res==0);
	fail_unless(strlen(errorResult)>0);

	strcpy(xml, "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\"><Contents><Key>_usr_bin_tftp</Key></Contents></ListBucketResult>");
	res=remoteListOfFiles->parseListOfFiles(xml, strlen(xml), &isTruncated, lastKey, errorResult);
	fail_unless(res==1);
	fail_unless(remoteListOfFiles->count==0);

	strcpy(xml, "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\"><Contents></Contents></ListBucketResult>");
	res=remoteListOfFiles->parseListOfFiles(xml, strlen(xml), &isTruncated, lastKey, errorResult);
	fail_unless(res==1);
	fail_unless(remoteListOfFiles->count==0);

	strcpy(xml, "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\"></ListBucketResult>");
	res=remoteListOfFiles->parseListOfFiles(xml, strlen(xml), &isTruncated, lastKey, errorResult);
	fail_unless(res==1);
	fail_unless(remoteListOfFiles->count==0);

	strcpy(xml, "<ListBucketResult xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\"><Contents><ETag>\"d9cca721a735dac4efe709e0f3518373\"</ETag></Contents></ListBucketResult>");
	res=remoteListOfFiles->parseListOfFiles(xml, strlen(xml), &isTruncated, lastKey, errorResult);
	fail_unless(res==1);
	fail_unless(remoteListOfFiles->count==0);
} END_TEST

Suite *RemoteListOfFilesSuite(void) {
	Suite *s = suite_create("RemoteListOfFiles");

	TCase *tc1 = tcase_create("static");
	tcase_add_test(tc1, RemoteListOfFiles_extractMd5FromEtag);
	tcase_add_test(tc1, RemoteListOfFiles_extractMtimeFromHeaders);
	suite_add_tcase(s, tc1);

	TCase *tc2 = tcase_create("main");
	tcase_add_checked_fixture (tc2, RemoteListOfFiles_setup, RemoteListOfFiles_teardown);
	tcase_add_test(tc2, RemoteListOfFiles_basicAdd);
	suite_add_tcase(s, tc2);

	TCase *tc3 = tcase_create("parseXML");
	tcase_add_checked_fixture (tc3, RemoteListOfFiles_setup, RemoteListOfFiles_teardown);
	tcase_add_test(tc3, RemoteListOfFiles_parseFirstXML);
	tcase_add_test(tc3, RemoteListOfFiles_parseSecondXML);
	suite_add_tcase(s, tc3);

	TCase *tc4 = tcase_create("parseBrokenXML");
	tcase_add_checked_fixture (tc4, RemoteListOfFiles_setup, RemoteListOfFiles_teardown);
	tcase_add_test(tc4, RemoteListOfFiles_parseBrokenXML);
	suite_add_tcase(s, tc4);

	return s;
}

