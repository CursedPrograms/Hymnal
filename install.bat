@echo off
setlocal
cd /d "%~dp0"
rem Installs Hymnal for this user: %LOCALAPPDATA%\Programs\Hymnal, with Start menu and Desktop shortcuts.
rem Run build.bat first (or unzip a release and run this beside it).
set "SRC=dist\Hymnal"
if exist "Hymnal.exe" set "SRC=."
if not exist "%SRC%\Hymnal.exe" ( echo Build it first: build.bat & exit /b 1 )
tasklist /fi "imagename eq Hymnal.exe" | find /i "Hymnal.exe" >nul
if not errorlevel 1 ( echo Hymnal is open: close it and run this again. & exit /b 1 )
set "INST=%LOCALAPPDATA%\Programs\Hymnal"
if not exist "%INST%" mkdir "%INST%"
xcopy /e /i /q /y "%SRC%\*" "%INST%\" >nul || ( echo Couldn't copy to %INST% & exit /b 1 )
powershell -NoProfile -Command "$s=New-Object -ComObject WScript.Shell; foreach($d in @([Environment]::GetFolderPath('Programs'),[Environment]::GetFolderPath('Desktop'))){ $l=$s.CreateShortcut((Join-Path $d 'Hymnal.lnk')); $l.TargetPath='%INST%\Hymnal.exe'; $l.WorkingDirectory='%INST%'; $l.IconLocation='%INST%\Hymnal.exe,0'; $l.Description='Hymnal: songs with vocals, made on your own PC'; $l.Save() }"
echo Installed: %INST%
if /i not "%~1"=="--quiet" echo Start it from the Start menu or the Desktop.
exit /b 0
