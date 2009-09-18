# Microsoft Developer Studio Project File - Name="DNA_makesdna" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=DNA_makesdna - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "DNA_makesdna.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "DNA_makesdna.mak" CFG="DNA_makesdna - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "DNA_makesdna - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "DNA_makesdna - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "DNA_makesdna - Win32 MT DLL Release" (based on "Win32 (x86) Console Application")
!MESSAGE "DNA_makesdna - Win32 MT DLL Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "DNA_makesdna - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\source\blender\makesdna\intern"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\makesdna"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo /o"DNA_makesdna.bsc"
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 libguardedalloc.a BLI_blenlib.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /pdb:"DNA_makesdna.pdb" /machine:I386 /nodefaultlib:"libc.lib" /libpath:"..\..\..\..\lib\windows\guardedalloc\lib" /libpath:"..\..\..\obj\windows\blender\blenlib"
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Desc=Building DNA
PostBuild_Cmds=CD ..\..\..\source\blender\makesdna\intern\	DNA_makesdna.exe dna.c
# End Special Build Tool

!ELSEIF  "$(CFG)" == "DNA_makesdna - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\source\blender\makesdna\intern"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\makesdna\debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /YX /J /FD /GZ /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo /o"DNA_makesdna.bsc"
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 libguardedalloc.a BLI_blenlib.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /pdb:"DNA_makesdna.pdb" /debug /machine:I386 /nodefaultlib:"libc.lib" /pdbtype:sept /libpath:"..\..\..\..\lib\windows\guardedalloc\lib\debug" /libpath:"..\..\..\obj\windows\blender\blenlib\debug"
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Desc=Building DNA
PostBuild_Cmds=CD ..\..\..\source\blender\makesdna\intern\	DNA_makesdna.exe dna.c
# End Special Build Tool

!ELSEIF  "$(CFG)" == "DNA_makesdna - Win32 MT DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "DNA_makesdna___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "DNA_makesdna___Win32_MT_DLL_Release"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\source\blender\makesdna\intern"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\makesdna\mtdll"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /YX /J /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo /o"DNA_makesdna.bsc"
# ADD BSC32 /nologo /o"DNA_makesdna.bsc"
LINK32=link.exe
# ADD BASE LINK32 BLI_blenlib.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /pdb:"DNA_makesdna.pdb" /machine:I386 /out:"..\..\..\source\blender\makesdna\intern\DNA_makesdna.exe" /libpath:"..\..\..\..\obj\windows\blender\blenlib"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 libguardedalloc.a /nologo /subsystem:console /pdb:"DNA_makesdna.pdb" /machine:I386 /libpath:"..\..\..\..\lib\windows\guardedalloc\lib" /libpath:"..\..\..\..\obj\windows\blender\blenlib"
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=CD ..\..\..\source\blender\makesdna\intern\	DNA_makesdna.exe dna.c
# End Special Build Tool

!ELSEIF  "$(CFG)" == "DNA_makesdna - Win32 MT DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "DNA_makesdna___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "DNA_makesdna___Win32_MT_DLL_Debug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\source\blender\makesdna\intern"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\makesdna\mtdll_debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /YX /J /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\..\..\..\lib\windows\guardedalloc\include" /I "..\..\..\source\blender\makesdna" /I "..\..\..\source\blender\blenlib" /I "..\..\..\source\blender\blenkernel" /I "..\..\..\source\blender" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo /o"DNA_makesdna.bsc"
# ADD BSC32 /nologo /o"DNA_makesdna.bsc"
LINK32=link.exe
# ADD BASE LINK32 BLI_blenlib.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /pdb:"DNA_makesdna.pdb" /debug /machine:I386 /out:"..\..\..\source\blender\makesdna\debug\DNA_makesdna.exe" /pdbtype:sept /libpath:"..\..\..\..\obj\windows\blender\blenlib\debug"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 libguardedalloc.a /nologo /subsystem:console /pdb:"DNA_makesdna.pdb" /debug /machine:I386 /pdbtype:sept /libpath:"..\..\..\..\lib\windows\guardedalloc\lib"
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=CD ..\..\..\source\blender\makesdna\intern\	DNA_makesdna.exe dna.c
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "DNA_makesdna - Win32 Release"
# Name "DNA_makesdna - Win32 Debug"
# Name "DNA_makesdna - Win32 MT DLL Release"
# Name "DNA_makesdna - Win32 MT DLL Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\intern\makesdna.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_action_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_actuator_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_armature_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_camera_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_constraint_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_controller_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_curve_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_customdata_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_documentation.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_effect_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_fileglobal_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_group_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_ID.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_image_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_ipo_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_key_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_lamp_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_lattice_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_listBase.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_material_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_mesh_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_meshdata_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_meta_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_modifier_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_nla_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_object_force.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_object_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_oops_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_packedFile_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_property_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_radio_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_scene_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_screen_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_scriptlink_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_sdna_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_sensor_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_sequence_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_sound_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_space_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_text_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_texture_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_userdef_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_vec_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_vfont_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_view2d_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_view3d_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_wave_types.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\makesdna\DNA_world_types.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
