# Microsoft Developer Studio Project File - Name="OpenALSoundSystem" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=OpenALSoundSystem - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "OpenALSoundSystem.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "OpenALSoundSystem.mak" CFG="OpenALSoundSystem - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "OpenALSoundSystem - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "OpenALSoundSystem - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "OpenALSoundSystem - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\..\obj\windows\intern\soundsystem\openal"
# PROP Intermediate_Dir "..\..\..\..\..\obj\windows\intern\soundsystem\openal"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\intern" /I "..\..\..\..\SoundSystem" /I "..\..\..\..\SoundSystem\sdl" /I "..\..\..\..\moto\include" /I "..\..\..\..\string" /I "..\..\..\..\..\..\lib\windows\openal\include" /I "..\..\..\..\..\..\lib\windows\sdl\include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\..\obj\windows\intern\soundsystem\openal\libOpenALSoundSystem.lib"

!ELSEIF  "$(CFG)" == "OpenALSoundSystem - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "OpenALSoundSystem___Win32_Debug"
# PROP BASE Intermediate_Dir "OpenALSoundSystem___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\..\obj\windows\intern\soundsystem\openal\debug"
# PROP Intermediate_Dir "..\..\..\..\..\obj\windows\intern\soundsystem\openal\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\..\intern" /I "..\..\..\..\SoundSystem" /I "..\..\..\..\SoundSystem\sdl" /I "..\..\..\..\moto\include" /I "..\..\..\..\string" /I "..\..\..\..\..\..\lib\windows\sdl\include" /I "..\..\..\..\..\..\lib\windows\openal\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\..\obj\windows\intern\soundsystem\openal\debug\libOpenALSoundSystem.lib"

!ENDIF 

# Begin Target

# Name "OpenALSoundSystem - Win32 Release"
# Name "OpenALSoundSystem - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\openal\SND_OpenALDevice.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\sdl\SND_SDLCDDevice.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\openal\SND_OpenALDevice.h
# End Source File
# End Group
# End Target
# End Project
