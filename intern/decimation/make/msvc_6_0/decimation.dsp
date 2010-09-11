# Microsoft Developer Studio Project File - Name="decimation" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=decimation - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "decimation.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "decimation.mak" CFG="decimation - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "decimation - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "decimation - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "decimation - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\intern\decimation"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\intern\decimation"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /Ob2 /I "..\..\..\..\..\lib\windows\container\include\\" /I "..\..\..\..\..\lib\windows\memutil\include\\" /I "..\..\..\..\..\lib\windows\moto\include\\" /I"..\..\..\moto\include" /I"..\..\..\memutil" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /FD /c
# ADD BASE RSC /l 0x413 /d "NDEBUG"
# ADD RSC /l 0x413 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\intern\decimation\libdecimation.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO Copying header files	XCOPY /Y ..\..\extern\*.h ..\..\..\..\..\lib\windows\decimation\include\	ECHO Copying lib	XCOPY /Y ..\..\..\..\obj\windows\intern\decimation\*.lib ..\..\..\..\..\lib\windows\decimation\lib\*.a	ECHO Done
# End Special Build Tool

!ELSEIF  "$(CFG)" == "decimation - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\intern\decimation\debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\intern\decimation\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W4 /Gm /GX /ZI /Od /I "..\..\..\..\..\lib\windows\container\include\\" /I "..\..\..\..\..\lib\windows\memutil\include\\" /I "..\..\..\..\..\lib\windows\moto\include\\" /I"..\..\..\moto\include" /I"..\..\..\memutil" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FD /GZ /c
# ADD BASE RSC /l 0x413 /d "_DEBUG"
# ADD RSC /l 0x413 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\intern\decimation\debug\libdecimation.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO Copying header files	XCOPY /Y ..\..\extern\*.h ..\..\..\..\..\lib\windows\decimation\include\	ECHO Copying lib	XCOPY /Y ..\..\..\..\obj\windows\intern\decimation\debug\*.lib ..\..\..\..\..\lib\windows\decimation\lib\debug\*.a	ECHO Copying Debug info.	XCOPY /Y ..\..\..\..\obj\windows\intern\decimation\debug\vc60.* ..\..\..\..\..\lib\windows\decimation\lib\debug\	ECHO Done
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "decimation - Win32 Release"
# Name "decimation - Win32 Debug"
# Begin Group "intern"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\intern\LOD_decimation.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\LOD_DecimationClass.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\LOD_EdgeCollapser.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\LOD_EdgeCollapser.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\LOD_ExternBufferEditor.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\LOD_ExternNormalEditor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\LOD_ExternNormalEditor.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\LOD_FaceNormalEditor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\LOD_FaceNormalEditor.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\LOD_ManMesh2.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\LOD_ManMesh2.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\LOD_MeshBounds.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\LOD_MeshException.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\LOD_MeshPrimitives.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\LOD_MeshPrimitives.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\LOD_QSDecimator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\LOD_QSDecimator.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\LOD_Quadric.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\LOD_QuadricEditor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\LOD_QuadricEditor.h
# End Source File
# End Group
# Begin Group "extern"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\extern\LOD_decimation.h
# End Source File
# End Group
# End Target
# End Project
