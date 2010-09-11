# Microsoft Developer Studio Project File - Name="BL_glut" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=BL_glut - Win32 Profile
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BL_glut.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BL_glut.mak" CFG="BL_glut - Win32 Profile"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BL_glut - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "BL_glut - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "BL_glut - Win32 Profile" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BL_glut - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\blender\glut"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\..\..\source\blender\\" /I "..\..\..\..\lib\windows\glut-3.7\include" /I "..\..\..\source\blender\glut-win" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\blender\glut\BL_glut.lib"

!ELSEIF  "$(CFG)" == "BL_glut - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\blender\glut\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /I "..\..\..\..\lib\windows\glut-3.7\include" /I "..\..\..\source\blender\glut-win" /I "..\..\..\source\blender\\" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /FR /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\blender\glut\debug\BL_glut.lib"

!ELSEIF  "$(CFG)" == "BL_glut - Win32 Profile"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "BL_glut___Win32_Profile"
# PROP BASE Intermediate_Dir "BL_glut___Win32_Profile"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "BL_glut___Win32_Profile"
# PROP Intermediate_Dir "BL_glut___Win32_Profile"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\..\..\..\lib\windows\glut-3.7\include" /I "..\..\..\source\blender\glut-win" /I "..\..\..\source\blender\\" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /O2 /I "..\..\..\..\lib\windows\glut-3.7\include" /I "..\..\..\source\blender\glut-win" /I "..\..\..\source\blender\\" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\obj\windows\blender\glut\debug\BL_glut.lib"
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\blender\glut\profile\BL_glut.lib"

!ENDIF 

# Begin Target

# Name "BL_glut - Win32 Release"
# Name "BL_glut - Win32 Debug"
# Name "BL_glut - Win32 Profile"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_8x13.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_9x15.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_bitmap.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_blender.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_bwidth.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_cindex.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_cmap.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_cursor.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_dials.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_draw.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_dstr.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_event.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_ext.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_fullscrn.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_get.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_hel10.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_hel12.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_hel18.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_helb10.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_helb12.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_helb14.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_helb8.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_init.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_input.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_mesa.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_modifier.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_mroman.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_overlay.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_roman.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_scr12.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_scr14.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_scr15.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_shapes.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_space.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_stroke.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_swap.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_swidth.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_tablet.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_teapot.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_tr10.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_tr24.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_util.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_vidresize.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_warp.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_win.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\glut_winmisc.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\win32_glx.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\win32_menu.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\win32_util.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\source\blender\glut-win\win32_x11.c"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\source\blender\glut\blenderglut.h
# End Source File
# End Group
# End Target
# End Project
