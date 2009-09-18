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
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\..\lib\windows\sdl\include\\" /I "..\..\..\source\blender\img" /I "..\..\..\source\blender\renderui" /I "..\..\..\..\lib\windows\soundsystem\include\\" /I "..\..\..\..\lib\windows\python\include\python2.0\\" /I "..\..\..\..\lib\windows\bmfont\include" /I "..\..\..\source\blender\ftfont" /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\renderconverter" /I "..\..\..\source\blender\verify" /I "..\..\..\source\blender\readstreamglue" /I "..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\blender\blenloader" /I "..\..\..\source\blender\quicktime" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\misc" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\radiosity\extern\include" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\gameengine\network" /I "..\..\..\..\lib\windows\decimation\include" /I "..\..\..\source\blender\blenpluginapi\\" /I "..\..\..\..\lib\windows\blenkey\include" /I "..\..\..\..\lib\windows\ghost\include" /I "..\..\..\..\lib\windows\bsp\include" /I "..\..\..\..\lib\windows\opennl\include" /I "..\..\..\intern\elbeem\extern" /I "..\..\..\..\lib\windows\memutil\include" /I "..\..\..\..\lib\windows\pthreads\include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "_CONSOLE" /D "xGAMEBLENDER" /D "WITH_QUICKTIME" /D "INTERNATIONAL" /FR /J /FD /c
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
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\..\..\lib\windows\sdl\include\\" /I "..\..\..\source\blender\img" /I "..\..\..\source\blender\renderui" /I "..\..\..\..\lib\windows\soundsystem\include\\" /I "..\..\..\..\lib\windows\python\include\python2.0\\" /I "..\..\..\..\lib\windows\bmfont\include" /I "..\..\..\source\blender\ftfont" /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\renderconverter" /I "..\..\..\source\blender\verify" /I "..\..\..\source\blender\readstreamglue" /I "..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\blender\blenloader" /I "..\..\..\source\blender\quicktime" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\misc" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\radiosity\extern\include" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\gameengine\network" /I "..\..\..\..\lib\windows\decimation\include" /I "..\..\..\source\blender\blenpluginapi\\" /I "..\..\..\..\lib\windows\blenkey\include" /I "..\..\..\..\lib\windows\ghost\include" /I "..\..\..\..\lib\windows\bsp\include" /I "..\..\..\..\lib\windows\opennl\include" /I "..\..\..\intern\elbeem\extern" /I "..\..\..\..\lib\windows\memutil\include" /I "..\..\..\..\lib\windows\pthreads\include" /D "WIN32" /D "_MBCS" /D "_LIB" /D "_CONSOLE" /D "xGAMEBLENDER" /D "WITH_QUICKTIME" /D "INTERNATIONAL" /U "_DEBUG" /J /FD /GZ /c
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

SOURCE=..\..\..\source\blender\src\bfont.ttf.c
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

SOURCE=..\..\..\source\blender\src\cursors.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\intern\dna.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\drawaction.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\drawarmature.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\drawdeps.c
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

SOURCE=..\..\..\source\blender\src\drawnode.c
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

SOURCE=..\..\..\source\blender\src\drawtime.c
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

SOURCE=..\..\..\source\blender\src\editimasel.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editipo.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editipo_lib.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editipo_mods.c
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

SOURCE=..\..\..\source\blender\src\editmesh_add.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editmesh_lib.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editmesh_loop.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editmesh_mods.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editmesh_tools.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editmode_undo.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editnla.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\editnode.c
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

SOURCE=..\..\..\source\blender\src\edittime.c
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

SOURCE=..\..\..\source\blender\src\fluidsim.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\ghostwinlay.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\glutil.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\hddaudio.c
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

SOURCE=..\..\..\source\blender\src\header_node.c
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

SOURCE=..\..\..\source\blender\src\header_time.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\header_view3d.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\headerbuttons.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\imagepaint.c
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

SOURCE=..\..\..\source\blender\src\interface_icons.c
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

SOURCE=..\..\..\source\blender\src\lorem.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\mainqueue.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\meshtools.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\multires.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\mywindow.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\oops.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\outliner.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\parametrizer.c
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

SOURCE=..\..\..\source\blender\src\preview.blend.c
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

SOURCE=..\..\..\source\blender\src\retopo.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\scrarea.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\screendump.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\sculptmode.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\seqaudio.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\seqeffects.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\seqscopes.c
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

SOURCE=..\..\..\source\blender\src\transform.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\transform_constraints.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\transform_conversions.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\transform_generics.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\transform_manipulator.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\transform_numinput.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\transform_snap.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\unwrapper.c
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

SOURCE=..\..\..\source\blender\include\BDR_drawaction.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BDR_drawmesh.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BDR_drawobject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BDR_editcurve.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BDR_editface.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BDR_editmball.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BDR_editobject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BDR_sculptmode.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BDR_unwrapper.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BDR_vpaint.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_butspace.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_cursors.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_drawimage.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_drawoops.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_drawscene.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_drawscript.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_drawseq.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_drawtext.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_editaction.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_editarmature.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_editconstraint.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_editdeform.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_editfont.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_editgroup.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_editkey.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_editlattice.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_editmesh.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_editmode_undo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_editnla.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_editoops.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_editsca.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_editseq.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_editsima.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_editsound.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_editview.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_fsmenu.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_gl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_glutil.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_graphics.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_imasel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_interface.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_keyval.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_language.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_mainqueue.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_meshtools.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_mywindow.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_oops.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_outliner.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_poseobject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_previewrender.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_renderwin.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_resources.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_retopo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_scrarea.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_screen.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_space.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_spacetypes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_tbcallback.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_toets.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_toolbox.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_transform.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_usiblender.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_writeavicodec.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_writeimage.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BIF_writemovie.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\blendef.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BPI_script.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_buttons.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_drawimasel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_drawipo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_drawnla.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_drawoops.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_drawview.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_edit.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_editaction.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_editaction_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_editipo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_editipo_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_editnla_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_filesel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_headerbuttons.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_seqaudio.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_sequence.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_time.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_trans_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\BSE_view.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\butspace.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\editmesh.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\multires.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\parametrizer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\parametrizer_intern.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\include\transform.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\src\winlay.h
# End Source File
# End Group
# End Target
# End Project
