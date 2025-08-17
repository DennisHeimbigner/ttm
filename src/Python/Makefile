.PHONEY: check
.PHONEY: clean

TTMPY = ttm3.py # Python 3 version
TTM2PY = ttm2.py # Python 2 version; no longer supported.

TTMCMD = python ./${TTMPY}
PDBCMD   = -m pdb ./${TTMPY}

# Non-flag arguments to ttm.py
TESTARGS = a b c

check:: test_all.log test_misc.log
	diff -wBb ../C/test_all.baseline test_all.log
	diff -wBb ../C/test_misc.baseline test_misc.log

test_all.log: ../C/test_all.ttm ${TTMPY}
	${TTMCMD} -p ../C/test_all.ttm -f ../C/test.rs -\& -Pshowcall -T -d1 -dt ${TESTARGS} > $@

test_misc.log: ../C/test_misc.log ${TTMPY}
	${TTMCMD} -p ../C/test_misc.ttm -f ../C/test.rs -\& -Pshowcall -T -d1 -dt ${TESTARGS} > $@

clean:: cmake-clean
	rm -f test_all.log
	rm -f test_misc.log

debug:: ${TTMPY}
	python3 ${PDBCMD}
