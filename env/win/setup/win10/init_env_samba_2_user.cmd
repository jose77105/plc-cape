@REM This batch needs to be executed from the current user account

@REM /** @brief Share the folders where the source code and the products of building reside */

@echo off
echo Mapping drivers...
net use X: \\localhost\plc-cape-src
net use Y: \\localhost\plc-cape-bin
echo Locally available through drives X: and Y:
pause
