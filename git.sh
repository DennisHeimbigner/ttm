#!/bin/sh
set -x
GIT=git
DST=$GIT/ttm

FILES=`cat Manifest |tr -d '\r' |tr '\n' '  '`

rm -fr ${GIT}
mkdir -p ${DST}

for f in ${FILES} ; do
DIR=`dirname $f`
mkdir -p ${DST}/${DIR}
cp $f $DST/$f
done

