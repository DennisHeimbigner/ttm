top_srcdir = ../..
MEM = 1
CYGWIN = 1

.PHONEY: check
.PHONEY: clean

#TTM = ttm
TTM = ttm8


CC = gcc
CCWARN = -Wsign-compare -Wall -Wdeclaration-after-statement
CCDEBUG = -g -O0
ifdef MEM
ifndef CYGWIN
CCMEM = -fsanitize=address -fsanitize=leak
endif
endif

all: ${TTM}.exe

clean::
	rm -f ${TTM}.exe ttm ttm.txt test.output test.stderr test.stdout

# Assemble ttm8.c from pieces
xttm8.c: ttm8.h forward.h vsl.h state.h
	rm -f $@ $@.bak
	cp ttm8.h $@
	sed -i.bak -e '/^\#include[ ][ ]*\"forward.h\".*/r forward.h' -e 's|^\#include[ ][ ]*\"forward.h\".*|/*\0*/|' $@
	sed -i.bak -e '/^\#include[ ][ ]*\"vsl.h\".*/r vsl.h' -e 's|^\#include[ ][ ]*\"vsl.h\".*|/*\0*/|' $@

${TTM}.exe: ${TTM}.c forward.h debug.h vsl.h state.h
	${CC} ${CCWARN} ${CCDEBUG} -o $@ ${CCMEM} ${LASAN} $< 2>&1 | head -n 30

# This is to check cpp (C preprocessor) expansions
ttm.txt: ${TTM}.c
	rm -f ttm.txt
	gcc -E -Wall -Wdeclaration-after-statement $< > ttm.txt

TESTPROG = -p ${top_srcdir}/tests/test.ttm

GDBTESTPROG = -p ${top_srcdir}/src/C/dbgvvcfffcgtggvvvtthhhgggggjjtju,v,t,t.ttm
TESTARGS = a b c
TESTRFLAG = -f ${top_srcdir}/tests/test.rs
TESTTRACE = -dt
TESTBARE = -B

TESTCMD = ./${TTM}.exe ${TESTTRACE} ${TESTPROG} ${TESTRFLAG} ${TESTARGS}
GDBTESTCMD = ./${TTM}.exe ${TESTTRACE} ${GDBTESTPROG}

check:: ${TTM}.exe
	rm -f ./test.stdout test.stderr test.output
	${TESTCMD} 2> test.stderr > ./test.stdout
	cat test.stderr test.stdout > test.output # Provide a unified output for diff with baseline
	diff -w ${top_srcdir}/tests/test.baseline ./test.output

gdb:: ${TTM}.exe
	echo gdb --args ${GDBTESTCMD}

git::
	${SH} ${top_srcdir}/src/git.sh
