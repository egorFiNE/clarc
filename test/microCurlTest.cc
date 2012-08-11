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
#include "utils.h"
#include "test.h"
#include <curl/curl.h>
}

#include "microCurl.h"
#include "amazonCredentials.h"

START_TEST(MicroCurl_mainTest) {
	AmazonCredentials *amazonCredentials = new AmazonCredentials(
		"AKIAIOSFODNN7EXAMPLE", 
		"wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY",
		"bucket", "end-point"
	);

	MicroCurl *microCurl = new MicroCurl(amazonCredentials);
	microCurl->method=METHOD_GET;
	microCurl->url = "http://egorfine.com/ru/";

	microCurl->prepare();
	CURLcode res = microCurl->go();
	fail_unless(res==CURLE_OK);
	fail_unless(microCurl->httpStatusCode==200);

	fail_unless(strcmp(microCurl->getHeader("content-type"), "text/html")==0);
} END_TEST

Suite *MicroCurlSuite(void) {
	Suite *s = suite_create("MicroCurl");

	TCase *tc = tcase_create("Main");
	tcase_add_test(tc, MicroCurl_mainTest);
	suite_add_tcase(s, tc);

	return s;
}
