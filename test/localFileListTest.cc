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

#include "localFileList.h"

LocalFileList *localFileList;

void LocalFileList_setup(void) {
	localFileList = new LocalFileList();
}

void LocalFileList_teardown(void) {
	delete localFileList;
}

START_TEST(LocalFileList_empty) {
	fail_unless(localFileList->count==0);
	fail_unless(localFileList->calculateTotalSize()==0);
} END_TEST

START_TEST(LocalFileList_addFiles) {
	localFileList->add("/nowhere", 201);
	fail_unless(localFileList->count==1);
	fail_unless(localFileList->calculateTotalSize()==201);

	localFileList->add("/aboutWhat", 303);
	fail_unless(localFileList->count==2);
	fail_unless(localFileList->calculateTotalSize()==201+303);
} END_TEST


START_TEST(LocalFileList_recurseIn) {
	localFileList->recurseIn("", "./data/recurse-in-test");
	fail_unless(localFileList->count==4);
	fail_unless(localFileList->calculateTotalSize()==17);
	fail_unless(strcmp(localFileList->paths[0], "/123")==0);
	fail_unless(localFileList->sizes[0]==4);
	fail_unless(strcmp(localFileList->paths[1], "/1234")==0);
	fail_unless(localFileList->sizes[1]==5);
} END_TEST

START_TEST(LocalFileList_recurseInEmpty) {
	localFileList->recurseIn("", "./data/recurse-in-test-empty");
	fail_unless(localFileList->count==0);
	fail_unless(localFileList->calculateTotalSize()==0);
} END_TEST

START_TEST(LocalFileList_recurseInEmptySubdir) {
	char path[1024] = "./data/recurse-in-test-empty-subdir";
	mkdir(path, 0755);
	char path2[1024];
	sprintf(path2, "%s/%s", path, "sirko");
	mkdir(path2, 0755);

	localFileList->recurseIn("", path);
	fail_unless(localFileList->count==0);
	fail_unless(localFileList->calculateTotalSize()==0);
} END_TEST

Suite *LocalFileListSuite(void) {
	Suite *s = suite_create("LocalFileList");

	TCase *tc1 = tcase_create("non-filesystem tests");
	tcase_add_checked_fixture (tc1, LocalFileList_setup, LocalFileList_teardown);
	tcase_add_test(tc1, LocalFileList_empty);
	tcase_add_test(tc1, LocalFileList_addFiles);
	suite_add_tcase(s, tc1);

	TCase *tc2 = tcase_create("filesystem tests");
	tcase_add_checked_fixture (tc2, LocalFileList_setup, LocalFileList_teardown);
	tcase_add_test(tc2, LocalFileList_recurseIn);
	tcase_add_test(tc2, LocalFileList_recurseInEmpty);
	tcase_add_test(tc2, LocalFileList_recurseInEmptySubdir);
	suite_add_tcase(s, tc2);

	return s;
}
