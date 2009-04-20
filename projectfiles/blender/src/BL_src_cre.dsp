# Microsoft Developer Studio Project File - Name="BL_src_cre" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=BL_src_cre - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BL_src_cre.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BL_src_cre.mak" CFG="BL_src_cre - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BL_src_cre - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "BL_src_cre - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BL_src_cre - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\blender\src_cre"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\src_cre"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\renderui" /I "..\..\..\source\gameengine\soundsystem\\" /I "..\..\..\..\lib\windows\python\include\python2.2\\" /I "..\..\..\source\blender\renderconverter" /I "..\..\..\source\blender\verify" /I "..\..\..\source\blender\readstreamglue" /I "..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\blender\blenloader" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\misc" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\radiosity\extern\include" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\gameengine\network" /I "..\..\..\..\lib\windows\decimation\include" /I "..\..\..\source\blender\blenpluginapi\\" /I "..\..\..\..\lib\windows\blenkey\include" /I "..\..\..\..\lib\windows\pthreads\include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "_CONSOLE" /D "NAN_GAME" /D "GAME" /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\obj\windows\blender\src\BL_src_cre.lib"

!ELSEIF  "$(CFG)" == "BL_src_cre - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\blender\src_cre\debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\src_cre\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\renderui" /I "..\..\..\source\gameengine\soundsystem\\" /I "..\..\..\..\lib\windows\python\include\python2.2\\" /I "..\..\..\source\blender\renderconverter" /I "..\..\..\source\blender\verify" /I "..\..\..\source\blender\readstreamglue" /I "..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\blender\blenloader" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\misc" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\radiosity\extern\include" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\gameengine\network" /I "..\..\..\..\lib\windows\decimation\include" /I "..\..\..\source\blender\blenpluginapi\\" /I "..\..\..\..\lib\windows\blenkey\include" /I "..\..\..\..\lib\windows\pthreads\include" /D "WIN32" /D "_MBCS" /D "_LIB" /D "_CONSOLE" /D "NAN_GAME" /D "GAME" /U "_DEBUG" /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\obj\windows\blender\src\debug\BL_src_cre.lib"

!ENDIF 

# Begin Target

# Name "BL_src_cre - Win32 Release"
# Name "BL_src_cre - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\blender\src\cre\license.jpeg.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\cre\license_key.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# End Target
# End Project
