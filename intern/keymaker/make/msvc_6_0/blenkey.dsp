# Microsoft Developer Studio Project File - Name="blenkey" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=blenkey - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "blenkey.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "blenkey.mak" CFG="blenkey - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "blenkey - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "blenkey - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "blenkey - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\intern\blenkey\"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\intern\blenkey\"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\.." /I "..\..\..\..\lib\windows\openssl\include" /I "..\..\python" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\intern\blenkey\libblenkey.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO Copying header files	XCOPY /E ..\..\blenkey.h ..\..\..\..\lib\windows\blenkey\include\	ECHO Copying lib	XCOPY /E ..\..\..\..\obj\windows\intern\blenkey\*.lib ..\..\..\..\lib\windows\blenkey\lib\*.a	ECHO Done
# End Special Build Tool

!ELSEIF  "$(CFG)" == "blenkey - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\intern\blenkey\debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\intern\blenkey\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\.." /I "..\..\..\..\lib\windows\openssl\include" /I "..\..\python" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\intern\blenkey\debug\libblenkey.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO Copying header files	XCOPY /E ..\..\blenkey.h ..\..\..\..\lib\windows\blenkey\include\	ECHO Copying lib	XCOPY /E ..\..\..\..\obj\windows\intern\blenkey\debug\*.lib ..\..\..\..\lib\windows\blenkey\lib\debug\*.a
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "blenkey - Win32 Release"
# Name "blenkey - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\key.c
# End Source File
# Begin Source File

SOURCE=..\..\keyloader.c
# End Source File
# Begin Source File

SOURCE=..\..\mt19937int.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "intern"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\key_internal.h
# End Source File
# Begin Source File

SOURCE=..\..\mt19937int.h
# End Source File
# End Group
# Begin Group "extern"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\blenkey.h
# End Source File
# End Group
# End Group
# End Target
# End Project
