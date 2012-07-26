VERSION=0.0.5
BASIC_FLAGS=-O3 -Wall -D_FILE_OFFSET_BITS=64 `xml2-config --cflags` -DVERSION=\"$(VERSION)\" -Wno-write-strings -ftrapv -pthread
LDFLAGS_BASIC=`xml2-config --libs` -L/usr/local/lib -lcurl /usr/local/lib/libre2.a

UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
CFLAGS=$(BASIC_FLAGS)
CXXFLAGS=$(BASIC_FLAGS) -Weffc++
LDFLAGS=$(LDFLAGS_BASIC) -lsqlite3 
endif

ifeq ($(UNAME), Darwin)
CFLAGS=$(BASIC_FLAGS) -D_DARWIN_FEATURE_64_BIT_INODE 
CXXFLAGS=$(BASIC_FLAGS) -D_DARWIN_FEATURE_64_BIT_INODE  -Weffc++
LDFLAGS=$(LDFLAGS_BASIC) -lz -lssl -lcrypto ../libsqlite3.a
endif

OBJS=localFileList.o fileListStorage.o upload.o base64.o help.o utils.o curlResponse.o amazonCredentials.o \
	main.o remoteListOfFiles.o filePattern.o amzHeaders.o sha1.o hmac-sha1.o memxor.o logger.o threads.o destination.o
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
