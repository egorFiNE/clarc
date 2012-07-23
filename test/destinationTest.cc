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

#include "destination.h"

#define PRINT_DEBUG printf("bucket=%s, endPoint=%s, folder=%s, isValid=%d\n", destination->bucket, destination->endPoint, destination->folder, destination->isValid())

START_TEST(Destination_mainTest) {
	Destination *destination;

	// folder
	destination = new Destination("s3://sirko/folder");
	fail_unless(destination->isValid());
	fail_unless(strcmp(destination->folder, "folder")==0);
	fail_unless(strcmp(destination->bucket, "sirko")==0);
	fail_unless(strcmp(destination->endPoint, "s3.amazonaws.com")==0);
	fail_unless(strcmp(destination->absoluteString(), "s3://sirko.s3.amazonaws.com/folder/")==0);
	delete destination;

	// folder and endPoint
	destination = new Destination("s3://sirko.s3.amazonaws.com/folder");
	fail_unless(destination->isValid());
	fail_unless(strcmp(destination->folder, "folder")==0);
	fail_unless(strcmp(destination->bucket, "sirko")==0);
	fail_unless(strcmp(destination->endPoint, "s3.amazonaws.com")==0);
	fail_unless(strcmp(destination->absoluteString(), "s3://sirko.s3.amazonaws.com/folder/")==0);
	delete destination;

	// complex bucket name and folder
	destination = new Destination("s3://sirko.vasya.com/folder");
	fail_unless(destination->isValid());
	fail_unless(strcmp(destination->folder, "folder")==0);
	fail_unless(strcmp(destination->bucket, "sirko.vasya.com")==0);
	fail_unless(strcmp(destination->endPoint, "s3.amazonaws.com")==0);
	fail_unless(strcmp(destination->absoluteString(), "s3://sirko.vasya.com.s3.amazonaws.com/folder/")==0);
	delete destination;

	// complex bucket name ending with slash
	destination = new Destination("s3://sirko.vasya.com/");
	fail_unless(destination->isValid());
	fail_unless(strcmp(destination->folder, "")==0);
	fail_unless(strcmp(destination->bucket, "sirko.vasya.com")==0);
	fail_unless(strcmp(destination->endPoint, "s3.amazonaws.com")==0);
	fail_unless(strcmp(destination->absoluteString(), "s3://sirko.vasya.com.s3.amazonaws.com/")==0);
	delete destination;

	// complex bucket name not ending with slash
	destination = new Destination("s3://sirko.vasya.com");
	fail_unless(destination->isValid());
	fail_unless(strcmp(destination->folder, "")==0);
	fail_unless(strcmp(destination->bucket, "sirko.vasya.com")==0);
	fail_unless(strcmp(destination->endPoint, "s3.amazonaws.com")==0);
	fail_unless(strcmp(destination->absoluteString(), "s3://sirko.vasya.com.s3.amazonaws.com/")==0);
	delete destination;

	// complex bucket name and non-default endpoint with no slash
	destination = new Destination("s3://sirko.vasya.com.s3-ap-southeast-1.amazonaws.com");
	fail_unless(destination->isValid());
	fail_unless(strcmp(destination->folder, "")==0);
	fail_unless(strcmp(destination->bucket, "sirko.vasya.com")==0);
	fail_unless(strcmp(destination->endPoint, "s3-ap-southeast-1.amazonaws.com")==0);
	fail_unless(strcmp(destination->absoluteString(), "s3://sirko.vasya.com.s3-ap-southeast-1.amazonaws.com/")==0);
	delete destination;

	// complex bucket name and non-default endpoint with slash
	destination = new Destination("s3://sirko.vasya.com.s3-ap-southeast-1.amazonaws.com/");
	fail_unless(destination->isValid());
	fail_unless(strcmp(destination->folder, "")==0);
	fail_unless(strcmp(destination->bucket, "sirko.vasya.com")==0);
	fail_unless(strcmp(destination->endPoint, "s3-ap-southeast-1.amazonaws.com")==0);
	fail_unless(strcmp(destination->absoluteString(), "s3://sirko.vasya.com.s3-ap-southeast-1.amazonaws.com/")==0);
	delete destination;

	// complex bucket name and non-default endpoint with slash and folder
	destination = new Destination("s3://sirko.vasya.com.s3-ap-southeast-1.amazonaws.com/folder");
	fail_unless(destination->isValid());
	fail_unless(strcmp(destination->folder, "folder")==0);
	fail_unless(strcmp(destination->bucket, "sirko.vasya.com")==0);
	fail_unless(strcmp(destination->endPoint, "s3-ap-southeast-1.amazonaws.com")==0);
	fail_unless(strcmp(destination->absoluteString(), "s3://sirko.vasya.com.s3-ap-southeast-1.amazonaws.com/folder/")==0);
	delete destination;

	// complex bucket name and non-default endpoint with slash and folder ending with slash
	destination = new Destination("s3://sirko.vasya.com.s3-ap-southeast-1.amazonaws.com/folder/");
	fail_unless(destination->isValid());
	fail_unless(strcmp(destination->folder, "folder")==0);
	fail_unless(strcmp(destination->bucket, "sirko.vasya.com")==0);
	fail_unless(strcmp(destination->endPoint, "s3-ap-southeast-1.amazonaws.com")==0);
	fail_unless(strcmp(destination->absoluteString(), "s3://sirko.vasya.com.s3-ap-southeast-1.amazonaws.com/folder/")==0);
	delete destination;

	// complex bucket name and non-default endpoint with subfolder
	destination = new Destination("s3://sirko.vasya.com.s3-ap-southeast-1.amazonaws.com/folder/subfolder");
	fail_if(destination->isValid());
	delete destination;
} END_TEST

START_TEST(Destination_invalid) {
	Destination *destination;

	destination = new Destination(NULL);
	fail_if(destination->isValid());
	delete destination;

	destination = new Destination("");
	fail_if(destination->isValid());
	delete destination;

	destination = new Destination("sirko");
	fail_if(destination->isValid());
	delete destination;

	destination = new Destination("s3://");
	fail_if(destination->isValid());
	delete destination;

	destination = new Destination("s3:///");
	fail_if(destination->isValid());
	delete destination;
} END_TEST

Suite *DestinationSuite(void) {
	Suite *s = suite_create("Destination");

	TCase *tc = tcase_create("Main");
	tcase_add_test(tc, Destination_mainTest);
	tcase_add_test(tc, Destination_invalid);
	suite_add_tcase(s, tc);

	return s;
}
