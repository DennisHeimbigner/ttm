:: Assume that ttm has been built using Visual Studio
:: and is in ./Debug/ttm.exe pr ./Release/ttm.exe

set top_srcdir=..\..\..
::set top_srcdir=d:\git\ttm

set vcvarsall=c:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat

set TTM=ttm.exe

:: Functions
:: Skip around functions
goto main

:: Clean Function
:clean
del /f ttm.exe
del /f ttm.obj
del /f test.output
del /f test.stderr
del /f test.stdout
exit /B 0

:main

:: Clean
call :clean

:: Compile
call "%vcvarsall%" x86_amd64
echo on
cl /Tc %top_srcdir%\src\C\ttm.c

set TESTPROG=-p %top_srcdir%\tests\test.ttm
set TESTARGS=a b c
set TESTRFLAG=-f %top_srcdir%\tests\test.rs
set TESTTRACE=-dt
set TESTCMD=%TTM% %TESTPROG% %TESTTRACE% %TESTRFLAG% %TESTARGS%

:: Check
%TESTCMD% 2> test.stderr > test.stdout
:: Provide a unified output for diff with baseline
type test.stderr test.stdout 2> NUL: > test.output
diff -w %top_srcdir%\tests\test.baseline test.output

:: Clean
call :clean

exit
