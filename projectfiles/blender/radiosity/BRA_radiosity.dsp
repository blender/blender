# Microsoft Developer Studio Project File - Name="BRA_radiosity" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=BRA_radiosity - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BRA_radiosity.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BRA_radiosity.mak" CFG="BRA_radiosity - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BRA_radiosity - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "BRA_radiosity - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BRA_radiosity - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\blender\radiosity"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\radiosity"
# PROP Target_Dir ""
LINK32=link.exe -lib
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\radiosity\extern\include" /I "..\..\..\source\blender" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\misc" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\render\intern\include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BRA_radiosity - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\blender\radiosity\debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\radiosity\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\radiosity\extern\include" /I "..\..\..\source\blender" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\misc" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\render\intern\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "BRA_radiosity - Win32 Release"
# Name "BRA_radiosity - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\blender\radiosity\intern\source\raddisplay.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\radiosity\intern\source\radfactors.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\radiosity\intern\source\radio.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\radiosity\intern\source\radnode.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\radiosity\intern\source\radpostprocess.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\radiosity\intern\source\radpreprocess.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\radiosity\intern\source\radrender.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\source\blender\radiosity\extern\include\radio.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\radiosity\extern\include\radio_types.h
# End Source File
# End Group
# End Target
# End Project
