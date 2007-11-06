# Microsoft Developer Studio Project File - Name="SoundSystem" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=SoundSystem - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "SoundSystem.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "SoundSystem.mak" CFG="SoundSystem - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "SoundSystem - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "SoundSystem - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "SoundSystem - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\intern\soundsystem"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\intern\soundsystem"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c 
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../../" /I "../../../../../lib/windows/string/include" /I "../../../../../lib/windows/moto/include" /I "../../dummy" /I "../../openal" /I "..\..\..\string" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\intern\soundsystem\libSoundSystem.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO Copying header files	XCOPY /Y ..\..\*.h ..\..\..\..\..\lib\windows\SoundSystem\include\*.h	ECHO Copying lib	XCOPY /Y ..\..\..\..\obj\windows\intern\soundsystem\*.lib ..\..\..\..\..\lib\windows\SoundSystem\lib\*.a	ECHO Done
# End Special Build Tool

!ELSEIF  "$(CFG)" == "SoundSystem - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "SoundSystem___Win32_Debug"
# PROP BASE Intermediate_Dir "SoundSystem___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\intern\soundsystem\debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\intern\soundsystem\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "../../" /I "../../../../../lib/windows/string/include" /I "../../../../../lib/windows/moto/include" /I "../../dummy" /I "../../openal" /I "..\..\..\string" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\intern\soundsystem\debug\libSoundSystem.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO Copying header files	XCOPY /Y ..\..\*.h ..\..\..\..\..\lib\windows\SoundSystem\include\*.h	ECHO Copying lib	XCOPY /Y ..\..\..\..\obj\windows\intern\soundsystem\debug\*.lib ..\..\..\..\..\lib\windows\SoundSystem\lib\debug\*.a	ECHO Done
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "SoundSystem - Win32 Release"
# Name "SoundSystem - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\intern\SND_AudioDevice.cpp
# End Source File
# Begin Source File

SOURCE="..\..\intern\SND_C-api.cpp"
# End Source File
# Begin Source File

SOURCE=..\..\intern\SND_CDObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\SND_DeviceManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\SND_IdObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\SND_Scene.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\SND_SoundListener.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\SND_SoundObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\SND_Utils.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\SND_WaveCache.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\SND_WaveSlot.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\intern\SND_AudioDevice.h
# End Source File
# Begin Source File

SOURCE="..\..\SND_C-api.h"
# End Source File
# Begin Source File

SOURCE=..\..\SND_CDObject.h
# End Source File
# Begin Source File

SOURCE=..\..\SND_DependKludge.h
# End Source File
# Begin Source File

SOURCE=..\..\SND_DeviceManager.h
# End Source File
# Begin Source File

SOURCE=..\..\SND_IAudioDevice.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\SND_IdObject.h
# End Source File
# Begin Source File

SOURCE=..\..\SND_Object.h
# End Source File
# Begin Source File

SOURCE=..\..\SND_Scene.h
# End Source File
# Begin Source File

SOURCE=..\..\SND_SoundListener.h
# End Source File
# Begin Source File

SOURCE=..\..\SND_SoundObject.h
# End Source File
# Begin Source File

SOURCE=..\..\SND_Utils.h
# End Source File
# Begin Source File

SOURCE=..\..\SND_WaveCache.h
# End Source File
# Begin Source File

SOURCE=..\..\SND_WaveSlot.h
# End Source File
# Begin Source File

SOURCE=..\..\SoundDefines.h
# End Source File
# End Group
# End Target
# End Project
