#include <iostream>
using namespace std;

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <curl/curl.h>
#include <errno.h>
#include <sqlite3.h>
#include <dispatch/dispatch.h>

extern "C" {
#include "utils.h"
#include "base64.h"
#include "curlResponse.h"
#include <pwd.h>
#include <grp.h>
}

#include "localFileList.h"
#include "fileListStorage.h"
#include "upload.h"
#include "settings.h"
#include "amazonCredentials.h"
#include "amzHeaders.h"

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
	this->showProgress=1;
	this->lastProgressUpdate = 0;
	this->useRrs = 0;
	this->makeAllPublic = 0;

	this->systemQueryQueue = dispatch_queue_create("com.egorfine.systemquery", NULL);
}

Uploader::~Uploader() {
	this->amazonCredentials = NULL;
}

void Uploader::extractMD5FromETagHeaders(char *headers, char *md5) {
	*md5=0;
	char *etag = strstr(headers, "ETag: ");
	if (etag) {
		strncpy(md5, (etag+7), 32);
		*(md5+32)=0;
	}
}

CURLcode Uploader::uploadFile(
	char *localPath, 
	char *remotePath, 
	char *contentType, 
	struct stat *fileInfo, 	
	uint32_t *httpStatusCode, 
	char *md5,
	char *errorResult
) {
	char method[4] = "PUT";

	FILE *fin = fopen(localPath, "rb");  
	if (!fin) {
		sprintf(errorResult, "Cannot open file %s: %s", localPath, strerror(errno));
		return UPLOAD_FILE_FUNCTION_FAILED;
	}

	char *date = getIsoDate(); 

  CURL *curl = curl_easy_init();
	char *escapedRemotePath=curl_easy_escape(curl, remotePath, 0);

	char *canonicalizedResource=(char *)malloc(strlen(amazonCredentials->bucket) + strlen(escapedRemotePath) + 4);
	sprintf(canonicalizedResource, "/%s/%s", amazonCredentials->bucket, escapedRemotePath);

	__block char *gidHeader = (char *) malloc(1024);
	__block char *uidHeader = (char *) malloc(1024);

	// I believe this is only needed for Linux, as I have seen it failing when called in multiple threads.
	// -- EE
  dispatch_sync(this->systemQueryQueue, ^{
		struct passwd *uid = getpwuid(fileInfo->st_uid);
		struct group *gid = getgrgid(fileInfo->st_gid);
		sprintf(gidHeader, (char *) "%" PRIu64 " %s", (uint64_t) fileInfo->st_gid, gid->gr_name);
  	sprintf(uidHeader, (char *) "%" PRIu64 " %s", (uint64_t) fileInfo->st_uid, uid->pw_name);
	});

	AmzHeaders *amzHeaders = new AmzHeaders();
	if (this->makeAllPublic) {
		amzHeaders->add((char *) "x-amz-acl", (char *) "public-read");
	} else { 
		amzHeaders->add((char *) "x-amz-acl", (char *) "private");
	}
	amzHeaders->add((char *) "x-amz-meta-gid", gidHeader);
	amzHeaders->add((char *) "x-amz-meta-mode", (char *) "%o", fileInfo->st_mode);
	amzHeaders->add((char *) "x-amz-meta-mtime", (char *) "%llu", (uint64_t) fileInfo->st_mtime);
	amzHeaders->add((char *) "x-amz-meta-uid", uidHeader);

	if (this->useRrs) {
		amzHeaders->add((char *) "x-amz-storage-class",(char *) "REDUCED_REDUNDANCY");
	}

	free(gidHeader);
	free(uidHeader);

	char *amzHeadersToSign = amzHeaders->serializeIntoStringToSign();

	char *stringToSign = (char *)malloc(strlen(canonicalizedResource) + strlen(amzHeadersToSign) + 1024); // 1k is enough to hold other headers
	sprintf(stringToSign, "%s\n%s\n%s\n%s\n", method, "", contentType, date);
	strcat(stringToSign, amzHeadersToSign);
	strcat(stringToSign, canonicalizedResource);

	char *authorization = amazonCredentials->createAuthorizationHeader(stringToSign); 

	free(stringToSign);
	free(amzHeadersToSign);
	free(canonicalizedResource);

	if (authorization==NULL) {
		free(date);
	  curl_easy_cleanup(curl);
		free(escapedRemotePath);
		strcpy(errorResult, "Error in auth.");
		return UPLOAD_FILE_FUNCTION_FAILED;
	}

  char *url = amazonCredentials->generateUrl(escapedRemotePath); 
	free(escapedRemotePath);

  struct CurlResponse curlResponse;
  CurlResponseInit(&curlResponse);

  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
  curl_easy_setopt(curl, CURLOPT_PUT, 1L);
  //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

  curl_easy_setopt(curl, CURLOPT_URL, url);
	free(url);

  curl_easy_setopt(curl, CURLOPT_READDATA, fin);

  struct curl_slist *slist = NULL;

	char dateHeader[128];
	sprintf(dateHeader, "Date: %s", date);
  slist = curl_slist_append(slist, dateHeader);
	free(date);

  char authorizationHeader[128];
  sprintf(authorizationHeader, "Authorization: %s", authorization);
  slist = curl_slist_append(slist, authorizationHeader);
  free(authorization);

  char contentTypeHeader[128];
  sprintf(contentTypeHeader, "Content-Type: %s", contentType);
  slist = curl_slist_append(slist, contentTypeHeader);

  slist = curl_slist_append(slist, "Expect:");

  slist = amzHeaders->serializeIntoCurl(slist);
  delete amzHeaders;

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT);
	curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, MAXCONNECTS);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, LOW_SPEED_TIME);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, LOW_SPEED_LIMIT);

  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&curlResponse);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlResponseBodyCallback);

	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &CurlResponseHeadersCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, (void *)&curlResponse); 

	struct UploadProgress *uploadProgress = (struct UploadProgress *)malloc(sizeof(struct UploadProgress));
	uploadProgress->path = remotePath;
	uploadProgress->ullast  = 0;
	uploadProgress->uploader = this;
	curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, &progressFunction);
	curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, (void *) uploadProgress);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);

  curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t) fileInfo->st_size);

  char *curlErrors = (char *) malloc(CURL_ERROR_SIZE);
  curlErrors[0]=0;
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlErrors);

	CURLcode res = curl_easy_perform(curl);

  fclose(fin); 

  curl_slist_free_all(slist);
	free(uploadProgress);

  *md5=0;
  *httpStatusCode = 0;

	if (res==CURLE_OK) {
		this->progress(remotePath, 0, (double) fileInfo->st_size, (double) fileInfo->st_size);

		long _httpStatus = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &_httpStatus);
	  *httpStatusCode = (uint32_t) _httpStatus;

	  this->extractMD5FromETagHeaders(curlResponse.headers, md5);

	  curl_easy_cleanup(curl);
	  free(curlErrors);
	  CurlResponseFree(&curlResponse);

	  return CURLE_OK;

	} else { 
		strcpy(errorResult, curlErrors);

	  curl_easy_cleanup(curl);
	  free(curlErrors);
	  CurlResponseFree(&curlResponse);

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
	do {
		errorResult[0]=0;
		CURLcode res=this->uploadFile(localPath, remotePath, contentType, fileInfo, httpStatusCode, md5, errorResult);
		if (res==CURLE_OK) {
			return UPLOAD_SUCCESS;
		}

		if (HTTP_SHOULD_RETRY_ON(res)) {
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
	dispatch_queue_t sqlQueue, 
	char *errorResult
) {
	uint32_t httpStatus=0;
	char *remoteMd5 = (char *) malloc(33);
	remoteMd5[0]=0;

	int res = this->uploadFileWithRetry(localPath, remotePath, contentType, fileInfo, &httpStatus, remoteMd5, errorResult);

	if (res!=UPLOAD_SUCCESS) {
		free(remoteMd5);
		return UPLOAD_FAILED;
	}

	if (httpStatus==200) {
		__block int toReturn=0;

    dispatch_sync(sqlQueue, ^{
			int sqlRes = fileListStorage->store(remotePath, remoteMd5, (uint32_t) fileInfo->st_mtime);
			if (sqlRes==STORAGE_SUCCESS) {
//		printf("\n[Upload] %s: Uploaded successfully, md5=%s\n", remotePath, remoteMd5);
		
				toReturn = UPLOAD_SUCCESS;
			} else {
				sprintf(errorResult, "Oops, database error");
				toReturn = UPLOAD_FAILED;
			}
		});
		
		free(remoteMd5);
		return toReturn;
		
	}	else {
		sprintf(errorResult, "Amazon returned HTTP status %d", httpStatus);
		free(remoteMd5);
		return UPLOAD_FAILED;
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

int Uploader::uploadFiles(FileListStorage *fileListStorage, char *prefix) {
	__block LocalFileList *files = new LocalFileList();
	files->recurseIn((char *) "", prefix);

	this->totalSize =  files->calculateTotalSize();
	char *hrSizeString = hrSize(this->totalSize);
	printf("[Upload] Total size of files: %s\n", hrSizeString);
	free(hrSizeString);

	dispatch_queue_t globalQueue = dispatch_get_global_queue(0, 0);
	dispatch_queue_t sqlQueue = dispatch_queue_create("com.egorfine.db", NULL);
	dispatch_group_t group = dispatch_group_create();
	
	dispatch_semaphore_t threadsCount = dispatch_semaphore_create(20);

	__block int failed=0;

	for (int i=0;i<files->count;i++) {
		char *path = (files->paths[i]+1);
	
		// we don't skip the storage file. We will just re upload it later.

		char *realLocalPath = this->createRealLocalPath(prefix, path);
		
		__block struct stat fileInfo;
		if (lstat(realLocalPath, &fileInfo)<0) {
			printf("\n[Upload] FAIL %s: Cannot open file: %s\n", path, strerror(errno));
			free(realLocalPath);
			this->uploadedSize+=files->sizes[i];
			continue;
		}
		
		if (!(fileInfo.st_mode & S_IFREG)) {
			printf("\n[Upload] FAIL %s: Path is not a regular file.\n", path);
			free(realLocalPath);
			this->uploadedSize+=files->sizes[i];
			continue;
		}
		
		if (fileInfo.st_size==0) {
			printf("\n[Upload] WARNING %s: Zero sized file, skipped.\n", path);
			free(realLocalPath);
			this->uploadedSize+=files->sizes[i];
			continue;
		}
		
		if (fileInfo.st_size>=MAX_S3_FILE_SIZE) {
			printf("\n[Upload] FAIL %s: File too large (%" PRIu64 " bytes while only %" PRIu64 " allowed), skipped\n", 
				path, (uint64_t) fileInfo.st_size, (uint64_t) MAX_S3_FILE_SIZE);
			free(realLocalPath);
			this->uploadedSize+=files->sizes[i];
			continue;
		}
	
		char md5[33] = "";
		uint64_t mtime=0;
		int res = fileListStorage->lookup(path, md5, &mtime);
		if (res!=STORAGE_SUCCESS) {
			printf("\n[Upload] FAIL %s: Oops, database query failed.\n", path);
			free(realLocalPath);
			return UPLOAD_FAILED;
		}

		if (res>0 && mtime == (uint64_t) fileInfo.st_mtime) {
			//printf("\tNot changed.\n");
			this->uploadedSize+=files->sizes[i];
			free(realLocalPath);
			continue;
		}
		
		char *contentType = guessContentType(path);
	
		dispatch_semaphore_wait(threadsCount, DISPATCH_TIME_FOREVER);
		dispatch_group_async(group, globalQueue, ^{
			char errorResult[1024*100];
			if (!failed) {
				int res = this->uploadFileWithRetryAndStore(
					fileListStorage, realLocalPath, path, contentType, 
					&fileInfo,
					sqlQueue,
					errorResult
				);
				if (res==UPLOAD_FAILED) {
					printf("\n[Upload] FAIL %s: %s\n", path, errorResult);
					failed=1;
				}
			}
			
			free(realLocalPath);
			
			dispatch_semaphore_signal(threadsCount);
		});
		
		if (failed) {
			break;
		}
	}

	dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
	printf("\n[Upload] All uploads done.\n");

	delete files;

	return failed ? UPLOAD_FAILED : UPLOAD_SUCCESS;
}

int Uploader::uploadDatabase(char *databasePath, char *databaseFilename) { 
	struct stat fileInfo;
	if (lstat(databasePath, &fileInfo)<0) {
		printf("[Upload] Oops database upload failed: file %s doesn't exists\n", databaseFilename);
		return UPLOAD_FAILED;
	}

	this->showProgress = 0;
	
	char md5[33] = "";
	uint32_t httpStatusCode=0;

	char errorResult[1024*100];
	int res = this->uploadFileWithRetry(databasePath, databaseFilename, 
		(char *)"application/octet-stream", &fileInfo, &httpStatusCode, md5, errorResult);

	if (res==UPLOAD_SUCCESS && httpStatusCode!=200) {
		printf("[Upload] Database upload failed with HTTP status %d\n", httpStatusCode);

	} if (res==UPLOAD_SUCCESS && httpStatusCode==200) {
		printf("[Upload] Database uploaded.\n");

	} else { 
		printf("[Upload] Database upload failed: %s.\n", errorResult);
	}

	return res;
}
