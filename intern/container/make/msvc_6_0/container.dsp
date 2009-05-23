# Microsoft Developer Studio Project File - Name="container" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=container - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "container.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "container.mak" CFG="container - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "container - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "container - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "container - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\intern\container"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\intern\container"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /Ob2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x413 /d "NDEBUG"
# ADD RSC /l 0x413 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../../../../obj/windows/intern/container/libcontainer.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO Copying header files	XCOPY /Y ..\..\*.h ..\..\..\..\..\lib\windows\container\include\	ECHO Copying lib	XCOPY /Y ..\..\..\..\obj\windows\intern\container\*.lib ..\..\..\..\..\lib\windows\container\lib\*.a	ECHO Done
# End Special Build Tool

!ELSEIF  "$(CFG)" == "container - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\intern\container\debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\intern\container\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x413 /d "_DEBUG"
# ADD RSC /l 0x413 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\intern\container\debug\libcontainer.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO Copying header files	XCOPY /Y ..\..\*.h ..\..\..\..\..\lib\windows\container\include\	ECHO Copying lib	XCOPY /Y ..\..\..\..\obj\windows\intern\container\debug\*.lib ..\..\..\..\..\lib\windows\container\lib\debug\*.a	ECHO Copying Debug info.	XCOPY /Y ..\..\..\..\obj\windows\intern\container\debug\vc60.* ..\..\..\..\..\lib\windows\container\lib\debug\	ECHO Done
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "container - Win32 Release"
# Name "container - Win32 Debug"
# Begin Group "intern"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\intern\CTR_List.cpp

!IF  "$(CFG)" == "container - Win32 Release"

# ADD CPP /I "../extern" /I "../../"

!ELSEIF  "$(CFG)" == "container - Win32 Debug"

# ADD CPP /I "../../"

!ENDIF 

# End Source File
# End Group
# Begin Source File

SOURCE=..\..\CTR_List.h
# End Source File
# Begin Source File

SOURCE=..\..\CTR_Map.h
# End Source File
# Begin Source File

SOURCE=..\..\CTR_TaggedIndex.h
# End Source File
# Begin Source File

SOURCE=..\..\CTR_TaggedSetOps.h
# End Source File
# Begin Source File

SOURCE=..\..\CTR_UHeap.h
# End Source File
# End Target
# End Project
