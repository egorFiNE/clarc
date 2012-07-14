#ifndef _FILELISTSTORAGE_H
#define _FILELISTSTORAGE_H

#include <iostream>
using namespace std;

#include "sqlite3.h"
#include "remoteListOfFiles.h"

#define STORAGE_FAILED 0
#define STORAGE_SUCCESS 1

class FileListStorage
{
private:
	sqlite3 *sqlite;
	int createTable();
	int putDbVersion();
	
public:
	FileListStorage(char *path, char *errorResult);
	~FileListStorage();

	int lookup(char *remotePath, char *md5, uint64_t *mtime);
	int store(char *remotePath, char *md5, uint64_t mtime);
	int storeRemoteListOfFiles(RemoteListOfFiles *remoteListOfFiles);
};

#endif
