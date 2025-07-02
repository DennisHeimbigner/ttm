top_srcdir = ../..
MEM = 1

.PHONEY: check
.PHONEY: clean

TTM = ttm

CC = gcc
CCWARN = -Wsign-compare -Wall -Wdeclaration-after-statement
CCDEBUG = -g -O0
ifdef MEM
CCMEM = -fsanitize=address -fsanitize=leak
endif

all: ${TTM}.exe

clean::
	rm -f ${TTM}.exe ${TTM} ttm.txt
	rm -f test.output test.stderr test.stdout
	rm -f test_misc.output test_misc.stderr test_misc.stdout

# Assemble ttm8.c from pieces
INCLUDES = builtins.h  const.h  debug.h  decls.h  forward.h  hash.h  io.h  macros.h  types.h utf8.h  va.h
  
# Create a single file by replacing #includes with the actual file
ttm8.c: ttm.c ${INCLUDES}
	rm -f $@ $@.bak
	cp ttm8.h $@
	for I in ${INCLUDES} ; do sed -i.bak -e "/^\#include[ ][ ]*\"${I}\".*/r ${I}" -e "s|^\#include[ ][ ]*\"${I}\".*|/*\0*/|" $@; done

${TTM}.exe: ${TTM}.c ${INCLUDES}
	${CC} ${CCWARN} ${CCDEBUG} ${CCMEM} -o $@ $<

# This is to check cpp (C preprocessor) expansions
ttm.txt: ${TTM}.c
	rm -f ttm.txt
	${CC} -E ${CCWARN} ${CCDEBUG} ${CCMEM} $< > ttm.txt

TESTPROG = -p ./test.ttm
TESTARGS = a b c
TESTRFLAG = -f ${top_srcdir}/tests/test.rs

BASELINE = ./test.baseline
	
check:: ${TTM}.exe
	rm -f ./test.stdout test.stderr test.output
	./${TTM}.exe -Pshowcall -d1 -dt ${TESTPROG} ${TESTRFLAG} ${TESTARGS} > ./test.stdout 2> test.stderr
	cat test.stderr test.stdout > test.output # Provide a unified output for diff with baseline
	diff -w ${BASELINE} ./test.output
