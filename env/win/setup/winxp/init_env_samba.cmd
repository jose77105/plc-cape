@REM /** @brief Share the folders where the source code and the products of building reside */

@echo off
echo Sharing resources...
net share plc-cape-src=J:\dev\src\beaglebone\plc-cape
net share plc-cape-bin-bbb=K:\dev\bin\beaglebone\plc-cape-bbb
net use X: \\127.0.0.1\plc-cape-src
net use Y: \\127.0.0.1\plc-cape-bin-bbb
echo Resources shared as 'plc-cape-src' and 'plc-cape-bin-bbb'
echo Locally available through drives X: and Y:
pause
