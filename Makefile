# This Makefile just invokes the corresponding 
# targets in the per-language subdirectories.

.PHONEY: check
.PHONEY: clean

all:: check

check::
	cd ./src/C; make check
	cd ./src/Python; make check
	
clean::
	cd ./src/C; make clean
	cd ./src/Python; make clean
	cd ./src/Java; make clean

# Generate a directory called "git" that contains everything in the Manifest
git::
	sh ./git.sh
