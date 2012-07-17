CC=clang
CXX=clang++
CFLAGS_BASIC=-O0 -g -fblocks -Wall -D_FILE_OFFSET_BITS=64  `xml2-config --cflags`  -DVERSION=\"test\" -I../src/ -Wno-deprecated-writable-strings
CXXFLAGS_BASIC=$(CFLAGS_BASIC) -O0 -Wno-write-strings  -g
LDFLAGS_BASIC=`xml2-config --libs` -L/usr/local/lib -lcurl -lcheck

UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
CFLAGS=$(CFLAGS_BASIC)
CXXFLAGS=$(CXXFLAGS_BASIC)
LDFLAGS=$(LDFLAGS_BASIC) -ldispatch -lBlocksRuntime -lsqlite3 
endif

ifeq ($(UNAME), Darwin)
CFLAGS=$(CFLAGS_BASIC) -D_DARWIN_FEATURE_64_BIT_INODE 
CXXFLAGS=$(CXXFLAGS_BASIC) -D_DARWIN_FEATURE_64_BIT_INODE 
LDFLAGS=$(LDFLAGS_BASIC) -lz -lssl -lcrypto ../libsqlite3.a
endif

OBJS=../src/base64.o ../src/sha1.o ../src/hmac-sha1.o ../src/memxor.o ../src/curlResponse.o ../src/utils.o
SOURCES=../src/localFileList.cc ../src/amazonCredentials.cc ../src/amzHeaders.cc ../src/fileListStorage.cc ../src/remoteListOfFiles.cc ../src/upload.cc
TESTS=./localFileListTest.cc ./amazonCredentialsTest.cc ./amzHeadersTest.cc ./fileListStorageTest.cc ./remoteListOfFilesTest.cc ./uploadTest.cc
all: test

clean:
	rm -f test *.o

test: $(OBJS) $(HEADERS) $(SOURCES) $(TESTS) ./test.cc
	$(CXX) $(CFLAGS) -o test $(SOURCES) $(TESTS) ./test.cc $(OBJS) $(LDFLAGS)
