@echo off
setlocal
cd /d "%~dp0"
pushd ..\limpeza-windows
if errorlevel 1 exit /b 1
call compilar.bat
if errorlevel 1 goto falha
popd
pushd ..\registro-windows
if errorlevel 1 exit /b 1
call compilar.bat
if errorlevel 1 goto falha
popd
pushd ..\arquivos-windows
if errorlevel 1 exit /b 1
call compilar.bat
if errorlevel 1 goto falha
popd
call compilar.bat
exit /b %errorlevel%
:falha
popd
exit /b 1
