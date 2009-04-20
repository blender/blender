# Microsoft Developer Studio Project File - Name="BPY_python" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=BPY_python - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BPY_python.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BPY_python.mak" CFG="BPY_python - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BPY_python - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "BPY_python - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BPY_python - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\blender\bpython"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\bpython"
# PROP Target_Dir ""
MTL=midl.exe
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\..\lib\windows\python\include\python2.4" /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\blender\img" /I "..\..\..\source\blender\radiosity\extern\include" /I "..\..\..\..\lib\windows\bmfont\include" /I "..\..\source\blender\misc" /I "..\..\..\lib\windows\guardedalloc\include" /I "..\..\source\blender\blenlib" /I "..\..\source\kernel\gen_messaging" /I "..\..\source\blender\include" /I "..\..\source\blender" /I "..\..\source\blender\makesdna" /I "..\..\source\blender\blenkernel" /I "..\..\source\blender\blenloader" /I "..\..\source\blender\bpython\include" /I "..\..\source\blender\imbuf" /I "..\..\source\blender\render\extern\include" /I "..\..\source\blender\radiosity\extern\include" /I "..\..\source\kernel\gen_system" /I "..\..\source\blender\renderconverter\\" /I "..\..\source\blender\renderui\\" /I "..\..\source\blender\python" /I "../../../../lib/windows/bsp/include" /I "..\..\..\source\blender\renderconverter" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\avi" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\blender\blenloader" /I "..\..\..\source\blender" /I "..\..\..\source\blender\bpython\include" /I "../../../source/gameengine\SoundSystem\\" /I "../../../../lib/windows/iksolver/include" /I "..\..\..\source\blender\python" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BPY_python - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\blender\bpython\debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\bpython\debug"
# PROP Target_Dir ""
MTL=midl.exe
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /w /W0 /Gm /GX /Zi /Od /I "..\..\..\..\lib\windows\python\include\python2.4" /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\blender\img" /I "..\..\..\..\lib\windows\bmfont\include" /I "..\..\source\blender\misc" /I "..\..\..\lib\windows\guardedalloc\include" /I "..\..\source\blender\blenlib" /I "..\..\source\kernel\gen_messaging" /I "..\..\source\blender\include" /I "..\..\source\blender" /I "..\..\source\blender\makesdna" /I "..\..\source\blender\blenkernel" /I "..\..\source\blender\blenloader" /I "..\..\..\source\blender\radiosity\extern\include" /I "..\..\source\blender\bpython\include" /I "..\..\source\blender\imbuf" /I "..\..\source\blender\render\extern\include" /I "..\..\source\kernel\gen_system" /I "..\..\source\blender\renderconverter\\" /I "..\..\source\blender\renderui\\" /I "..\..\source\blender\python" /I "../../../../lib/windows/bsp/include" /I "..\..\..\source\blender\renderconverter" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\avi" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\blender\blenloader" /I "..\..\..\source\blender" /I "..\..\..\source\blender\bpython\include" /I "../../../source/gameengine\SoundSystem\\" /I "../../../../lib/windows/iksolver/include" /I "..\..\..\source\blender\python" /D "WIN32" /D "_MBCS" /D "_LIB" /U "_DEBUG" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "BPY_python - Win32 Release"
# Name "BPY_python - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Armature.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\BezTriple.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\BGL.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Blender.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Bone.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\BPY_interface.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\BPY_menus.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Camera.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\charRGBA.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\constant.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Constraint.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\CurNurb.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Curve.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Draw.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Effect.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\euler.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\EXPP_interface.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Font.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\gen_utils.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Geometry.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Group.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\IDProp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Image.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Ipo.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Ipocurve.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Key.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Lamp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Lattice.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Library.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\logic.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Material.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Mathutils.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\matrix.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Mesh.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\meshPrimitive.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Metaball.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Modifier.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\MTex.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\NLA.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\NMesh.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Noise.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Object.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Particle.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\point.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Pose.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\quat.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Registry.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\rgbTuple.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Scene.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\sceneRadio.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\sceneRender.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\sceneTimeLine.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Sound.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\SurfNurb.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Sys.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Text.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Text3d.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Texture.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Types.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\vector.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Window.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\windowTheme.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\World.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Armature.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\BezTriple.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\BGL.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Blender.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Bone.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\BPY_extern.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\BPY_menus.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\bpy_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Camera.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\charRGBA.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\constant.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Constraint.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\CurNurb.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Curve.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Draw.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Effect.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\euler.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\EXPP_interface.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Font.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\gen_utils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Geometry.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Group.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Image.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Ipo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Ipocurve.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Key.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Lamp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Lattice.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Material.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Mathutils.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\matrix.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Mesh.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\meshPrimitive.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Metaball.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Modifier.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\modules.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\MTex.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\NLA.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\NMesh.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Object.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Particle.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\point.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\quat.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Registry.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\rgbTuple.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Scene.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\sceneRadio.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\sceneRender.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\sceneTimeLine.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Sound.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\SurfNurb.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Sys.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Text.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Text3d.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Texture.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\vector.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\Window.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\windowTheme.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\python\api2_2x\World.h
# End Source File
# End Group
# End Target
# End Project
