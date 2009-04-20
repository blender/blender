# Microsoft Developer Studio Project File - Name="boolop" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=boolop - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "boolop.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "boolop.mak" CFG="boolop - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "boolop - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "boolop - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "boolop - Win32 Release"

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
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\source\blender\makesdna" /I "..\..\..\..\source\blender\makesdna\\" /I "..\..\..\..\..\lib\windows\moto\include\\" /I "..\..\..\..\..\lib\windows\container\include\\" /I "..\..\..\..\..\lib\windows\memutil\include\\" /I "../../extern" /I "..\..\..\..\..\lib\windows\guardedalloc\include\\" /I "..\..\..\..\source\blender\blenlib\\" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x407 /d "NDEBUG"
# ADD RSC /l 0x407 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO copy to lib folder	XCOPY /Y .\release\*.lib ..\..\..\..\..\lib\windows\boolop\lib\*.lib
# End Special Build Tool

!ELSEIF  "$(CFG)" == "boolop - Win32 Debug"

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
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\..\..\..\source\blender\makesdna\\" /I "..\..\..\..\..\lib\windows\moto\include\\" /I "..\..\..\..\..\lib\windows\container\include\\" /I "..\..\..\..\..\lib\windows\memutil\include\\" /I "../../extern" /I "..\..\..\..\..\lib\windows\guardedalloc\include\\" /I "..\..\..\..\source\blender\blenlib\\" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x407 /d "_DEBUG"
# ADD RSC /l 0x407 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "boolop - Win32 Release"
# Name "boolop - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\intern\BOP_BBox.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_BSPNode.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_BSPTree.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Edge.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Face.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Face2Face.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Interface.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_MathUtils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Merge.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Mesh.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Segment.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Splitter.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Tag.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Triangulator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Vertex.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\intern\BOP_BBox.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_BSPNode.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_BSPTree.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Chrono.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Edge.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Face.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Face2Face.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Indexs.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_MathUtils.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Merge.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Mesh.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Segment.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Splitter.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Tag.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Triangulator.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\BOP_Vertex.h
# End Source File
# End Group
# End Target
# End Project
