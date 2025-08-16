# TTM Build, Test and Install Instructions

## Implementations

There are two versions of the ttm interpreter in different programming languages:
1. C
2. Python

There is also an obsolete Java implementation. It needs to be updated to match the other implementations.

### 1. C Interpreter

The C interpreter is constructed using the file ttm.c plus a number
of .h include files.  A Makefile exists to create ttm.exe.
A CMakeLists.txt builder also is provided, but note that it is invoked
using "cmake*" rules in the "ttm/src/C/Makefile".

As an aside, there is a rule in src/C/Makefile to create a single source file -- named
unityttm.c -- by replacing the "#include" files with the corresponding file.

At the beginning of ttm.c, there are some directives that control
features of the interpreter.  Currently the only directives are as follows:
1. \#define HAVE_MEMMOVE - define this if your C compiler/library
   supports the memmove() function, undefine otherwise.

You may also need to change the following lines in the Makefile.
1. CC - to specify the C compiler
2. CCWARN - compile time checks
3. CCDEBUG - to set the optimization and debug levels

Otherwise, compiling is as simple as "${CC} -o ttm ttm.c"
where ${CC} is your local C compiler.

Using the Makefile, you can invoke this command:
````
make clean all check
````
The executable -- ttm.exe -- should be in ttm/src/C and it is a self
contained executable.

#### 1.1 Windows Support

There are two methods for doing a Windows build.
1. Using Cmake
2. Using Visual Studio directly

##### The CMake Windows build

The commands necessary for building  the ttm executable in CMake
are embedded in the Makefile. The primary command is "make cmake".
This will create a directory named "build" and then compile and test
ttm in that directory. It should be noted that it is possible to
use cmake to build ttm using gcc (or e.g. clang).

##### The Visual Studio build

It is also possible to build ttm using Visual Studio directly.
To this end, the file ttm.vcproj and ttm.sln are included in the
distribution.

To build, invoke Visual Studio, and open the ttm.vcxproj file as an exsting project.
1. Build the solution, which should put ttm.exe into ttm/src/C/Windows/<arch>/<config>.
    As a rule, arch is "x64" and config is Debug.
2. exit Visual Studio.

The executable -- ttm.exe -- should be in ttm/src/C/Windows/<arch>/<config>
and it is a self contained executable.

In order to use the Makefile, you will need to have either Cygwin or Mingw installed
so that the programs bash, sed, diff, etc. are available.

The Makefile has some parameters -- ARCH and CONFIG.
These are used to locate the ttm.exe created by Visual Studio.

The exact build process may vary slightly depending on the Visual Studio version.
1. Start up Visual Studio
2. Click "Open a project or solution"
3. Select .../ttm/src/C/ttm.vcxproj to open
4. Select menu build->build Solution
5. Exit

This should leave the file ../ttm/src/C/Windows/Debug/ttm.exe.
At this point, it should be possible to execute the command:
````
make installvs
````
This will copy ttm.exe to ttm/src/C/Windows.
Using the Makefile (assuming youre current directory is ttm/src/C).
you can invoke
````
make check
````

### 2. Python Interpreter
The python implementation is contained in the single file
ttm3.py. It has been upgraded to use python version 3.12 (or
presumably later). The older python 2 version is still available
as ttm2.py, but is no longer supported.
Since python is interpretive, ttm3.py does not
need to be compiled per-se, just executed.

To build and test, the command
````
make python-check
````
should be sufficient.

# Change Log

## Nov 15, 2012
* Initial commit

## July 4, 2025
* Convert from using UTF-32 internally to using UTF-8.
* Added a number of new non-standard functions, mostly to support testing.
