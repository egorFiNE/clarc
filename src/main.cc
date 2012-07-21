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
#include <libxml/xmlmemory.h>
#include "help.h"
#include "logger.h"
}

#include "localFileList.h"
#include "fileListStorage.h"
#include "amazonCredentials.h"
#include "remoteListOfFiles.h"
#include "upload.h"
#include "filePattern.h"

static char *accessKeyId=NULL, *secretAccessKey=NULL, *bucket=NULL, *endPoint = (char *) "s3.amazonaws.com",
	*source=NULL, *databasePath=NULL, *databaseFilename= (char *)".files.sqlite3";
static int performRebuild=0, performUpload=0, makeAllPublic=0, useRrs=0, showProgress=0, skipSsl=0;

FilePattern *excludeFilePattern;

FILE *logStream;
int logLevel = LOG_ERR;


int rebuildDatabase(RemoteListOfFiles *remoteListOfFiles, AmazonCredentials *amazonCredentials, FileListStorage *fileListStorage) {
	int res = remoteListOfFiles->downloadList();
	if (res==LIST_FAILED) { 
		return LIST_FAILED;
	}

  LOG(LOG_INFO, "[MetaUpdate] Got %d files, updating meta information", remoteListOfFiles->count);

	res = remoteListOfFiles->resolveMtimes();
	if (res==LIST_FAILED) { 
		return LIST_FAILED;
	}

	if (fileListStorage->storeRemoteListOfFiles(remoteListOfFiles) == STORAGE_FAILED) {
    LOG(LOG_FATAL, "[MetaUpdate] Failed to store list of files");
		return LIST_FAILED;
	} else { 
		return LIST_SUCCESS;
	}
}

static char *endPoints[] = {
	(char *) "s3.amazonaws.com",
	(char *) "s3-us-west-2.amazonaws.com",
	(char *) "s3-us-west-1.amazonaws.com",
	(char *) "s3-eu-west-1.amazonaws.com",
	(char *) "s3-ap-southeast-1.amazonaws.com",
	(char *) "s3-ap-northeast-1.amazonaws.com",
	(char *) "s3-sa-east-1.amazonaws.com",

#ifdef TEST
	(char *) "test.dev",
#endif
	
	NULL
};

int validateEndpoint(char *endPoint) {
	int i=0;
	while (char *e=endPoints[i++]) {
		if (e && strcmp(e, endPoint)==0) {
			return 1;
		}
	}

 	printf("--endPoint %s not valid, use one of:\n", endPoint);

 	i=0;
	while (char *e=endPoints[i++]) {
		printf("\t%s\n", e);
	}

	return 0;
}

char *buildDatabaseFilePath(char *databaseFilename, char *databasePath) {
	uint64_t len = strlen(databasePath)+strlen(databaseFilename)+1;
	char *databaseFilePath = (char *) malloc(len);
	bzero(databaseFilePath, len);
	strcat(databaseFilePath, databasePath);
	strcat(databaseFilePath, "/");
	strcat(databaseFilePath, databaseFilename);
	return databaseFilePath;
}

void showVersion() {
	printf("clarc version " VERSION " (c) 2012 Egor Egorov <me@egorfine.com>  |  MIT License  |  http://egorfine.com/clarc/\n");
}

int parseCommandline(int argc, char *argv[]) {
	if (argc==1) {
		showVersion();
		printf("Perhaps, ask --help?\n");
		exit(0);	
	}

  static struct option longOpts[] = {
    { "accessKeyId",       required_argument,  NULL,  0 },
    { "secretAccessKey",   required_argument,  NULL,  0 },
    { "bucket",            required_argument,  NULL,  0 },
    { "endPoint",          required_argument,  NULL,  0 },
    { "public",            no_argument,        NULL,  0 },
    { "rrs",               no_argument,        NULL,  0 },
    { "rss",               no_argument,        NULL,  0 }, // common typo
    { "skipSsl",           no_argument,        NULL,  0 },

    { "source",            required_argument,  NULL,  0 },
    { "ddbPath",           required_argument,  NULL,  0 },
    { "dbFilename",        required_argument,  NULL,  0 },

    { "exclude",           required_argument,  NULL,  0 },
    { "excludeFromFile",   required_argument,  NULL,  0 },

    { "progress",          no_argument,        NULL,  0 },
    { "logLevel",          required_argument,  NULL,  0 },

    { "rebuild",           no_argument,        NULL,  0 },
    { "upload",            no_argument,        NULL,  0 },

    { "version",           no_argument,        NULL,  'V' },
    { "help",              no_argument,        NULL,  'h' },

    { NULL,                0,                  NULL,  0 }
  };

	int ch, longIndex;
  while ((ch = getopt_long(argc, argv, "Vh", longOpts, &longIndex)) != -1) {
  	if (ch=='V') {
  		showVersion();
  		exit(0);
  	} else if (ch=='h') {
  		showHelp();
  		exit(0);
  	}

  	if (ch!=0) {
  		continue;
  	}

  	const char *longName = longOpts[longIndex].name;

  	if (strcmp(longName, "accessKeyId")==0) {
  		accessKeyId = strdup(optarg);

  	} else if (strcmp(longName, "secretAccessKey")==0) {
  		secretAccessKey = strdup(optarg);

  	} else if (strcmp(longName, "bucket")==0) {
  		bucket = strdup(optarg);

  	} else if (strcmp(longName, "endPoint")==0) {
  		endPoint = strdup(optarg);

  	} else if (strcmp(longName, "public")==0) {
  		makeAllPublic = 1;

    } else if (strcmp(longName, "skipSsl")==0) {
      skipSsl = 1;

  	} else if (strcmp(longName, "rrs")==0) {
  		useRrs = 1;

  	} else if (strcmp(longName, "rss")==0) {
  		printf("Warning: you spelled --rss; you meant -rrs which stands for Reduced Redundancy Storage.\n");
  		useRrs = 1;

    } else if (strcmp(longName, "progress")==0) {
      showProgress = 1;

    } else if (strcmp(longName, "logLevel")==0) {
      logLevel = atoi(optarg);
      if (logLevel <= 0 || logLevel > 5) {
        printf("--logLevel must be 1..5\n");
        exit(1);
      }

  	} else if (strcmp(longName, "source")==0) {
  		source = strdup(optarg);

  	} else if (strcmp(longName, "dbPath")==0) {
  		databasePath = strdup(optarg);

  	} else if (strcmp(longName, "dbFilename")==0) {
  		databaseFilename = strdup(optarg);

  	} else if (strcmp(longName, "rebuild")==0) {
  		performRebuild=1;

  	} else if (strcmp(longName, "upload")==0) {
  		performUpload=1;

  	} else if (strcmp(longName, "exclude")==0) {
  		if (!excludeFilePattern->add(optarg)) {
  			printf("Pattern `%s' is not valid.\n", optarg);
  			exit(1);
  		}
  	} else if (strcmp(longName, "excludeFromFile")==0) {
  		if (!excludeFilePattern->readFile(optarg)) {
  			printf("Cannot read or parse patterns in %s\n", optarg);
  			exit(1);
  		}
  	}
  }

  int failed = 0;

  if (!source) {
  	printf("Specify --source.\n"); 
  	failed = 1;
  }

  if (source && source[0]!='/') {
  	printf("--source must be absolute path.\n");
		failed=1;
  }

  if (!accessKeyId) {
  	printf("Specify --accessKeyId.\n"); 
  	failed = 1;
  }

  if (!secretAccessKey) {
  	printf("Specify --secretAccessKey.\n"); 
  	failed = 1;
  }

  if (!bucket) {
  	printf("Specify --bucket.\n"); 
  	failed = 1;
  }

  if (!validateEndpoint(endPoint)) {
  	failed = 1;
  }

  if (!performRebuild && !performUpload) {
  	printf("What shall I do? Say --rebuild and/or --upload!\n");
  	failed = 1;
  }

  if (failed) {
  	return 0;
  }

  while (source[strlen(source)-1]=='/') {
  	source[strlen(source)-1]=0;
  }

  if (!databasePath) {
  	databasePath = source;
  } else { 
	  while (databasePath[strlen(databasePath)-1]=='/') {
	  	databasePath[strlen(databasePath)-1]=0;
	  }
	}

	return 1;
}

int verifySource(char *source) {
	struct stat fileInfo;
	if (lstat(source, &fileInfo)<0) {
		printf("%s doesn't exists.\n", source);
		return 0;
	}

	if (!(fileInfo.st_mode & S_IFDIR)) {
		printf("%s isn't a directory.\n", source);
		return 0;
	}

	return 1;
}

int main(int argc, char *argv[]) {
	int res;
  curl_global_init(CURL_GLOBAL_ALL);
  xmlInitParser();

  logStream = stdout;
  logLevel = LOG_DBG;

	excludeFilePattern = new FilePattern();

  if (!parseCommandline(argc, argv)) {
  	exit(1);
  }

  if (!verifySource(source)) {
  	exit(1);
  }

  AmazonCredentials *amazonCredentials = new AmazonCredentials(
  	accessKeyId, 
  	secretAccessKey,
  	bucket, endPoint
  );

	RemoteListOfFiles *remoteListOfFiles = new RemoteListOfFiles(amazonCredentials);
  remoteListOfFiles->showProgress = showProgress;
  if (skipSsl) {
    remoteListOfFiles->useSsl=0;
  }

	res = remoteListOfFiles->checkAuth();
	if (res == AUTH_FAILED_BUCKET_DOESNT_EXISTS) {
    LOG(LOG_FATAL, "[Auth] Failed: bucket doesn't exists, exit");
		exit(1);		
	} else if (res == AUTH_FAILED) {
		LOG(LOG_FATAL, "[Auth] FAIL, exit");
		exit(1);		
	}
  LOG(LOG_INFO, "[Auth] Success");

	char *databaseFilePath = buildDatabaseFilePath(databaseFilename, databasePath);
	LOG(LOG_DBG, "[Storage] Database path = %s", databaseFilePath);

	char errorResult[1024*100];
	FileListStorage *fileListStorage = new FileListStorage(databaseFilePath, errorResult);
	if (strlen(errorResult)>0) {
		LOG(LOG_FATAL, "[Storage] FAIL: %s, exit", errorResult);
		exit(1);
	}

	if (performRebuild) {
		if (rebuildDatabase(remoteListOfFiles, amazonCredentials, fileListStorage)==LIST_FAILED) {
			exit(1);
		}
	}

	if (performUpload) {
		if (excludeFilePattern->count==0) {
			delete excludeFilePattern;
			excludeFilePattern=NULL;
		}

		Uploader *uploader = new Uploader(amazonCredentials, excludeFilePattern);
		uploader->useRrs = useRrs;
		uploader->makeAllPublic = makeAllPublic;
    uploader->showProgress = showProgress;
    if (skipSsl) {
      uploader->useSsl=0;
    }

		res = uploader->uploadFiles(fileListStorage, source);

		if (res==UPLOAD_FAILED) {
			exit(1);
		}

		res = uploader->uploadDatabase(databaseFilePath, databaseFilename);
		if (res==UPLOAD_FAILED) {
			exit(1);
		}

		delete uploader;
	}

	free(databaseFilePath);
	delete fileListStorage;
	delete amazonCredentials;

	exit(0);
}
