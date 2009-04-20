# Microsoft Developer Studio Project File - Name="KX_network" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=KX_network - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "KX_network.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "KX_network.mak" CFG="KX_network - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "KX_network - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "KX_network - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "KX_network - Win32 MT DLL Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "KX_network - Win32 MT DLL Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "KX_network - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\gameengine\ketsji\network"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\ketsji\network"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GR /GX /O2 /I "..\..\..\..\..\lib\windows\python\include\python2.4" /I "..\..\..\..\..\lib\windows\string\include" /I "..\..\..\..\source\gameengine\ketsji" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\GameLogic" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\Network" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "KX_network - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\gameengine\ketsji\network\debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\ketsji\network\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GR /GX /Zi /Od /I "..\..\..\..\..\lib\windows\python\include\python2.4" /I "..\..\..\..\..\lib\windows\string\include" /I "..\..\..\..\source\gameengine\ketsji" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\GameLogic" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\Network" /D "WIN32" /D "_MBCS" /D "_LIB" /U "_DEBUG" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "KX_network - Win32 MT DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "KX_network___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "KX_network___Win32_MT_DLL_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\gameengine\ketsji\network\mtdll_debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\ketsji\network\mtdll_debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /Gm /GX /ZI /Od /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\GameLogic" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\Network" /I "..\..\..\..\..\lib\windows\python\include\python1.5" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /I "..\..\..\..\..\lib\windows\python\include\python2.2" /I "..\..\..\..\..\lib\windows\string\include" /I "..\..\..\..\source\gameengine\ketsji" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\GameLogic" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\Network" /D "WIN32" /D "_MBCS" /D "_LIB" /U "_DEBUG" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\..\obj\windows\gameengine\ketsji\network\debug\KX_network.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "KX_network - Win32 MT DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "KX_network___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "KX_network___Win32_MT_DLL_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\gameengine\ketsji\network\mtdll"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\ketsji\network\mtdll"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /GX /O2 /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\GameLogic" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\Network" /I "..\..\..\..\..\lib\windows\python\include\python2.0" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD CPP /nologo /MD /GX /O2 /I "..\..\..\..\..\lib\windows\python\include\python2.0" /I "..\..\..\..\..\lib\windows\python\include\python2.2" /I "..\..\..\..\..\lib\windows\string\include" /I "..\..\..\..\source\gameengine\ketsji" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\GameLogic" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\Network" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\..\obj\windows\gameengine\ketsji\network\KX_network.lib"
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "KX_network - Win32 Release"
# Name "KX_network - Win32 Debug"
# Name "KX_network - Win32 MT DLL Debug"
# Name "KX_network - Win32 MT DLL Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\Ketsji\KXNetwork\KX_NetworkEventManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\Ketsji\KXNetwork\KX_NetworkMessageActuator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\Ketsji\KXNetwork\KX_NetworkMessageSensor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\Ketsji\KXNetwork\KX_NetworkObjectActuator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\Ketsji\KXNetwork\KX_NetworkObjectSensor.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\Ketsji\KXNetwork\KX_NetworkEventManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\Ketsji\KXNetwork\KX_NetworkMessageActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\Ketsji\KXNetwork\KX_NetworkMessageSensor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\Ketsji\KXNetwork\KX_NetworkObjectActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\Ketsji\KXNetwork\KX_NetworkObjectSensor.h
# End Source File
# End Group
# End Target
# End Project
