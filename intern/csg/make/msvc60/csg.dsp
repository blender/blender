# Microsoft Developer Studio Project File - Name="csg" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=csg - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "csg.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "csg.mak" CFG="csg - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "csg - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "csg - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "csg - Win32 Release"

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
# ADD CPP /nologo /G6 /MT /W3 /GX /I "../../intern/blender" /I "../../extern" /I "../../intern" /I "../../../../../lib/windows/memutil/include" /I "../.." /I "../../../../../lib/windows/moto/include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /FD /c
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
PostBuild_Cmds=ECHO Copying header files	XCOPY /E /Y ..\..\extern\*.h ..\..\..\..\..\lib\windows\bsp\include\	ECHO Copying lib	XCOPY /E /Y ..\..\..\..\obj\windows\intern\bsp\*.lib ..\..\..\..\lib\windows\bsp\lib\*.a	ECHO Done
# End Special Build Tool

!ELSEIF  "$(CFG)" == "csg - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\intern\bsp\debug\"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\intern\bsp\debug\"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../../intern/blender" /I "../../extern" /I "../../intern" /I "../../../../../lib/windows/memutil/include" /I "../.." /I "../../../../../lib/windows/moto/include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FD /GZ /c
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
PostBuild_Cmds=ECHO Copying header files	XCOPY /E /Y ..\..\extern\*.h ..\..\..\..\..\lib\windows\bsp\include\	ECHO Copying lib	XCOPY /E /Y ..\..\..\..\obj\windows\intern\bsp\debug\*.lib ..\..\..\..\..\lib\windows\bsp\lib\debug\*.a	ECHO Copying Debug info.	XCOPY /E /Y ..\..\..\..\obj\windows\intern\bsp\debug\vc60.* ..\..\..\..\..\lib\windows\bsp\lib\debug\	ECHO Done
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "csg - Win32 Release"
# Name "csg - Win32 Debug"
# Begin Group "AABBTree"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\intern\CSG_BBox.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_BBoxTree.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_BBoxTree.h
# End Source File
# End Group
# Begin Group "inlines"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\intern\CSG_ConnectedMeshWrapper.inl
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_Math.inl
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_Triangulate.inl
# End Source File
# End Group
# Begin Group "blender"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\intern\blender\CSG_BlenderMesh.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\blender\CSG_BlenderVProp.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_BlenderVProp.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\blender\CSG_CsgOp.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\blender\CSG_CsgOp.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\blender\CSG_Interface.cpp
# End Source File
# Begin Source File

SOURCE=..\..\extern\CSG_Interface.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\blender\CSG_MeshBuilder.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\intern\CSG_BooleanOp.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_BooleanOp.inl
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_ConnectedMesh.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_CVertex.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_GeometryBinder.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_IndexDefs.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_Math.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_Mesh.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_MeshCopier.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_MeshWrapper.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_MeshWrapper.inl
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_Polygon.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_SplitFunction.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_TreeQueries.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_Triangulate.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\CSG_Vertex.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\MT_Line3.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\MT_Line3.h
# End Source File
# End Target
# End Project
