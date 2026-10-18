@echo off
chcp 65001 >nul
cd /d "%~dp0"
where node >nul 2>nul
if errorlevel 1 (
  echo Instale o Node.js 24 LTS e abra novamente este arquivo.
  pause
  exit /b 1
)
if not exist "config.json" (
  copy /y "config.example.json" "config.json" >nul
  echo Preencha token e guildId no config.json. Salve e feche o editor.
  start /wait notepad "config.json"
)
if not exist "node_modules\discord.js\package.json" (
  call npm.cmd ci
  if errorlevel 1 (
    echo Nao foi possivel instalar as dependencias.
    pause
    exit /b 1
  )
)
node index.js
if errorlevel 1 (
  echo O bot foi encerrado com erro. Confira a mensagem acima.
  pause
  exit /b 1
)
