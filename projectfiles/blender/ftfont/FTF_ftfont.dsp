# Microsoft Developer Studio Project File - Name="FTF_ftfont" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=FTF_ftfont - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "FTF_ftfont.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "FTF_ftfont.mak" CFG="FTF_ftfont - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "FTF_ftfont - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "FTF_ftfont - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "FTF_ftfont - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\blender\ftfont"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\ftfont"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "../../../source\blender\makesdna" /I "../../../source\blender\blenloader" /I "../../../source\blender\blenlib" /I "../../../../lib/windows/ftgl/include" /I "../../../../lib/windows/freetype/include" /I "../../../lib/windows/iconv/include" /I "../../../../lib/windows/gettext/include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "WCHAR_T16" /D "HAVE_LC_MESSAGES" /YX /FD /c
# ADD BASE RSC /l 0x411 /d "NDEBUG"
# ADD RSC /l 0x411 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "FTF_ftfont - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\blender\ftfont"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\ftfont"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "../../../source\blender\blenlib" /I "../../../source\blender\makesdna" /I "../../../../lib/windows/ftgl/include" /I "../../../../lib/windows/freetype/include" /I "../../../lib/windows/iconv/include" /I "../../../../lib/windows/gettext/include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "WCHAR_T16" /D "HAVE_LC_MESSAGES" /D "FTGL_LIBRARY_STATIC" /YX /FD /GZ /c
# ADD BASE RSC /l 0x411 /d "_DEBUG"
# ADD RSC /l 0x411 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "FTF_ftfont - Win32 Release"
# Name "FTF_ftfont - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\blender\ftfont\intern\FTF_Api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\ftfont\intern\FTF_TTFont.cpp
# End Source File
# End Group
# Begin Group "extern"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\source\blender\ftfont\FTF_Api.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\ftfont\FTF_Settings.h
# End Source File
# End Group
# Begin Group "intern"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\source\blender\ftfont\intern\FTF_TTFont.h
# End Source File
# End Group
# End Target
# End Project
