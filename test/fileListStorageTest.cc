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

#include "fileListStorage.h"

FileListStorage *fileListStorage;

void FileListStorage_basic_teardown(void) {
	delete fileListStorage;
	unlink("./db.sqlite3");
}

START_TEST(FileListStorage_basic) {
	char errorResult[1024];
	fileListStorage = new FileListStorage("./db.sqlite3", errorResult);
	fail_unless(strcmp(errorResult, "")==0);

	int res; 
	char md5[33]="trash";
	uint64_t mtime=2;

	res = fileListStorage->lookup("nothing", md5, &mtime);
	fail_unless(res, STORAGE_SUCCESS);
	fail_unless(strcmp(md5, "")==0);
	fail_unless(mtime==0);

	strcpy(md5, "trash");
	mtime=3;
	res = fileListStorage->lookup("", md5, &mtime);
	fail_unless(res, STORAGE_SUCCESS);
	fail_unless(strcmp(md5, "")==0);
	fail_unless(mtime==0);

	strcpy(md5, "trash");
	mtime=3;
	res = fileListStorage->lookup(NULL, md5, &mtime);
	fail_unless(res, STORAGE_SUCCESS);
	fail_unless(strcmp(md5, "")==0);
	fail_unless(mtime==0);

	strcpy(md5, "68b329da9893e34099c7d8ad5cb9c940");
	mtime=1342551142;
	res = fileListStorage->store("vasya", md5, mtime);
	fail_unless(res, STORAGE_SUCCESS);

	strcpy(md5, "trash");
	mtime=3;
	res = fileListStorage->lookup("vasya", md5, &mtime);
	fail_unless(res, STORAGE_SUCCESS);
	fail_unless(strcmp(md5, "68b329da9893e34099c7d8ad5cb9c940")==0);
	fail_unless(mtime==1342551142);	
} END_TEST

START_TEST(FileListStorage_openFailures) {
	char errorResult[1024];
	fileListStorage = new FileListStorage("/cannot/open/this", errorResult);
	fail_unless(strcmp(errorResult, "")!=0);
} END_TEST


Suite *FileListStorageSuite(void) {
	Suite *s = suite_create("FileListStorage");

	TCase *tc = tcase_create("main");
	tcase_add_checked_fixture (tc, NULL, FileListStorage_basic_teardown);
	tcase_add_test(tc, FileListStorage_basic);
	suite_add_tcase(s, tc);

	TCase *tc2 = tcase_create("main");
	tcase_add_test(tc2, FileListStorage_openFailures);
	suite_add_tcase(s, tc2);

	return s;
}
