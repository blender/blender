# Microsoft Developer Studio Project File - Name="BRE_render" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=BRE_render - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BRE_render.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BRE_render.mak" CFG="BRE_render - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BRE_render - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "BRE_render - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BRE_render - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\blender\render"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\render"
# PROP Target_Dir ""
LINK32=link.exe -lib
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\..\lib\windows\sdl\include" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\kernel\gen_messaging" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\blender\quicktime" /I "..\..\..\source\blender\blenloader" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\misc" /I "..\..\..\source\kernel" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\radiosity\extern\include" /I "..\..\..\source\blender\render\intern\include" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender" /I "..\..\..\source\blender\yafray" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "WITH_QUICKTIME" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BRE_render - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\blender\render\debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\render\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\..\lib\windows\sdl\include" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\kernel\gen_messaging" /I "..\..\..\source\blender\imbuf" /I "..\..\..\source\blender\quicktime" /I "..\..\..\source\blender\blenloader" /I "..\..\..\source\blender\include" /I "..\..\..\source\blender\misc" /I "..\..\..\source\kernel" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\radiosity\extern\include" /I "..\..\..\source\blender\render\intern\include" /I "..\..\..\source\blender\render\extern\include" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\python" /I "..\..\..\source\blender" /I "..\..\..\source\blender\yafray" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "WITH_QUICKTIME" /YX /J /FD /GZ /c
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

# Name "BRE_render - Win32 Release"
# Name "BRE_render - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\source\convertblender.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\source\envmap.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\source\gammaCorrectionTables.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\source\imagetexture.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\source\initrender.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\source\pipeline.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\source\pixelblending.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\source\pixelshading.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\source\ray.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\source\rendercore.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\source\renderdatabase.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\source\shadbuf.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\source\texture.c
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\source\zbuf.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\include\edgeRender.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\include\envmap.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\include\gammaCorrectionTables.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\include\initrender.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\include\pixelblending.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\include\pixelshading.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\include\rendercore.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\include\shadbuf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\include\texture.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\render\intern\include\zbuf.h
# End Source File
# End Group
# End Target
# End Project
