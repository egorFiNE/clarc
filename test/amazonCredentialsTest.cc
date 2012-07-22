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

#include "amazonCredentials.h"

AmazonCredentials *amazonCredentials;

void AmazonCredentials_setup(void) {
	amazonCredentials = new AmazonCredentials(
		"AKIAIOSFODNN7EXAMPLE", 
		"wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY",
		"bucket", "end-point"
	);
}

void AmazonCredentials_teardown(void) {
	delete amazonCredentials;
}

START_TEST(AmazonCredentials_generateUrl) {
	char *path;

	path = amazonCredentials->generateUrl("wow.html", 0);
	fail_if(strcmp(path, "http://bucket.end-point/wow.html") != 0, NULL);

	path = amazonCredentials->generateUrl("wow.html", 1);
	fail_if(strcmp(path, "https://bucket.end-point/wow.html") != 0, NULL);

	path = amazonCredentials->generateUrl("/wow.html", 0);
	fail_if(strcmp(path, "http://bucket.end-point/wow.html") != 0, NULL);

	path = amazonCredentials->generateUrl("/", 0);
	fail_if(strcmp(path, "http://bucket.end-point/") != 0, NULL);

	path = amazonCredentials->generateUrl("", 0);
	fail_if(strcmp(path, "http://bucket.end-point/") != 0, NULL);

	path = amazonCredentials->generateUrl(NULL, 0);
	fail_if(path!=NULL, NULL);
} END_TEST


START_TEST(AmazonCredentials_createAuthorizationHeader) {
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

	TCase *tc = tcase_create("AmazonCredentials");
	tcase_add_checked_fixture (tc, AmazonCredentials_setup, AmazonCredentials_teardown);
	tcase_add_test(tc, AmazonCredentials_generateUrl);
	tcase_add_test(tc, AmazonCredentials_createAuthorizationHeader);
	suite_add_tcase(s, tc);

	return s;
}

