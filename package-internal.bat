@echo off
echo Building Ermine Engine for Internal Distribution...

REM Set variables
set BUILD_CONFIG=Editor-Release
set DIST_DIR=dist\Internal
set VERSION=DEV-%date:~-4,4%%date:~-10,2%%date:~-7,2%

REM Clean previous build
rmdir /S /Q %DIST_DIR% 2>nul
mkdir %DIST_DIR%

REM Build the solution
echo Building solution...
msbuild Ermine.sln /p:COnfiguration=%BUILD_CONFIG% /p:Platform=x64 /m

REM Copy project folders
echo Copying project folders...
xcopy /Y /E /I Build\bin\%BUILD_CONFIG%-windows-x86_64\Ermine-Editor %DIST_DIR%\Ermine-Editor\
xcopy /Y /E /I Build\bin\%BUILD_CONFIG%-windows-x86_64\Ermine-Game.lion_rcdbase %DIST_DIR%\Ermine-Game.lion_rcdbase\
xcopy /Y /E /I Build\bin\%BUILD_CONFIG%-windows-x86_64\recastnavigation %DIST_DIR%\recastnavigation\
xcopy /Y /E /I Build\bin\%BUILD_CONFIG%-windows-x86_64\Resources %DIST_DIR%\Resources\

REM Copy assets (if needed for editor)
REM xcopy /Y /E /I Assets %DIST_DIR%\Assets\
echo Copying dll to root...
xcopy /Y Build\bin\%BUILD_CONFIG%-windows-x86_64\Ermine-Engine\Ermine-Engine.dll %DIST_DIR%\

REM Create version file
echo Version: %VERSION% > %DIST_DIR%\VERSION.txt
echo Build Date: %date% %time% >> %DIST_DIR%\VERSION.txt

REM Create ZIP archive
echo Creating archive...
powershell Compress-Archive -Path %DIST_DIR%\* -DestinationPath ErmineEngine-Internal-%VERSION%.zip -Force

echo.
echo ========================================
echo Package created: ErmineEngine-Internal-%VERSION%.zip
echo ========================================
pause