# Microsoft Developer Studio Project File - Name="solid" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=solid - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "solid.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "solid.mak" CFG="solid - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "solid - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "solid - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "solid - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
LINK32=cwlink.exe
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../../include" /I "../../src/convex" /I "../../src/complex" /D "NDEBUG" /D "QHULL" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /FD /c
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
PostBuild_Cmds=XCOPY   /Y   ..\..\include\SOLID*.h   ..\..\..\..\..\lib\windows\solid\include\solid\  	XCOPY   /Y   Release\*.lib   ..\..\..\..\..\lib\windows\solid\lib\ 
# End Special Build Tool

!ELSEIF  "$(CFG)" == "solid - Win32 Debug"

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
LINK32=cwlink.exe
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MT /W3 /GX /Zd /Od /I "../../include" /I "../../src/convex" /I "../../src/complex" /D "_DEBUG" /D "QHULL" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
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
PostBuild_Cmds=XCOPY   /Y   ..\..\include\SOLID*.h   ..\..\..\..\..\lib\windows\solid\include\solid\  	XCOPY   /Y   Debug\*.lib   ..\..\..\..\..\lib\windows\solid\lib\Debug\ 
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "solid - Win32 Release"
# Name "solid - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\..\src\DT_C-api.cpp"
# End Source File
# Begin Source File

SOURCE=..\..\src\DT_Encounter.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\DT_Object.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\DT_RespTable.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\DT_Scene.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\src\DT_AlgoTable.h
# End Source File
# Begin Source File

SOURCE=..\..\src\DT_Encounter.h
# End Source File
# Begin Source File

SOURCE=..\..\src\DT_Object.h
# End Source File
# Begin Source File

SOURCE=..\..\src\DT_Response.h
# End Source File
# Begin Source File

SOURCE=..\..\src\DT_RespTable.h
# End Source File
# Begin Source File

SOURCE=..\..\src\DT_Scene.h
# End Source File
# End Group
# End Target
# End Project
