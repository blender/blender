# Microsoft Developer Studio Project File - Name="memutil" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=memutil - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "memutil.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "memutil.mak" CFG="memutil - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "memutil - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "memutil - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "memutil - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\intern\memutil\"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\intern\memutil\"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /Ob2 /I "../../" /I "../../../" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
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
PostBuild_Cmds=ECHO Copying header files	XCOPY /Y ..\..\*.h ..\..\..\..\..\lib\windows\memutil\include\	ECHO Copying lib	XCOPY /Y ..\..\..\..\obj\windows\intern\memutil\*.lib ..\..\..\..\..\lib\windows\memutil\lib\*.a	ECHO Done
# End Special Build Tool

!ELSEIF  "$(CFG)" == "memutil - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\intern\memutil\debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\intern\memutil\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../../" /I "../../../" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
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
PostBuild_Cmds=ECHO Copying header files	XCOPY /Y ..\..\*.h ..\..\..\..\..\lib\windows\memutil\include\	ECHO Copying lib	XCOPY /Y ..\..\..\..\obj\windows\intern\memutil\debug\*.lib ..\..\..\..\..\lib\windows\memutil\lib\debug\*.a	ECHO Copying Debug info.	XCOPY /Y ..\..\..\..\obj\windows\intern\memutil\debug\vc60.* ..\..\..\..\..\lib\windows\memutil\lib\debug\	ECHO Done
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "memutil - Win32 Release"
# Name "memutil - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\MEM_Allocator.h
# End Source File
# Begin Source File

SOURCE=..\..\MEM_CacheLimiter.h
# End Source File
# Begin Source File

SOURCE="..\..\intern\MEM_CacheLimiterC-Api.cpp"
# End Source File
# Begin Source File

SOURCE=..\..\..\guardedalloc\MEM_guardedalloc.h
# End Source File
# Begin Source File

SOURCE="..\..\intern\MEM_RefCountedC-Api.cpp"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "intern"

# PROP Default_Filter ""
# End Group
# Begin Group "extern"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\MEM_NonCopyable.h
# End Source File
# Begin Source File

SOURCE=..\..\MEM_RefCounted.h
# End Source File
# Begin Source File

SOURCE="..\..\MEM_RefCountedC-Api.h"
# End Source File
# Begin Source File

SOURCE=..\..\MEM_RefCountPtr.h
# End Source File
# Begin Source File

SOURCE=..\..\MEM_SmartPtr.h
# End Source File
# End Group
# End Group
# End Target
# End Project
