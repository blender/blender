# Microsoft Developer Studio Project File - Name="blenpluginapi" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=blenpluginapi - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "blenpluginapi.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "blenpluginapi.mak" CFG="blenpluginapi - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "blenpluginapi - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "blenpluginapi - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "blenpluginapi - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\blender\blenpluginapi"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\blender\blenpluginapi"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\..\source\blender" /I "..\..\..\..\source\blender\blenpluginapi" /I "..\..\..\..\source\blender\blenlib" /I "..\..\..\..\source\blender\imbuf" /I "..\..\..\..\source\blender\makesdna" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\blender\blenpluginapi.lib"

!ELSEIF  "$(CFG)" == "blenpluginapi - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\blender\blenpluginapi\debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\blender\blenpluginapi\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\..\source\blender" /I "..\..\..\..\source\blender\blenpluginapi" /I "..\..\..\..\source\blender\blenlib" /I "..\..\..\..\source\blender\imbuf" /I "..\..\..\..\source\blender\makesdna" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# SUBTRACT CPP /X
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\blender\debug\blenpluginapi.lib"

!ENDIF 

# Begin Target

# Name "blenpluginapi - Win32 Release"
# Name "blenpluginapi - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\..\source\blender\blenpluginapi\intern\pluginapi.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\..\source\blender\blenpluginapi\documentation.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\blenpluginapi\floatpatch.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\blenpluginapi\iff.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\blenpluginapi\plugin.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\blenpluginapi\util.h
# End Source File
# End Group
# End Target
# End Project
