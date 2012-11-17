GIT=git
DST=$GIT/ttm

FILES=`cat Inventory |tr -d '\r' |tr '\n' '  '`

rm -fr ${GIT}
mkdir -p ${DST}

for f in ${FILES} ; do
DIR=`dirname $f`
if test -d $f ; then
mkdir -p ${DST}/${f}
else
mkdir -p ${DST}/${DIR}
cp $f $DST/$f
fi
done
