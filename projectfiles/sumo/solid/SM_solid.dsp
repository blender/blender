# Microsoft Developer Studio Project File - Name="SM_solid" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=SM_solid - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "SM_solid.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "SM_solid.mak" CFG="SM_solid - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "SM_solid - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "SM_solid - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "SM_solid - Win32 MT DLL Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "SM_solid - Win32 MT DLL Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "SM_solid - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\sumo\solid"
# PROP Intermediate_Dir "..\..\..\obj\windows\sumo\solid"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\source\gameengine\physics\sumo\SOLID-3.0\include" /I "../../../../lib/windows/moto/include" /I "..\..\..\source\gameengine\physics\sumo\include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /J /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SM_solid - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\sumo\solid\debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\sumo\solid\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\..\source\gameengine\physics\sumo\SOLID-3.0\include" /I "../../../../lib/windows/moto/include" /I "..\..\..\source\gameengine\physics\sumo\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SM_solid - Win32 MT DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "SM_solid___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "SM_solid___Win32_MT_DLL_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\sumo\solid\mtdll_debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\sumo\solid\mtdll_debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /Gm /GX /ZI /Od /I "..\..\..\source\sumo\SOLID-3.0\include" /I "../../../../../lib/windows/moto/include" /I "..\..\..\source\sumo\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /I "..\..\..\source\gameengine\physics\sumo\SOLID-3.0\include" /I "../../../../lib/windows/moto/include" /I "..\..\..\source\gameengine\physics\sumo\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\sumo\solid\debug\SM_solid.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SM_solid - Win32 MT DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "SM_solid___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "SM_solid___Win32_MT_DLL_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\sumo\solid\mtdll"
# PROP Intermediate_Dir "..\..\..\obj\windows\sumo\solid\mtdll"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /GX /O2 /I "..\..\..\source\sumo\SOLID-3.0\include" /I "../../../../../lib/windows/moto/include" /I "..\..\..\source\sumo\include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD CPP /nologo /MD /GX /O2 /I "..\..\..\source\gameengine\physics\sumo\SOLID-3.0\include" /I "../../../../lib/windows/moto/include" /I "..\..\..\source\gameengine\physics\sumo\include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\sumo\solid\SM_solid.lib"
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "SM_solid - Win32 Release"
# Name "SM_solid - Win32 Debug"
# Name "SM_solid - Win32 MT DLL Debug"
# Name "SM_solid - Win32 MT DLL Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\BBoxTree.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\Box.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\Complex.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\Cone.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\Convex.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\Cylinder.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\DT_BP_C-api.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\DT_BP_Endpoint.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\DT_BP_Proxy.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\DT_BP_Scene.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\DT_C-api.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\DT_DoubleBase.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\DT_Encounter.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\DT_FloatBase.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\DT_LineSegment.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\DT_Object.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\DT_RespTable.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\DT_Scene.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\Polyhedron.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\Polytope.cpp"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\src\Sphere.cpp"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\AlgoTable.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\BBoxTree.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\Box.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\Complex.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\Cone.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\Convex.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\Cylinder.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\DT_AABBox.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\DT_BBox.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\DT_BP_Endpoint.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\DT_BP_Proxy.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\DT_BP_Scene.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\DT_DoubleBase.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\DT_Encounter.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\DT_FloatBase.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\DT_LineSegment.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\DT_Object.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\DT_Response.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\DT_RespTable.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\DT_Scene.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\DT_VertexBase.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\DT_VertexBased.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\IndexArray.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\Polyhedron.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\Polytope.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\Shape.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Physics\Sumo\SOLID-3.0\include\Sphere.h"
# End Source File
# End Group
# End Target
# End Project
