cmake_minimum_required(VERSION 3.1)

project(clarc VERSION 0.0.1
                  DESCRIPTION "Very nice project")

find_package(PkgConfig)
pkg_check_modules(LIBRE2 REQUIRED re2)

find_package(libXML2 REQUIRED)
find_package(openssl REQUIRED)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(LIBSQLITE REQUIRED sqlite3)

add_executable(clarc localFileList.cc fileListStorage.cc upload.cc base64.c help.c utils.c amazonCredentials.cc main.cc remoteListOfFiles.cc filePattern.cc sha1.c hmac-sha1.c memxor.c logger.c threads.cc destination.cc deleter.cc md5.c microCurl.cc)
target_link_libraries(clarc PUBLIC ${LIBRE2_LDFLAGS}  ${LIBRE2_LIBRARIES} ${LIBXML2_LIBRARY} $ENV{OPENSSL_ROOT_DIR}/lib/libcrypto.a $ENV{OPENSSL_ROOT_DIR}/lib/libssl.a ${LIBCURL_LIBRARIES} ${LIBSQLITE_LIBRARIES})
target_include_directories(clarc PUBLIC ${LIBRE2_INCLUDE_DIRS} ${LIBXML2_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
# target_compile_options(clarc PUBLIC ${LIBRE2_CFLAGS_OTHER})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wno-writable-strings -Ofast -D_DARWIN_FEATURE_64_BIT_INODE -D_FILE_OFFSET_BITS=64")
set(CMAKE_CC_FLAGS "-D_DARWIN_FEATURE_64_BIT_INODE -D_FILE_OFFSET_BITS=64")
