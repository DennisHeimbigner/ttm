TESTCMD=test.ttm a b c

.PHONEY: check

all: ttm.exe

clean:
	rm -f ttm.exe ttm ttm.txt test.output tmp

ttm.exe: ttm.c
	gcc -Wall -Wdeclaration-after-statement -g -O0 -o ttm ttm.c

ttm.txt::
	rm -f ttm.txt
	gcc -E -Wall -Wdeclaration-after-statement ttm.c > ttm.txt

check:: ttm.exe
	rm -f ./tmp ./test.output
	echo "line1.line2" >>./tmp
	echo "line3" >>./tmp
	./ttm ${TESTCMD} <./tmp >& ./test.output
	diff -w ./test.baseline ./test.output

T1=t.ttm a b c
q0:: ttm.exe
	./ttm ${T1}
qq:: ttm.exe
	gdb --args ttm ${T1}



