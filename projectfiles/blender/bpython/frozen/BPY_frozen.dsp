# Microsoft Developer Studio Project File - Name="BPY_frozen" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=BPY_frozen - Win32 Profile
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BPY_frozen.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BPY_frozen.mak" CFG="BPY_frozen - Win32 Profile"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BPY_frozen - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "BPY_frozen - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "BPY_frozen - Win32 Profile" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BPY_frozen - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "..\..\..\..\..\obj\windows\blender\bpython\frozen"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\..\lib\windows\python\include\python2.0" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x413 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\..\obj\windows\blender\bpython\frozen\BPY_frozen.lib"

!ELSEIF  "$(CFG)" == "BPY_frozen - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "..\..\..\..\..\obj\windows\blender\bpython\frozen\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\..\..\..\lib\windows\python\include\python2.0" /D "WIN32" /D "_MBCS" /D "_LIB" /U "_DEBUG" /YX /FD /GZ /c
# ADD BASE RSC /l 0x413 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\..\obj\windows\blender\bpython\frozen\debug\BPY_frozen.lib"

!ELSEIF  "$(CFG)" == "BPY_frozen - Win32 Profile"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "BPY_frozen___Win32_Profile"
# PROP BASE Intermediate_Dir "BPY_frozen___Win32_Profile"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "BPY_frozen___Win32_Profile"
# PROP Intermediate_Dir "BPY_frozen___Win32_Profile"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\..\..\..\lib\windows\python\include\python2.0" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /O2 /I "..\..\..\..\lib\windows\python\include\python2.0" /D "WIN32" /D "_MBCS" /D "_LIB" /U "_DEBUG" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\..\obj\windows\blender\bpython\frozen\debug\BPY_frozen.lib"
# ADD LIB32 /nologo /out:"..\..\..\..\..\..\obj\windows\blender\bpython\frozen\debug\BPY_frozen.lib"

!ENDIF 

# Begin Target

# Name "BPY_frozen - Win32 Release"
# Name "BPY_frozen - Win32 Debug"
# Name "BPY_frozen - Win32 Profile"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\config.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\frozen.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_beta.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_beta__Objects.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_beta__Scenegraph.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_blenderos.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_Converter.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_Converter__importer.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_Converter__importer__VRMLimporter.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_Converter__importloader.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_copy.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_copy_reg.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_gzip.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_mcf.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_mcf__utils.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_mcf__utils__hier_rx.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_mcf__utils__hierobj.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_mcf__utils__singletonlist.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_repr.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_simpleparse.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_simpleparse__bootstrap.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_simpleparse__generator.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_string.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_TextTools.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_TextTools__Constants.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_TextTools__Constants__Sets.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_TextTools__Constants__TagTables.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_TextTools__mxTextTools.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_TextTools__TextTools.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_types.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_utils.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_utils__quat.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_utils__vect.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_vrml.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_vrml__basenodes.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_vrml__fieldcoercian.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_vrml__loader.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_vrml__parser.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_vrml__scenegraph.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_vrml__utils.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_vrml__utils__collapse.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_vrml__utils__err.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_vrml__utils__namespace.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\blender\bpython\frozen\M_vrml__utils__typeclasses.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# End Target
# End Project
