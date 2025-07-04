top_srcdir = ../../..
srcCdir = ${top_srcdir}/src/C

.PHONEY: check
.PHONEY: clean

ARCH = x64
STATE = Debug

TTM = ttm

all: check

TESTPROG = -p ${srcCdir}/test.ttm
TESTARGS = a b c
TESTRFLAG = -f ${srcCdir}/test.rs
BASELINE = ${srcCdir}/test.baseline
	
check:: ttm.output ttm_misc.output
	diff -wBb ../test.baseline test.output
	diff -wBb ../test_misc.baseline test_misc.output

ttm.output: ${TTM}.exe ${TESTFILES}
	rm -f test.stdout test.stderr test.output
	rm -f test_misc.stdout test_misc.stderr test_misc.output
	./${TTM}.exe -Pshowcall -d1 -dt ${TESTPROG} ${TESTRFLAG} ${TESTARGS} > ./test.stdout 2> test.stderr
	cat test.stderr test.stdout | awk -f ../cleanoutput.awk > test.output

ttm_misc.output: ${TTM}.exe ${TESTFILES}
	./${TTM}.exe -Pshowcall -d1 -dt ${TESTPROG} ${TESTRFLAG} ${TESTARGS} > ./test_misc.stdout 2> test_misc.stderr
	cat test_misc.stderr test_misc.stdout | awk -f ../cleanoutput.awk > test_misc.output

${TTM}.exe: ${ARCH}/${STATE}/${TTM}.exe
	cp ${ARCH}/${STATE}/${TTM}.exe .

clean::
	rm -f ${TTM}.exe ${TTM}.txt
	rm -f test.output test.stderr test.stdout
	rm -f test_misc.output test_misc.stderr test_misc.stdout

# Remove unneeded Visual Studio files
veryclean:: clean
	rm -fr ttm x64 .vs
