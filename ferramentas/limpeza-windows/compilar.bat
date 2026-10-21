@echo off
setlocal
cd /d "%~dp0"
where cl >nul 2>nul
if errorlevel 1 (
  echo Abra o x64 Native Tools Command Prompt do Visual Studio.
  exit /b 1
)
cl /nologo /std:c++17 /EHsc /W4 /MT /LD limpeza.cpp /Fe:Limpeza.dll /link /DEF:limpeza.def shell32.lib ole32.lib uuid.lib user32.lib
if errorlevel 1 exit /b 1
echo DLL compilada. Nao ha limpeza automatica durante a compilacao.
echo Para executar: rundll32.exe "%CD%\Limpeza.dll",Limpar
