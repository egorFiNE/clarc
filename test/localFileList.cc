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
}

#include "localFileList.h"

LocalFileList *localFileList;

void setup(void) {
	localFileList = new LocalFileList();
}

void teardown(void) {
	delete localFileList;
}

START_TEST(empty) {
	fail_unless(localFileList->count==0);
	fail_unless(localFileList->calculateTotalSize()==0);
} END_TEST

START_TEST(addFiles) {
	localFileList->add("/nowhere", 201);
	fail_unless(localFileList->count==1);
	fail_unless(localFileList->calculateTotalSize()==201);

	localFileList->add("/aboutWhat", 303);
	fail_unless(localFileList->count==2);
	fail_unless(localFileList->calculateTotalSize()==201+303);
} END_TEST


START_TEST(recurseIn) {
	localFileList->recurseIn("", "./recurse-in-test");
	fail_unless(localFileList->count==4);
	fail_unless(localFileList->calculateTotalSize()==17);
	fail_unless(strcmp(localFileList->paths[0], "/123")==0);
	fail_unless(localFileList->sizes[0]==4);
	fail_unless(strcmp(localFileList->paths[1], "/1234")==0);
	fail_unless(localFileList->sizes[1]==5);
} END_TEST

Suite *LocalFileListSuite(void) {
	Suite *s = suite_create("LocalFileList");

	TCase *tc1 = tcase_create("non-filesystem tests");
	tcase_add_checked_fixture (tc1, setup, teardown);
	tcase_add_test(tc1, empty);
	tcase_add_test(tc1, addFiles);
	suite_add_tcase(s, tc1);

	TCase *tc2 = tcase_create("filesystem tests");
	tcase_add_checked_fixture (tc2, setup, teardown);
	tcase_add_test(tc2, recurseIn);
	suite_add_tcase(s, tc2);

	return s;
}

int main(int argc, char *argv[]) {
	int number_failed;
	Suite *s = LocalFileListSuite();
	SRunner *sr = srunner_create(s);
	srunner_run_all(sr, CK_VERBOSE);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
