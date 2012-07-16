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

#include "amazonCredentials.h"

AmazonCredentials *amazonCredentials;

void setup(void) {
  amazonCredentials = new AmazonCredentials(
  	"AKIAIOSFODNN7EXAMPLE", 
  	"wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY",
  	"bucket", "end-point"
  );
}

void teardown(void) {
	delete amazonCredentials;
}

START_TEST(generateUrl) {
	char *path;

	path = amazonCredentials->generateUrl("wow.html");
	fail_if(strcmp(path, "http://bucket.end-point/wow.html") != 0, NULL);

	path = amazonCredentials->generateUrl("/wow.html");
	fail_if(strcmp(path, "http://bucket.end-point/wow.html") != 0, NULL);

	path = amazonCredentials->generateUrl("/");
	fail_if(strcmp(path, "http://bucket.end-point/") != 0, NULL);

	path = amazonCredentials->generateUrl("");
	fail_if(strcmp(path, "http://bucket.end-point/") != 0, NULL);

	path = amazonCredentials->generateUrl(NULL);
	fail_if(path!=NULL, NULL);
} END_TEST


START_TEST(createAuthorizationHeader) {
	char stringToSign[2048] = 
		"GET\n" \
		"\n" \
		"\n" \
		"Tue, 27 Mar 2007 19:36:42 +0000\n" \
		"/johnsmith/photos/puppy.jpg";

	char *auth = amazonCredentials->createAuthorizationHeader(stringToSign);
	fail_if(strcmp(auth, "AWS AKIAIOSFODNN7EXAMPLE:bWq2s1WEIj+Ydj0vQ697zp+IXMU=") != 0, NULL);
	free(auth);
} END_TEST


Suite *AmazonCredentialsSuite(void) {
	Suite *s = suite_create("AmazonCredentials");

	TCase *tc_core = tcase_create("AmazonCredentials");
	tcase_add_checked_fixture (tc_core, setup, teardown);
	tcase_add_test(tc_core, generateUrl);
	tcase_add_test(tc_core, createAuthorizationHeader);
	suite_add_tcase(s, tc_core);

	return s;
}

int main(int argc, char *argv[]) {
	int number_failed;
	Suite *s = AmazonCredentialsSuite();
	SRunner *sr = srunner_create(s);
	srunner_run_all(sr, CK_VERBOSE);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
