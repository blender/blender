# Microsoft Developer Studio Project File - Name="BL_imbuf" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=BL_imbuf - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BL_imbuf.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BL_imbuf.mak" CFG="BL_imbuf - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BL_imbuf - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "BL_imbuf - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "BL_imbuf - Win32 MT DLL Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "BL_imbuf - Win32 MT DLL Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BL_imbuf - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\blender\imbuf"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\imbuf"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\avi" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\include" /I "..\..\..\..\lib\windows\jpeg\include" /I "..\..\..\..\lib\windows\zlib\include" /I "..\..\..\..\lib\windows\png\include" /I "..\..\..\source\blender\makesdna" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BL_imbuf - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\blender\imbuf\debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\imbuf\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\avi" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\include" /I "..\..\..\..\lib\windows\jpeg\include" /I "..\..\..\..\lib\windows\zlib\include" /I "..\..\..\..\lib\windows\png\include" /I "..\..\..\source\blender\makesdna" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BL_imbuf - Win32 MT DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "BL_imbuf___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "BL_imbuf___Win32_MT_DLL_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\blender\imbuf\mtdll_debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\imbuf\mtdll_debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /Gm /GX /ZI /Od /I "..\..\..\source\blender\avi" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\include" /I "..\..\..\..\lib\windows\jpeg\include" /I "..\..\..\source\blender\makesdna" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\avi" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\include" /I "..\..\..\..\lib\windows\jpeg\include" /I "..\..\..\..\lib\windows\zlib\include" /I "..\..\..\..\lib\windows\png\include" /I "..\..\..\source\blender\makesdna" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\blender\imbuf\debug\BL_imbuf.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BL_imbuf - Win32 MT DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "BL_imbuf___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "BL_imbuf___Win32_MT_DLL_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\blender\imbuf\mtdll"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\imbuf\mtdll"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /GX /O2 /I "..\..\..\source\blender\avi" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\include" /I "..\..\..\..\lib\windows\jpeg\include" /I "..\..\..\source\blender\makesdna" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD CPP /nologo /MD /GX /O2 /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\avi" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\include" /I "..\..\..\..\lib\windows\jpeg\include" /I "..\..\..\..\lib\windows\zlib\include" /I "..\..\..\..\lib\windows\png\include" /I "..\..\..\source\blender\makesdna" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\blender\imbuf\BL_imbuf.lib"
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "BL_imbuf - Win32 Release"
# Name "BL_imbuf - Win32 Debug"
# Name "BL_imbuf - Win32 MT DLL Debug"
# Name "BL_imbuf - Win32 MT DLL Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\allocimbuf.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\amiga.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\anim.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\antialias.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\bitplanes.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\bmp_decode.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\cmap.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\cspace.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\data.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\dither.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\divers.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\filter.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\ham.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\hamx.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\iff.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\imageprocess.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\iris.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\jpeg.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\png_decode.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\png_encode.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\readimage.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\rectop.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\rotate.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\scaling.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\targa.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\util.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\writeimage.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\IMB_allocimbuf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\IMB_amiga.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\IMB_bitplanes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\IMB_bmp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\IMB_cmap.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\IMB_divers.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\IMB_filter.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\IMB_ham.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\IMB_hamx.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\IMB_iff.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\IMB_imbuf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\IMB_imbuf_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\IMB_iris.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\IMB_jpeg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\IMB_png.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\IMB_targa.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\imbuf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\imbuf_patch.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\imbuf\intern\matrix.h
# End Source File
# End Group
# End Target
# End Project
