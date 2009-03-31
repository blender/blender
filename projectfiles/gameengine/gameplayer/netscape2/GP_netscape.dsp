# Microsoft Developer Studio Project File - Name="GP_netscape2" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=GP_netscape2 - Win32 Profile
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "GP_netscape.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "GP_netscape.mak" CFG="GP_netscape2 - Win32 Profile"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "GP_netscape2 - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "GP_netscape2 - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "GP_netscape2 - Win32 Profile" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "GP_netscape2 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "C:\Program Files\Netscape\Communicator\Program\Plugins"
# PROP Intermediate_Dir ".\Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\..\..\..\source\kernel\gen_messaging" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\ketsji" /I "..\..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\..\source\gameengine\rasterizer" /I "..\..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\..\source\gameengine\scenegraph" /I "..\..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\source\gameengine\soundsystem" /I "..\..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\..\source\gameengine\network" /I "..\..\..\..\source\gameengine\network\loopbacknetwork" /I "..\..\..\..\source\gameengine\GamePlayer\Netscape2\windows_sdk" /I "..\..\..\..\source\sumo\include" /I "..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\source\sumo\fuzzics\include" /I "..\..\..\..\..\lib\windows\openal\include" /I "..\..\..\..\..\lib\windows\python\include\python1.5" /I "..\..\..\..\source\gameengine\gameplayer\common" /I "../../../../source/blender/blenkernel" /I "../../../../source/blender/makesdna" /I "../../../../source/blender/blenlib" /I "../../../../source/blender/blenloader" /I "../../../../source/blender/render/extern/include" /I "../../../../source/blender/imbuf" /I "..\..\..\..\source\gameengine\converter" /I "..\..\..\..\source\gameengine\gameplayer\common\windows" /I "..\..\..\..\source\gameengine\gameplayer\Netscape2\windows" /I "..\..\..\..\..\lib\windows\iksolver\include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 libblenkey.a libeay32.lib libz.a ws2_32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libjpeg.lib opengl32.lib glu32.lib openal_static.lib dxguid.lib /nologo /subsystem:windows /dll /machine:I386 /out:"C:\Program Files\Netscape\Communicator\Program\Plugins\npBlender3DPlugin.dll" /libpath:"../../../../../lib/windows/python/lib" /libpath:"../../../../../lib/windows/jpeg/lib" /libpath:"../../../../../lib/windows/zlib/lib/" /libpath:"..\..\..\..\..\lib\windows\blenkey\lib" /libpath:"..\..\..\..\..\lib\windows\openssl\lib" /libpath:"..\..\..\..\..\lib\windows\iksolver\lib\\" /libpath:"../../../../../lib/windows/openal/lib"

!ELSEIF  "$(CFG)" == "GP_netscape2 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "C:\Program Files\Netscape\Communicator\Program\Plugins"
# PROP Intermediate_Dir ".\Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /I "..\..\..\..\source\kernel\gen_messaging" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\ketsji" /I "..\..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\..\source\gameengine\rasterizer" /I "..\..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\..\source\gameengine\scenegraph" /I "..\..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\source\gameengine\soundsystem" /I "..\..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\..\source\gameengine\network" /I "..\..\..\..\source\gameengine\network\loopbacknetwork" /I "..\..\..\..\source\gameengine\GamePlayer\Netscape2\windows_sdk" /I "..\..\..\..\source\sumo\include" /I "..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\source\sumo\fuzzics\include" /I "..\..\..\..\..\lib\windows\openal\include" /I "..\..\..\..\..\lib\windows\python\include\python1.5" /I "..\..\..\..\source\gameengine\gameplayer\common" /I "../../../../source/blender/blenkernel" /I "../../../../source/blender/makesdna" /I "../../../../source/blender/blenlib" /I "../../../../source/blender/blenloader" /I "../../../../source/blender/render/extern/include" /I "../../../../source/blender/imbuf" /I "..\..\..\..\source\gameengine\converter" /I "..\..\..\..\source\gameengine\gameplayer\common\windows" /I "..\..\..\..\source\gameengine\gameplayer\Netscape2\windows" /I "..\..\..\..\..\lib\windows\iksolver\include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
# ADD LINK32 libblenkey.a libeay32.lib libz.a ws2_32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libjpeg.lib opengl32.lib glu32.lib openal_static.lib dxguid.lib /nologo /subsystem:windows /dll /debug /machine:I386 /out:"C:\Program Files\Netscape\Communicator\Program\Plugins\npBlender3DPlugin.dll" /libpath:"../../../../../lib/windows/python/lib" /libpath:"../../../../../lib/windows/jpeg/lib" /libpath:"../../../../../lib/windows/openal/lib" /libpath:"../../../../../lib/windows/zlib/lib/" /libpath:"..\..\..\..\..\lib\windows\blenkey\lib" /libpath:"..\..\..\lib\windows\openssl\lib" /libpath:"..\..\..\lib\windows\iksolver\lib\\"

!ELSEIF  "$(CFG)" == "GP_netscape2 - Win32 Profile"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "GP_netscape2___Win32_Profile"
# PROP BASE Intermediate_Dir "GP_netscape2___Win32_Profile"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "GP_netscape2___Win32_Profile"
# PROP Intermediate_Dir "GP_netscape2___Win32_Profile"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\ketsji" /I "..\..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\..\source\gameengine\rasterizer" /I "..\..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\..\source\gameengine\scenegraph" /I "..\..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\source\gameengine\soundsystem" /I "..\..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\..\source\gameengine\network" /I "..\..\..\..\source\gameengine\network\loopbacknetwork" /I "..\..\..\..\source\gameengine\GamePlayer\Netscape\windows_sdk" /I "..\..\..\..\source\sumo\include" /I "..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\source\sumo\fuzzics\include" /I "..\..\..\..\..\lib\windows\openal\include" /I "..\..\..\..\..\lib\windows\python\include\python1.5" /I "..\..\..\..\source\gameengine\gameplayer\common" /I "../../../../source/blender/blenkernel" /I "../../../../source/blender/makesdna" /I "../../../../source/blender/blenlib" /I "../../../../source/blender/blenloader" /I "../../../../source/blender/render/extern/include" /I "../../../../source/blender/imbuf" /I "..\..\..\..\source\gameengine\converter" /I "..\..\..\..\source\gameengine\gameplayer\common\windows" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /YX /FD /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /O2 /I "..\..\..\..\source\kernel\gen_messaging" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\ketsji" /I "..\..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\..\source\gameengine\rasterizer" /I "..\..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\..\source\gameengine\scenegraph" /I "..\..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\source\gameengine\soundsystem" /I "..\..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\..\source\gameengine\network" /I "..\..\..\..\source\gameengine\network\loopbacknetwork" /I "..\..\..\..\source\gameengine\GamePlayer\Netscape2\windows_sdk" /I "..\..\..\..\source\sumo\include" /I "..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\source\sumo\fuzzics\include" /I "..\..\..\..\..\lib\windows\openal\include" /I "..\..\..\..\..\lib\windows\python\include\python1.5" /I "..\..\..\..\source\gameengine\gameplayer\common" /I "../../../../source/blender/blenkernel" /I "../../../../source/blender/makesdna" /I "../../../../source/blender/blenlib" /I "../../../../source/blender/blenloader" /I "../../../../source/blender/render/extern/include" /I "../../../../source/blender/imbuf" /I "..\..\..\..\source\gameengine\converter" /I "..\..\..\..\source\gameengine\gameplayer\common\windows" /I "..\..\..\..\source\gameengine\gameplayer\Netscape2\windows" /I "..\..\..\..\..\lib\windows\iksolver\include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /YX /FD /c
# SUBTRACT CPP /Fr
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libjpeg.lib opengl32.lib glu32.lib openal_static.lib dxguid.lib /nologo /subsystem:windows /dll /debug /machine:I386 /out:"C:\Program Files\Netscape\Communicator\Program\Plugins\NPBlender.dll" /libpath:"../../../../../lib/windows/python/lib" /libpath:"../../../../../lib/windows/jpeg/lib" /libpath:"../../../../../lib/windows/openal/lib"
# ADD LINK32 libblenkey.a libeay32.lib libz.a ws2_32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libjpeg.lib opengl32.lib glu32.lib openal_static.lib dxguid.lib /nologo /subsystem:windows /dll /debug /machine:I386 /out:"C:\Program Files\Netscape\Communicator\Program\Plugins\npBlender3DPlugin.dll" /libpath:"../../../../../lib/windows/python/lib" /libpath:"../../../../../lib/windows/jpeg/lib" /libpath:"../../../../../lib/windows/openal/lib"

!ENDIF 

# Begin Target

# Name "GP_netscape2 - Win32 Release"
# Name "GP_netscape2 - Win32 Debug"
# Name "GP_netscape2 - Win32 Profile"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;for;f90"
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Netscape2\windows\Blender3DPlugin.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\makesdna\intern\dna.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Netscape2\windows\netscape_plugin_Plugin.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Netscape2\windows\npblender.def
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Netscape2\windows\npblender.rc
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Netscape2\npshell.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Netscape2\windows\npwin.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Netscape2\windows\plgwnd.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\blenkernel\bad_level_call_stubs\stubs.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Netscape\plgwnd.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\NSPlugin\autodownload.html
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Netscape\Testing\NetscapeExample.html
# End Source File
# End Target
# End Project
