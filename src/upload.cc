#include <iostream>
using namespace std;

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <errno.h>
#include <sqlite3.h>
#include <time.h>	
#include <unistd.h>

extern "C" {
#include "utils.h"
#include "base64.h"
#include <pwd.h>
#include <grp.h>
#include "logger.h"
#include "md5.h"

// this is declared explicitly instead of including unistd.h because some
// linux distributions have clang broken with it. Fortunately, sleep() is 
// the same on all sane OS.
unsigned int sleep(unsigned int seconds);
}

#include "localFileList.h"
#include "fileListStorage.h"
#include "upload.h"
#include "settings.h"
#include "amazonCredentials.h"

pthread_mutex_t uidMutex;

struct UploadProgress {
	char *path;
	double ullast;
	Uploader *uploader;
};

int progressFunction(struct UploadProgress *uploadProgress, double dltotal, double dlnow, double ultotal, double ulnow) {
	double uploadedBytes = ulnow - uploadProgress->ullast;
	if (uploadedBytes>0) {
		uploadProgress->ullast = ulnow;
		uploadProgress->uploader->progress(uploadProgress->path, uploadedBytes, ulnow, ultotal);
	}
	return 0;
}

void Uploader::progress(char *path, double uploadedBytes, double ulnow, double ultotal) {
	this->uploadedSize+=(uint64_t) uploadedBytes;
	if (this->showProgress) { 
		time_t timeDiff = time(NULL) - this->lastProgressUpdate;
		if (timeDiff>=1) {
			double percent = (double)this->uploadedSize / (double)this->totalSize;
			char *uploadedSizeHr = hrSize(this->uploadedSize);
			char *totalSizeHr = hrSize(this->totalSize);
			printf("\r[Upload] %.1f%% Done (%s out of %s)           \r", percent*100, uploadedSizeHr, totalSizeHr);
			fflush(stdout);
			free(uploadedSizeHr);
			free(totalSizeHr);
			this->lastProgressUpdate = time(NULL);
		}
	}
}

Uploader::Uploader(AmazonCredentials *amazonCredentials) {
	this->amazonCredentials = amazonCredentials;
	this->totalSize=0;
	this->uploadedSize=0;
	this->lastProgressUpdate = 0;
	this->useRrs = 0;
	this->makeAllPublic = 0;
	this->useSsl = 1;
	this->dryRun = 0;
	this->connectTimeout = CONNECT_TIMEOUT;
	this->networkTimeout = LOW_SPEED_TIME;
	this->uploadThreads = UPLOAD_THREADS;
	this->failed=0;
	this->showProgress=0;
	this->threads = NULL;
	this->destinationFolder = NULL;	

	pthread_mutex_init(&uidMutex, NULL);
}

Uploader::~Uploader() {
	this->amazonCredentials = NULL;

	pthread_mutex_destroy(&uidMutex);
}

void Uploader::extractMD5FromETagHeaders(char *md5HeaderValue, char *md5) {
	*md5=0;
	if (md5HeaderValue==NULL || strlen(md5HeaderValue)!=34) {
		return; 
	}

	if (*md5HeaderValue==0x22 && *(md5HeaderValue+33)==0x22) { // double quotes
		strncpy(md5, (md5HeaderValue+1), 32);
		*(md5+32)=0;
	}
}

void Uploader::addUidAndGidHeaders(uid_t uid, gid_t gid, MicroCurl *microCurl) {
	pthread_mutex_lock(&uidMutex);
	struct passwd *uidS = getpwuid(uid);
	struct group *gidS = getgrgid(gid);

	char *uidHeader;
	char *gidHeader;
	asprintf(&uidHeader, (char *) "%" PRIu64 " %s", (uint64_t) uid, uidS->pw_name);
	asprintf(&gidHeader, (char *) "%" PRIu64 " %s", (uint64_t) gid, gidS->gr_name);

	pthread_mutex_unlock(&uidMutex);

	microCurl->addHeader("x-amz-meta-uid", uidHeader);
	microCurl->addHeader("x-amz-meta-gid", gidHeader);

	free(gidHeader);
	free(uidHeader);	
}

CURLcode Uploader::uploadFile(
	char *localPath, 
	char *remotePath, 
	char *url,
	char *contentType, 
	struct stat *fileInfo, 	
	uint32_t *httpStatusCode, 
	char *md5,
	char *errorResult
) {
	int isSoftLink = (fileInfo->st_mode & S_IFLNK)==S_IFLNK ? 1 : 0;

	FILE *fin = NULL;

	if (!isSoftLink) {
		fin = fopen(localPath, "rb");
		if (!fin) {
			sprintf(errorResult, "Cannot open file %s: %s", localPath, strerror(errno));
			return UPLOAD_FILE_FUNCTION_FAILED;
		}
	}

	MicroCurl *microCurl = new MicroCurl(amazonCredentials);
	microCurl->method = METHOD_PUT;

	Uploader::addUidAndGidHeaders(fileInfo->st_uid, fileInfo->st_gid, microCurl);

	microCurl->addHeader((char *) "x-amz-acl", this->makeAllPublic ? (char *) "public-read" : (char *) "private");

	if (this->useRrs) {
		microCurl->addHeader((char *) "x-amz-storage-class",(char *) "REDUCED_REDUNDANCY");
	}

	microCurl->addHeaderFormat((char *) "x-amz-meta-mode", (char *) "%o", fileInfo->st_mode);
	microCurl->addHeaderFormat((char *) "x-amz-meta-mtime", (char *) "%llu", (uint64_t) fileInfo->st_mtime);

	char *softLinkData = NULL;
	if (isSoftLink) {
		softLinkData = (char *) malloc(1024);
		ssize_t linkLength = readlink(localPath, softLinkData, 1024);
		if (softLinkData < 0) {
			softLinkData = (char *) realloc(softLinkData, 1024*100);
			linkLength = readlink(localPath, softLinkData, 1024*100); // eat this
		}
		softLinkData[linkLength]=0;
		microCurl->addHeader((char *) "x-amz-meta-symlink", softLinkData);
		microCurl->addHeader((char *) "x-amz-meta-localpath", localPath);
	} 

	char *escapedRemotePath = microCurl->escapePath(remotePath);

	char *canonicalizedResource;
	asprintf(&canonicalizedResource, "/%s/%s", amazonCredentials->bucket, escapedRemotePath);
	microCurl->canonicalizedResource = canonicalizedResource; //will be freed in microcurl

	char *postUrl;
	if (url) {
		postUrl = strdup(url);
	} else { 
		postUrl = amazonCredentials->generateUrl(escapedRemotePath, this->useSsl); 
	}
	free(escapedRemotePath);

	microCurl->url = strdup(postUrl);
	free(postUrl);

	if (isSoftLink) {
		microCurl->postData = strdup(softLinkData);
		microCurl->postSize = (uint32_t) strlen(softLinkData);
	} else { 
		microCurl->fileIn = fin;
		microCurl->fileSize = fileInfo->st_size;
	}

	microCurl->addHeader("Content-type", contentType);
	microCurl->addHeader("Expect", "");

	microCurl->connectTimeout = this->connectTimeout;
	microCurl->maxConnects = MAXCONNECTS;
	microCurl->networkTimeout = this->networkTimeout;
	microCurl->lowSpeedLimit = LOW_SPEED_LIMIT;

	struct UploadProgress *uploadProgress = (struct UploadProgress *)malloc(sizeof(struct UploadProgress));
	uploadProgress->path = remotePath;
	uploadProgress->ullast = 0;
	uploadProgress->uploader = this;

	CURL *curl = microCurl->prepare();
	if (curl==NULL) {
		// we don't free things here because it's a fatal error and we'll exit soon anyway
		strcpy(errorResult, "Error in auth module");
		delete microCurl;
		return UPLOAD_FILE_FUNCTION_FAILED;
	}

	curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, &progressFunction);
	curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, (void *) uploadProgress);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);

	CURLcode res = microCurl->go();

	if (isSoftLink) {
		free(softLinkData);
	} else { 
		fclose(fin); 
	}

	free(uploadProgress);

	*md5=0;
	*httpStatusCode = microCurl->httpStatusCode;

	if (res==CURLE_OK) {
		this->progress(remotePath, 0, (double) fileInfo->st_size, (double) fileInfo->st_size);

		if (microCurl->httpStatusCode==307) {
			char *location = microCurl->getHeader("location");
			strcpy(errorResult, location);
			free(location);

		} else if (microCurl->httpStatusCode==200) { 
			char *etag = microCurl->getHeader("etag");
			Uploader::extractMD5FromETagHeaders(etag, md5); 
			free(etag);
		}

		delete microCurl;
		return CURLE_OK;

	} else { 
		strcpy(errorResult, microCurl->curlErrors);

		delete microCurl;
		return res;
	}
}

int Uploader::uploadFileWithRetry(
	char *localPath, 
	char *remotePath, 
	char *contentType, 
	struct stat *fileInfo,
	uint32_t *httpStatusCode, 
	char *md5,
	char *errorResult
) {
	int cUploads=0;
	char *url = NULL;
	do {
		errorResult[0]=0;

		LOG(LOG_DBG, "[Upload] Uploading %s -> %s", localPath, remotePath);

		CURLcode res=this->uploadFile(localPath, remotePath, url, contentType, fileInfo, httpStatusCode, md5, errorResult);
		
		if (url!=NULL) {
			free(url);
			url=NULL;
		}

		if (*httpStatusCode==307) {
			url = strdup(errorResult); 
			cUploads=0;
			LOG(LOG_INFO, "[Upload] Retrying %s: Amazon asked to reupload to: %s", remotePath, errorResult);
			continue;
		}

		if (res==CURLE_OK) {
			return UPLOAD_SUCCESS;
		}

		if (HTTP_SHOULD_RETRY_ON(res)) {
			LOG(LOG_WARN, "[Upload] %s failed (%s), retrying soon", remotePath, curl_easy_strerror(res));
			int sleepTime = cUploads*RETRY_SLEEP_TIME; 
			sleep(sleepTime);
		} else { 
			return UPLOAD_FAILED;
		}
		cUploads++;
	} while (cUploads<RETRY_FAIL_AFTER); 

	return UPLOAD_FAILED;
}

int Uploader::uploadFileWithRetryAndStore(
	FileListStorage *fileListStorage, 
	char *localPath, 
	char *remotePath, 
	char *contentType, 
	struct stat *fileInfo,
	char *errorResult
) {
	uint32_t httpStatus=0;
	char *remoteMd5 = (char *) malloc(330);
	remoteMd5[0]=0;

	int res = this->uploadFileWithRetry(localPath, remotePath, contentType, fileInfo, &httpStatus, remoteMd5, errorResult);

	if (res!=UPLOAD_SUCCESS) {
		free(remoteMd5);
		return UPLOAD_FAILED;
	}

	if (httpStatus==200) {
		int toReturn=0;
		if (strlen(remoteMd5)==32) {
			int sqlRes = fileListStorage->store(remotePath, remoteMd5, (uint32_t) fileInfo->st_mtime);
			if (sqlRes==STORAGE_SUCCESS) {
				toReturn = UPLOAD_SUCCESS;
			} else {
				sprintf(errorResult, "Oops, database error");
				toReturn = UPLOAD_FAILED;
			}
		} else { 
			sprintf(errorResult, "Amazon did not return MD5");
			toReturn = UPLOAD_FAILED;
		}
		
		free(remoteMd5);
		return toReturn;
		
	} else {
		sprintf(errorResult, "Amazon returned HTTP status %d", httpStatus);
		free(remoteMd5);
		return UPLOAD_FAILED;
	}
}

struct ThreadCommand {
	uint8_t threadNumber;
	Uploader *self;
	FileListStorage *fileListStorage;
	char *storedMd5;
	int shouldCheckMd5;
	char *realLocalPath;
	char *path;
	char *contentType;
	struct stat *fileInfo;
};

void *uploader_runOverThreadFunc(void *arg) {
	struct ThreadCommand *threadCommand = (struct ThreadCommand *)arg;

	threadCommand->self->runOverThread(
		threadCommand->threadNumber, 
		threadCommand->fileListStorage,
		threadCommand->realLocalPath,
		threadCommand->path,
		threadCommand->contentType,
		threadCommand->fileInfo,
		threadCommand->storedMd5, 
		threadCommand->shouldCheckMd5
	);

	threadCommand->self->threads->markFree(threadCommand->threadNumber);

	free(threadCommand->realLocalPath);
	free(threadCommand->fileInfo);
	free(threadCommand->storedMd5);

	free(arg); 

	pthread_exit(NULL);
}

char *Uploader::calculateFileMd5(char *localPath) {
	FILE *fin = fopen(localPath, "r");
	if (!fin) {
		return NULL;
	}

	md5_state_t state;
	md5_byte_t digest[16];
	md5_init(&state);

	char *buffer = (char *)malloc(1024*1024);
	size_t bytesRead;
	while ((bytesRead=fread(buffer, 1, 1024*1024, fin))) {
		md5_append(&state, (const md5_byte_t *)buffer, (int) bytesRead);
	}
	fclose(fin);
	free(buffer);

	md5_finish(&state, digest);

	char *md5_1;
	asprintf(&md5_1, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", 
		digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7], digest[8], digest[9], 
		digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]
	);

	return md5_1;
}

int Uploader::updateMeta(
	FileListStorage *fileListStorage,
	char *realLocalPath, 
	char *remotePath, 
	char *contentType, 
	struct stat *fileInfo,
	char *errorResult
) {
	MicroCurl *microCurl = new MicroCurl(amazonCredentials);
	microCurl->method = METHOD_PUT;

	microCurl->addHeader((char *) "x-amz-acl", this->makeAllPublic ? (char *) "public-read" : (char *) "private");

	if (this->useRrs) {
		microCurl->addHeader((char *) "x-amz-storage-class",(char *) "REDUCED_REDUNDANCY");
	}

	microCurl->addHeaderFormat((char *) "x-amz-meta-mode", (char *) "%o", fileInfo->st_mode);
	microCurl->addHeaderFormat((char *) "x-amz-meta-mtime", (char *) "%llu", (uint64_t) fileInfo->st_mtime);

	char *escapedRemotePath = microCurl->escapePath(remotePath);

	char *canonicalizedResource;
	asprintf(&canonicalizedResource, "/%s/%s", amazonCredentials->bucket, escapedRemotePath);
	microCurl->canonicalizedResource = canonicalizedResource; //will be freed in microcurl
	microCurl->addHeader((char*) "x-amz-copy-source", canonicalizedResource);

	microCurl->addHeader("x-amz-metadata-directive", "REPLACE");
	microCurl->addHeader("Expect", "");	
	microCurl->addHeader("Content-type", contentType);

	Uploader::addUidAndGidHeaders(fileInfo->st_uid, fileInfo->st_gid, microCurl);

	microCurl->url = amazonCredentials->generateUrl(escapedRemotePath, this->useSsl); 
	free(escapedRemotePath);

	microCurl->networkTimeout = this->networkTimeout;
	microCurl->connectTimeout = this->connectTimeout;
	microCurl->maxConnects = MAXCONNECTS;
	microCurl->lowSpeedLimit = LOW_SPEED_LIMIT;

	if (microCurl->prepare()==NULL) {
		strcpy(errorResult, "Error in auth module");
		delete microCurl;
		return 0;
	}

	CURLcode res = microCurl->go();

	if (res==CURLE_OK) {
		if (microCurl->httpStatusCode==307) {
			// FIXME
		}

		delete microCurl;
		return LIST_SUCCESS;

	} else { 
		strcpy(errorResult, "Error performing request");

		delete microCurl;
		return LIST_FAILED;
	}

	return 1;
}

void Uploader::runOverThread(
	uint8_t threadNumber, 
	FileListStorage *fileListStorage,
	char *realLocalPath, 
	char *path, 
	char *contentType, 
	struct stat *fileInfo,
	char *storedMd5,
	int shouldCheckMd5
) {
	int res;
	char errorResult[1024*20];
	errorResult[0]=0;

	int isSoftLink = (fileInfo->st_mode & S_IFLNK)==S_IFLNK ? 1 : 0;

	if (!isSoftLink && shouldCheckMd5) { // we never check contents of symlinks
		char *localMd5 = this->calculateFileMd5(realLocalPath);
		int isSame = strcmp(localMd5, storedMd5)==0 ? 1 : 0;
		free(localMd5);
		if (isSame) {
			LOG(LOG_INFO, "[Upload] %s: data not changed (md5=%s), updating meta", path, storedMd5);
			if (!this->dryRun) {
				res = this->updateMeta(fileListStorage, realLocalPath, path, contentType, fileInfo, errorResult);
				if (res) {
					int sqlRes = fileListStorage->store(path, storedMd5, (uint32_t) fileInfo->st_mtime);
					if (sqlRes!=STORAGE_SUCCESS) {
						LOG(LOG_FATAL, "[Upload] Database failure                       ");
						this->failed=1;
					}
				} else {
					LOG(LOG_FATAL, "[Upload] FAIL meta update %s: %s                  ", path, errorResult);
					this->failed=1;
				} 
			}
			return;
		}
	}

	if (this->dryRun) {
		LOG(LOG_DBG, "[Upload] [dry] %s -> %s", realLocalPath, path);
		return;
	}

	res = this->uploadFileWithRetryAndStore(
		fileListStorage, realLocalPath, path, contentType, 
		fileInfo, errorResult
	);

	if (res==UPLOAD_FAILED) {
		LOG(LOG_FATAL, "[Upload] FAIL %s: %s                  ", path, errorResult);
		this->failed=1;
	} else {
		LOG(LOG_INFO, "[Upload] Uploaded %s              ", path);
	}	
}

char *Uploader::createRealLocalPath(char *prefix, char *path) {
	uint64_t len = strlen(prefix) + strlen(path) + 2;
	char *realLocalPath = (char *) malloc(len);
	realLocalPath[0]=0;
	strcat(realLocalPath, prefix);
	strcat(realLocalPath, "/");
	strcat(realLocalPath, path);
	return realLocalPath;
}

void Uploader::logDebugMtime(char *path, uint32_t mtimeDb, uint32_t mtimeFs) {
	if (mtimeDb==0) {
		LOG(LOG_DBG, "[Upload] %s: no stored mtime", path);
	} else { 
		struct tm *timeinfo;
		time_t m;
		m = (time_t) mtimeDb;
		timeinfo = gmtime(&m);
		char *mtimeDbHr = (char *)malloc(128);
		mtimeDbHr[0]=0;
		strftime(mtimeDbHr, 128, "%F %T", timeinfo);

		m = (time_t) mtimeFs;
		timeinfo = gmtime(&m);
		char *mtimeFsHr = (char *)malloc(128);
		mtimeFsHr[0]=0;
		strftime(mtimeFsHr, 128, "%F %T", timeinfo);
		LOG(LOG_DBG, "[Upload] %s: stored mtime=%d (%s), filesystem mtime=%d (%s)", 
			path,
			mtimeDb, mtimeDbHr, mtimeFs, mtimeFsHr
		);
	}
}

int Uploader::uploadFiles(FileListStorage *fileListStorage, LocalFileList *files, char *prefix) {
	this->totalSize = files->calculateTotalSize();
	char *hrSizeString = hrSize(this->totalSize);
	LOG(LOG_INFO, "[Upload] Total size of files: %s", hrSizeString);
	free(hrSizeString);

	this->failed=0;

	this->threads = new Threads(this->uploadThreads);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	for (uint32_t i=0;i<files->count;i++) {
		if (this->failed) {
			break;
		}

		char *path = (files->paths[i]+1);

		char *remotePath;
		if (this->destinationFolder!=NULL && strlen(this->destinationFolder)>0) { 
			asprintf(&remotePath, "%s/%s", destinationFolder, path);
		} else { 
			remotePath = strdup(path); // not really elegant, I know
		}

		char *realLocalPath = Uploader::createRealLocalPath(prefix, path);
		
		struct stat *fileInfo = (struct stat *) malloc(sizeof(struct stat));

		if (lstat(realLocalPath, fileInfo)<0) {
			LOG(LOG_ERR, "[Upload] FAIL %s: Cannot stat file: %s", path, strerror(errno));
			free(realLocalPath);
			free(fileInfo);
			this->uploadedSize+=files->sizes[i];
			continue;
		}
				
		if (fileInfo->st_size>=MAX_S3_FILE_SIZE) {
			LOG(LOG_WARN, "[Upload] WARNING %s: File too large (%" PRIu64 " bytes while only %" PRIu64 " allowed), skipped", 
				path, (uint64_t) fileInfo->st_size, (uint64_t) MAX_S3_FILE_SIZE);
			free(realLocalPath);
			free(remotePath);
			free(fileInfo);
			this->uploadedSize+=files->sizes[i];
			continue;
		}
	
		char *md5 = (char *) malloc(33);
		uint64_t mtime=0;
		int res = fileListStorage->lookup(remotePath, md5, &mtime);
		if (res!=STORAGE_SUCCESS) {
			LOG(LOG_FATAL, "[Upload] FAIL %s: Oops, database query failed", remotePath);
			free(realLocalPath);
			free(remotePath);
			free(fileInfo);
			this->failed=1;
			continue;
		}

		if (mtime>0 && mtime == (uint64_t) fileInfo->st_mtime) {
			LOG(LOG_DBG, "[Upload] %s not changed", path);
			this->uploadedSize+=files->sizes[i];
			free(fileInfo);
			free(remotePath);
			free(realLocalPath);
			continue;
		} else { 
			this->logDebugMtime(path, (uint32_t) mtime, (uint32_t) fileInfo->st_mtime);
		}

		int threadNumber = threads->sleepTillThreadFree();
		threads->markBusy(threadNumber);

		struct ThreadCommand *threadCommand = (struct ThreadCommand *) malloc(sizeof(struct ThreadCommand));
		threadCommand->threadNumber = threadNumber;
		threadCommand->self = this;

		threadCommand->fileInfo = fileInfo; 
		threadCommand->contentType = guessContentType(path);
		threadCommand->path = remotePath;
		threadCommand->realLocalPath = realLocalPath;
		threadCommand->fileListStorage = fileListStorage;
		threadCommand->storedMd5 = md5; // free()ed in runOverThread 
		threadCommand->shouldCheckMd5 =  mtime > 0 ? 1 : 0; 

		pthread_t threadId;
		int rc = pthread_create(&threadId, &attr, uploader_runOverThreadFunc, (void *)threadCommand);
		threads->setThreadId(threadNumber, threadId);
		if (rc) {
			LOG(LOG_FATAL, "Return code from pthread_create() is %d, exit", rc);
			exit(-1);
		}
	}

	threads->sleepTillAllThreadsFree();
	delete this->threads;

	pthread_attr_destroy(&attr);

	LOG(LOG_INFO, "[Upload] Finished %s                     ", this->failed ? "with errors" : "successfully");

	return this->failed ? UPLOAD_FAILED : UPLOAD_SUCCESS;
}

int Uploader::uploadDatabase(char *databasePath, char *databaseFilename) { 
	struct stat fileInfo;
	if (stat(databasePath, &fileInfo)<0) {
		LOG(LOG_FATAL, "[Upload] Oops database upload failed: file %s doesn't exists", databaseFilename);
		return UPLOAD_FAILED;
	}

	this->showProgress = 0;
	
	char md5[33] = "";
	uint32_t httpStatusCode=0;

	char *remoteDatabasePath;

	if (this->destinationFolder!=NULL) {
		asprintf(&remoteDatabasePath, "%s/%s", this->destinationFolder, databaseFilename);
	} else { 
		remoteDatabasePath = strdup(databaseFilename);
	}

	char errorResult[1024*100];
	int res = this->uploadFileWithRetry(databasePath, remoteDatabasePath,
		(char *)"application/octet-stream", &fileInfo, &httpStatusCode, md5, errorResult);

	if (res==UPLOAD_SUCCESS && httpStatusCode!=200) {
		LOG(LOG_ERR, "[Upload] Database upload failed with HTTP status %d", httpStatusCode);

	} if (res==UPLOAD_SUCCESS && httpStatusCode==200) {
		LOG(LOG_INFO, "[Upload] Database uploaded");

	} else { 
		LOG(LOG_ERR, "[Upload] Database upload failed: %s", errorResult);
	}

	return res;
}
