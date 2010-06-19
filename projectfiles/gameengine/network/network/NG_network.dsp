# Microsoft Developer Studio Project File - Name="NG_network" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=NG_network - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "NG_network.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "NG_network.mak" CFG="NG_network - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "NG_network - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "NG_network - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "NG_network - Win32 MT DLL Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "NG_network - Win32 MT DLL Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "NG_network - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\gameengine\network\network"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\network\network"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GR /GX /O2 /I "..\..\..\..\..\lib\windows\string\include" /I "..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\source\kernel\gen_system" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "NG_network - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\gameengine\network\network\debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\network\network\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GR /GX /Zi /Od /I "..\..\..\..\..\lib\windows\string\include" /I "..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\source\kernel\gen_system" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "NG_network - Win32 MT DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "NG_network___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "NG_network___Win32_MT_DLL_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\gameengine\network\network\mtdll_debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\network\network\mtdll_debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /Gm /GX /ZI /Od /I "..\..\..\..\source\kernel\gen_system" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /I "..\..\..\..\..\lib\windows\string\include" /I "..\..\..\..\source\kernel\gen_system" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\..\obj\windows\gameengine\network\network\debug\NG_network.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "NG_network - Win32 MT DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "NG_network___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "NG_network___Win32_MT_DLL_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\gameengine\network\network\mtdll"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\network\network\mtdll"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /GX /O2 /I "..\..\..\..\source\kernel\gen_system" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD CPP /nologo /MD /GX /O2 /I "..\..\..\..\..\lib\windows\string\include" /I "..\..\..\..\source\kernel\gen_system" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\..\obj\windows\gameengine\network\network\NG_network.lib"
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "NG_network - Win32 Release"
# Name "NG_network - Win32 Debug"
# Name "NG_network - Win32 MT DLL Debug"
# Name "NG_network - Win32 MT DLL Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\Network\NG_NetworkMessage.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\Network\NG_NetworkObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\Network\NG_NetworkScene.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\Network\NG_NetworkDeviceInterface.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\Network\NG_NetworkMessage.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\Network\NG_NetworkObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\Network\NG_NetworkScene.h
# End Source File
# End Group
# End Target
# End Project
