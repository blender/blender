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
# ADD CPP /nologo /MT /W3 /GX /I "../../../../../lib/windows/sdl/include" /I "../../../../../lib/windows/zlib/include" /I "../../../../../lib/windows/png/include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "NOGUI" /D "MSVC6" /D "ELBEEM_BLENDER" /YX /FD /c
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
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "../../../../../lib/windows/sdl/include" /I "../../../../../lib/windows/zlib/include" /I "../../../../../lib/windows/png/include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "NOGUI" /D "MSVC6" /D "ELBEEM_BLENDER" /YX /FD /GZ /c
# ADD BASE RSC /l 0x407 /d "_DEBUG"
# ADD RSC /l 0x407 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

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

SOURCE=..\..\intern\blendercall.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\cfglexer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\cfgparser.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\elbeem.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\factory_fsgr.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\isosurface.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\lbminterface.cpp
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

SOURCE=..\..\intern\ntl_lightobject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_ray.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_raytracer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\ntl_scene.cpp
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

SOURCE=..\..\intern\utilities.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\intern\attributes.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\cfgparser.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\lbmfsgrsolver.h
# End Source File
# End Group
# End Target
# End Project
