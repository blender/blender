# Microsoft Developer Studio Project File - Name="bsplib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=bsplib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "bsplib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "bsplib.mak" CFG="bsplib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "bsplib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "bsplib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "bsplib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\intern\bsp\"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\intern\bsp\"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /Ob2 /I "../../../../lib/windows/memutil/include" /I "../.." /I "../../../../lib/windows/moto/include" /I "../../../../lib/windows/container/include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x413 /d "NDEBUG"
# ADD RSC /l 0x413 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\intern\bsp\libbsp.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO Copying header files	XCOPY /E ..\..\extern\*.h ..\..\..\..\lib\windows\bsp\include\	ECHO Copying lib	XCOPY /E ..\..\..\..\obj\windows\intern\bsp\*.lib ..\..\..\..\lib\windows\bsp\lib\*.a	ECHO Done
# End Special Build Tool

!ELSEIF  "$(CFG)" == "bsplib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\intern\bsp\debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\intern\bsp\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../../../../lib/windows/memutil" /I "../.." /I "../../../../lib/windows/moto/include" /I "../../../../lib/windows/container/include" /I "../../../../lib/windows/memutil/include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x413 /d "_DEBUG"
# ADD RSC /l 0x413 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\intern\bsp\debug\libbsp.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO Copying header files	XCOPY /E ..\..\extern\*.h ..\..\..\..\lib\windows\bsp\include\	ECHO Copying lib	XCOPY /E ..\..\..\..\obj\windows\intern\bsp\debug\*.lib ..\..\..\..\lib\windows\bsp\lib\debug\*.a	ECHO Copying Debug info.	XCOPY /E ..\..\..\..\obj\windows\intern\bsp\debug\vc60.* ..\..\..\..\lib\windows\bsp\lib\debug\	ECHO Done
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "bsplib - Win32 Release"
# Name "bsplib - Win32 Debug"
# Begin Group "intern"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\intern\BSP_CSGException.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_CSGHelper.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_CSGHelper.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_CSGISplitter.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_CSGMesh.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_CSGMesh.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_CSGMesh_CFIterator.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_CSGMeshBuilder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_CSGMeshBuilder.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_CSGMeshSplitter.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_CSGMeshSplitter.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_CSGNCMeshSplitter.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_CSGNCMeshSplitter.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_CSGUserData.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_CSGUserData.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_FragNode.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_FragNode.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_FragTree.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_FragTree.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_MeshFragment.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_MeshFragment.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_MeshPrimitives.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_MeshPrimitives.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_Triangulate.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BSP_Triangulate.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_BooleanOps.cpp
# End Source File
# End Group
# Begin Group "extern"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\extern\CSG_BooleanOps.h
# End Source File
# End Group
# End Target
# End Project
