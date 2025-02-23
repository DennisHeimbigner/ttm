TTMPY = ttm.py # Python 3 version
TTM2PY = ttm2.py # Python 2 version; no longer supported.

TESTPROG = ../../tests/test.ttm
#TESTPROG = ./t.ttm
TESTRS = ../../tests/test.rs
TESTBASELINE = ../../tests/test.baseline

# Arguments to ttm.py
TTMPROG = -p ${TESTPROG}
TTMRFLAG = -f ${TESTRS}
TTMTRACE = -dt
TTMARGS = a b c
TTMCMD = ${TTMTRACE} ${TTMPROG} ${TTMRFLAG} ${TTMARGS}

TESTCMD = ./${TTMPY} ${TTMCMD}
PDBCMD   = -m pdb ${TESTCMD}

check:: ${TTMPY}
	rm -f ./test.output ./test.stderr ./test.stdout
	python3 ${TESTCMD} 2> ./test.stderr > ./test.stdout
	cat ./test.stderr ./test.stdout > test.output
	diff -w ${TESTBASELINE} ./test.output

clean::
	rm -f ./test.output ./test.stderr ./test.stdout

see:: ${TTMPY}
	python3 ${TESTCMD}

debug:: ${TTMPY}
	python3 ${PDBCMD}
