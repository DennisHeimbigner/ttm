TTMPY = ttm.py # Python 3 version
TTM2PY = ttm2.py # Python 2 version; no longer supported.

TESTPROG = ../../tests/test.ttm
#TESTPROG = ./t.ttm
TESTRS = ../../tests/test.rs
TESTBASELINE = ../../tests/test.baseline

# Arguments to ttm.py
TTMPROG = -p ${TESTPROG}
TTMRFLAG = -f ${TESTRS}
TTMDEBUG = -dT
TTMTRACE = -dt
TTMARGS = a b c
TTMCMD = ${TTMPROG} ${TTMRFLAG} ${TTMDEBUG} ${TTMARGS}
TTMTRACECMD = ${TTMPROG} ${TTMRFLAG} ${TTMTRACE} ${TTMARGS}

TESTCMD = ./${TTMPY} ${TTMCMD}
TRACECMD = ./${TTMPY} ${TTMTRACECMD}
PDBCMD   = -m pdb ${TESTCMD}

check:: ${TTMPY}
	rm -f ./test.output
	python3 ${TESTCMD} &> ./test.output
	diff -w ${TESTBASELINE} ./test.output

see:: ${TTMPY}
	python3 ${TESTCMD}

trace:: ${TTMPY}
	python3 ${TRACECMD}

debug:: ${TTMPY}
	python3 ${PDBCMD}
