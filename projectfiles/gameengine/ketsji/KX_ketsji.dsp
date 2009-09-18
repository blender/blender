# Microsoft Developer Studio Project File - Name="KX_ketsji" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=KX_ketsji - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "KX_ketsji.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "KX_ketsji.mak" CFG="KX_ketsji - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "KX_ketsji - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "KX_ketsji - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "KX_ketsji - Win32 MT DLL Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "KX_ketsji - Win32 MT DLL Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "KX_ketsji - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\gameengine\ketsji"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\ketsji"
# PROP Target_Dir ""
LINK32=link.exe -lib
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GR /GX /O2 /I "..\..\..\source\gameengine\physics\common\dummy" /I "..\..\..\extern\solid" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\..\lib\windows\moto\include" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\..\lib\windows\soundsystem\include" /I "..\..\..\source\gameengine\rasterizer" /I "..\..\..\source\gameengine\scenegraph" /I "..\..\..\source\gameengine\gamelogic" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\source\sumo\include" /I "..\..\..\source\sumo\fuzzics\include" /I "..\..\..\source\gameengine\network" /I "..\..\..\source\gameengine\Converter" /I "..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\source\gameengine\physics" /I "..\..\..\source\gameengine\physics\common" /I "..\..\..\source\gameengine\physics\dummy" /I "..\..\..\source\gameengine\physics\sumo" /I "..\..\..\source\gameengine\physics\sumo\fuzzics\include" /I "..\..\..\source\gameengine\physics\sumo\include" /I "..\..\..\source\gameengine\physics\BlOde" /I "..\..\..\..\lib\windows\python\include\python2.4" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\imbuf" /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\gameengine\physics\bullet" /I "..\..\..\extern\bullet\linearmath" /I "..\..\..\extern\bullet\Bulletdynamics" /I "..\..\..\extern\bullet\Bullet" /I "..\..\..\source\gameengine\Rasterizer\RAS_OpenGLRasterizer" /I "..\..\..\source\blender\blenlib" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "EXP_PYTHON_EMBEDDING" /D "USE_SUMO_SOLID" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "KX_ketsji - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\gameengine\ketsji\debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\ketsji\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GR /GX /Zi /Od /I "..\..\..\source\gameengine\physics\common\dummy" /I "..\..\..\extern\solid" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\..\lib\windows\moto\include" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\..\lib\windows\soundsystem\include" /I "..\..\..\source\gameengine\rasterizer" /I "..\..\..\source\gameengine\scenegraph" /I "..\..\..\source\gameengine\gamelogic" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\source\sumo\include" /I "..\..\..\source\sumo\fuzzics\include" /I "..\..\..\source\gameengine\network" /I "..\..\..\source\gameengine\Converter" /I "..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\source\gameengine\physics" /I "..\..\..\source\gameengine\physics\common" /I "..\..\..\source\gameengine\physics\dummy" /I "..\..\..\source\gameengine\physics\sumo" /I "..\..\..\source\gameengine\physics\sumo\fuzzics\include" /I "..\..\..\source\gameengine\physics\sumo\include" /I "..\..\..\source\gameengine\physics\BlOde" /I "..\..\..\..\lib\windows\python\include\python2.4" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\imbuf" /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\gameengine\physics\bullet" /I "..\..\..\extern\bullet\linearmath" /I "..\..\..\extern\bullet\Bulletdynamics" /I "..\..\..\extern\bullet\Bullet" /I "..\..\..\source\gameengine\Rasterizer\RAS_OpenGLRasterizer" /I "..\..\..\source\blender\blenlib" /D "JANCODEPANCO" /D "WIN32" /D "_MBCS" /D "_LIB" /D "EXP_PYTHON_EMBEDDING" /D "USE_SUMO_SOLID" /U "_DEBUG" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "KX_ketsji - Win32 MT DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "KX_ketsji___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "KX_ketsji___Win32_MT_DLL_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\gameengine\ketsji\mtdll_debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\ketsji\mtdll_debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
MTL=midl.exe
# ADD BASE CPP /nologo /Gm /GX /ZI /Od /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\soundsystem" /I "..\..\..\source\gameengine\rasterizer" /I "..\..\..\source\gameengine\scenegraph" /I "..\..\..\source\gameengine\gamelogic" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\source\sumo\include" /I "..\..\..\source\sumo\fuzzics\include" /I "..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\lib\windows\python\include\python1.5" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "EXP_PYTHON_EMBEDDING" /YX /J /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /I "..\..\..\..\lib\windows\python\include\python2.4" /I "..\..\..\source\gameengine\physics\common\dummy" /I "..\..\..\..\lib\windows\python\include\python2.2" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\..\lib\windows\moto\include" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\soundsystem" /I "..\..\..\source\gameengine\rasterizer" /I "..\..\..\source\gameengine\scenegraph" /I "..\..\..\source\gameengine\gamelogic" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\source\sumo\include" /I "..\..\..\source\sumo\fuzzics\include" /I "..\..\..\source\gameengine\network" /I "..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\source\gameengine\physics" /I "..\..\..\source\gameengine\physics\common" /I "..\..\..\source\gameengine\physics\dummy" /I "..\..\..\source\gameengine\physics\sumo" /I "..\..\..\source\gameengine\physics\sumo\fuzzics\include" /I "..\..\..\source\gameengine\physics\sumo\include" /I "..\..\..\source\gameengine\physics\BlOde" /D "WIN32" /D "_MBCS" /D "_LIB" /D "EXP_PYTHON_EMBEDDING" /U "_DEBUG" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\gameengine\ketsji\debug\KX_ketsji.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "KX_ketsji - Win32 MT DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "KX_ketsji___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "KX_ketsji___Win32_MT_DLL_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\gameengine\ketsji\mtdll"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\ketsji\mtdll"
# PROP Target_Dir ""
LINK32=link.exe -lib
MTL=midl.exe
# ADD BASE CPP /nologo /GX /O2 /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\soundsystem" /I "..\..\..\source\gameengine\rasterizer" /I "..\..\..\source\gameengine\scenegraph" /I "..\..\..\source\gameengine\gamelogic" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\source\sumo\include" /I "..\..\..\source\sumo\fuzzics\include" /I "..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\lib\windows\python\include\python2.0" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "EXP_PYTHON_EMBEDDING" /YX /J /FD /c
# ADD CPP /nologo /MD /GX /O2 /I "..\..\..\..\lib\windows\python\include\python2.4" /I "..\..\..\source\gameengine\physics\common\dummy" /I "..\..\..\..\lib\windows\python\include\python2.2" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\..\lib\windows\moto\include" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\soundsystem" /I "..\..\..\source\gameengine\rasterizer" /I "..\..\..\source\gameengine\scenegraph" /I "..\..\..\source\gameengine\gamelogic" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\source\sumo\include" /I "..\..\..\source\sumo\fuzzics\include" /I "..\..\..\source\gameengine\network" /I "..\..\..\source\gameengine\ketsji\kxnetwork" /I "..\..\..\source\gameengine\physics" /I "..\..\..\source\gameengine\physics\common" /I "..\..\..\source\gameengine\physics\dummy" /I "..\..\..\source\gameengine\physics\sumo" /I "..\..\..\source\gameengine\physics\sumo\fuzzics\include" /I "..\..\..\source\gameengine\physics\sumo\include" /I "..\..\..\source\gameengine\physics\BlOde" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "EXP_PYTHON_EMBEDDING" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\gameengine\ketsji\KX_ketsji.lib"
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "KX_ketsji - Win32 Release"
# Name "KX_ketsji - Win32 Debug"
# Name "KX_ketsji - Win32 MT DLL Debug"
# Name "KX_ketsji - Win32 MT DLL Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "ActuatorsImp"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_CameraActuator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_CDActuator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_ConstraintActuator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_GameActuator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_IpoActuator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_ObjectActuator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_SCA_AddObjectActuator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_SCA_EndObjectActuator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_SCA_ReplaceMeshActuator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_SceneActuator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_SoundActuator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_TrackToActuator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_VisibilityActuator.cpp
# End Source File
# End Group
# Begin Group "SG_ControllersImp"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_CameraIpoSGController.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_IPO_SGController.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_LightIpoSGController.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_ObColorIpoSGController.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_WorldIpoController.cpp
# End Source File
# End Group
# Begin Group "SensorsImp"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_MouseFocusSensor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_NearSensor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_RadarSensor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_RaySensor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_TouchSensor.cpp
# End Source File
# End Group
# Begin Group "IposImp"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_OrientationInterpolator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_PositionInterpolator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_ScalarInterpolator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_ScalingInterpolator.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\BL_Material.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\BL_Shader.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\BL_Texture.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_BlenderMaterial.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_BulletPhysicsController.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_Camera.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_ConstraintWrapper.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_ConvertPhysicsObjects.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_EmptyObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_GameObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_IPhysicsController.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_KetsjiEngine.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_Light.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_MaterialIpoController.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_MeshProxy.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_MotionState.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_OdePhysicsController.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_PhysicsObjectWrapper.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_PolygonMaterial.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_PyConstraintBinding.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_PyMath.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_PythonInit.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_RayCast.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_RayEventManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_Scene.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_SG_BoneParentNodeRelationship.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_SG_NodeRelationships.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_SumoPhysicsController.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_TimeCategoryLogger.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_TimeLogger.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_TouchEventManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_VehicleWrapper.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_VertexProxy.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_WorldInfo.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "Actuators"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_CameraActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_CDActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_ConstraintActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_GameActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_IpoActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_ObjectActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_SCA_AddObjectActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_SCA_EndObjectActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_SCA_ReplaceMeshActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_SceneActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_SoundActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_TrackToActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_VisibilityActuator.h
# End Source File
# End Group
# Begin Group "SG_Controllers"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_CameraIpoSGController.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_IPO_SGController.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_LightIpoSGController.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_ObColorIpoSGController.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_WorldIpoController.h
# End Source File
# End Group
# Begin Group "Sensors"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_MouseFocusSensor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_NearSensor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_RadarSensor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_RaySensor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_TouchSensor.h
# End Source File
# End Group
# Begin Group "Ipos"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_IInterpolator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_IScalarInterpolator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_OrientationInterpolator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_PositionInterpolator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_ScalarInterpolator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_ScalingInterpolator.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\BL_Material.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\BL_Shader.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\BL_Texture.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_BlenderMaterial.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_BulletPhysicsController.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_Camera.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_ConstraintWrapper.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_ConvertPhysicsObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_EmptyObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_GameObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_IPhysicsController.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_IPOTransform.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_ISceneConverter.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_ISystem.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_KetsjiEngine.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_Light.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_MaterialIpoController.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_MeshProxy.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_MotionState.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_OdePhysicsController.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_PhysicsObjectWrapper.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_PolygonMaterial.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_PyConstraintBinding.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_PyMath.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_PythonInit.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_RayCast.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_RayEventManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_Scene.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_SG_BoneParentNodeRelationship.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_SG_NodeRelationships.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_SumoPhysicsController.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_TimeCategoryLogger.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_TimeLogger.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_TouchEventManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_VehicleWrapper.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_VertexProxy.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Ketsji\KX_WorldInfo.h
# End Source File
# End Group
# End Target
# End Project
