# Microsoft Developer Studio Project File - Name="KX_blenderhook" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=KX_blenderhook - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "KX_blenderhook.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "KX_blenderhook.mak" CFG="KX_blenderhook - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "KX_blenderhook - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "KX_blenderhook - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "KX_blenderhook - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\gameengine\blenderhook"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\blenderhook"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GR /GX /O2 /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\extern\solid" /I "..\..\..\source\gameengine\physics\sumo" /I "..\..\..\source\gameengine\physics\common" /I "..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\..\lib\windows\python\include\python2.4" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\..\lib\windows\bmfont\include" /I "..\..\..\source\gameengine\Converter\\" /I "..\..\..\source\blender\imbuf" /I "..\..\..\..\lib\windows\moto\include" /I "..\..\..\source\gameengine\ketsji" /I "..\..\..\source\gameengine\physics\sumo\fuzzics\include" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\gameengine\rasterizer" /I "..\..\..\source\gameengine\gamelogic" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\source\gameengine\network" /I "..\..\..\source\gameengine\scenegraph" /I "..\..\..\source\sumo\include" /I "..\..\..\source\sumo\fuzzics\include" /I "..\..\..\source\gameengine\network\loopbacknetwork" /I "..\..\..\..\lib\windows\soundsystem\include" /I "..\..\..\source\blender\misc" /I "..\..\..\source\blender\blenloader" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "KX_blenderhook - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\gameengine\blenderhook\debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\blenderhook\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD CPP /nologo /MTd /W3 /Gm /GR /GX /Zi /Od /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\extern\solid" /I "..\..\..\source\gameengine\physics\sumo" /I "..\..\..\source\gameengine\physics\common" /I "..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\..\lib\windows\python\include\python2.4" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\..\lib\windows\bmfont\include" /I "..\..\..\source\gameengine\Converter\\" /I "..\..\..\source\blender\imbuf" /I "..\..\..\..\lib\windows\moto\include" /I "..\..\..\source\gameengine\ketsji" /I "..\..\..\source\gameengine\physics\sumo\fuzzics\include" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\gameengine\rasterizer" /I "..\..\..\source\gameengine\gamelogic" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\source\gameengine\network" /I "..\..\..\source\gameengine\scenegraph" /I "..\..\..\source\sumo\include" /I "..\..\..\source\sumo\fuzzics\include" /I "..\..\..\source\gameengine\network\loopbacknetwork" /I "..\..\..\..\lib\windows\soundsystem\include" /I "..\..\..\source\blender\misc" /I "..\..\..\source\blender\blenloader" /D "WIN32" /D "_MBCS" /D "_LIB" /U "_DEBUG" /YX /J /FD /GZ /c
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

# Name "KX_blenderhook - Win32 Release"
# Name "KX_blenderhook - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\gameengine\BlenderRoutines\BL_KetsjiEmbedStart.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\BlenderRoutines\KX_BlenderCanvas.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\BlenderRoutines\KX_BlenderGL.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\BlenderRoutines\KX_BlenderInputDevice.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\BlenderRoutines\KX_BlenderKeyboardDevice.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\BlenderRoutines\KX_BlenderMouseDevice.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\BlenderRoutines\KX_BlenderPolyMaterial.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\BlenderRoutines\KX_BlenderRenderTools.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\BlenderRoutines\KX_BlenderSystem.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\source\gameengine\BlenderRoutines\KX_BlenderCanvas.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\BlenderRoutines\KX_BlenderGL.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\BlenderRoutines\KX_BlenderInputDevice.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\BlenderRoutines\KX_BlenderKeyboardDevice.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\BlenderRoutines\KX_BlenderMouseDevice.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\BlenderRoutines\KX_BlenderPolyMaterial.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\BlenderRoutines\KX_BlenderRenderTools.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\BlenderRoutines\KX_BlenderSystem.h
# End Source File
# End Group
# End Target
# End Project
