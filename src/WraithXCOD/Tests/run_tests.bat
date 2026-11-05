@echo off
rem Builds and runs the standalone unit tests (MSVC build tools only, no MFC needed).
rem Set GREYHOUND_BO3_ZONE_DIR=<BO3>\zone to also check the retail core_common.ff (read only).
setlocal
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not exist "%TEMP%\hh_tests" mkdir "%TEMP%\hh_tests"
cl /nologo /EHsc /std:c++17 /Fo"%TEMP%\hh_tests\\" /Fe"%TEMP%\hh_tests\t.exe" "%~dp0HeadlessArgsTests.cpp" || exit /b 1
"%TEMP%\hh_tests\t.exe" || exit /b 1
cl /nologo /EHsc /std:c++17 /Fo"%TEMP%\hh_tests\\" /Fe"%TEMP%\hh_tests\o.exe" "%~dp0OfflineTests.cpp" || exit /b 1
"%TEMP%\hh_tests\o.exe"
