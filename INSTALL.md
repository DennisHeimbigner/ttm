# TTM Installation

## Implementations

There are three versions of the ttm interpreter in different programming languages:
1. C
2. Java
3. Python

### 1. C Interpreter
The C interpreter is deliberately contained in a single,
self contained C file: ttm.c.  A Makefile exists to create
ttm.exe. Use of automake and/or CMake may be attempted in the future.
At the beginning of ttm.c, there are some directives
that control features of the interpreter.  Currently the
only directives are as follows:
1. \#define HAVE_MEMMOVE - define this if your C compiler/library
   supports the memmove() function, undefine otherwise.

You may also need to change the following lines in the Makefile.
1. CC - to specify the C compiler
2. CCWARN - compile time checks
3. CCDEBUG - to set the optimization and debug levels

Otherwise, compiling is as simple as "${CC} -o ttm ttm.c"
where ${CC} is your local C compiler.

#### 1.1 Windows Support

A Windows .vcxproj file is defined in the directory "Windows".
It was built using Microsoft Visual Studio Community 2022
(64-bit) Version 17.11.5.  It currently will build and run and
pass tests.

### 2. Java Interpreter

The Java implementation is contained in the single file
src/main/java/ucar/ttm/TTM.java.  An ant build.xml file exists
to compile it.  The Java version is noticably slower than the C
version.

### 3. Python Interpreter
The python implementation is contained in the single file
ttm.py. It has been upgraded to use python version 3.12 (or
presumably later). The older python 2 version is still available
as ttm2.py, but is no longer supported.
Since python is interpretive, ttm.py does not
need to be compiled per-se, just executed.

## Build and Test

The following Makefile files exist.
1. ttm/src/C/Makefile
2. ttm/src/C/Windows/Makefile
3. ttm/src/Python/Makefile

For C and Python, it should be possible to build and test using this command:
````
make check
````

The Windows build is a bit more complicated, and requires several steps.
The exact process may vary slightly depending on the Visual Studio version.
1. Start up Visual Studio
2. Click "Open a project or solution"
3. Select .../ttm/src/C/Windows/ttm.vcxproj to open
4. Select menu Build->Build Solution
5. Exit

This should leave the file ../ttm/src/C/Windows/Debug/ttm.exe.
At this point, it should be possible to execute the command:
````
make check
````
