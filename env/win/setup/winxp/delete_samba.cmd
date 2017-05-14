@echo off
echo Sharing resources...
net use X: /delete
net use Y: /delete
net share /delete plc-cape-src
net share /delete plc-cape-bin
echo Resources deleted
pause
