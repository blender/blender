# Microsoft Developer Studio Project File - Name="GP_axctl" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=GP_axctl - Win32 MT DLL Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "GP_axctl.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "GP_axctl.mak" CFG="GP_axctl - Win32 MT DLL Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "GP_axctl - Win32 MT DLL Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "GP_axctl - Win32 MT DLL Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "GP_axctl - Win32 MT DLL Debug"

# PROP BASE Use_MFC 2
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "GP_axctl___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "GP_axctl___Win32_MT_DLL_Debug"
# PROP BASE Target_Ext "ocx"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 2
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\gameengine\gameplayer\axctl\mtdll_debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\gameplayer\axctl\mtdll_debug"
# PROP Target_Ext "ocx"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\network" /I "..\..\..\..\source\gameengine\network\loopbacknetwork" /I "..\..\..\..\source\gameengine\gameplayer\common" /I "..\..\..\..\source\gameengine\rasterizer" /I "..\..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\source\sumo\include" /I "..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\source\sumo\Fuzzics\include" /I "..\..\..\..\..\lib\windows\python\include\python1.5" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_WINDLL" /D "_AFXDLL" /D "_MBCS" /D "_USRDLL" /FR /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\..\..\..\..\lib\windows\python\include\python2.2" /I "..\..\..\..\source\kernel\gen_messaging" /I "..\..\..\..\..\lib\windows\bmfont\include" /I "..\..\..\..\..\lib\windows\string\include" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\ketsji" /I "..\..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\..\source\gameengine\rasterizer" /I "..\..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\..\source\gameengine\scenegraph" /I "..\..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\source\gameengine\soundsystem" /I "..\..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\..\source\gameengine\network" /I "..\..\..\..\source\gameengine\network\loopbacknetwork" /I "..\..\..\..\source\sumo\include" /I "..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\source\sumo\fuzzics\include" /I "..\..\..\..\..\lib\windows\openal\include" /I "..\..\..\..\source\gameengine\gameplayer\common" /I "../../../../source/blender/blenkernel" /I "../../../../source/blender/makesdna" /I "../../../../source/blender/blenlib" /I "../../../../source/blender/blenloader" /I "../../../../source/blender/render/extern/include" /I "../../../../source/blender/imbuf" /I "..\..\..\..\source\gameengine\converter" /I "..\..\..\..\source\gameengine\GamePlayer\common" /I "..\..\..\..\source\gameengine\GamePlayer\common\windows" /I "..\..\..\..\..\lib\windows\iksolver\include" /D "WIN32" /D "_WINDOWS" /D "_WINDLL" /D "_AFXDLL" /D "_MBCS" /D "_USRDLL" /U "_DEBUG" /YX /FD /GZ /c
# SUBTRACT CPP /Fr
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 opengl32.lib glu32.lib glaux.lib /nologo /subsystem:windows /dll /debug /machine:I386 /out:"Debug/ActiveXgp.ocx" /pdbtype:sept /libpath:"..\..\..\..\..\lib\windows\python\lib" /libpath:"..\..\..\..\..\lib\windows\openal\lib\lib_release"
# SUBTRACT BASE LINK32 /nodefaultlib
# ADD LINK32 openal_static.lib libmoto.a libguardedalloc.a libbmfont.a libstring.a ws2_32.lib opengl32.lib glu32.lib glaux.lib dxguid.lib ole32.lib vfw32.lib libblenkey.a libeay32.lib libz.a libpng.a libjpeg.a odelib.lib /nologo /subsystem:windows /dll /debug /machine:I386 /nodefaultlib:"libc.lib" /nodefaultlib:"libcmt.lib" /nodefaultlib:"msvcrt.lib" /out:"..\..\..\..\obj\windows\debug\Blender3DPlugin.ocx" /pdbtype:sept /libpath:"..\..\..\..\..\lib\windows\ode\lib\mt_dll" /libpath:"..\..\..\..\..\lib\windows\openal\lib" /libpath:"..\..\..\..\..\lib\windows\guardedalloc\lib" /libpath:"..\..\..\..\..\lib\windows\moto\lib" /libpath:"../../../../../lib/windows/bmfont/lib" /libpath:"../../../../../lib/windows/string/lib" /libpath:"../../../../../lib/windows/python/lib" /libpath:"..\..\..\..\..\lib\windows\openssl\lib\mt_dll" /libpath:"..\..\..\..\..\lib\windows\jpeg\lib" /libpath:"..\..\..\..\..\lib\windows\bmfont/lib" /libpath:"..\..\..\..\..\lib\windows\string\lib" /libpath:"..\..\..\..\..\lib\windows\python\lib" /libpath:"..\..\..\..\..\lib\windows\openssl\lib\\" /libpath:"..\..\..\..\..\lib\windows\zlib\lib\\" /libpath:"..\..\..\..\..\lib\windows\blenkey\lib\\" /libpath:"..\..\..\..\..\lib\windows\png\lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "GP_axctl - Win32 MT DLL Release"

# PROP BASE Use_MFC 2
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "GP_axctl___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "GP_axctl___Win32_MT_DLL_Release"
# PROP BASE Target_Ext "ocx"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 2
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\gameengine\gameplayer\axctl\mtdll"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\gameplayer\axctl\mtdll"
# PROP Target_Ext "ocx"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_WINDLL" /D "_AFXDLL" /D "_MBCS" /D "_USRDLL" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\..\..\..\..\lib\windows\python\include\python2.0" /I "..\..\..\..\..\lib\windows\python\include\python2.2" /I "..\..\..\..\source\kernel\gen_messaging" /I "..\..\..\..\..\lib\windows\bmfont\include" /I "..\..\..\..\..\lib\windows\string\include" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\ketsji" /I "..\..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\..\source\gameengine\rasterizer" /I "..\..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\..\source\gameengine\scenegraph" /I "..\..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\source\gameengine\soundsystem" /I "..\..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\..\source\gameengine\network" /I "..\..\..\..\source\gameengine\network\loopbacknetwork" /I "..\..\..\..\source\sumo\include" /I "..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\source\sumo\fuzzics\include" /I "..\..\..\..\..\lib\windows\openal\include" /I "..\..\..\..\source\gameengine\gameplayer\common" /I "../../../../source/blender/blenkernel" /I "../../../../source/blender/makesdna" /I "../../../../source/blender/blenlib" /I "../../../../source/blender/blenloader" /I "../../../../source/blender/render/extern/include" /I "../../../../source/blender/imbuf" /I "..\..\..\..\source\gameengine\converter" /I "..\..\..\..\source\gameengine\GamePlayer\common" /I "..\..\..\..\source\gameengine\GamePlayer\common\windows" /I "..\..\..\..\..\lib\windows\iksolver\include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_WINDLL" /D "_AFXDLL" /D "_MBCS" /D "_USRDLL" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 opengl32.lib glu32.lib glaux.lib /nologo /subsystem:windows /dll /machine:I386 /out:"Release/ActiveXgp.ocx"
# ADD LINK32 openal_static.lib libmoto.a libguardedalloc.a libbmfont.a libstring.a ws2_32.lib opengl32.lib glu32.lib glaux.lib dxguid.lib ole32.lib vfw32.lib libblenkey.a libeay32.lib libz.a libpng.a libjpeg.a odelib.lib /nologo /subsystem:windows /dll /machine:I386 /nodefaultlib:"libc.lib" /nodefaultlib:"libcmt.lib" /nodefaultlib:"msvcrtd.lib" /out:"..\..\..\..\obj\windows\Blender3DPlugin.ocx" /libpath:"..\..\..\..\..\lib\windows\ode\lib\mt_dll" /libpath:"..\..\..\..\..\lib\windows\openal\lib" /libpath:"..\..\..\..\..\lib\windows\guardedalloc\lib" /libpath:"..\..\..\..\..\lib\windows\moto\lib" /libpath:"..\..\..\..\..\lib\windows\bmfont/lib" /libpath:"..\..\..\..\..\lib\windows\string\lib" /libpath:"..\..\..\..\..\lib\windows\python\lib" /libpath:"..\..\..\..\..\lib\windows\openssl\lib\mt_dll" /libpath:"..\..\..\..\..\lib\windows\jpeg\lib" /libpath:"..\..\..\..\..\lib\windows\zlib\lib\\" /libpath:"..\..\..\..\..\lib\windows\blenkey\lib\\" /libpath:"..\..\..\..\..\lib\windows\png\lib"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "GP_axctl - Win32 MT DLL Debug"
# Name "GP_axctl - Win32 MT DLL Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\BlenderDataPathProperty.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\BlenderPlayer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\BlenderPlayer.def
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\BlenderPlayer.odl
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\BlenderPlayer.rc
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\BlenderPlayerCtl.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\BlenderPlayerPpg.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\CControlRefresher.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\makesdna\intern\dna.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\MemoryResource.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\RawImageRsrc.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\SafeControl.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\StdAfx.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\blenkernel\bad_level_call_stubs\stubs.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\BlenderDataPathProperty.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\BlenderPlayer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\BlenderPlayerCtl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\BlenderPlayerPpg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\CControlRefresher.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\MemoryResource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\RawImageRsrc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\Resource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\SafeControl.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\BlenderPlayer.ico
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\BlenderPlayerCtl.bmp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\splash.bmp
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\ActiveXandNetscapeTest.html
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\BlenderPlayer.html
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\BlenderPlayerDuo.html
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\load.blend
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\logo_blender.raw
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\logo_blender3d.raw
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\common\logo_nan.raw
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\ReadMe.txt
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ActiveX\ReadMeBuilding.txt
# End Source File
# End Target
# End Project
