@echo off
echo Sharing resources...
net share /delete plc-cape-src
net share /delete plc-cape-bin
echo Resources deleted
pause
