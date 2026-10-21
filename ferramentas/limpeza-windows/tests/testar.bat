@echo off
setlocal
cd /d "%~dp0.."
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /MT tests\testes.cpp /Fe:TestesLimpeza.exe /link shell32.lib ole32.lib uuid.lib user32.lib advapi32.lib
if errorlevel 1 exit /b 1
TestesLimpeza.exe "%CD%\Limpeza.dll"
exit /b %errorlevel%
