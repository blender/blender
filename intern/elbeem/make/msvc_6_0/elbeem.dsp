# Microsoft Developer Studio Project File - Name="elbeem" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=elbeem - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "elbeem.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "elbeem.mak" CFG="elbeem - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "elbeem - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "elbeem - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "elbeem - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /I "../../../../../lib/windows/sdl/include" /I "../../../../../lib/windows/zlib/include" /I "../../../../../lib/windows/png/include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "NOGUI" /D "ELBEEM_BLENDER" /YX /FD /c
# ADD BASE RSC /l 0x407 /d "NDEBUG"
# ADD RSC /l 0x407 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Release\blender_elbeem.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO copy to lib ...	xcopy /Y .\release\blender_elbeem.lib ..\..\..\..\..\lib\windows\elbeem\lib\*.*
# End Special Build Tool

!ELSEIF  "$(CFG)" == "elbeem - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../../../../../lib/windows/sdl/include" /I "../../../../../lib/windows/zlib/include" /I "../../../../../lib/windows/png/include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "NOGUI" /D "ELBEEM_BLENDER" /YX /FD /GZ /c
# ADD BASE RSC /l 0x407 /d "_DEBUG"
# ADD RSC /l 0x407 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Debug\blender_elbeem.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO copy to lib ...	xcopy /Y .\debug\blender_elbeem.lib ..\..\..\..\..\lib\windows\elbeem\lib\debug\*.*
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "elbeem - Win32 Release"
# Name "elbeem - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\intern\attributes.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\elbeem.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\isosurface.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_blenderdumper.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_bsptree.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_geometrymodel.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_geometryobject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_lighting.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_ray.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_world.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\parametrizer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\particletracer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\simulation_object.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\solver_adap.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\solver_init.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\solver_interface.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\solver_main.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\solver_util.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\utilities.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\intern\attributes.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\isosurface.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\mcubes_tables.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_blenderdumper.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_bsptree.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_geometryclass.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_geometrymodel.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_geometryobject.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_geometryshader.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_lighting.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_lightobject.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_material.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_matrices.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_ray.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_renderglobals.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_rndstream.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_scene.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_triangle.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_vector3dim.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_world.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\parametrizer.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\particletracer.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\simulation_object.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\solver_class.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\solver_dimenions.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\solver_interface.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\solver_relax.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\utilities.h
# End Source File
# End Group
# End Target
# End Project
