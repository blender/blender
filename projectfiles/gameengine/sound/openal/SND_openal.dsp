# Microsoft Developer Studio Project File - Name="SND_openal" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=SND_openal - Win32 Profile
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "SND_openal.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "SND_openal.mak" CFG="SND_openal - Win32 Profile"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "SND_openal - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "SND_openal - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "SND_openal - Win32 MT DLL Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "SND_openal - Win32 MT DLL Release" (based on "Win32 (x86) Static Library")
!MESSAGE "SND_openal - Win32 Profile" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "SND_openal - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "..\..\..\..\..\obj\windows\gameengine\soundsystem\openal"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../../../../source/gameengine/soundsystem/intern" /I "../../../../source/gameengine/soundsystem" /I "..\..\..\..\lib\windows\string\include" /I "../../../../lib/windows/moto/include" /I "..\..\..\..\lib\windows\openal\include" /I "..\..\..\..\source\kernel\gen_system" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\..\obj\windows\gameengine\soundsystem\openal\SND_openal.lib"

!ELSEIF  "$(CFG)" == "SND_openal - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "SND_openal___Win32_Debug"
# PROP BASE Intermediate_Dir "SND_openal___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "..\..\..\..\..\obj\windows\gameengine\soundsystem\openal\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\..\..\source\gameengine\soundsystem" /I "../../../../source/gameengine/soundsystem/intern" /I "../../../../source/gameengine/soundsystem" /I "..\..\..\..\lib\windows\string\include" /I "../../../../lib/windows/moto/include" /I "..\..\..\..\lib\windows\openal\include" /I "..\..\..\..\source\kernel\gen_system" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\..\obj\windows\gameengine\soundsystem\openal\debug\SND_openal.lib"

!ELSEIF  "$(CFG)" == "SND_openal - Win32 MT DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "SND_openal___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "SND_openal___Win32_MT_DLL_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\..\obj\windows\gameengine\soundsystem\openal\mtdll_debug"
# PROP Intermediate_Dir "..\..\..\..\..\obj\windows\gameengine\soundsystem\openal\mtdll_debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /Gm /GX /ZI /Od /I "../../../../lib/windows/moto/include" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\soundsystem" /I "..\..\..\..\lib\windows\openal\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /I "..\..\..\..\source\gameengine\soundsystem" /I "../../../../source/gameengine/soundsystem/intern" /I "../../../../source/gameengine/soundsystem" /I "..\..\..\..\lib\windows\string\include" /I "../../../../lib/windows/moto/include" /I "..\..\..\..\lib\windows\openal\include" /I "..\..\..\..\source\kernel\gen_system" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\..\obj\windows\gameengine\soundsystem\openal\debug\SND_openal.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SND_openal - Win32 MT DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "SND_openal___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "SND_openal___Win32_MT_DLL_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\..\obj\windows\gameengine\soundsystem\openal\mtdll"
# PROP Intermediate_Dir "..\..\..\..\..\obj\windows\gameengine\soundsystem\openal\mtdll"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /GX /O2 /I "../../../../lib/windows/moto/include" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\soundsystem" /I "..\..\..\..\lib\windows\openal\include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD CPP /nologo /MD /GX /O2 /I "..\..\..\..\source\gameengine\soundsystem" /I "../../../../source/gameengine/soundsystem/intern" /I "../../../../source/gameengine/soundsystem" /I "..\..\..\..\lib\windows\string\include" /I "../../../../lib/windows/moto/include" /I "..\..\..\..\lib\windows\openal\include" /I "..\..\..\..\source\kernel\gen_system" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\..\obj\windows\gameengine\soundsystem\openal\SND_openal.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SND_openal - Win32 Profile"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "SND_openal___Win32_Profile"
# PROP BASE Intermediate_Dir "SND_openal___Win32_Profile"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "SND_openal___Win32_Profile"
# PROP Intermediate_Dir "SND_openal___Win32_Profile"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /O2 /I "../../../../lib/windows/moto/include" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\soundsystem" /I "..\..\..\..\lib\windows\openal\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /O2 /I "..\..\..\..\source\gameengine\soundsystem" /I "../../../../source/gameengine/soundsystem/intern" /I "../../../../source/gameengine/soundsystem" /I "..\..\..\..\lib\windows\string\include" /I "../../../../lib/windows/moto/include" /I "..\..\..\..\lib\windows\openal\include" /I "..\..\..\..\source\kernel\gen_system" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\gameengine\soundsystem\openal\debug\SND_openal.lib"
# ADD LIB32 /nologo /out:"..\..\..\..\..\obj\windows\gameengine\soundsystem\openal\profile\SND_openal.lib"

!ENDIF 

# Begin Target

# Name "SND_openal - Win32 Release"
# Name "SND_openal - Win32 Debug"
# Name "SND_openal - Win32 MT DLL Debug"
# Name "SND_openal - Win32 MT DLL Release"
# Name "SND_openal - Win32 Profile"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\SoundSystem\openal\SND_OpenALDevice.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\SoundSystem\openal\SND_OpenALDevice.h
# End Source File
# End Group
# End Target
# End Project
