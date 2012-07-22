VERSION=0.0.4
BASIC_FLAGS=-O0 -g  -DTEST -Wall -D_FILE_OFFSET_BITS=64 `xml2-config --cflags` -DVERSION=\"$(VERSION)\" -Wno-write-strings -ftrapv -pthread
LDFLAGS_BASIC=`xml2-config --libs` -L/usr/local/lib -lcurl /usr/local/lib/libre2.a

UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
CFLAGS=$(BASIC_FLAGS)
CXXFLAGS=$(BASIC_FLAGS)
LDFLAGS=$(LDFLAGS_BASIC) -lsqlite3 
endif

ifeq ($(UNAME), Darwin)
CFLAGS=$(BASIC_FLAGS) -D_DARWIN_FEATURE_64_BIT_INODE 
CXXFLAGS=$(BASIC_FLAGS) -D_DARWIN_FEATURE_64_BIT_INODE 
LDFLAGS=$(LDFLAGS_BASIC) -lz -lssl -lcrypto ../libsqlite3.a
endif

OBJS=localFileList.o fileListStorage.o upload.o base64.o help.o utils.o curlResponse.o amazonCredentials.o main.o remoteListOfFiles.o filePattern.o amzHeaders.o sha1.o hmac-sha1.o memxor.o logger.o threads.o
HEADERS=*.h

all: clarc

clean:
	rm -f ../clarc help.c *.o

help.c: help.txt help.h
	sh makeHelpC.sh
	$(CC) -c help.c 

clarc: $(OBJS) $(HEADERS) help.c
	$(CXX) -o ../clarc $(OBJS) $(LDFLAGS) 

