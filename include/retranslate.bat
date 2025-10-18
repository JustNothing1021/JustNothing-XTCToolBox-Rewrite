
@echo off
chcp 65001
cls
setlocal enabledelayedexpansion
set "src=widget_ui"
set "dst=widgets\custom"
set "uic=C:\Program Files (x86)\Qt Designer\uic.exe"
set "suffix=h"
if not exist "%dst%" mkdir "%dst%"
if not exist "%src%" (
    echo Source folder "%src%" not found!
    echo.
    echo.
    timeout /t 5 /nobreak > nul
    exit /b
)
if not exist "%uic%" (
    echo uic.exe not found!
    echo.
    timeout /t 5 /nobreak > nul
    exit /b
)
set /a total=0
for %%i in ("%src%\*.ui") do set /a total+=1
set /a processing=0
for /f %%i in ('powershell -Command "Get-Date -Uformat %%s"') do set "start=%%i"
for %%i in ("%src%\*.ui") do (
	set /a processing+=1
	for /f "tokens=1 delims=." %%j in ("%%i") do set "fileName=%%j"
	echo. | set /p dummy="translating - %%i"
    for /f %%j in ("!fileName!") do set "fileNameWithoutPath=%%~nj"
	"%uic%" "%%i" > "%dst%\!fileNameWithoutPath!.%suffix%"
	echo. - finished ^(!processing!/!total!^)
)
for /f %%i in ('powershell -Command "Get-Date -Uformat %%s"') do set "end=%%i"
for /f %%i in ('powershell -Command "[Math]::Round(!end! - !start!, 3)"') do set "time=%%i"
echo.
echo.
echo. Task completed. Elapsed !time! seconds.
echo.
echo.
timeout /t 5 /nobreak > nul
