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

int main(int argc, char *argv[]) {
	int number_failed;
	SRunner *sr = srunner_create(LocalFileListSuite());
	srunner_add_suite(sr, AmazonCredentialsSuite());
	srunner_add_suite(sr, AmzHeadersSuite());
	srunner_add_suite(sr, FileListStorageSuite());
	srunner_add_suite(sr, RemoteListOfFilesSuite());
	srunner_add_suite(sr, UploadSuite());
	srunner_run_all(sr, CK_VERBOSE);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
