SHELL := /bin/bash

# Enable if you want to check for memory leaks
#MEM = 1

.PHONEY: check
.PHONEY: clean

TTM = ttm

CC = gcc
CCWARN = -Wall
CCDEBUG = -g -O0
ifdef MEM
CCMEM = -fsanitize=address -fsanitize=leak
endif

CFLAGS = ${CCWARN} ${CCDEBUG} ${CCMEM}

all: clean ${TTM}.exe

# Assemble ttm8.c from pieces
INCLUDES = builtins.h  const.h  debug.h  decls.h  forward.h  hash.h  io.h  macros.h  types.h utf8.h  va.h

${TTM}.exe: ${TTM}.c ${INCLUDES}
	${CC} ${CFLAGS} -o $@ $<

# Create a single, unified file by replacing #includes with the actual file
unityttm.c: ${TTM}.c ${INCLUDES}
	rm -f $@ $@.bak
	cp ${TTM}.c $@
	for I in ${INCLUDES} ; do sed -i.bak -e "/^\#include[ ][ ]*\"${I}\".*/r ${I}" -e "s|^\#include[ ][ ]*\"${I}\".*|/*\0*/|" $@; done

##################################################
TESTARGS = a b c

check:: ${TTM}.exe test_all.log test_misc.log
	diff -wBb test_all.baseline test_all.log
	diff -wBb test_misc.baseline test_misc.log

test_all.log: test_all.ttm ${TTM}.exe
	./${TTM}.exe -\& -Pshowcall -T -d1 -dt -p test_all.ttm -f ./test.rs ${TESTARGS} > $@

test_misc.log: test_misc.ttm ${TTM}.exe
	./${TTM}.exe -\& -Pshowcall -T -d1 -dt -p test_misc.ttm -f ./test.rs ${TESTARGS} > $@

clean::
	rm -f ${TTM}.exe ${TTM} ${TTM}.txt ${TTM}.obj
	rm -f test_all.log
	rm -f test_misc.log
	rm -f *.bak

# This is to check cpp (C preprocessor) expansions
${TTM}.txt::
	rm -f ${TTM}.txt
	${CC} -E ${CCWARN} ${CCDEBUG} ${CCMEM} ${TTM}.c  > ${TTM}.txt


##################################################
# CMake Construction

# Specify use of Visual Studio
VS=1

ifndef VS
CCFLAG="-DCMAKE_C_COMPILER=/usr/bin/gcc"
endif

CFG = Debug

# G = -G 'Visual Studio 17 2022'
# G="-G Unix Makefiles"

.PHONEY: cmake cmake-check cmake-test cmake-compile

BUILDDIR = build

# These files need to be copied to ${BUILDDIR} for testing purposes
BUILDSRC = test.rs

CTESTLOG = test_suite.log

cmake: cmake-check

# Someday this can be done in CMakeLists.Txt
cmake-check: cmake-test cmake-fix
	diff -wBb test_all.baseline  ${BUILDDIR}/test_all.log
	diff -wBb test_misc.baseline ${BUILDDIR}/test_misc.log

cmake-test: cmake-compile
	cd ${BUILDDIR}; ctest -V -C ${CFG} > ${CTESTLOG}

cmake-compile: ${BUILDDIR}
	cd ${BUILDDIR}; cmake --build .

# All the test output is in ${BUILD}/${CTESTLOG}
# So this file must be processed to produce the <test>.log files 
# Someday this can be done in CMakeLists.Txt
cmake-fix: ${BUILDDIR}/${CTESTLOG}
	awk -f cmake-fix.awk test="test_all"  ${BUILDDIR}/${CTESTLOG} > ${BUILDDIR}/test_all.log
	awk -f cmake-fix.awk test="test_misc" ${BUILDDIR}/${CTESTLOG} > ${BUILDDIR}/test_misc.log

# Create ${BUILDDIR} directory and fill with files used in testing
${BUILDDIR}::
	rm -fr ./${BUILDDIR}
	mkdir -p ./${BUILDDIR}
	cp ${BUILDSRC} ${BUILDDIR}
	cd ${BUILDDIR}; CFLAGS="${CFLAGS}" cmake ${CCFLAG} ${G} ..
