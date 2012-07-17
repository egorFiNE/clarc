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

AmzHeaders *amzHeaders;

void AmzHeaders_setup(void) {
	amzHeaders = new AmzHeaders();
}

void AmzHeaders_teardown(void) {
	delete amzHeaders;
}

START_TEST(AmzHeaders_mainTest) {
	char *serialized;

	serialized = amzHeaders->serializeIntoStringToSign();
	fail_unless(strcmp(serialized, "")==0);

	amzHeaders->add("Sirko", "vasya");

	serialized = amzHeaders->serializeIntoStringToSign();
	fail_unless(strcmp(serialized, "Sirko:vasya\n")==0);

	amzHeaders->add("Second", "%s==%d", "one", 1);

	serialized = amzHeaders->serializeIntoStringToSign();
	fail_unless(strcmp(serialized, "Sirko:vasya\nSecond:one==1\n")==0);
} END_TEST

Suite *AmzHeadersSuite(void) {
	Suite *s = suite_create("AmzHeaders");

	TCase *tc = tcase_create("Main");
	tcase_add_checked_fixture (tc, AmzHeaders_setup, AmzHeaders_teardown);
	tcase_add_test(tc, AmzHeaders_mainTest);
	suite_add_tcase(s, tc);

	return s;
}
