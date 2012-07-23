#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

char *getIsoDate();
char *guessContentType(char *filename);
char *hrSize(uint64_t size);
void extractLocationFromHeaders(char *headers, char *locationResult);
