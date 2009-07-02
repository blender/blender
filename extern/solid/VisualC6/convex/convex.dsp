# Microsoft Developer Studio Project File - Name="convex" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=convex - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "convex.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "convex.mak" CFG="convex - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "convex - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "convex - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "convex - Win32 Release"

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
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../../include" /I "../../../qhull/include" /D "NDEBUG" /D "QHULL" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "convex - Win32 Debug"

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
# ADD CPP /nologo /MT /W3 /GX /Zd /Od /I "../../include" /I "../../../qhull/include" /D "_DEBUG" /D "QHULL" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "convex - Win32 Release"
# Name "convex - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\src\convex\DT_Accuracy.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Box.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Cone.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Convex.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Cylinder.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Facet.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_LineSegment.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_PenDepth.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Point.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Polyhedron.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Polytope.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Sphere.cpp
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Triangle.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\src\convex\DT_Accuracy.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Array.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Box.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Cone.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Convex.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Cylinder.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Facet.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_GJK.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Hull.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_IndexArray.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_LineSegment.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Minkowski.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_PenDepth.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Point.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Polyhedron.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Polytope.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Shape.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Sphere.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Transform.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_Triangle.h
# End Source File
# Begin Source File

SOURCE=..\..\src\convex\DT_VertexBase.h
# End Source File
# End Group
# End Target
# End Project
