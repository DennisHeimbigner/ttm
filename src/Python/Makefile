TTMPY = ttm3.py # Python 3 version
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
	python3 ${TTMPROG} ${TTMCMD} 2> ./test.stderr > ./test.stdout
	cat ./test.stderr ./test.stdout > test.output
	diff -w ${TESTBASELINE} ../../C/test.output

clean::
	rm -f ./test.output ./test.stderr ./test.stdout

debug:: ${TTMPY}
	python3 ${PDBCMD}
