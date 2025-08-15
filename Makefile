# This Makefile just invokes the corresponding 
# targets in the per-language subdirectories.

.PHONEY: check
.PHONEY: clean

all:: check

check::
	cd ./src/C; make check
	cd ./src/Python; make check
	
clean::
	cd ./src/C; make very-clean
	cd ./src/Python; make very-clean

# Generate a directory called "git" that contains everything in the Manifest
git::
	sh ./git.sh
