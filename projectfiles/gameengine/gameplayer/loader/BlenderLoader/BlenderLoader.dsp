# Microsoft Developer Studio Project File - Name="BlenderLoader" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=BlenderLoader - Win32 Profile
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BlenderLoader.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BlenderLoader.mak" CFG="BlenderLoader - Win32 Profile"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BlenderLoader - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "BlenderLoader - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "BlenderLoader - Win32 Profile" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BlenderLoader - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\..\..\obj\windows\gameengine\gameplayer\BlenderLoader"
# PROP Intermediate_Dir "..\..\..\..\..\..\obj\windows\gameengine\gameplayer\BlenderLoader"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "../../../../../../lib/windows/python/include/python2.0" /I "../../../../../source/blender/encrypt" /I "../../../../../source/blender/blenkernel" /I "../../../../../source/blender/makesdna" /I "../../../../../source/blender/blenlib" /I "../../../../../source/blender/blenloader" /I "../../../../../source/blender/render/extern/include" /I "../../../../../source/gameengine/Rasterizer" /I "../../../../../../lib\windows\sdl\SDL-1.1.7\include" /I "../../../../../source/gameengine/GameLogic" /I "..\..\..\..\..\source\sumo\include" /I "..\..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\..\source\sumo\Fuzzics\include" /I "..\..\..\..\..\source\kernel\gen_system" /I "../../../../../source/blender/imbuf" /I "../../../../../source/gameengine/SoundSystem" /I "../../../../../source/gameengine/SoundSystem/SND_OpenAL" /I "../../../../../source/gameengine/Ketsji" /I "../../../../../source/gameengine/Expressions" /I "../../../../../source/gameengine/SceneGraph" /I "../../../../../source/gameengine/ketsji/kxnetwork" /I "../../../../../source/gameengine/network" /I "../../../../../source/gameengine/network/loopbacknetwork" /I "../../../../../source/gameengine/GamePlayer/common" /I "../../../../../../lib/windows/iksolver/include" /I "../../../../../source/gameengine/SoundSystem/SND_Dummy" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 libguardedalloc.a libmoto.a sdlmain.lib sdl.lib dxguid.lib ole32.lib libjpeg.lib glu32.lib opengl32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386 /out:"..\..\..\..\..\..\obj\windows\BlenderLoader.exe" /libpath:"../../../../../../lib/windows/moto/lib" /libpath:"..\..\..\..\..\..\lib\windows\guardedalloc\lib" /libpath:"../../../../../../lib/windows/python/lib" /libpath:"../../../../../../lib/windows/jpeg/lib" /libpath:"../../../../../../lib/windows/sdl/SDL-1.1.7\Lib"

!ELSEIF  "$(CFG)" == "BlenderLoader - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\..\..\obj\windows\gameengine\gameplayer\BlenderLoader\debug"
# PROP Intermediate_Dir "..\..\..\..\..\..\obj\windows\gameengine\gameplayer\BlenderLoader\debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /I "../../../../../../lib/windows/python/include/python1.5" /I "../../../../../source/blender/encrypt" /I "../../../../../source/blender/blenkernel" /I "../../../../../source/blender/makesdna" /I "../../../../../source/blender/blenlib" /I "../../../../../source/blender/blenloader" /I "../../../../../source/blender/render/extern/include" /I "../../../../../source/gameengine/Rasterizer" /I "../../../../../../lib\windows\sdl\SDL-1.1.7\include" /I "../../../../../source/gameengine/GameLogic" /I "..\..\..\..\..\source\sumo\include" /I "..\..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\..\source\sumo\Fuzzics\include" /I "..\..\..\..\..\source\kernel\gen_system" /I "../../../../../source/blender/imbuf" /I "../../../../../source/gameengine/SoundSystem" /I "../../../../../source/gameengine/SoundSystem/SND_OpenAL" /I "../../../../../source/gameengine/Ketsji" /I "../../../../../source/gameengine/Expressions" /I "../../../../../source/gameengine/SceneGraph" /I "../../../../../source/gameengine/ketsji/kxnetwork" /I "../../../../../source/gameengine/network" /I "../../../../../source/gameengine/network/loopbacknetwork" /I "../../../../../source/gameengine/GamePlayer/common" /I "../../../../../../lib/windows/iksolver/include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fr /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 sdldebug.lib libguardedalloc.a libmoto.a sdlmain.lib sdl.lib dxguid.lib ole32.lib libjpeg.lib glu32.lib opengl32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /out:"..\..\..\..\..\..\obj\windows\debug\BlenderLoader.exe" /pdbtype:sept /libpath:"..\..\..\..\..\..\lib\windows\guardedalloc\lib" /libpath:"../../../../../../lib/windows/python/lib" /libpath:"../../../../../../lib/windows/jpeg/lib" /libpath:"../../../../../../lib/windows/sdl/SDL-1.1.7\Lib"

!ELSEIF  "$(CFG)" == "BlenderLoader - Win32 Profile"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "BlenderLoader___Win32_Profile"
# PROP BASE Intermediate_Dir "BlenderLoader___Win32_Profile"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "BlenderLoader___Win32_Profile"
# PROP Intermediate_Dir "BlenderLoader___Win32_Profile"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /I "../../../../../../lib/windows/python/include/python1.5" /I "../../../../../source/blender/blenkernel" /I "../../../../../source/blender/makesdna" /I "../../../../../source/blender/blenlib" /I "../../../../../source/blender/blenloader" /I "../../../../../source/blender/render/extern/include" /I "../../../../../source/gameengine/Rasterizer" /I "../../../../../../lib\windows\sdl\SDL-1.1.7\include" /I "../../../../../source/gameengine/GameLogic" /I "..\..\..\..\..\source\sumo\include" /I "..\..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\..\source\sumo\Fuzzics\include" /I "..\..\..\..\..\source\kernel\gen_system" /I "../../../../../source/blender/imbuf" /I "../../../../../source/gameengine/SoundSystem" /I "../../../../../source/gameengine/SoundSystem/SND_OpenAL" /I "../../../../../source/gameengine/Ketsji" /I "../../../../../source/gameengine/Expressions" /I "../../../../../source/gameengine/SceneGraph" /I "../../../../../source/gameengine/ketsji/kxnetwork" /I "../../../../../source/gameengine/network" /I "../../../../../source/gameengine/network/loopbacknetwork" /I "../../../../../source/gameengine/GamePlayer/common" /I "../../../../../../lib/windows/iksolver/include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /W3 /Gm /GX /Zi /O2 /I "../../../../../../lib/windows/python/include/python1.5" /I "../../../../../source/blender/encrypt" /I "../../../../../source/blender/blenkernel" /I "../../../../../source/blender/makesdna" /I "../../../../../source/blender/blenlib" /I "../../../../../source/blender/blenloader" /I "../../../../../source/blender/render/extern/include" /I "../../../../../source/gameengine/Rasterizer" /I "../../../../../../lib\windows\sdl\SDL-1.1.7\include" /I "../../../../../source/gameengine/GameLogic" /I "..\..\..\..\..\source\sumo\include" /I "..\..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\..\source\sumo\Fuzzics\include" /I "..\..\..\..\..\source\kernel\gen_system" /I "../../../../../source/blender/imbuf" /I "../../../../../source/gameengine/SoundSystem" /I "../../../../../source/gameengine/SoundSystem/SND_OpenAL" /I "../../../../../source/gameengine/Ketsji" /I "../../../../../source/gameengine/Expressions" /I "../../../../../source/gameengine/SceneGraph" /I "../../../../../source/gameengine/ketsji/kxnetwork" /I "../../../../../source/gameengine/network" /I "../../../../../source/gameengine/network/loopbacknetwork" /I "../../../../../source/gameengine/GamePlayer/common" /I "../../../../../../lib/windows/iksolver/include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 sdldebug.lib sdlmain.lib sdl.lib dxguid.lib ole32.lib libjpeg.lib glu32.lib opengl32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /out:"..\..\..\..\..\..\obj\windows\debug\BlenderLoader.exe" /pdbtype:sept /libpath:"../../../../../../lib/windows/python/lib" /libpath:"../../../../../../lib/windows/jpeg/lib" /libpath:"../../../../../../lib/windows/sdl/SDL-1.1.7\Lib"
# ADD LINK32 sdldebug.lib libguardedalloc.a libmoto.a sdlmain.lib sdl.lib dxguid.lib ole32.lib libjpeg.lib glu32.lib opengl32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /out:"..\..\..\..\..\..\obj\windows\profile\BlenderLoader.exe" /pdbtype:sept /libpath:"..\..\..\..\..\..\lib\windows\guardedalloc\lib" /libpath:"../../../../../../lib/windows/python/lib" /libpath:"../../../../../../lib/windows/jpeg/lib" /libpath:"../../../../../../lib/windows/sdl/SDL-1.1.7\Lib"

!ENDIF 

# Begin Target

# Name "BlenderLoader - Win32 Release"
# Name "BlenderLoader - Win32 Debug"
# Name "BlenderLoader - Win32 Profile"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\makesdna\intern\dna.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\GamePlayer\Loaders\Blender\loader.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\GamePlayer\Loaders\Blender\SDLKeyboardDevice.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\GamePlayer\Loaders\Blender\SDLSystem.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\blenkernel\bad_level_call_stubs\stubs.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\GamePlayer\Loaders\Blender\SDLCanvas.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\GamePlayer\Loaders\Blender\SDLInputDevice.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\GamePlayer\Loaders\Blender\SDLKeyboardDevice.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\GamePlayer\Loaders\Blender\SDLSystem.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=..\..\..\..\..\..\lib\windows\openal\lib\alc\ALc.obj
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\..\lib\windows\openal\lib\alu\ALu.obj
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\..\lib\windows\openal\lib\alut\ALut.obj
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\..\lib\windows\openal\lib\OpenAL32\alBuffer.obj
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\..\lib\windows\openal\lib\OpenAL32\alEax.obj
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\..\lib\windows\openal\lib\OpenAL32\alError.obj
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\..\lib\windows\openal\lib\OpenAL32\alExtension.obj
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\..\lib\windows\openal\lib\OpenAL32\alListener.obj
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\..\lib\windows\openal\lib\OpenAL32\alSource.obj
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\..\lib\windows\openal\lib\OpenAL32\alState.obj
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\..\lib\windows\openal\lib\OpenAL32\OpenAL32.obj
# End Source File
# End Target
# End Project
