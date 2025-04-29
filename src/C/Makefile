
top_srcdir = ../..
MEM = 1

.PHONEY: check
.PHONEY: clean

#TTM = ttm
TTM = ttm8

CC = gcc
CCWARN = -Wsign-compare -Wall -Wdeclaration-after-statement
CCDEBUG = -g -O0
ifdef MEM
CCMEM = -fsanitize=address -fsanitize=leak
endif

all: ${TTM}.exe

clean::
	rm -f ${TTM}.exe ttm ttm.txt test.output test.stderr test.stdout

# Assemble ttm8.c from pieces
HFILES = const.h types.h decls.h macros.h forward.h hash.h va.h io.h utf8.h debug.h builtins.h

xttm8.c: ttm8.h forward.h va.h state.h builtins.h io.h
	rm -f $@ $@.bak
	cp ttm8.h $@
	for H in ${HFILES} ; do sed -i.bak -e "/^\#include[ ][ ]*\"${H}\".*/r ${H}" -e "s|^\#include[ ][ ]*\"${H}\".*|/*\0*/|" $@; done

${TTM}.exe: ${TTM}.c ${HFILES}
	${CC} ${CCWARN} ${CCDEBUG} -o $@ ${CCMEM} ${LASAN} $< 2>&1 | head -n 30

# This is to check cpp (C preprocessor) expansions
ttm.txt: ${TTM}.c
	rm -f ttm.txt
	gcc -E -Wall -Wdeclaration-after-statement $< > ttm.txt

#TESTPROG = -p ${top_srcdir}/tests/test.ttm
TESTPROG = -p ./test.ttm
GDBTESTPROG = -p ${top_srcdir}/src/C/dbg.ttm
TESTARGS = a b c
TESTRFLAG = -f ${top_srcdir}/tests/test.rs
#TESTTRACE = -dt
#TESTBARE = -B

#BASELINE = ${top_srcdir}/tests/test.baseline
BASELINE = ./test.baseline

TESTCMD = ./${TTM}.exe ${TESTTRACE} ${TESTPROG} ${TESTRFLAG} ${TESTARGS}
GDBTESTCMD = ./${TTM}.exe ${TESTBARE} ${TESTTRACE} ${GDBTESTPROG} ${TESTRFLAG} ${TESTARGS}

check:: ${TTM}.exe
	rm -f ./test.stdout test.stderr test.output
	${TESTCMD} > ./test.stdout 2> test.stderr
	cat test.stderr test.stdout > test.output # Provide a unified output for diff with baseline
	diff -w ${BASELINE} ./test.output

gdb:: ${TTM}.exe
	echo gdb --args ${GDBTESTCMD}

mem:: ${TTM}.exe
	echo ${GDBTESTCMD}

git::
	${SH} ${top_srcdir}/src/git.sh
