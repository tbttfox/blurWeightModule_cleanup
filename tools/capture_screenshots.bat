@echo off
REM Launches GUI Maya and captures README screenshots into docs\images.
REM Uses a throwaway MAYA_APP_DIR so your own Maya prefs are not touched.
REM Build and install the plugins first (quick_compile.bat).
setlocal
if "%MAYA_VERSION%"=="" SET MAYA_VERSION=2026
SET REPO=%~dp0..
SET MAYA_EXE=C:\Program Files\Autodesk\Maya%MAYA_VERSION%\bin\maya.exe
SET MAYA_APP_DIR=%TEMP%\blurWeightModule_screenshots_prefs
SET MAYA_MODULE_PATH=%REPO%\modules;%MAYA_MODULE_PATH%
SET MAYA_NO_HOME=1
SET MAYA_DISABLE_CIP=1
SET MAYA_DISABLE_CER=1
SET MAYA_DISABLE_ADP=1
SET BLURWEIGHT_CAPTURE_SCRIPT=%~dp0capture_screenshots.py
"%MAYA_EXE%" -noAutoloadPlugins -script "%~dp0capture_screenshots.mel"
