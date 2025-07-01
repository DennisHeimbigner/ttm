.PHONEY: check
.PHONEY: clean

git::
	sh ./git.sh

check::
	cd ./src/C; make check
	cd ./src/Python; make check
	cd ./src/Java; make check
	
clean::
	cd ./src/C; make clean
	cd ./src/Python; make clean
	cd ./src/Java; make clean
