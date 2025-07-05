.PHONEY: check
.PHONEY: clean

TTMPY = ttm3.py # Python 3 version
TTM2PY = ttm2.py # Python 2 version; no longer supported.

TESTCMD = python ./${TTMPY}
PDBCMD   = -m pdb ./${TTMPY}

# Non-flag arguments to ttm.py
TTMARGS = a b c

check:: test.output test_misc.output
	diff -wBb ../C/test.baseline test.output
	diff -wBb ../C/test_misc.baseline test_misc.output

test.output: ${TTMPY} ${TESTFILES}
	rm -f test.stdout test.stderr test.output
	${TESTCMD} -Pshowcall -T -d1 -dt -p test.ttm -f ../C/test.rs ${TESTARGS} > ./test.stdout 2> test.stderr
	cat test.stderr test.stdout > test.output

test_misc.output: ${TTMPY} ${TESTFILES}
	rm -f test_misc.stdout test_misc.stderr test_misc.output
	${TESTCMD} -Pshowcall -T -d1 -dt -p test_misc.ttm -f ../C/test.rs ${TESTARGS} > ./test_misc.stdout 2> test_misc.stderr
	cat test_misc.stderr test_misc.stdout > test_misc.output

clean::
	rm -f ./test.output ./test.stderr ./test.stdout
	rm -f ./test_misc.output ./test_misc.stderr ./test_misc.stdout

debug:: ${TTMPY}
	python3 ${PDBCMD}
