# Microsoft Developer Studio Project File - Name="GP_ghost" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=GP_ghost - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "GP_ghost.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "GP_ghost.mak" CFG="GP_ghost - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "GP_ghost - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "GP_ghost - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "GP_ghost - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\gameplayer\ghost\"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\..\..\lib\windows\ode\include" /I "..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\source\gameengine\Physics\Ode" /I "..\..\..\..\source\gameengine\Physics" /I "..\..\..\..\..\lib\windows\python\include\python2.2" /I "..\..\..\..\..\lib\windows\bmfont\include" /I "..\..\..\..\source\kernel\gen_messaging" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\ketsji" /I "..\..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\..\source\gameengine\rasterizer" /I "..\..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\..\source\gameengine\scenegraph" /I "..\..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\source\gameengine\soundsystem" /I "..\..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\..\source\gameengine\network" /I "..\..\..\..\source\gameengine\network\loopbacknetwork" /I "..\..\..\..\source\sumo\include" /I "..\..\..\..\source\sumo\fuzzics\include" /I "..\..\..\..\..\lib\windows\openal\include" /I "..\..\..\..\source\gameengine\gameplayer\common" /I "../../../../source/blender/blenkernel" /I "../../../../source/blender/makesdna" /I "../../../../source/blender/blenlib" /I "../../../../source/blender/blenloader" /I "../../../../source/blender/readblenfile" /I "../../../../source/blender/render/extern/include" /I "../../../../source/blender/imbuf" /I "..\..\..\..\source\gameengine\converter" /I "..\..\..\..\source\gameengine\gameplayer\common\windows" /I "..\..\..\..\..\lib\windows\string\include" /I "..\..\..\..\..\lib\windows\ghost\include" /I "..\..\..\..\..\lib\windows\iksolver\include" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 libguardedalloc.a libstring.a libghost.a odelib.lib fmodvc.lib libbmfont.a ws2_32.lib kernel32.lib user32.lib gdi32.lib vfw32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib libjpeg.a opengl32.lib glu32.lib openal_static.lib dxguid.lib libblenkey.a libeay32.lib libpng.a libz.a libmoto.a /nologo /subsystem:console /machine:I386 /nodefaultlib:"libcd.lib" /nodefaultlib:"libc.lib" /nodefaultlib:"msvcrt.lib" /nodefaultlib:"msvcrtd.lib" /nodefaultlib:"msvcprt.lib" /out:"..\..\..\..\obj\windows\blenderplayer.exe" /libpath:"..\..\..\..\..\lib\windows\ode\lib" /libpath:"..\..\..\..\..\lib\windows\bmfont\lib" /libpath:"..\..\..\..\..\lib\windows\guardedalloc\lib" /libpath:"..\..\..\..\..\lib\windows\string\lib" /libpath:"..\..\..\..\..\lib\windows\ghost\lib" /libpath:"..\..\..\..\..\lib\windows\jpeg/lib" /libpath:"..\..\..\..\..\lib\windows\moto\lib\\" /libpath:"..\..\..\..\..\lib\windows\png\lib" /libpath:"..\..\..\..\..\lib\windows\zlib\lib" /libpath:"..\..\..\..\..\lib\windows\ode-0.03\lib" /libpath:"../../../../../lib/windows/fmod/lib" /libpath:"..\..\..\..\..\lib\windows\python\lib" /libpath:"../../../../../lib/windows/openal/lib" /libpath:"..\..\..\..\..\lib\windows\openssl\lib" /libpath:"..\..\..\..\..\lib\windows\blenkey\lib\\"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "GP_ghost - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\gameplayer\ghost\debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "../../../../../lib/windows/moto/include" /I "..\..\..\..\source\gameengine\Physics\Ode" /I "..\..\..\..\source\gameengine\Physics" /I "..\..\..\..\..\lib\windows\python\include\python2.2" /I "..\..\..\..\..\lib\windows\bmfont\include" /I "..\..\..\..\source\kernel\gen_messaging" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\gameengine\ketsji" /I "..\..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\..\source\gameengine\rasterizer" /I "..\..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\..\source\gameengine\scenegraph" /I "..\..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\source\gameengine\soundsystem" /I "..\..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\..\source\gameengine\network" /I "..\..\..\..\source\gameengine\network\loopbacknetwork" /I "..\..\..\..\source\sumo\include" /I "..\..\..\..\source\sumo\fuzzics\include" /I "..\..\..\..\..\lib\windows\openal\include" /I "..\..\..\..\source\gameengine\gameplayer\common" /I "../../../../source/blender/blenkernel" /I "../../../../source/blender/makesdna" /I "../../../../source/blender/blenlib" /I "../../../../source/blender/blenloader" /I "../../../../source/blender/readblenfile" /I "../../../../source/blender/render/extern/include" /I "../../../../source/blender/imbuf" /I "..\..\..\..\source\gameengine\converter" /I "..\..\..\..\source\gameengine\gameplayer\common\windows" /I "..\..\..\..\..\lib\windows\string\include" /I "..\..\..\..\..\lib\windows\ghost\include" /I "../../../../../lib/windows/iksolver/include" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "dSINGLE" /U "_DEBUG" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 libguardedalloc.a libstring.a libghost.a odelib.lib fmodvc.lib libbmfont.a ws2_32.lib kernel32.lib user32.lib gdi32.lib vfw32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib opengl32.lib glu32.lib openal_static.lib libjpeg.a dxguid.lib libblenkey.a libeay32.lib libpng.a libz.a libmoto.a /nologo /subsystem:console /debug /machine:I386 /nodefaultlib:"libcd.lib" /nodefaultlib:"libc.lib" /nodefaultlib:"libcmt.lib" /nodefaultlib:"msvcrt.lib" /nodefaultlib:"msvcrtd.lib" /nodefaultlib:"msvcprtd.lib" /out:"..\..\..\..\obj\windows\debug\blenderplayer.exe" /pdbtype:sept /libpath:"..\..\..\..\..\lib\windows\ode\lib" /libpath:"..\..\..\..\..\lib\windows\bmfont\lib\\" /libpath:"..\..\..\..\..\lib\windows\guardedalloc\lib" /libpath:"..\..\..\..\..\lib\windows\string\lib\\" /libpath:"..\..\..\..\..\lib\windows\ghost\lib\\" /libpath:"..\..\..\..\..\lib\windows\jpeg\lib" /libpath:"..\..\..\..\..\lib\windows\moto\lib\\" /libpath:"..\..\..\..\..\lib\windows\zlib\lib\\" /libpath:"..\..\..\..\..\lib\windows\png\lib\\" /libpath:"..\..\..\..\..\lib\windows\ode-0.03\lib" /libpath:"../../../../../lib/windows/fmod/lib" /libpath:"..\..\..\..\..\lib\windows\python\lib" /libpath:"../../../../../lib/windows/openal/lib" /libpath:"..\..\..\..\..\lib\windows\openssl\lib" /libpath:"..\..\..\..\..\lib\windows\blenkey\lib\\"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "GP_ghost - Win32 Release"
# Name "GP_ghost - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\..\source\blender\makesdna\intern\dna.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ghost\GPG_Application.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ghost\GPG_Canvas.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ghost\GPG_ghost.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ghost\GPG_KeyboardDevice.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ghost\GPG_System.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\blenkernel\bad_level_call_stubs\stubs.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\icons\winplayer.rc
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ghost\GPG_Application.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ghost\GPG_Canvas.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ghost\GPG_KeyboardDevice.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ghost\GPG_System.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\..\..\..\source\icons\winplayer.ico
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\ghost\Makefile
# End Source File
# End Target
# End Project
