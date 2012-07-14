#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "fileListStorage.h"

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
	sqlite3_stmt *fileListStorageStoreStmt;
	
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

int FileListStorage::storeRemoteListOfFiles(RemoteListOfFiles *remoteListOfFiles) {
	char *sErrMsg = 0;
	sqlite3_stmt *stmt;
	const char *tail = 0;

	if (sqlite3_exec(this->sqlite, "BEGIN TRANSACTION", NULL, NULL, &sErrMsg) != SQLITE_OK) {
		return STORAGE_FAILED;
	}

	if (sqlite3_exec(this->sqlite, "DELETE FROM files", NULL, NULL, &sErrMsg) != SQLITE_OK) {
		return STORAGE_FAILED;
	}

	if (sqlite3_prepare_v2(this->sqlite,  "INSERT INTO files (filePath, md5, mtime) VALUES (?,?,?)", 10240, &stmt, &tail) != SQLITE_OK) {
		return STORAGE_FAILED;
	}

	for (uint32_t i=0;i<remoteListOfFiles->count;i++) {
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

		//printf("'%s' %u %s\n", remoteListOfFiles->md5s[i], remoteListOfFiles->mtimes[i], remoteListOfFiles->paths[i]);
	}

	if (sqlite3_exec(this->sqlite, "END TRANSACTION", NULL, NULL, &sErrMsg) != SQLITE_OK) {
		return STORAGE_FAILED;
	}

	sqlite3_finalize(stmt);
	return STORAGE_SUCCESS;
}
