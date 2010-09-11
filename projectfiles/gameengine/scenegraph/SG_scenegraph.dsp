# Microsoft Developer Studio Project File - Name="SG_SceneGraph" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=SG_SceneGraph - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "SG_SceneGraph.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "SG_SceneGraph.mak" CFG="SG_SceneGraph - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "SG_SceneGraph - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "SG_SceneGraph - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "SG_SceneGraph - Win32 MT DLL Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "SG_SceneGraph - Win32 MT DLL Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "SG_SceneGraph - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\gameengine\scenegraph"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\scenegraph"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GR /GX /O2 /I "../../../../lib/windows/moto/include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SG_SceneGraph - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\gameengine\scenegraph\debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\scenegraph\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GR /GX /Zi /Od /I "../../../../lib/windows/moto/include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SG_SceneGraph - Win32 MT DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "SG_SceneGraph___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "SG_SceneGraph___Win32_MT_DLL_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\gameengine\scenegraph\mtdll_debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\scenegraph\mtdll_debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /Gm /GX /ZI /Od /I "../../../../../lib/windows/moto/include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /I "../../../../lib/windows/moto/include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\gameengine\scenegraph\debug\SG_SceneGraph.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SG_SceneGraph - Win32 MT DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "SG_SceneGraph___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "SG_SceneGraph___Win32_MT_DLL_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\gameengine\scenegraph\mtdll"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\scenegraph\mtdll"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /GX /O2 /I "../../../../../lib/windows/moto/include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD CPP /nologo /MD /GX /O2 /I "../../../../lib/windows/moto/include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\gameengine\scenegraph\SG_SceneGraph.lib"
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "SG_SceneGraph - Win32 Release"
# Name "SG_SceneGraph - Win32 Debug"
# Name "SG_SceneGraph - Win32 MT DLL Debug"
# Name "SG_SceneGraph - Win32 MT DLL Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\gameengine\SceneGraph\SG_BBox.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SceneGraph\SG_Controller.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SceneGraph\SG_IObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SceneGraph\SG_Node.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SceneGraph\SG_Spatial.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SceneGraph\SG_Tree.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\source\gameengine\SceneGraph\SG_BBox.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SceneGraph\SG_Controller.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SceneGraph\SG_IObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SceneGraph\SG_Node.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SceneGraph\SG_ParentRelation.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SceneGraph\SG_Spatial.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SceneGraph\SG_Tree.h
# End Source File
# End Group
# End Target
# End Project
