@echo off
setlocal
cd /d "%~dp0"
where cl >nul 2>nul
if errorlevel 1 (
  echo Abra o x64 Native Tools Command Prompt do Visual Studio 2022.
  exit /b 1
)
if not exist bin\x64\Release mkdir bin\x64\Release
if not exist obj mkdir obj
rc /nologo /fo obj\FSC.res FSC.rc
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /O2 /MT /DUNICODE /D_UNICODE main.cpp obj\FSC.res /Fo:obj\main.obj /Fe:bin\x64\Release\FSC.exe /link /SUBSYSTEM:WINDOWS /MANIFEST:NO d2d1.lib dwrite.lib ole32.lib user32.lib gdi32.lib
if errorlevel 1 exit /b 1
copy /Y ..\limpeza-windows\Limpeza.dll bin\x64\Release\Limpeza.dll >nul
if errorlevel 1 exit /b 1
copy /Y ..\registro-windows\Registro.dll bin\x64\Release\Registro.dll >nul
if errorlevel 1 exit /b 1
copy /Y ..\arquivos-windows\Files.dll bin\x64\Release\arquivos.dll >nul
if errorlevel 1 exit /b 1
echo Pronto: bin\x64\Release\FSC.exe
