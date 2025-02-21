top_srcdir = ../..

MEM = 1

.PHONEY: check
.PHONEY: clean

CC = gcc
CCWARN = -Wsign-compare -Wall -Wdeclaration-after-statement
CCDEBUG = -g -O0
ifdef MEM
CCMEM = -fsanitize=address -fsanitize=leak
#-static-libasan
LASAN = -lasan
endif

all: ttm.exe

clean::
	rm -f ttm.exe ttm ttm.txt test.output

ttm.exe: ttm.c
	${CC} ${CCWARN} ${CCDEBUG} -o ttm ttm.c ${CCMEM} ${LASAN}

# This is to check cpp (C preprocessor) expansions
ttm.txt::
	rm -f ttm.txt
	gcc -E -Wall -Wdeclaration-after-statement ttm.c > ttm.txt

TESTPROG = -p ${top_srcdir}/tests/test.ttm
TESTARGS = a b c
TESTRFLAG = -f ${top_srcdir}/tests/test.rs
TESTCMD = ./ttm ${TESTPROG} ${TESTRFLAG} ${TESTARGS}

check:: ttm.exe
	rm -f ./test.output
	${TESTCMD} >& ./test.output
	diff -w ${top_srcdir}/tests/test.baseline ./test.output

git::
	${SH} ${top_srcdir}/src/git.sh
