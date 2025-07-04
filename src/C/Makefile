top_srcdir = ../..
#MEM = 1

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

# Assemble ttm8.c from pieces
INCLUDES = builtins.h  const.h  debug.h  decls.h  forward.h  hash.h  io.h  macros.h  types.h utf8.h  va.h
  
# Create a single, unified file by replacing #includes with the actual file
unityttm.c: ${TTM}.c ${INCLUDES}
	rm -f $@ $@.bak
	cp ${TTM}.c $@
	for I in ${INCLUDES} ; do sed -i.bak -e "/^\#include[ ][ ]*\"${I}\".*/r ${I}" -e "s|^\#include[ ][ ]*\"${I}\".*|/*\0*/|" $@; done

${TTM}.exe: ${TTM}.c ${INCLUDES}
	${CC} ${CCWARN} ${CCDEBUG} ${CCMEM} -o $@ $<

TESTARGS = a b c
TESTRFLAG = -f ./test.rs
BASELINE = ./test.baseline
	
check:: test.output test_misc.output
	diff -wBb test.baseline test.output
	diff -wBb test_misc.baseline test_misc.output

test.output: ${TTM}.exe ${TESTFILES}
	rm -f test.stdout test.stderr test.output
	./${TTM}.exe -Pshowcall -d1 -dt -p test.ttm ${TESTRFLAG} ${TESTARGS} > ./test.stdout 2> test.stderr
	cat test.stderr test.stdout | awk -f cleanoutput.awk > test.output

test_misc.output: ${TTM}.exe ${TESTFILES}
	rm -f test_misc.stdout test_misc.stderr test_misc.output
	./${TTM}.exe -Pshowcall -d1 -dt -p test_misc.ttm ${TESTRFLAG} ${TESTARGS} > ./test_misc.stdout 2> test_misc.stderr
	cat test_misc.stderr test_misc.stdout | awk -f cleanoutput.awk > test_misc.output

clean::
	rm -f ${TTM}.exe ${TTM} ${TTM}.txt ${TTM}.obj
	rm -f test.output test.stderr test.stdout
	rm -f test_misc.output test_misc.stderr test_misc.stdout
	rm -f *.bak

# This is to check cpp (C preprocessor) expansions
${TTM}.txt: ${TTM}.c
	rm -f ${TTM}.txt
	${CC} -E ${CCWARN} ${CCDEBUG} ${CCMEM} $< > ${TTM}.txt

