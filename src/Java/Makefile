# Enable if you want to check for memory leaks
#MEM = 1

.PHONEY: build
.PHONEY: check
.PHONEY: clean

all: build

build: ./main/java/ucar/ttm/TTM.java
	ant

##################################################
TESTARGS = a b c
	
check:: test.output test_misc.output
	diff -wBb test.baseline test.output
	diff -wBb test_misc.baseline test_misc.output

test.output: ${TTM}.exe ${TESTFILES}
	rm -f test.stdout test.stderr test.output
	./${TTM}.exe -Pshowcall -T -d1 -dt -p test.ttm -f ./test.rs ${TESTARGS} > ./test.stdout 2> test.stderr
	cat test.stderr test.stdout > test.output

test_misc.output: ${TTM}.exe ${TESTFILES}
	rm -f test_misc.stdout test_misc.stderr test_misc.output
	./${TTM}.exe -Pshowcall -T -d1 -dt -p test_misc.ttm -f ./test.rs ${TESTARGS} > ./test_misc.stdout 2> test_misc.stderr
	cat test_misc.stderr test_misc.stdout > test_misc.output

clean::
	rm -f ${TTM}.exe ${TTM} ${TTM}.txt ${TTM}.obj
	rm -f test.output test.stderr test.stdout
	rm -f test_misc.output test_misc.stderr test_misc.stdout
	rm -f *.bak

# This is to check cpp (C preprocessor) expansions
${TTM}.txt: ${TTM}.c
	rm -f ${TTM}.txt
	${CC} -E ${CCWARN} ${CCDEBUG} ${CCMEM} $< > ${TTM}.txt

