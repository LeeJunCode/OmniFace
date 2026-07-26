@echo off
setlocal
cd /d "%~dp0"

:: 创建必需的运行时目录
if not exist "data\output"    mkdir "data\output"
if not exist "data\upload\video" mkdir "data\upload\video"
if not exist "data\upload\img"   mkdir "data\upload\img"

start "" "OmniFace.exe"
