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
# PROP Output_Dir "..\..\..\..\..\obj\windows\blender\bpython"
# PROP Intermediate_Dir "..\..\..\..\..\obj\windows\blender\bpython"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\..\..\..\lib\windows\bmfont\include" /I "..\..\..\..\..\..\lib\windows\python\include\python2.2" /I "..\..\..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\renderconverter" /I "..\..\..\..\..\source\blender\render\extern\include" /I "..\..\..\..\..\source\blender\avi" /I "..\..\..\..\..\source\blender\blenloader" /I "..\..\..\..\..\source\blender" /I "..\..\..\..\..\source\blender\include" /I "..\..\..\..\..\source\blender\blenkernel" /I "..\..\..\..\..\source\blender\makesdna" /I "..\..\..\..\..\source\blender\blenlib" /I "..\..\..\..\..\source\blender\bpython\include" /I "..\..\..\..\..\source\blender\blenlib\intern" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "FUTURE_PYTHON_API" /YX /J /FD /c
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
# PROP Output_Dir "..\..\..\..\..\obj\windows\blender\bpython\debug"
# PROP Intermediate_Dir "..\..\..\..\..\obj\windows\blender\bpython\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\..\..\..\..\lib\windows\python\include\python2.2" /I "..\..\..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\..\..\..\lib\windows\bmfont\include" /I "..\..\..\source\blender\renderconverter" /I "..\..\..\..\..\source\blender\render\extern\include" /I "..\..\..\..\..\source\blender\avi" /I "..\..\..\..\..\source\blender\blenloader" /I "..\..\..\..\..\source\blender" /I "..\..\..\..\..\source\blender\include" /I "..\..\..\..\..\source\blender\blenkernel" /I "..\..\..\..\..\source\blender\makesdna" /I "..\..\..\..\..\source\blender\blenlib" /I "..\..\..\..\..\source\blender\bpython\include" /I "..\..\..\..\..\source\blender\blenlib\intern" /D "WIN32" /D "_MBCS" /D "_LIB" /D "FUTURE_PYTHON_API" /U "_DEBUG" /YX /J /FD /GZ /c
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

SOURCE=..\..\..\..\..\source\blender\bpython\intern\b_import.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\b_interface.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\BPY_constobject.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\BPY_image.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\BPY_ipo.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\BPY_links.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\BPY_main.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\BPY_object.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\BPY_scene.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\BPY_text.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\BPY_tools.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\opy_blender.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\opy_datablock.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\opy_draw.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\opy_matrix.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\opy_nmesh.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\opy_vector.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\opy_window.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\api.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\b_import.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\b_interface.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\BPY_constobject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\include\BPY_extern.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\BPY_listbase_macro.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\BPY_macros.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\BPY_main.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\BPY_modules.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\include\BPY_objtypes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\BPY_tools.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\BPY_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\BPY_window.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\opy_datablock.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\opy_nmesh.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\opy_vector.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\py_general.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\py_gui.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\py_object_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\blender\bpython\intern\py_objects.h
# End Source File
# End Group
# End Target
# End Project
