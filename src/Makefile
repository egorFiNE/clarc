VERSION=0.0.8
BASIC_FLAGS=-Wall -D_FILE_OFFSET_BITS=64 `xml2-config --cflags` -DVERSION=\"$(VERSION)\" -Wno-write-strings -pthread -ftrapv 
LDFLAGS_BASIC=`xml2-config --libs` -L/usr/local/lib -lcurl /usr/local/lib/libre2.a

UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
FLAGS=$(BASIC_FLAGS) -Ofast -funsafe-loop-optimizations -ftracer  -fvariable-expansion-in-unroller -fprefetch-loop-arrays  -ffast-math  -mfpmath=sse -ftrapv -fstack-check
CFLAGS=$(FLAGS)
CXXFLAGS=$(FLAGS)
LDFLAGS=$(LDFLAGS_BASIC) -lsqlite3 
endif

ifeq ($(UNAME), Darwin)
FLAGS=$(BASIC_FLAGS) -ffast-math  -ftrapv  -O3
CFLAGS=$(FLAGS) -D_DARWIN_FEATURE_64_BIT_INODE 
CXXFLAGS=$(FLAGS) -D_DARWIN_FEATURE_64_BIT_INODE 
LDFLAGS=$(LDFLAGS_BASIC) -lz -lssl -lcrypto ../libsqlite3.a
endif

OBJS=localFileList.o fileListStorage.o upload.o base64.o help.o utils.o amazonCredentials.o \
	main.o remoteListOfFiles.o filePattern.o sha1.o hmac-sha1.o memxor.o logger.o threads.o \
	destination.o deleter.o md5.o microCurl.o
HEADERS=*.h

prefix=/usr/local
bindir=$(prefix)/bin
mandir=$(prefix)/share/man/man1
INSTALL=install
INSTALL_PROGRAM=$(INSTALL)

all: clarc

clean:
	rm -f clarc help.c *.o

help.c: help.txt help.h
	sh makeHelpC.sh
	$(CC) -c help.c 

clarc: $(OBJS) $(HEADERS) help.c
	$(CXX) -o clarc $(OBJS) $(LDFLAGS) 

install: clarc
	strip clarc
	$(INSTALL) -m 0755 clarc $(DESTDIR)$(bindir)/clarc
	$(INSTALL) -m 0644 clarc.1 $(DESTDIR)$(mandir)/clarc.1
