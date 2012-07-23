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
#include "amazonCredentials.h"
#include "upload.h"

Uploader *uploader;
AmazonCredentials *Upload_amazonCredentials;

void Upload_setup(void) {
	Upload_amazonCredentials = new AmazonCredentials(
		"AKIAIOSFODNN7EXAMPLE", 
		"wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY",
		"bucket", "end-point"
	);
}

void Upload_teardown(void) {
	delete Upload_amazonCredentials;
}

START_TEST(Upload_extractMD5FromETagHeaders) {
	char md5[1024]="";
	Uploader::extractMD5FromETagHeaders("sdfsdf", md5);
	fail_unless(strlen(md5)==0);
	Uploader::extractMD5FromETagHeaders("", md5);
	fail_unless(strlen(md5)==0);
	Uploader::extractMD5FromETagHeaders(NULL, md5);
	fail_unless(strlen(md5)==0);

	Uploader::extractMD5FromETagHeaders("sdfsdf\nETag: \"68b329da9893e34099c7d8ad5cb9c940\"\nsdfsdf", md5);
	fail_unless(strcmp(md5, "68b329da9893e34099c7d8ad5cb9c940")==0);

	Uploader::extractMD5FromETagHeaders("sdfsdf\nETag: \"68b329da9893e34099c7d8ad5cb9c940\"", md5);
	fail_unless(strcmp(md5, "68b329da9893e34099c7d8ad5cb9c940")==0);

	Uploader::extractMD5FromETagHeaders("ETag: \"68b329da9893e34099c7d8ad5cb9c9401\"", md5);
	fail_unless(strlen(md5)==0);

	Uploader::extractMD5FromETagHeaders("ETag: \"68b329da9893e34099c7d8ad5cb9c94\"", md5);
	fail_unless(strlen(md5)==0);
} END_TEST

Suite *UploadSuite(void) {
	Suite *s = suite_create("Upload");

	TCase *tc1 = tcase_create("static");
	tcase_add_test(tc1, Upload_extractMD5FromETagHeaders);
	suite_add_tcase(s, tc1);

	return s;
}

