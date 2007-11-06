# Microsoft Developer Studio Project File - Name="BRE_yafray" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=BRE_yafray - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BRE_yafray.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BRE_yafray.mak" CFG="BRE_yafray - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BRE_yafray - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "BRE_yafray - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BRE_yafray - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\blender\yafray"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\yafray"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\misc" /I "..\..\..\source\blender\renderconverter" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\render\intern\include" /I "..\..\..\source\blender\imbuf" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x413 /d "NDEBUG"
# ADD RSC /l 0x413 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BRE_yafray - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\misc" /I "..\..\..\source\blender\renderconverter" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\render\intern\include" /I "..\..\..\source\blender\imbuf" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /Fp"..\..\..\obj\windows\blender\render\debug/BRE_yafray.pch" /YX /Fo"..\..\..\obj\windows\blender\render\debug/" /Fd"..\..\..\obj\windows\blender\render\debug/" /J /FD /GZ /c
# ADD BASE RSC /l 0x413 /d "_DEBUG"
# ADD RSC /l 0x413 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "BRE_yafray - Win32 Release"
# Name "BRE_yafray - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\blender\yafray\intern\api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\yafray\intern\export_File.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\yafray\intern\export_Plugin.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\yafray\intern\yafexternal.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\yafray\intern\yafexternal.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\yafray\intern\yafray_Render.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\source\blender\yafray\intern\export_File.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\yafray\intern\export_Plugin.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\yafray\YafRay_Api.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\yafray\intern\yafray_Render.h
# End Source File
# End Group
# End Target
# End Project
