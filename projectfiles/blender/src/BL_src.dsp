# Microsoft Developer Studio Project File - Name="BL_src" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=BL_SRC - WIN32 DEBUG
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BL_src.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BL_src.mak" CFG="BL_SRC - WIN32 DEBUG"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BL_src - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "BL_src - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BL_src - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\blender\src"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\src"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\..\lib\windows\sdl\include\\" /I "..\..\..\source\blender\img" /I "..\..\..\source\blender\renderui" /I "..\..\..\..\lib\windows\soundsystem\include\\" /I "..\..\..\..\lib\windows\python\include\python2.0\\" /I "..\..\..\..\lib\windows\bmfont\include" /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\renderconverter" /I "..\..\..\source\blender\verify" /I "..\..\..\source\blender\readstreamglue" /I "..\..\..\source\blender\ftfont" /I "..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\blender\blenloader" /I "..\..\..\source\blender\quicktime" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\misc" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\radiosity\extern\include" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\gameengine\network" /I "..\..\..\..\lib\windows\decimation\include" /I "..\..\..\source\blender\blenpluginapi\\" /I "..\..\..\..\lib\windows\blenkey\include" /I "..\..\..\..\lib\windows\ghost\include" /I "..\..\..\..\lib\windows\bsp\include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "_CONSOLE" /D "GAMEBLENDER" /D "WITH_QUICKTIME" /D "INTERNATIONAL" /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BL_src - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\blender\src\debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\src\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\..\..\lib\windows\sdl\include\\" /I "..\..\..\source\blender\img" /I "..\..\..\source\blender\renderui" /I "..\..\..\..\lib\windows\soundsystem\include\\" /I "..\..\..\..\lib\windows\python\include\python2.0\\" /I "..\..\..\..\lib\windows\bmfont\include" /I "..\..\..\source\blender\ftfont" /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\renderconverter" /I "..\..\..\source\blender\verify" /I "..\..\..\source\blender\readstreamglue" /I "..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\blender\blenloader" /I "..\..\..\source\blender\quicktime" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\misc" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\radiosity\extern\include" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\gameengine\network" /I "..\..\..\..\lib\windows\decimation\include" /I "..\..\..\source\blender\blenpluginapi\\" /I "..\..\..\..\lib\windows\blenkey\include" /I "..\..\..\..\lib\windows\ghost\include" /I "..\..\..\..\lib\windows\bsp\include" /D "WIN32" /D "_MBCS" /D "_LIB" /D "_CONSOLE" /D "GAMEBLENDER" /D "WITH_QUICKTIME" /D "INTERNATIONAL" /U "_DEBUG" /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "BL_src - Win32 Release"
# Name "BL_src - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\blender\src\B.blend.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\Bfont.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\blenderbuttons.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\booleanops.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\booleanops_mesh.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\butspace.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\buttons_editing.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\buttons_logic.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\buttons_object.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\buttons_scene.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\buttons_script.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\buttons_shading.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\cmap.tga.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\cmovie.tga.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\intern\dna.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\drawaction.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\drawimage.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\drawimasel.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\drawipo.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\drawmesh.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\drawnla.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\drawobject.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\drawoops.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\drawscene.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\drawscript.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\drawseq.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\drawsound.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\drawtext.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\drawview.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\edit.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editaction.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editarmature.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editconstraint.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editcurve.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editdeform.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editface.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editfont.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editgroup.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editika.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editimasel.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editipo.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editkey.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editlattice.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editmball.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editmesh.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editnla.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editobject.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editoops.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editscreen.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editseq.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editsima.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editsound.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editview.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\eventdebug.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\filesel.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\ghostwinlay.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\glutil.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\header_action.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\header_buttonswin.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\header_filesel.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\header_image.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\header_imasel.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\header_info.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\header_ipo.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\header_nla.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\header_oops.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\header_script.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\header_seq.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\header_sound.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\header_text.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\header_view3d.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\headerbuttons.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\imasel.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\interface.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\interface_draw.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\interface_panel.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\keyval.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\language.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\mainqueue.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\mywindow.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\oops.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\playanim.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenpluginapi\intern\pluginapi.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\poseobject.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\previewrender.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\renderwin.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\resources.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\scrarea.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\screendump.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\seqaudio.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\sequence.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\space.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\spacetypes.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\splash.jpg.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\swapbuffers.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\toets.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\toolbox.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\usiblender.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\view.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\vpaint.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\writeavicodec.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\writeimage.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\writemovie.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_butspace.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_drawscript.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_editfont.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_editoops.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_imasel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_language.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_previewrender.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_resources.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_spacetypes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_writeavicodec.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\blendef.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_filesel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\butspace.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\winlay.h
# End Source File
# End Group
# End Target
# End Project
