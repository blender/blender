# Microsoft Developer Studio Project File - Name="OpenNL" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=OpenNL - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "OpenNL.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "OpenNL.mak" CFG="OpenNL - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "OpenNL - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "OpenNL - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "OpenNL - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "../../../../obj/windows/intern/opennl"
# PROP Intermediate_Dir "../../../../obj/windows/intern/opennl/imf"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "../../extern" /I "../../superlu" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x407 /d "NDEBUG"
# ADD RSC /l 0x407 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../../../../obj/windows/intern/opennl\blender_ONL.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO copy header files	XCOPY /Y ..\..\extern\*.h ..\..\..\..\..\lib\windows\opennl\include\*.h	ECHO copy library	XCOPY /Y ..\..\..\..\obj\windows\intern\openNL\*.lib ..\..\..\..\..\lib\windows\openNL\*.lib
# End Special Build Tool

!ELSEIF  "$(CFG)" == "OpenNL - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "../../../../obj/windows/intern/opennl/Debug/"
# PROP Intermediate_Dir "../../../../obj/windows/intern/opennl/imf/Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "../../extern" /I "../../superlu" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x407 /d "_DEBUG"
# ADD RSC /l 0x407 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../../../../obj/windows/intern/opennl/Debug/blender_ONL.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO copy header files	XCOPY /Y ..\..\extern\*.h ..\..\..\..\..\lib\windows\opennl\include\*.h	ECHO copy library	XCOPY /Y ..\..\..\..\obj\windows\intern\openNL\debug\*.lib ..\..\..\..\..\lib\windows\openNL\debug\*.lib
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "OpenNL - Win32 Release"
# Name "OpenNL - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\superlu\colamd.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\get_perm_c.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\heap_relax_snode.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\lsame.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\memory.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\mmd.c
# End Source File
# Begin Source File

SOURCE=..\..\intern\opennl.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\relax_snode.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\scolumn_bmod.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\scolumn_dfs.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\scopy_to_ucol.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\sgssv.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\sgstrf.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\sgstrs.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\smemory.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\smyblas2.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\sp_coletree.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\sp_ienv.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\sp_preorder.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\spanel_bmod.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\spanel_dfs.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\spivotL.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\spruneL.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\ssnode_bmod.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\ssnode_dfs.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\ssp_blas2.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\ssp_blas3.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\strsv.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\superlu_timer.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\sutil.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\util.c
# End Source File
# Begin Source File

SOURCE=..\..\superlu\xerbla.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\superlu\Cnames.h
# End Source File
# Begin Source File

SOURCE=..\..\superlu\colamd.h
# End Source File
# Begin Source File

SOURCE=..\..\extern\ONL_opennl.h
# End Source File
# Begin Source File

SOURCE=..\..\superlu\ssp_defs.h
# End Source File
# Begin Source File

SOURCE=..\..\superlu\supermatrix.h
# End Source File
# Begin Source File

SOURCE=..\..\superlu\util.h
# End Source File
# End Group
# End Target
# End Project
