@echo off
setlocal
cd /d "%~dp0"
where cl >nul 2>nul
if errorlevel 1 (
  echo Abra o x64 Native Tools Command Prompt do Visual Studio.
  exit /b 1
)
cl /nologo /std:c++17 /EHsc /W4 /MT /LD registro.cpp /Fe:Registro.dll /link /DEF:registro.def advapi32.lib user32.lib
if errorlevel 1 exit /b 1
echo DLL compilada. A compilacao nao executa a limpeza.
