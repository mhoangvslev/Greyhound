@echo off
rem Builds and runs the standalone headless-mode unit tests (MSVC build tools only, no MFC needed).
setlocal
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not exist "%TEMP%\hh_tests" mkdir "%TEMP%\hh_tests"
cl /nologo /EHsc /std:c++17 /Fo"%TEMP%\hh_tests\\" /Fe"%TEMP%\hh_tests\t.exe" "%~dp0HeadlessArgsTests.cpp" || exit /b 1
"%TEMP%\hh_tests\t.exe"
