# Microsoft Developer Studio Project File - Name="BLI_blenlib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=BLI_blenlib - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BLI_blenlib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BLI_blenlib.mak" CFG="BLI_blenlib - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BLI_blenlib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "BLI_blenlib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "BLI_blenlib - Win32 MT DLL Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "BLI_blenlib - Win32 MT DLL Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BLI_blenlib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\blender\blenlib"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\blenlib"
# PROP Target_Dir ""
LINK32=link.exe -lib
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\..\lib\windows\freetype\include" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\makesdna" /I "../../../../lib/windows/zlib/include" /I "..\..\..\..\lib\windows\sdl\include" /I "..\..\..\..\lib\windows\pthreads\include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "WITH_FREETYPE2" /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BLI_blenlib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\blender\blenlib\debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\blenlib\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\..\lib\windows\freetype\include" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\makesdna" /I "../../../../lib/windows/zlib/include" /I "..\..\..\..\lib\windows\sdl\include" /I "..\..\..\..\lib\windows\pthreads\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "WITH_FREETYPE2" /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BLI_blenlib - Win32 MT DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "BLI_blenlib___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "BLI_blenlib___Win32_MT_DLL_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\blender\blenlib\mtdll_debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\blenlib\mtdll_debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
MTL=midl.exe
# ADD BASE CPP /nologo /Gm /GX /ZI /Od /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\makesdna" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\makesdna" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\blender\blenlib\debug\BLI_blenlib.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BLI_blenlib - Win32 MT DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "BLI_blenlib___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "BLI_blenlib___Win32_MT_DLL_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\blender\blenlib\mtdll"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\blenlib\mtdll"
# PROP Target_Dir ""
LINK32=link.exe -lib
MTL=midl.exe
# ADD BASE CPP /nologo /GX /O2 /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\makesdna" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD CPP /nologo /MD /GX /O2 /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\makesdna" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\blender\blenlib\BLI_blenlib.lib"
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "BLI_blenlib - Win32 Release"
# Name "BLI_blenlib - Win32 Debug"
# Name "BLI_blenlib - Win32 MT DLL Debug"
# Name "BLI_blenlib - Win32 MT DLL Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\arithb.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\BLI_dynstr.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\BLI_ghash.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\BLI_heap.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\BLI_linklist.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\BLI_memarena.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\dynlib.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\edgehash.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\fileops.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\freetypefont.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\gsqueue.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\jitter.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\matrixops.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\noise.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\psfont.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\rand.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\rct.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\scanfill.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\storage.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\threads.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\time.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\util.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\vectorops.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\winstuff.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\BLI_arithb.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\BLI_blenlib.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\BLI_callbacks.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\BLI_edgehash.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\BLI_editVert.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\BLI_fileops.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\BLI_ghash.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\BLI_linklist.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\BLI_memarena.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\BLI_scanfill.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\BLI_storage.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\BLI_storage_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\intern\BLI_util.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\BLI_winstuff.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\MTC_matrixops.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\MTC_vectorops.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\PIL_dynlib.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenlib\PIL_time.h
# End Source File
# End Group
# End Target
# End Project
