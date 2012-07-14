#!/bin/bash
echo "#include <stdio.h> \nvoid showHelp() {\nprintf(\n"  > help.c
cat help.txt  |  sed 's/"/\\"/g'  | sed 's/^/"/g' | sed 's/$/\\n" /g' >> help.c
echo "); }" >> help.c

