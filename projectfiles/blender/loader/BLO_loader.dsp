# Microsoft Developer Studio Project File - Name="BLO_loader" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=BLO_loader - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BLO_loader.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BLO_loader.mak" CFG="BLO_loader - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BLO_loader - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "BLO_loader - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "BLO_loader - Win32 MT DLL Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "BLO_loader - Win32 MT DLL Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BLO_loader - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\blender\loader"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\loader"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\source\blender\blenloader" /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\streamglue\\" /I "..\..\..\source\blender\readblenfile\\" /I "..\..\..\source\blender\writeblenfile\\" /I "..\..\..\source\blender\deflate\\" /I "..\..\..\source\blender\inflate\\" /I "..\..\..\..\lib\windows\zlib\include" /I "..\..\..\source\blender\readblenfile" /I "..\..\..\source\kernel\gen_messaging" /I "..\..\..\source\blender\readstreamglue" /I "..\..\..\source\blender\writestreamglue" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\python" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BLO_loader - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\blender\loader\debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\loader\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\blenloader" /I "..\..\..\source\blender\streamglue\\" /I "..\..\..\source\blender\readblenfile\\" /I "..\..\..\source\blender\writeblenfile\\" /I "..\..\..\source\blender\deflate\\" /I "..\..\..\source\blender\inflate\\" /I "..\..\..\..\lib\windows\zlib\include" /I "..\..\..\source\blender\readblenfile" /I "..\..\..\source\kernel\gen_messaging" /I "..\..\..\source\blender\readstreamglue" /I "..\..\..\source\blender\writestreamglue" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\python" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BLO_loader - Win32 MT DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "BLO_loader___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "BLO_loader___Win32_MT_DLL_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\blender\loader\mtdll_debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\loader\mtdll_debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /Gm /GX /ZI /Od /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\python" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\blenloader" /I "..\..\..\source\blender\streamglue\\" /I "..\..\..\source\blender\readblenfile\\" /I "..\..\..\source\blender\writeblenfile\\" /I "..\..\..\source\blender\deflate\\" /I "..\..\..\source\blender\inflate\\" /I "..\..\..\..\lib\windows\zlib\include" /I "..\..\..\source\blender\readblenfile" /I "..\..\..\source\kernel\gen_messaging" /I "..\..\..\source\blender\readstreamglue" /I "..\..\..\source\blender\writestreamglue" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\python" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\blender\loader\debug\BLO_loader.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BLO_loader - Win32 MT DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "BLO_loader___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "BLO_loader___Win32_MT_DLL_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\blender\loader\mtdll"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\loader\mtdll"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /GX /O2 /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\python" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD CPP /nologo /MD /GX /O2 /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\blenloader" /I "..\..\..\source\blender\streamglue\\" /I "..\..\..\source\blender\readblenfile\\" /I "..\..\..\source\blender\writeblenfile\\" /I "..\..\..\source\blender\deflate\\" /I "..\..\..\source\blender\inflate\\" /I "..\..\..\..\lib\windows\zlib\include" /I "..\..\..\source\blender\readblenfile" /I "..\..\..\source\kernel\gen_messaging" /I "..\..\..\source\blender\readstreamglue" /I "..\..\..\source\blender\writestreamglue" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\python" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\blender\loader\BLO_loader.lib"
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "BLO_loader - Win32 Release"
# Name "BLO_loader - Win32 Debug"
# Name "BLO_loader - Win32 MT DLL Debug"
# Name "BLO_loader - Win32 MT DLL Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\blender\blenloader\intern\genfile.c
# ADD CPP /I "..\..\..\source\blender"
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenloader\intern\readblenentry.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenloader\intern\readfile.c
# ADD CPP /I "..\..\..\source\blender"
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenloader\intern\undofile.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenloader\intern\writefile.c
# ADD CPP /I "..\..\..\source\blender"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\source\blender\blenloader\BLO_genfile.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenloader\BLO_readfile.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenloader\BLO_soundfile.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenloader\BLO_undofile.h
# End Source File
# End Group
# End Target
# End Project
