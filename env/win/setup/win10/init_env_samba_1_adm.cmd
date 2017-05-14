@REM This batch needs to be executed from the Administrator account

@REM /** @brief Share the folders where the source code and the products of building reside */

@echo off
echo Sharing resources...
net share plc-cape-src=J:\dev\src\beaglebone\plc-cape /grant:todos,read /grant:mireia,full
net share plc-cape-bin=K:\dev\bin\beaglebone\plc-cape-bbb /grant:todos,full /grant:mireia,full
echo Resources shared as 'plc-cape-src' and 'plc-cape-bin'
pause
