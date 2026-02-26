@echo off
setlocal
set "SOURCE=%~dp0\.templates\Ermine Script (MonoBehaviour).zip"
set "DESTDIR=%USERPROFILE%\Documents\Visual Studio 2022\Templates\ItemTemplates"

if not exist "%DESTDIR%" mkdir "%DESTDIR%"
copy /Y "%SOURCE%" "%DESTDIR%\Ermine Script (MonoBehaviour).zip" >nul

echo Installed Ermine item template. Restart visual studio to see it in the Add New Item dialog.
endlocal