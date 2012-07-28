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

#include "amzHeaders.h"

START_TEST(AmzHeaders_mainTest) {
	AmzHeaders *amzHeaders = new AmzHeaders();

	char *serialized;

	serialized = amzHeaders->serializeIntoStringToSign();
	fail_unless(strcmp(serialized, "")==0);

	amzHeaders->add("Sirko", "vasya");

	serialized = amzHeaders->serializeIntoStringToSign();
	fail_unless(strcmp(serialized, "Sirko:vasya\n")==0);

	amzHeaders->add("Second", "%s==%d", "one", 1);

	serialized = amzHeaders->serializeIntoStringToSign();
	fail_unless(strcmp(serialized, "Second:one==1\nSirko:vasya\n")==0);

	delete amzHeaders;
} END_TEST

START_TEST(AmzHeaders_sortTest) {
	AmzHeaders *amzHeaders = new AmzHeaders();

	amzHeaders->add("Sirko", "vasya");
	amzHeaders->add("Albert", "%s", "Einstein");

	char *serialized = amzHeaders->serializeIntoStringToSign();
	fail_unless(strcmp(serialized, "Albert:Einstein\nSirko:vasya\n")==0);

	delete amzHeaders;
} END_TEST

Suite *AmzHeadersSuite(void) {
	Suite *s = suite_create("AmzHeaders");

	TCase *tc = tcase_create("Main");
	tcase_add_test(tc, AmzHeaders_mainTest);
	tcase_add_test(tc, AmzHeaders_sortTest);
	suite_add_tcase(s, tc);

	return s;
}
