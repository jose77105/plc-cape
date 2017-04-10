strComputer = "."
Set objWMIService = GetObject("winmgmts:\\" & strComputer & "\root\cimv2")
Set colItems = objWMIService.ExecQuery("Select * from Win32_Share",,48)
For Each objItem in colItems
    Wscript.Echo "Shared resource '" & objItem.Name & "'" & vbCrLf & vbCrLf & _
		"AccessMask: " & objItem.AccessMask & vbCrLf & _
		"AllowMaximum: " & objItem.AllowMaximum & vbCrLf & _
		"Caption: " & objItem.Caption & vbCrLf & _
		"Description: " & objItem.Description & vbCrLf & _
		"InstallDate: " & objItem.InstallDate & vbCrLf & _
		"MaximumAllowed: " & objItem.MaximumAllowed & vbCrLf & _
		"Path: " & objItem.Path & vbCrLf & _
		"Status: " & objItem.Status & vbCrLf & _
		"Type: " & objItem.Type
Next

Set colItems = objWMIService.ExecQuery("Select * from Win32_SecurityDescriptor",,48)
For Each objItem in colItems
    Wscript.Echo "Security Descriptor" & vbCrLf & vbCrLf & _
		"ControlFlags: " & objItem.ControlFlags & vbCrLf & _
		"DACL: " & objItem.DACL & vbCrLf & _
		"Group: " & objItem.Group & vbCrLf & _
		"Owner: " & objItem.Owner & vbCrLf & _
		"SACL: " & objItem.SACL & vbCrLf & _
		"TIME_CREATED: " & objItem.TIME_CREATED
Next