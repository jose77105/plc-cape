@echo off
echo Setting DEV_SRC_DIR and DEV_BIN_DIR
echo Note: it requires the 'setx.exe' tool to be in the PATH
REM 	http://stackoverflow.com/questions/17802788/will-setx-command-work-in-windows-xp
setx DEV_SRC_DIR X:\
setx DEV_BIN_DIR Y:\
echo Variables set
pause
