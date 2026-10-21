@echo off
setlocal
cd /d "%~dp0"
where cl >nul 2>nul
if errorlevel 1 (
  echo Abra o x64 Native Tools Command Prompt do Visual Studio.
  exit /b 1
)
cl /nologo /std:c++20 /EHsc /W4 /MT /LD Files.cpp /Fe:Files.dll /link shell32.lib ole32.lib uuid.lib user32.lib advapi32.lib
if errorlevel 1 exit /b 1
echo Files.dll compilada. A compilacao nao copia arquivos.
