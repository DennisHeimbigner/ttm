#!/bin/bash

set -x
# Extract the output capture file
SRCDIR=`dirname "$1"`
BASE=`basename "$1"`
shift # remainder of args are the test
# Execute the actual test program and redirect its output to a file
echo "$@" ">" ${BASE}.log
"$@" > ${BASE}.log
diff -wBb ${SRCDIR}/${BASE}.baseline ${BASE}.log

