top_srcdir = ../..

MEM = 1
CYGWIN = 1

.PHONEY: check
.PHONEY: clean

CC = gcc
CCWARN = -Wsign-compare -Wall -Wdeclaration-after-statement
CCDEBUG = -g -O0
ifdef MEM
ifndef CYGWIN
CCMEM = -fsanitize=address -fsanitize=leak
endif
endif

all: ttm.exe

clean::
	rm -f ttm.exe ttm ttm.txt test.output test.stderr test.stdout

ttm.exe: ttm.c
	${CC} ${CCWARN} ${CCDEBUG} -o ttm ttm.c ${CCMEM} ${LASAN}

# This is to check cpp (C preprocessor) expansions
ttm.txt::
	rm -f ttm.txt
	gcc -E -Wall -Wdeclaration-after-statement ttm.c > ttm.txt

TESTPROG = -p ${top_srcdir}/tests/test.ttm
#TESTPROG = -p ${top_srcdir}/src/C/t.ttm
TESTARGS = a b c
TESTRFLAG = -f ${top_srcdir}/tests/test.rs
TESTTRACE = -dt
TESTCMD = ./ttm ${TESTPROG} ${TESTTRACE} ${TESTRFLAG} ${TESTARGS}

check:: ttm.exe
	rm -f ./test.stdout test.stderr test.output
	${TESTCMD} 2> test.stderr > ./test.stdout
	cat test.stderr test.stdout > test.output # Provide a unified output for diff with baseline
	diff -w ${top_srcdir}/tests/test.baseline ./test.output

gdb:: ttm.exe
	gdb --args ${TESTCMD}

git::
	${SH} ${top_srcdir}/src/git.sh
