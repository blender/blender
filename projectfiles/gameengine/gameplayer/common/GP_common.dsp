# Microsoft Developer Studio Project File - Name="GP_common" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=GP_common - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "GP_common.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "GP_common.mak" CFG="GP_common - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "GP_common - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "GP_common - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "GP_common - Win32 MT DLL Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "GP_common - Win32 MT DLL Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "GP_common - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\gameengine\gameplayer\common"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\gameplayer\common"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\..\..\lib\windows\python\include\python2.0" /I "..\..\..\..\..\lib\windows\python\include\python2.2" /I "..\..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\..\source\gameengine\Converter" /I "..\..\..\..\source\gameengine\soundsystem\snd_dummy" /I "..\..\..\..\..\lib\windows\bmfont\include" /I "..\..\..\..\..\lib\windows\string\include" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\ketsji" /I "..\..\..\..\source\gameengine\GamePlayer\common\\" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\..\source\gameengine\rasterizer" /I "..\..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\..\source\gameengine\scenegraph" /I "..\..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\source\gameengine\soundsystem" /I "..\..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\..\source\gameengine\network" /I "..\..\..\..\source\gameengine\network\loopbacknetwork" /I "..\..\..\..\source\sumo\include" /I "..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\source\sumo\fuzzics\include" /I "..\..\..\..\..\lib\windows\openal\include" /I "../../../../source/blender/blenkernel" /I "../../../../source/blender/makesdna" /I "../../../../source/blender/blenlib" /I "../../../../source/blender/blenloader" /I "../../../../source/blender/render/extern/include" /I "../../../../source/blender/imbuf" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "GP_common - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\gameengine\gameplayer\common\debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\gameplayer\common\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\..\..\..\lib\windows\python\include\python2.2" /I "..\..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\..\source\gameengine\Converter" /I "..\..\..\..\source\gameengine\soundsystem\snd_dummy" /I "..\..\..\..\..\lib\windows\bmfont\include" /I "..\..\..\..\..\lib\windows\string\include" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\ketsji" /I "..\..\..\..\source\gameengine\GamePlayer\common\\" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\..\source\gameengine\rasterizer" /I "..\..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\..\source\gameengine\scenegraph" /I "..\..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\source\gameengine\soundsystem" /I "..\..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\..\source\gameengine\network" /I "..\..\..\..\source\gameengine\network\loopbacknetwork" /I "..\..\..\..\source\sumo\include" /I "..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\source\sumo\fuzzics\include" /I "..\..\..\..\..\lib\windows\openal\include" /I "../../../../source/blender/blenkernel" /I "../../../../source/blender/makesdna" /I "../../../../source/blender/blenlib" /I "../../../../source/blender/blenloader" /I "../../../../source/blender/render/extern/include" /I "../../../../source/blender/imbuf" /D "WIN32" /D "_MBCS" /D "_LIB" /U "_DEBUG" /YX /FD /GZ /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "GP_common - Win32 MT DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "GP_common___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "GP_common___Win32_MT_DLL_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\gameengine\gameplayer\common\mtdll_debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\gameplayer\common\mtdll_debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /MDd /Gm /GX /ZI /Od /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\ketsji" /I "..\..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\..\source\gameengine\rasterizer" /I "..\..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\..\source\gameengine\scenegraph" /I "..\..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\source\gameengine\soundsystem" /I "..\..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\..\source\gameengine\network" /I "..\..\..\..\source\gameengine\network\loopbacknetwork" /I "..\..\..\..\source\sumo\include" /I "..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\source\sumo\fuzzics\include" /I "..\..\..\..\..\lib\windows\openal\include" /I "..\..\..\..\..\lib\windows\python\include\python1.5" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /I "..\..\..\..\..\lib\windows\python\include\python2.0" /I "..\..\..\..\..\lib\windows\python\include\python2.2" /I "..\..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\..\source\gameengine\Converter" /I "..\..\..\..\source\gameengine\soundsystem\snd_dummy" /I "..\..\..\..\..\lib\windows\bmfont\include" /I "..\..\..\..\..\lib\windows\string\include" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\ketsji" /I "..\..\..\..\source\gameengine\GamePlayer\common\\" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\..\source\gameengine\rasterizer" /I "..\..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\..\source\gameengine\scenegraph" /I "..\..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\source\gameengine\soundsystem" /I "..\..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\..\source\gameengine\network" /I "..\..\..\..\source\gameengine\network\loopbacknetwork" /I "..\..\..\..\source\sumo\include" /I "..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\source\sumo\fuzzics\include" /I "..\..\..\..\..\lib\windows\openal\include" /I "../../../../source/blender/blenkernel" /I "../../../../source/blender/makesdna" /I "../../../../source/blender/blenlib" /I "../../../../source/blender/blenloader" /I "../../../../source/blender/render/extern/include" /I "../../../../source/blender/imbuf" /D "WIN32" /D "_MBCS" /D "_LIB" /U "_DEBUG" /YX /FD /GZ /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\gameengine\gameplayer\common\debug\GP_common.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "GP_common - Win32 MT DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "GP_common___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "GP_common___Win32_MT_DLL_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\gameengine\gameplayer\common\mtdll"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\gameplayer\common\mtdll"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /GX /O2 /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\ketsji" /I "..\..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\..\source\gameengine\rasterizer" /I "..\..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\..\source\gameengine\scenegraph" /I "..\..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\source\gameengine\soundsystem" /I "..\..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\..\source\gameengine\network" /I "..\..\..\..\source\gameengine\network\loopbacknetwork" /I "..\..\..\..\source\sumo\include" /I "..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\source\sumo\fuzzics\include" /I "..\..\..\..\..\lib\windows\openal\include" /I "..\..\..\..\..\lib\windows\python\include\python2.0" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD CPP /nologo /MD /GX /O2 /I "..\..\..\..\..\lib\windows\python\include\python1.5" /I "..\..\..\..\..\lib\windows\python\include\python2.0" /I "..\..\..\..\..\lib\windows\python\include\python2.2" /I "..\..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\..\source\gameengine\Converter" /I "..\..\..\..\source\gameengine\soundsystem\snd_dummy" /I "..\..\..\..\..\lib\windows\bmfont\include" /I "..\..\..\..\..\lib\windows\string\include" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\ketsji" /I "..\..\..\..\source\gameengine\GamePlayer\common\\" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\..\source\gameengine\rasterizer" /I "..\..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\..\source\gameengine\scenegraph" /I "..\..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\source\gameengine\soundsystem" /I "..\..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\..\source\gameengine\network" /I "..\..\..\..\source\gameengine\network\loopbacknetwork" /I "..\..\..\..\source\sumo\include" /I "..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\source\sumo\fuzzics\include" /I "..\..\..\..\..\lib\windows\openal\include" /I "../../../../source/blender/blenkernel" /I "../../../../source/blender/makesdna" /I "../../../../source/blender/blenlib" /I "../../../../source/blender/blenloader" /I "../../../../source/blender/render/extern/include" /I "../../../../source/blender/imbuf" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\gameengine\gameplayer\common\GP_common.lib"
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "GP_common - Win32 Release"
# Name "GP_common - Win32 Debug"
# Name "GP_common - Win32 MT DLL Debug"
# Name "GP_common - Win32 MT DLL Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\bmfont.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\GPC_Canvas.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\GPC_Engine.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\GPC_KeyboardDevice.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\GPC_MouseDevice.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\GPC_PolygonMaterial.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\GPC_RawImage.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\GPC_RawLoadDotBlendArray.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\GPC_RawLogoArrays.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\GPC_RenderTools.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\GPC_System.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\windows\GPW_Canvas.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\windows\GPW_Engine.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\windows\GPW_KeyboardDevice.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\windows\GPW_System.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\GPC_Canvas.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\GPC_Engine.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\GPC_KeyboardDevice.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\GPC_MouseDevice.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\GPC_PolygonMaterial.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\GPC_RenderTools.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\GPC_System.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\windows\GPW_Canvas.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\windows\GPW_Engine.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\windows\GPW_KeyboardDevice.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\windows\GPW_System.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\Makefile
# End Source File
# End Target
# End Project
