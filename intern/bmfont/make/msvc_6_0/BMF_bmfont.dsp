# Microsoft Developer Studio Project File - Name="BMF_bmfont" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=BMF_bmfont - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BMF_bmfont.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BMF_bmfont.mak" CFG="BMF_bmfont - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BMF_bmfont - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "BMF_bmfont - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BMF_bmfont - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "../../../../../obj/windows/intern/BMF_bmfont/"
# PROP Intermediate_Dir "../../../../../obj/windows/intern/BMF_bmfont/"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../.." /I "../../intern" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Desc=Copying BMFONT files library (release target) to lib tree.
PostBuild_Cmds=ECHO Copying header files	COPY "..\..\*.h" "..\..\..\..\..\develop\lib\windows\bmfont\include\"	ECHO Copying lib	COPY "..\..\..\..\..\obj\windows\intern\BMF_bmfont\BMF_bmfont.lib" "..\..\..\..\..\develop\lib\windows\bmfont\lib\libbmfont.a"	ECHO Done
# End Special Build Tool

!ELSEIF  "$(CFG)" == "BMF_bmfont - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "../../../../../obj/windows/intern/BMF_bmfont/debug"
# PROP Intermediate_Dir "../../../../../obj/windows/intern/BMF_bmfont/debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../.." /I "../../intern" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Desc=Copying BMFONT files library (debug target) to lib tree.
PostBuild_Cmds=ECHO Copying header files	COPY "..\..\*.h" "..\..\..\..\..\develop\lib\windows\bmfont\include"	ECHO Copying lib	COPY "..\..\..\..\..\obj\windows\intern\BMF_bmfont\debug\BMF_bmfont.lib" "..\..\..\..\..\develop\lib\windows\bmfont\lib\debug\libbmfont.a"	ECHO Done
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "BMF_bmfont - Win32 Release"
# Name "BMF_bmfont - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\intern\BMF_Api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BMF_BitmapFont.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BMF_font_helv10.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BMF_font_helv12.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BMF_font_helvb10.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BMF_font_helvb12.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BMF_font_helvb14.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BMF_font_helvb8.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BMF_font_scr12.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BMF_font_scr14.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BMF_font_scr15.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "intern"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\intern\BMF_BitmapFont.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BMF_FontData.h
# End Source File
# End Group
# Begin Group "extern"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\BMF_Api.h
# End Source File
# Begin Source File

SOURCE=..\..\BMF_Fonts.h
# End Source File
# Begin Source File

SOURCE=..\..\BMF_Settings.h
# End Source File
# End Group
# End Group
# End Target
# End Project
