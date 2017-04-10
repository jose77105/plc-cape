Function CreateSharedResource(strName, strPath)
	' The usage of 'Dim' is required to indicate that used variables are local to the 'Function'
	Dim intRC
	Const intType = 0				' 0 = Disk drive, 1 = Print Queue, 2 = Device...
	Const intMaxAllowed = 2			' Maximum number of users allowed to concurrently use this resource
	strDescr = strName
	intRC = objShare.Create(strPath, strName, intType, intMaxAllowed, strDescr)
	CreateSharedResource = intRC
	If intRC <> 0 Then
		WScript.Echo "Error when creating the shared resource at '" & strPath & "'"
	End If
End Function

strComputer = "."

Set objWMI = GetObject("winmgmts:\\" & strComputer & "\root\cimv2")
Set objShare = objWMI.Get("Win32_Share")

intRC = CreateSharedResource("plc-cape-src", "J:\dev\src\beaglebone\plc-cape")
intRC = intRC Or CreateSharedResource("plc-cape-bin", "K:\dev\bin\beaglebone\plc-cape-bbb")
if intRC = 0 Then
	Dim WshNetwork
	Set WshNetwork = WScript.CreateObject("WScript.Network")
	WshNetwork.MapNetworkDrive "X:", "\\127.0.0.1\plc-cape-src", false
	WshNetwork.MapNetworkDrive "Y:", "\\127.0.0.1\plc-cape-bin", false
	WScript.Echo "Shared resources created"
End If