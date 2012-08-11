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


START_TEST(Utils_guessContentType) {
	char *ct = guessContentType("vasya.jpg");
	fail_unless(strcmp(ct, "image/jpeg")==0);
} END_TEST

Suite *UtilsSuite(void) {
	Suite *s = suite_create("Utils");

	TCase *tc1 = tcase_create("main");
	tcase_add_test(tc1, Utils_guessContentType);
	suite_add_tcase(s, tc1);

	return s;
}

