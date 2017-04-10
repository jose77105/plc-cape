@echo off
REM To get list of current 'SUBST' units execute just 'SUBST'
REM To remove a 'SUBST' unit use 'SUBST /d X:'
echo Mapping X: and Y:...
subst X: J:\dev\src\beaglebone\plc-cape
subst Y: K:\dev\bin\beaglebone\plc-cape-bbb
echo Mapping done
pause
