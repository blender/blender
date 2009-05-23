# Microsoft Developer Studio Project File - Name="KX_converter" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=KX_converter - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "KX_converter.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "KX_converter.mak" CFG="KX_converter - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "KX_converter - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "KX_converter - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "KX_converter - Win32 MT DLL Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "KX_converter - Win32 MT DLL Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "KX_converter - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\gameengine\converter"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\converter"
# PROP Target_Dir ""
MTL=midl.exe
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GR /GX /O2 /I "..\..\..\..\lib\windows\python\include\python2.4" /I "..\..\..\source\gameengine\physics\common" /I "..\..\..\source\gameengine\physics\BlOde" /I "..\..\..\source\gameengine\physics\Dummy" /I "..\..\..\extern\solid" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\source\gameengine\BlenderRoutines" /I "..\..\..\source\sumo\include" /I "..\..\..\..\lib\windows\moto\include" /I "..\..\..\source\\gameengine\physics\sumo\Fuzzics\include" /I "..\..\..\source\gameengine\Ketsji" /I "..\..\..\source\gameengine\SceneGraph" /I "..\..\..\source\gameengine\rasterizer" /I "..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\lib\windows\soundsystem\include" /I "..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\source\gameengine\soundsystem\snd_blenderwavecache" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\gameengine\network" /I "..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\source\gameengine\physics" /I "..\..\..\source\gameengine\physics\ode" /I "..\..\..\source\gameengine\physics\dummy" /I "..\..\..\source\gameengine\physics\sumo" /I "..\..\..\source\gameengine\physics\bullet" /I "..\..\..\extern\bullet\linearmath" /I "..\..\..\extern\bullet\Bulletdynamics" /I "..\..\..\extern\bullet\Bullet" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "USE_SUMO_SOLID" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "KX_converter - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\gameengine\converter\debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\converter\debug"
# PROP Target_Dir ""
MTL=midl.exe
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GR /GX /Zi /Od /I "..\..\..\..\lib\windows\python\include\python2.4" /I "..\..\..\source\gameengine\physics\common" /I "..\..\..\source\gameengine\physics\BlOde" /I "..\..\..\source\gameengine\physics\Dummy" /I "..\..\..\extern\solid" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\source\gameengine\BlenderRoutines" /I "..\..\..\source\sumo\include" /I "..\..\..\..\lib\windows\moto\include" /I "..\..\..\source\\gameengine\physics\sumo\Fuzzics\include" /I "..\..\..\source\gameengine\Ketsji" /I "..\..\..\source\gameengine\SceneGraph" /I "..\..\..\source\gameengine\rasterizer" /I "..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\lib\windows\soundsystem\include" /I "..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\source\gameengine\soundsystem\snd_blenderwavecache" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\gameengine\network" /I "..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\source\gameengine\physics" /I "..\..\..\source\gameengine\physics\ode" /I "..\..\..\source\gameengine\physics\dummy" /I "..\..\..\source\gameengine\physics\sumo" /I "..\..\..\source\gameengine\physics\bullet" /I "..\..\..\extern\bullet\linearmath" /I "..\..\..\extern\bullet\Bulletdynamics" /I "..\..\..\extern\bullet\Bullet" /D "WIN32" /D "_MBCS" /D "_LIB" /D "USE_SUMO_SOLID" /U "_DEBUG" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "KX_converter - Win32 MT DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "KX_converter___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "KX_converter___Win32_MT_DLL_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\gameengine\converter\mtdll_debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\converter\mtdll_debug"
# PROP Target_Dir ""
MTL=midl.exe
LINK32=link.exe -lib
# ADD BASE CPP /nologo /Gm /GX /ZI /Od /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\source\gameengine\BlenderRoutines" /I "..\..\..\source\sumo\include" /I "..\..\..\..\lib\windows\moto\include" /I "..\..\..\source\sumo\Fuzzics\include" /I "..\..\..\source\gameengine\Ketsji" /I "..\..\..\source\gameengine\SceneGraph" /I "..\..\..\source\gameengine\rasterizer" /I "..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\source\gameengine\gamelogic" /I "..\..\..\source\gameengine\soundsystem" /I "..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\source\gameengine\soundsystem\snd_blenderwavecache" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender" /I "..\..\..\source\blender\makesdna" /I "..\..\..\..\lib\windows\python\include\python1.5" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /I "..\..\..\..\lib\windows\python\include\python2.0" /I "..\..\..\source\gameengine\physics" /I "..\..\..\source\gameengine\physics\ode" /I "..\..\..\source\gameengine\physics\dummy" /I "..\..\..\source\gameengine\physics\sumo\include" /I "..\..\..\..\lib\windows\python\include\python2.2" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\source\gameengine\BlenderRoutines" /I "..\..\..\source\sumo\include" /I "..\..\..\..\lib\windows\moto\include" /I "..\..\..\source\sumo\Fuzzics\include" /I "..\..\..\source\gameengine\Ketsji" /I "..\..\..\source\gameengine\SceneGraph" /I "..\..\..\source\gameengine\rasterizer" /I "..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\source\gameengine\gamelogic" /I "..\..\..\source\gameengine\soundsystem" /I "..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\source\gameengine\soundsystem\snd_blenderwavecache" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\gameengine\network" /I "..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\source\gameengine\physics\sumo" /I "..\..\..\source\gameengine\physics\sumo\fuzzics\include" /I "..\..\..\source\gameengine\physics\common" /I "..\..\..\source\gameengine\physics\BlOde" /D "WIN32" /D "_MBCS" /D "_LIB" /U "_DEBUG" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\gameengine\converter\debug\KX_converter.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "KX_converter - Win32 MT DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "KX_converter___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "KX_converter___Win32_MT_DLL_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\gameengine\converter\mtdll"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\converter\mtdll"
# PROP Target_Dir ""
MTL=midl.exe
LINK32=link.exe -lib
# ADD BASE CPP /nologo /GX /O2 /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\source\gameengine\BlenderRoutines" /I "..\..\..\source\sumo\include" /I "..\..\..\..\lib\windows\moto\include" /I "..\..\..\source\sumo\Fuzzics\include" /I "..\..\..\source\gameengine\Ketsji" /I "..\..\..\source\gameengine\SceneGraph" /I "..\..\..\source\gameengine\rasterizer" /I "..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\source\gameengine\gamelogic" /I "..\..\..\source\gameengine\soundsystem" /I "..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\source\gameengine\soundsystem\snd_blenderwavecache" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender" /I "..\..\..\source\blender\makesdna" /I "..\..\..\..\lib\windows\python\include\python2.0" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD CPP /nologo /MD /GX /O2 /I "..\..\..\..\lib\windows\python\include\python2.0" /I "..\..\..\source\gameengine\physics" /I "..\..\..\source\gameengine\physics\ode" /I "..\..\..\source\gameengine\physics\dummy" /I "..\..\..\source\gameengine\physics\sumo\include" /I "..\..\..\..\lib\windows\python\include\python2.2" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\source\gameengine\BlenderRoutines" /I "..\..\..\source\sumo\include" /I "..\..\..\..\lib\windows\moto\include" /I "..\..\..\source\sumo\Fuzzics\include" /I "..\..\..\source\gameengine\Ketsji" /I "..\..\..\source\gameengine\SceneGraph" /I "..\..\..\source\gameengine\rasterizer" /I "..\..\..\source\gameengine\rasterizer\ras_openglrasterizer" /I "..\..\..\source\gameengine\gamelogic" /I "..\..\..\source\gameengine\soundsystem" /I "..\..\..\source\gameengine\soundsystem\snd_openal" /I "..\..\..\source\gameengine\soundsystem\snd_blenderwavecache" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\gameengine\network" /I "..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\source\gameengine\physics\sumo" /I "..\..\..\source\gameengine\physics\sumo\fuzzics\include" /I "..\..\..\source\gameengine\physics\common" /I "..\..\..\source\gameengine\physics\BlOde" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\gameengine\converter\KX_converter.lib"
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "KX_converter - Win32 Release"
# Name "KX_converter - Win32 Debug"
# Name "KX_converter - Win32 MT DLL Debug"
# Name "KX_converter - Win32 MT DLL Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\BL_ActionActuator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\BL_ArmatureObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\BL_BlenderDataConversion.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\BL_DeformableGameObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\BL_MeshDeformer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\BL_SkinDeformer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\BL_SkinMeshObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\BlenderWorldInfo.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\Bullet\CcdPhysicsController.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\Bullet\CcdPhysicsEnvironment.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\KX_BlenderScalarInterpolator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\KX_BlenderSceneConverter.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\KX_ConvertActuators.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\KX_ConvertControllers.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\KX_ConvertProperties.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\KX_ConvertSensors.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\KX_IpoConvert.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\BL_ActionActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\BL_ArmatureObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\BL_BlenderDataConversion.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\BL_DeformableGameObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\BL_MeshDeformer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\BL_SkinDeformer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\BL_SkinMeshObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\BlenderWorldInfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\KX_BlenderScalarInterpolator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\KX_BlenderSceneConverter.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\KX_ConvertActuators.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\KX_ConvertControllers.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\KX_ConvertProperties.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\KX_ConvertSensors.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Converter\KX_IpoConvert.h
# End Source File
# End Group
# End Target
# End Project
