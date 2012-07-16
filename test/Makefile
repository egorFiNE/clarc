CC=clang
CXX=clang++
CFLAGS_BASIC=-O3 -fblocks -Wall -D_FILE_OFFSET_BITS=64  `xml2-config --cflags`  -DVERSION=\"test\" -I../src/
CXXFLAGS_BASIC=$(CFLAGS_BASIC) -Wno-write-strings -Wdeprecated-writable-strings
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

OBJS= ../src/base64.o ../src/sha1.o ../src/hmac-sha1.o ../src/memxor.o

all: test

clean:
	rm -f amazonCredentials localFileList *.o

amazonCredentials: $(OBJS) $(HEADERS) ../src/amazonCredentials.cc ./amazonCredentials.cc
	$(CXX) $(CFLAGS) -o amazonCredentials ../src/amazonCredentials.cc $(OBJS) $(LDFLAGS) ./amazonCredentials.cc


localFileList: $(OBJS) $(HEADERS) ../src/localFileList.cc ./localFileList.cc
	$(CXX) $(CFLAGS) -o localFileList ../src/localFileList.cc $(OBJS) $(LDFLAGS) ./localFileList.cc
