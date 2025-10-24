SHELL := /bin/bash

# Enable if you want to check for memory leaks
#MEM = 1

.PHONEY: check
.PHONEY: clean

##################################################
# Make Build: *nix* only (including Cygwin

TTM = ttm

CC = gcc
CCWARN = -Wall
CCDEBUG = -g -O0
CCDEF = -DMAKE_BUILD=1 # choices: CMAKE_BUILD, AUTOMAKE_BUILD, MAKE_BUILD
ifdef MEM
CCMEM = -fsanitize=address -fsanitize=leak
endif

CFLAGS = ${CCWARN} ${CCDEBUG} ${CCMEM} ${CCDEF}

all: clean ${TTM}.exe

# Assemble ttm8.c from pieces
INCLUDES = builtins.h  const.h  debug.h  decls.h  forward.h  hash.h  io.h  macros.h  types.h utf8.h  vutils.h

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


# This is to check cpp (C preprocessor) expansions
${TTM}.txt::
	rm -f ${TTM}.txt
	${CC} -E ${CCWARN} ${CCDEBUG} ${CCMEM} ${TTM}.c  > ${TTM}.txt

##################################################
# CMake Build: Windows or *nix*

# In order to execute the tests under Windows, you will need to have
# either Cygwin or Mingw installed so that the programs bash, sed,
# diff, awk, etc. are available.

# Specify use of Visual Studio; If not defined, then attempt to use gcc.
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

##################################################
# Direct Visual Studio Build

# In order to execute the tests, you will need to have either Cygwin or Mingw installed
# so that the programs bash, sed, diff, awk, etc. are available.

# To build, invoke Visual Studio, and open the .vcxproj file as an exsting project.
# The exact process may vary slightly depending on the Visual Studio version.
# 1. Start up Visual Studio
# 2. Click "Open a project or solution"
# 3. Select .../ttm/src/C/ttm.vcxproj to open
# 4. Select menu build->build Solution
# 5. Exit

# The build action should put ttm.exe into ttm/src/C/Windows/<arch>/<config>.

# Set Architecture and Config variables
ARCH=x64
CONFIG=Debug

# Move ttm.exe from <arch>/<config> to current directory
installvs: Windows/${ARCH}/${CONFIG}/${TTM}.exe
	cp Windows/${ARCH}/${CONFIG}/${TTM}.exe .

##################################################
# Cleanup Actions

cmake-clean::
	rm -fr build

clean:: cmake-clean
	rm -f ${TTM}.exe ${TTM} ${TTM}.txt ${TTM}.obj
	rm -f test_all.log
	rm -f test_misc.log
	rm -f *.bak

veryclean:: clean
	rm -fr ttm x64 .vs # Remove unneeded Visual Studio files
