@echo off
rem Buduje i uruchamia testy (wymaga Visual Studio Build Tools)
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0.."
if not exist build-test mkdir build-test
cl /nologo /std:c++20 /O2 /EHsc /Fobuild-test\ /Fe:build-test\tests.exe test\test_main.cpp src\timing\TimingMap.cpp src\analysis\Analyzer.cpp || exit /b 1
build-test\tests.exe
