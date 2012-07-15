#!/bin/bash
echo "#include <stdio.h>"  > help.c
echo "void showHelp() {"  >> help.c
echo "printf("  >> help.c
cat help.txt  |  sed 's/"/\\"/g'  | sed 's/^/"/g' | sed 's/$/\\n" /g' >> help.c
echo "); }" >> help.c

