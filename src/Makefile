VERSION=0.0.3
CC=clang
CXX=clang++
CFLAGS_BASIC=-O3 -g -fblocks -Wall -D_FILE_OFFSET_BITS=64  `xml2-config --cflags`  -DVERSION=\"$(VERSION)\"
CXXFLAGS_BASIC=$(CFLAGS_BASIC) -Wno-write-strings -g 
LDFLAGS_BASIC=`xml2-config --libs` -L/usr/local/lib -lcurl /usr/local/lib/libre2.a

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

OBJS=localFileList.o fileListStorage.o upload.o base64.o help.o utils.o curlResponse.o amazonCredentials.o main.o remoteListOfFiles.o filePattern.o amzHeaders.o sha1.o hmac-sha1.o memxor.o  logger.o
HEADERS=*.h

all: clarc

clean:
	rm -f ../clarc help.c *.o

help.c: help.txt help.h
	sh makeHelpC.sh
	$(CC) -c help.c 

clarc: $(OBJS) $(HEADERS) help.c
	$(CXX) $(CFLAGS) -o ../clarc $(OBJS) $(LDFLAGS) 

