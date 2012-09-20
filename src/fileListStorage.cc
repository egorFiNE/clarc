#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include "fileListStorage.h"
#include "localFileList.h"
extern "C" {
#include "logger.h"
}

int FileListStorage::createTable() {
	return sqlite3_exec(
		this->sqlite,
		"CREATE TABLE IF NOT EXISTS files ("
			"md5 TEXT NOT NULL, "
			"mtime BIGINT NOT NULL, "
			"filePath TEXT NOT NULL PRIMARY KEY "
		")",
		0,0,0
	);
}

int FileListStorage::putDbVersion() {
	// "hello" is a primary key to not let overwrite data.
	int res;

	res=sqlite3_exec(
		this->sqlite,
		"CREATE TABLE IF NOT EXISTS meta ("
			"hello TEXT NOT NULL PRIMARY KEY, "
			"dbVersion BIGINT NOT NULL "
		")",
		0,0,0
	);

	if (res!=SQLITE_OK) {
		return res;
	}

	// result of this statement is deliberately ignored
	sqlite3_exec(this->sqlite, "INSERT INTO meta (hello, dbVersion) VALUES ('hello', 1)",0,0,0);

	return SQLITE_OK;
}

FileListStorage::FileListStorage(char *path, char *errorResult) {
	errorResult[0]=0;
	if (sqlite3_open(path, &(this->sqlite))!=SQLITE_OK) {
		strcpy(errorResult, "Oops, cannot open database");
		return; 
	}

	if (this->createTable()!=SQLITE_OK) {
		strcpy(errorResult, "Oops, cannot create database table");
		return; 
	}

	if (this->putDbVersion()!=SQLITE_OK) {
		strcpy(errorResult, "Oops, cannot update version in table");
		return;
	}
}

FileListStorage::~FileListStorage() {
	char *sErrMsg = 0;	
	sqlite3_exec(this->sqlite, "VACUUM", NULL, NULL, &sErrMsg);
	sqlite3_close(this->sqlite);
}

int FileListStorage::lookup(char *remotePath, char *md5, uint64_t *mtime) {
	md5[0]=0;
	*mtime=0;

	if (remotePath==NULL) {
		return STORAGE_SUCCESS;
	}

	sqlite3_stmt *selectFileStmt=NULL;
	
	if (sqlite3_prepare(this->sqlite, "SELECT md5, mtime FROM files WHERE filePath=?", -1, &selectFileStmt, 0) != SQLITE_OK) {
		return STORAGE_FAILED;
	} 
	
	if (sqlite3_bind_text(selectFileStmt, 1, remotePath, (int) strlen(remotePath), SQLITE_STATIC) != SQLITE_OK) {
		sqlite3_finalize(selectFileStmt);
		return STORAGE_FAILED;
	}

	int s = sqlite3_step(selectFileStmt);
	if (s == SQLITE_ROW) {
		char *a = (char *)sqlite3_column_text(selectFileStmt, 0);
		strcpy(md5, a);

		*mtime = (uint64_t) sqlite3_column_int64(selectFileStmt, 1);

		sqlite3_finalize(selectFileStmt);
		return STORAGE_SUCCESS;

	} else if (s==SQLITE_DONE) {
		sqlite3_finalize(selectFileStmt);
		return STORAGE_SUCCESS;
	}

	sqlite3_finalize(selectFileStmt);
	return STORAGE_FAILED;
}

int FileListStorage::store(char *remotePath, char *md5, uint64_t mtime) {
	sqlite3_stmt *fileListStorageStoreStmt=NULL;
	
	if (sqlite3_prepare(this->sqlite, "REPLACE INTO files (filePath, md5, mtime) VALUES (?,?,?)", -1, &fileListStorageStoreStmt, 0) != SQLITE_OK) {
		return STORAGE_FAILED;
	} 
	
	if (sqlite3_bind_text(fileListStorageStoreStmt, 1, remotePath, (int) strlen(remotePath), SQLITE_STATIC) != SQLITE_OK) {
		sqlite3_finalize(fileListStorageStoreStmt);
		return STORAGE_FAILED;
	}

	if (sqlite3_bind_text(fileListStorageStoreStmt, 2, md5, 32, SQLITE_STATIC) != SQLITE_OK) {
		sqlite3_finalize(fileListStorageStoreStmt);
		return STORAGE_FAILED;
	}

	if (sqlite3_bind_int64(fileListStorageStoreStmt, 3, mtime) != SQLITE_OK) {
		sqlite3_finalize(fileListStorageStoreStmt);
		return STORAGE_FAILED;
	}

	if (sqlite3_step(fileListStorageStoreStmt) != SQLITE_DONE) {
		sqlite3_finalize(fileListStorageStoreStmt);
		return STORAGE_FAILED;
	}

	sqlite3_finalize(fileListStorageStoreStmt);

	return STORAGE_SUCCESS;
}

int FileListStorage::truncate() {
	char *sErrMsg = 0;
	if (sqlite3_exec(this->sqlite, "BEGIN TRANSACTION", NULL, NULL, &sErrMsg) != SQLITE_OK) {
		return STORAGE_FAILED;
	}

	if (sqlite3_exec(this->sqlite, "DELETE FROM files", NULL, NULL, &sErrMsg) != SQLITE_OK) {
		return STORAGE_FAILED;
	}

	if (sqlite3_exec(this->sqlite, "END TRANSACTION", NULL, NULL, &sErrMsg) != SQLITE_OK) {
		return STORAGE_FAILED;
	}

	return STORAGE_SUCCESS;
}

int FileListStorage::storeRemoteListOfFiles(RemoteListOfFiles *remoteListOfFiles) {
	if (remoteListOfFiles->count<=100) {
		return this->storeRemoteListOfFilesChunk(remoteListOfFiles, 0, remoteListOfFiles->count);
	}

	uint32_t chunks = (uint32_t) floor(remoteListOfFiles->count/100);
	if (chunks*100 < remoteListOfFiles->count) { 
		chunks++;
	}

	LOG(LOG_DBG, "[FileListStorage] Will store %d chunks (count = %d)", chunks, remoteListOfFiles->count);

	for (int i=0;i<chunks;i++) {
		uint32_t max = (i+1)*100;
		if (max>remoteListOfFiles->count) {
			max = remoteListOfFiles->count;
		}
		if (this->storeRemoteListOfFilesChunk(remoteListOfFiles, i*100, max) == STORAGE_FAILED) {
			return STORAGE_FAILED;
		}
	}

	return STORAGE_SUCCESS;
}

int FileListStorage::storeRemoteListOfFilesChunk(RemoteListOfFiles *remoteListOfFiles, uint32_t from, uint32_t to) {
	char *sErrMsg = 0;
	sqlite3_stmt *stmt = NULL;
	const char *tail = 0;

	LOG(LOG_DBG, "[FileListStorage] Storing from %d to %d (max %d)", from, to, remoteListOfFiles->count);

	if (sqlite3_exec(this->sqlite, "BEGIN TRANSACTION", NULL, NULL, &sErrMsg) != SQLITE_OK) {
		return STORAGE_FAILED;
	}

	if (sqlite3_prepare_v2(this->sqlite, "INSERT INTO files (filePath, md5, mtime) VALUES (?,?,?)", 10240, &stmt, &tail) != SQLITE_OK) {
		return STORAGE_FAILED;
	}

	for (uint32_t i=from;i<to;i++) {
		if (sqlite3_bind_text(stmt, 1, remoteListOfFiles->paths[i], -1, SQLITE_TRANSIENT) != SQLITE_OK) {
			return STORAGE_FAILED;
		}

		if (sqlite3_bind_text(stmt, 2, remoteListOfFiles->md5s[i], -1, SQLITE_TRANSIENT) != SQLITE_OK) {
			return STORAGE_FAILED;
		}
		if (sqlite3_bind_int64(stmt, 3, remoteListOfFiles->mtimes[i]) != SQLITE_OK) {
			return STORAGE_FAILED;
		}

		if (sqlite3_step(stmt) != SQLITE_DONE) {
			return STORAGE_FAILED;
		}

		sqlite3_clear_bindings(stmt);
		sqlite3_reset(stmt);
	}

	if (sqlite3_exec(this->sqlite, "END TRANSACTION", NULL, NULL, &sErrMsg) != SQLITE_OK) {
		return STORAGE_FAILED;
	}

	sqlite3_finalize(stmt);
	return STORAGE_SUCCESS;
}


LocalFileList * FileListStorage::calculateListOfFilesToDelete(LocalFileList *localFileList) {
	char *sErrMsg = 0;
	sqlite3_stmt *stmt = NULL;
	const char *tail = 0;

	char createTable[10240] = "CREATE TEMPORARY TABLE localFiles (" \
		"filePath TEXT NOT NULL PRIMARY KEY)";
	if (sqlite3_exec(this->sqlite, createTable, NULL, NULL, &sErrMsg) != SQLITE_OK) {
		return NULL;
	}

	if (sqlite3_exec(this->sqlite, "BEGIN TRANSACTION", NULL, NULL, &sErrMsg) != SQLITE_OK) {
		return NULL;
	}

	if (sqlite3_prepare_v2(this->sqlite, "INSERT INTO localFiles (filePath) VALUES (?)", 10240, &stmt, &tail) != SQLITE_OK) {
		return NULL;
	}

	for (uint32_t i=0;i<localFileList->count;i++) {
		if (sqlite3_bind_text(stmt, 1, (localFileList->paths[i]+1), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
			sqlite3_finalize(stmt);
			return NULL;
		}

		if (sqlite3_step(stmt) != SQLITE_DONE) {
			sqlite3_finalize(stmt);
			return NULL;
		}

		sqlite3_clear_bindings(stmt);
		sqlite3_reset(stmt);
	}

	sqlite3_finalize(stmt);

	if (sqlite3_exec(this->sqlite, "END TRANSACTION", NULL, NULL, &sErrMsg) != SQLITE_OK) {
		return STORAGE_FAILED;
	}

	sqlite3_stmt *selectFileStmt=NULL;
	
	if (sqlite3_prepare(this->sqlite, "SELECT filePath FROM files WHERE files.filePath NOT IN " \
		"(SELECT filePath FROM localFiles)", -1, &selectFileStmt, 0) != SQLITE_OK) {
		return NULL;
	} 
	
	LocalFileList *fileList = new LocalFileList(NULL);
	int s;
	do {
		s = sqlite3_step(selectFileStmt);
		if (s == SQLITE_ROW) {
			char *filePath = (char *)sqlite3_column_text(selectFileStmt, 0);
			fileList->add(filePath, 0);
		} else {
			break;
		}
	} while (true);

	sqlite3_finalize(selectFileStmt);

	sqlite3_exec(this->sqlite, (char *) "DROP TABLE localFiles", NULL, NULL, &sErrMsg);

	return fileList;
}

int FileListStorage::storeDeletedBatch(char **batch, uint32_t batchCount) {
	char *sErrMsg = 0;
	sqlite3_stmt *stmt = NULL;
	const char *tail = 0;

	if (sqlite3_exec(this->sqlite, "BEGIN TRANSACTION", NULL, NULL, &sErrMsg) != SQLITE_OK) {
		return STORAGE_FAILED;
	}

	if (sqlite3_prepare_v2(this->sqlite, "DELETE FROM files WHERE filePath=?", 10240, &stmt, &tail) != SQLITE_OK) {
		return STORAGE_FAILED;
	}

	for (uint32_t i=0;i<batchCount;i++) {
		if (sqlite3_bind_text(stmt, 1, batch[i], -1, SQLITE_TRANSIENT) != SQLITE_OK) {
			sqlite3_finalize(stmt);
			return STORAGE_FAILED;
		}

		if (sqlite3_step(stmt) != SQLITE_DONE) {
			sqlite3_clear_bindings(stmt);
			sqlite3_finalize(stmt);
			return STORAGE_FAILED;
		}

		sqlite3_clear_bindings(stmt);
		sqlite3_reset(stmt);
	}

	sqlite3_finalize(stmt);

	if (sqlite3_exec(this->sqlite, "END TRANSACTION", NULL, NULL, &sErrMsg) != SQLITE_OK) {
		return STORAGE_FAILED;
	}

	return STORAGE_SUCCESS;
}

