# Microsoft Developer Studio Project File - Name="BKE_blenkernel" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=BKE_blenkernel - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BKE_blenkernel.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BKE_blenkernel.mak" CFG="BKE_blenkernel - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BKE_blenkernel - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "BKE_blenkernel - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "BKE_blenkernel - Win32 MT DLL Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "BKE_blenkernel - Win32 MT DLL Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BKE_blenkernel - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\blender\blenkernel"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\blenkernel"
# PROP Target_Dir ""
MTL=midl.exe
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\..\lib\windows\decimation\include" /I "../../../../lib/windows/zlib/include" /I "..\..\..\intern\elbeem\extern" /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\renderconverter" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\avi" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\blender\blenloader" /I "..\..\..\source\blender" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\blenlib" /I "../../../source/gameengine\SoundSystem\\" /I "../../../../lib/windows/iksolver/include" /I "../../../../lib/windows/bsp/include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "WITH_FREETYPE2" /D "USE_CCGSUBSURFLIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BKE_blenkernel - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\blender\blenkernel\debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\blenkernel\debug"
# PROP Target_Dir ""
MTL=midl.exe
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\..\..\lib\windows\decimation\include" /I "../../../../lib/windows/zlib/include" /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\renderconverter" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\avi" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\blender\blenloader" /I "..\..\..\source\blender" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\blenlib" /I "../../../source/gameengine\SoundSystem\\" /I "../../../../lib/windows/iksolver/include" /I "../../../../lib/windows/bsp/include" /I "..\..\..\intern\elbeem\extern" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "WITH_FREETYPE2" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BKE_blenkernel - Win32 MT DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "BKE_blenkernel___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "BKE_blenkernel___Win32_MT_DLL_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\blender\blenkernel\mtdll_debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\blenkernel\mtdll_debug"
# PROP Target_Dir ""
MTL=midl.exe
LINK32=link.exe -lib
# ADD BASE CPP /nologo /Gm /GX /ZI /Od /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\avi" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\soundsystem" /I "..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\source\blender\blenloader" /I "..\..\..\source\blender" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\gameengine\soundsystem\snd_blenderwavecache" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# SUBTRACT BASE CPP /WX
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /I "..\..\..\source\blenloader" /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\renderconverter" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\avi" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\blender\blenloader" /I "..\..\..\source\blender" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\blenlib" /I "../../../source/gameengine\SoundSystem\\" /I "../../../../lib/windows/iksolver/include" /I "../../../../lib/windows/bsp/include" /I "..\..\..\intern\elbeem\extern" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# SUBTRACT CPP /WX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\blender\blenkernel\debug\BKE_blenkernel.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BKE_blenkernel - Win32 MT DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "BKE_blenkernel___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "BKE_blenkernel___Win32_MT_DLL_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\blender\blenkernel\mtdll"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\blenkernel\mtdll"
# PROP Target_Dir ""
MTL=midl.exe
LINK32=link.exe -lib
# ADD BASE CPP /nologo /GX /O2 /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\avi" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\soundsystem" /I "..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\source\blender\blenloader" /I "..\..\..\source\blender" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\gameengine\soundsystem\snd_blenderwavecache" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD CPP /nologo /MD /GX /O2 /I "..\..\..\source\gameengine\soundsystem" /I "..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\source\gameengine\soundsystem\snd_blenderwavecache" /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\renderconverter" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\avi" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\blender\blenloader" /I "..\..\..\source\blender" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\blenlib" /I "../../../source/gameengine\SoundSystem\\" /I "../../../../lib/windows/iksolver/include" /I "../../../../lib/windows/bsp/include" /I "..\..\..\intern\elbeem\extern" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\blender\blenkernel\BKE_blenkernel.lib"
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "BKE_blenkernel - Win32 Release"
# Name "BKE_blenkernel - Win32 Debug"
# Name "BKE_blenkernel - Win32 MT DLL Debug"
# Name "BKE_blenkernel - Win32 MT DLL Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\action.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\idprop.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\anim.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\armature.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\blender.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\bmfont.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\brush.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\CCGSubSurf.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\cdderivedmesh.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\colortools.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\constraint.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\curve.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\customdata.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\deform.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\depsgraph.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\DerivedMesh.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\displist.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\effect.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\exotic.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\font.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\group.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\icons.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\image.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\ipo.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\key.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\lattice.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\library.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\material.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\mball.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\mesh.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\modifier.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\nla.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\node.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\node_composite.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\node_shaders.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\object.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\packedFile.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\property.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\sca.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\scene.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\screen.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\script.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\softbody.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\sound.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\subsurf_ccg.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\text.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\texture.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\world.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\writeavi.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\writeframeserver.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_action.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_anim.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_armature.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_bad_level_calls.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_blender.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_bmfont.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_bmfont_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_booleanops.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_booleanops_mesh.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_brush.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_cdderivedmesh.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_constraint.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_curve.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_customdata.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_deform.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_depsgraph.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_DerivedMesh.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_displist.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_effect.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_endian.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_exotic.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_font.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_global.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_group.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_image.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_ipo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_key.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_lattice.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_library.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_main.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_material.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_mball.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_mesh.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_modifier.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_nla.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_object.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_packedFile.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_plugin_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_property.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_sca.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_scene.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_screen.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_script.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_softbody.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_sound.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_subsurf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_text.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_texture.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_utildefines.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_world.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\BKE_writeavi.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\intern\CCGSubSurf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\blenkernel\depsgraph_private.h
# End Source File
# End Group
# End Target
# End Project
