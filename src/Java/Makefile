# Enable if you want to check for memory leaks
#MEM = 1

.PHONEY: check
.PHONEY: clean

all: target/ttm.jar

target/ttm.jar: ./src/main/java/ttm/TTM.java
	ant

##################################################
TESTFILES =  ../C/test.ttm ../C/test_misc.ttm
TESTFILES += ../C/test.baseline ../C/test_misc.baseline

TESTARGS = a b c
	
check:: test.output test_misc.output
	diff -wBb ../C/test.baseline test.output
	diff -wBb ../C/test_misc.baseline test_misc.output

test.output: target/ttm.jar ${TESTFILES}
	rm -f test.stdout test.stderr test.output
	java -jar ./target/ttm.jar -Pshowcall -T -d1 -dt -p ../C/test.ttm -f ../C/test.rs ${TESTARGS} > ./test.stdout 2> test.stderr
	cat test.stderr test.stdout > test.output

test_misc.output: target/ttm.jar ${TESTFILES}
	rm -f test_misc.stdout test_misc.stderr test_misc.output
	java -jar ./target/ttm.jar -Pshowcall -T -d1 -dt -p ../C/test_misc.ttm -f ../C/test.rs ${TESTARGS} > ./test_misc.stdout 2> test_misc.stderr
	cat test_misc.stderr test_misc.stdout > test_misc.output

clean::
	rm -f test.output test.stderr test.stdout
	rm -f test_misc.output test_misc.stderr test_misc.stdout

cleanall::
	rm -fr target
