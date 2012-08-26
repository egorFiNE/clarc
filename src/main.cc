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
#include "sslThreadLocks.h"
}

#include "localFileList.h"
#include "fileListStorage.h"
#include "amazonCredentials.h"
#include "remoteListOfFiles.h"
#include "upload.h"
#include "filePattern.h"
#include "destination.h"
#include "deleter.h"

static char *accessKeyId=NULL, *secretAccessKey=NULL,
	*databasePath=NULL, *databaseFilename= (char *)".clarc.sqlite3",
	*source = NULL, *autoCreateBucketRegion = NULL;
static int performRebuild=0, performUpload=1, performDelete=0, makeAllPublic=0, useRrs=0, showProgress=0, skipSsl=0, dryRun=0,
	connectTimeout = 0, networkTimeout = 0, uploadThreads = 0, autoCreateBucket = 0;

FilePattern *excludeFilePattern;
Destination *destination;

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

	if (dryRun) {
		LOG(LOG_INFO, "[MetaUpdate] [dry] Skipped storing list of files");
		return LIST_SUCCESS;
	}

	if (fileListStorage->storeRemoteListOfFiles(remoteListOfFiles) == STORAGE_FAILED) {
		LOG(LOG_FATAL, "[MetaUpdate] Failed to store list of files");
		return LIST_FAILED;
	} else { 
		return LIST_SUCCESS;
	}
}

char *buildDatabaseFilePath(char *databaseFilename, char *databasePath) {
	uint64_t len = strlen(databasePath)+strlen(databaseFilename)+2;
	char *databaseFilePath = (char *) malloc(len);
	databaseFilePath[0]=0;
	strcat(databaseFilePath, databasePath);
	strcat(databaseFilePath, "/");
	strcat(databaseFilePath, databaseFilename);
	return databaseFilePath;
}

void showVersion() {
	printf("clarc version " VERSION " (c) 2012 Egor Egorov <me@egorfine.com>\nMIT License  |  http://egorfine.com/clarc/\n");
}

void clearString(char *str) {
	if (str==NULL) {
		return;
	}

	uint32_t i;
	for(i=0;i<strlen(str);i++) {
		str[i]='X';
	}
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
		{ "public",            no_argument,        NULL,  0 },
		{ "rrs",               no_argument,        NULL,  0 },
		{ "rss",               no_argument,        NULL,  0 }, // common typo
		{ "skipSsl",           no_argument,        NULL,  0 },
		{ "create",            required_argument,  NULL,  0 },

		{ "connectTimeout",    required_argument,  NULL,  0 },
		{ "networkTimeout",    required_argument,  NULL,  0 },
		{ "uploadThreads",     required_argument,  NULL,  0 },

		{ "dbPath",            required_argument,  NULL,  0 },
		{ "dbFilename",        required_argument,  NULL,  0 },

		{ "exclude",           required_argument,  NULL,  0 },
		{ "excludeFromFile",   required_argument,  NULL,  0 },

		{ "progress",          no_argument,        NULL,  0 },
		{ "logLevel",          required_argument,  NULL,  0 },

		{ "dryRun",            no_argument,        NULL,  0 },

		{ "delete",            no_argument,        NULL,  0 },
		{ "rebuild",           no_argument,        NULL,  0 },
		{ "rebuildOnly",       no_argument,        NULL,  0 },

		{ "version",           no_argument,        NULL,  'v' },
		{ "help",              no_argument,        NULL,  'h' },

		{ NULL,                0,                  NULL,  0 }
	};

	int ch, longIndex;
	while ((ch = getopt_long(argc, argv, "vh", longOpts, &longIndex)) != -1) {
		if (ch=='v') {
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
			clearString(optarg); // hide credentials from process list

		} else if (strcmp(longName, "secretAccessKey")==0) {
			secretAccessKey = strdup(optarg);
			clearString(optarg); // hide credentials from process list

		} else if (strcmp(longName, "create")==0) {
			autoCreateBucket = 1;
			autoCreateBucketRegion = strdup(optarg);

			if (!AmazonCredentials::isValidRegionForBucketCreate(autoCreateBucketRegion)) {
				printf("Region \"%s\" is not valid.\n", autoCreateBucketRegion);
				exit(1);
			}

		} else if (strcmp(longName, "networkTimeout")==0) {
			networkTimeout = atoi(optarg);
			if (networkTimeout<=0 || networkTimeout>=600) {
				printf("Network timeout invalid (must be 1..600 seconds)\n");
				exit(1);
			}

		} else if (strcmp(longName, "uploadThreads")==0) {
			uploadThreads = atoi(optarg);
			if (uploadThreads<=0 || uploadThreads>=100) {
				printf("Upload threads count invalid (must be 1..100)\n");
				exit(1);
			}

		} else if (strcmp(longName, "connectTimeout")==0) {
			connectTimeout = atoi(optarg);
			if (connectTimeout<=0 || connectTimeout>=600) {
				printf("Connect timeout invalid (must be 1..600 seconds)\n");
				exit(1);
			}

		} else if (strcmp(longName, "public")==0) {
			makeAllPublic = 1;

		} else if (strcmp(longName, "skipSsl")==0) {
			skipSsl = 1;

		} else if (strcmp(longName, "delete")==0) {
			performDelete = 1;

		} else if (strcmp(longName, "rrs")==0) {
			useRrs = 1;

		} else if (strcmp(longName, "rss")==0) {
			printf("Warning: you spelled --rss; you meant -rrs which stands for Reduced Redundancy Storage.\n");
			useRrs = 1;

		} else if (strcmp(longName, "dryRun")==0) {
			dryRun = 1;

		} else if (strcmp(longName, "progress")==0) {
			showProgress = 1;

		} else if (strcmp(longName, "logLevel")==0) {
			logLevel = atoi(optarg);
			if (logLevel <= 0 || logLevel > 5) {
				printf("--logLevel must be 1..5\n");
				exit(1);
			}

		} else if (strcmp(longName, "dbPath")==0) {
			databasePath = strdup(optarg);

		} else if (strcmp(longName, "dbFilename")==0) {
			databaseFilename = strdup(optarg);

		} else if (strcmp(longName, "rebuild")==0) {
			performRebuild=1;

		} else if (strcmp(longName, "rebuildOnly")==0) {
			performRebuild=1;
			performUpload=0;

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

	argc -= optind;
	argv += optind;

	if (argc!=2) {
		printf("Specify source and destination.\n");
		exit(1);
	}

	source = argv[0];
	char *destinationString = argv[1];

	int failed = 0;

	if (source[0]!='/') {
		source = realpath(source, NULL);
		if (source==NULL) {
			printf("Source folder is invalid.\n");
			failed=1;
		}
	}

	destination = new Destination(destinationString);
	if (!destination->isValid()) {
		printf("Destination is invalid.\n");
		delete destination;
		failed = 1;
	}

	if (accessKeyId==NULL) {
		char *candidate = getenv("S3_ACCESSKEYID");
		if (candidate!=NULL) {
			accessKeyId=strdup(candidate);
		}
	} 

	if (!accessKeyId) {
		printf("Specify --accessKeyId argument or S3_ACCESSKEYID environment variable\n"); 
		failed = 1;
	}

	if (secretAccessKey==NULL) {
		char *candidate = getenv("S3_SECRETACCESSKEY");
		if (candidate!=NULL) {
			secretAccessKey=strdup(candidate);
		}
	} 

	if (!secretAccessKey) {
		printf("Specify --secretAccessKey argument or S3_SECRETACCESSKEY environment variable\n"); 
		failed = 1;
	}

	if (performDelete && !performUpload) {
		printf("--delete cannot be specified with --rebuildOnly.\n");
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
	sslInitLocks();

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
		destination->bucket, destination->endPoint
	);

	char *absoluteString = destination->absoluteString();
	LOG(LOG_INFO, "[Main] %s -> %s", source, absoluteString);
	free(absoluteString);

	// explicitly commented out;  amazonCredentials must strdup!!
	// delete destination;

	RemoteListOfFiles *remoteListOfFiles = new RemoteListOfFiles(amazonCredentials);
	remoteListOfFiles->showProgress = showProgress;
	if (skipSsl) {
		remoteListOfFiles->useSsl=0;
	}
	if (networkTimeout) {
		remoteListOfFiles->networkTimeout = networkTimeout;
	}
	if (connectTimeout) {
		remoteListOfFiles->connectTimeout = connectTimeout;
	}

	if (dryRun) {
		LOG(LOG_INFO, "[Auth] [dry] Success");
	} else { 
		res = remoteListOfFiles->checkAuth();
		if (res == AUTH_FAILED_BUCKET_DOESNT_EXISTS) {
			if (autoCreateBucket) { 
				res = remoteListOfFiles->createBucket(autoCreateBucketRegion);
				if (res==CREATE_SUCCESS) {
					LOG(LOG_INFO, "[BucketCreate] Created bucket %s in region %s", amazonCredentials->bucket, autoCreateBucketRegion);
				} else { 
					exit(1);
				}
			} else { 
				LOG(LOG_FATAL, "[Auth] Failed: bucket doesn't exists and no --create supplied");
				exit(1);		
			}

		} else if (res == AUTH_FAILED) {
			LOG(LOG_FATAL, "[Auth] FAIL, exit");
			exit(1);		
		}
		LOG(LOG_INFO, "[Auth] Success");
	}
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
		excludeFilePattern->addDatabase(databaseFilename);

		LocalFileList *localFileList = new LocalFileList(excludeFilePattern);
		localFileList->recurseIn((char *) "", source);

		Uploader *uploader = new Uploader(amazonCredentials);
		uploader->useRrs = useRrs;
		uploader->makeAllPublic = makeAllPublic;
		uploader->showProgress = showProgress;
		uploader->destinationFolder = destination->folder;
		if (skipSsl) {
			uploader->useSsl=0;
		}
		if (networkTimeout) {
			uploader->networkTimeout = networkTimeout;
		}
		if (connectTimeout) {
			uploader->connectTimeout = connectTimeout;
		}
		if (uploadThreads) {
			uploader->uploadThreads = uploadThreads;
		}
		uploader->dryRun = dryRun;

#ifdef TEST
		do {
#endif

		res = uploader->uploadFiles(fileListStorage, localFileList, source);
		if (res==UPLOAD_SUCCESS) {
			if (performDelete) {
				LocalFileList *filesToDelete = fileListStorage->calculateListOfFilesToDelete(localFileList);
				
				Deleter *deleter = new Deleter(amazonCredentials, filesToDelete, fileListStorage, databaseFilename);
				deleter->dryRun = dryRun;
				if (skipSsl) {
					deleter->useSsl=0;
				}
				if (networkTimeout) {
					deleter->networkTimeout = networkTimeout;
				}
				if (connectTimeout) {
					deleter->connectTimeout = connectTimeout;
				}

				res = deleter->performDeletion();
				delete deleter;
				delete filesToDelete;
			}

			if (!dryRun) {
				res = uploader->uploadDatabase(databaseFilePath, databaseFilename);
				if (res==UPLOAD_SUCCESS) {
				}
			}
		}

#ifdef TEST
		fileListStorage->truncate();
		} while (true);
#endif

		delete localFileList;
		delete uploader;
	}

	free(databaseFilePath);

	delete fileListStorage;
	delete amazonCredentials;

	exit(0);
}
