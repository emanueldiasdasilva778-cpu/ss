@echo off
setlocal
cd /d "%~dp0.."
where cl >nul 2>nul
if errorlevel 1 exit /b 1
if not exist tests\bin\incompativel mkdir tests\bin\incompativel
cl /nologo /std:c++20 /utf-8 /EHsc /MT /LD /DFSC_MOCK_KIND=0 tests\mock.cpp /Fo:tests\bin\mock0.obj /Fe:tests\bin\Limpeza.dll
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /utf-8 /EHsc /MT /LD /DFSC_MOCK_KIND=1 tests\mock.cpp /Fo:tests\bin\mock1.obj /Fe:tests\bin\Registro.dll
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /utf-8 /EHsc /MT /LD /DFSC_MOCK_KIND=2 tests\mock.cpp /Fo:tests\bin\mock2.obj /Fe:tests\bin\arquivos.dll
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /utf-8 /EHsc /MT /LD /DFSC_MOCK_KIND=3 tests\mock.cpp /Fo:tests\bin\mock3.obj /Fe:tests\bin\incompativel\Limpeza.dll
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /MT tests\testes.cpp /Fo:tests\bin\testes.obj /Fe:tests\bin\TestesFSC.exe /link d2d1.lib dwrite.lib ole32.lib user32.lib advapi32.lib gdi32.lib shell32.lib
if errorlevel 1 exit /b 1
tests\bin\TestesFSC.exe "%CD%\tests\bin"
exit /b %errorlevel%
