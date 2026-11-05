@echo off
rem Builds greyhound_offline.exe (MSVC build tools only, no MFC). Usage: build_offline_cli.bat [output dir]
setlocal
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set OUT=%~1
if "%OUT%"=="" set OUT=%TEMP%\hh_tests
if not exist "%OUT%" mkdir "%OUT%"
cl /nologo /EHsc /std:c++14 /O2 /Fo"%OUT%\\" /Fe"%OUT%\greyhound_offline.exe" "%~dp0OfflineCli.cpp" || exit /b 1
