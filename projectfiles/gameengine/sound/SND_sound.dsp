# Microsoft Developer Studio Project File - Name="SND_sound" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=SND_sound - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "SND_sound.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "SND_sound.mak" CFG="SND_sound - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "SND_sound - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "SND_sound - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "SND_sound - Win32 MT DLL Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "SND_sound - Win32 MT DLL Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "SND_sound - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\gameengine\soundsystem"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\soundsystem"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\lib\windows\fmod\include" /I "..\..\..\source\gameengine\soundsystem" /I "..\..\..\source\gameengine\soundsystem\openal" /I "..\..\..\source\gameengine\soundsystem\fmod" /I "..\..\..\source\gameengine\soundsystem\dummy" /I "..\..\..\lib\windows\string\include" /I "../../../lib/windows/moto/include" /I "..\..\..\source\kernel\gen_system" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SND_sound - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "SND_sound___Win32_Debug"
# PROP BASE Intermediate_Dir "SND_sound___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\gameengine\soundsystem\debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\soundsystem\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\..\source\blender\include" /I "..\..\..\lib\windows\fmod\include" /I "..\..\..\source\gameengine\soundsystem" /I "..\..\..\source\gameengine\soundsystem\openal" /I "..\..\..\source\gameengine\soundsystem\fmod" /I "..\..\..\source\gameengine\soundsystem\dummy" /I "..\..\..\lib\windows\string\include" /I "../../../lib/windows/moto/include" /I "..\..\..\source\kernel\gen_system" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SND_sound - Win32 MT DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "SND_sound___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "SND_sound___Win32_MT_DLL_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\gameengine\soundsystem\mtdll_debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\soundsystem\mtdll_debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /Gm /GX /ZI /Od /I "..\..\..\source\gameengine\soundsystem\snd_openal" /I "../../../../lib/windows/moto/include" /I "..\..\..\source\kernel\gen_system" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /I "..\..\..\lib\windows\fmod\include" /I "..\..\..\source\gameengine\soundsystem" /I "..\..\..\source\gameengine\soundsystem\openal" /I "..\..\..\source\gameengine\soundsystem\fmod" /I "..\..\..\source\gameengine\soundsystem\dummy" /I "..\..\..\lib\windows\string\include" /I "../../../lib/windows/moto/include" /I "..\..\..\source\kernel\gen_system" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\gameengine\soundsystem\debug\SND_sound.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SND_sound - Win32 MT DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "SND_sound___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "SND_sound___Win32_MT_DLL_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\gameengine\soundsystem\mtdll"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\soundsystem\mtdll"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /GX /O2 /I "../../../../lib/windows/moto/include" /I "..\..\..\source\kernel\gen_system" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD CPP /nologo /MD /GX /O2 /I "..\..\..\lib\windows\fmod\include" /I "..\..\..\source\gameengine\soundsystem" /I "..\..\..\source\gameengine\soundsystem\openal" /I "..\..\..\source\gameengine\soundsystem\fmod" /I "..\..\..\source\gameengine\soundsystem\dummy" /I "..\..\..\lib\windows\string\include" /I "../../../lib/windows/moto/include" /I "..\..\..\source\kernel\gen_system" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\gameengine\soundsystem\SND_sound.lib"
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "SND_sound - Win32 Release"
# Name "SND_sound - Win32 Debug"
# Name "SND_sound - Win32 MT DLL Debug"
# Name "SND_sound - Win32 MT DLL Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\intern\SND_AudioDevice.cpp
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\SoundSystem\intern\SND_C-api.cpp"
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\intern\SND_CDObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\intern\SND_DeviceManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\intern\SND_IdObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\intern\SND_Scene.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\intern\SND_SoundListener.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\intern\SND_SoundObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\intern\SND_Utils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\intern\SND_WaveCache.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\intern\SND_WaveSlot.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\intern\SND_AudioDevice.h
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\SoundSystem\SND_C-api.h"
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\SND_CDObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\SND_DependKludge.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\SND_DeviceManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\SND_IAudioDevice.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\intern\SND_IdObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\SND_Object.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\SND_Scene.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\SND_SoundListener.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\SND_SoundObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\SND_Utils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\SND_WaveCache.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\SND_WaveSlot.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\SoundSystem\SoundDefines.h
# End Source File
# End Group
# End Target
# End Project
