#include <iostream>
using namespace std;

extern "C" {
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <check.h>
#include "test.h"
#include "utils.h"
}


START_TEST(Utils_extractLocationFromHeaders) {
	char locationResult[1024]="";

	extractLocationFromHeaders(NULL, locationResult);
	fail_unless(strlen(locationResult)==0);

	extractLocationFromHeaders("", locationResult);
	fail_unless(strlen(locationResult)==0);

	extractLocationFromHeaders("Location: ", locationResult);
	fail_unless(strlen(locationResult)==0);

	extractLocationFromHeaders("dfsfsd", locationResult);
	fail_unless(strlen(locationResult)==0);

	extractLocationFromHeaders("Location: \nsdf", locationResult);
	fail_unless(strlen(locationResult)==0);

	extractLocationFromHeaders("Location: s", locationResult);
	fail_unless(strcmp(locationResult, "s")==0);

	extractLocationFromHeaders("Location: sirko", locationResult);
	fail_unless(strcmp(locationResult, "sirko")==0);

	extractLocationFromHeaders("vasya\nLocation: sirko", locationResult);
	fail_unless(strcmp(locationResult, "sirko")==0);

	extractLocationFromHeaders("vasya\nLocation: sirko\nsdfsdfsdf\n", locationResult);
	fail_unless(strcmp(locationResult, "sirko")==0);

	extractLocationFromHeaders("vasya\r\nLocation: sirko\r\nsdfsdfsdf\r\n", locationResult);
	fail_unless(strcmp(locationResult, "sirko")==0);
} END_TEST

Suite *UtilsSuite(void) {
	Suite *s = suite_create("Utils");

	TCase *tc1 = tcase_create("main");
	tcase_add_test(tc1, Utils_extractLocationFromHeaders);
	suite_add_tcase(s, tc1);

	return s;
}

