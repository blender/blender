# Microsoft Developer Studio Project File - Name="iksolver" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=iksolver - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "iksolver.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "iksolver.mak" CFG="iksolver - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "iksolver - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "iksolver - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "iksolver - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\intern\iksolver"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\intern\iksolver"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /Ob2 /I "..\..\..\..\intern\moto\include" /I "..\..\..\..\intern\memutil" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x413 /d "NDEBUG"
# ADD RSC /l 0x413 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\intern\iksolver\libiksolver.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO Copying header files	XCOPY /Y ..\..\extern\*.h ..\..\..\..\..\lib\windows\iksolver\include\	ECHO Copying lib	XCOPY /Y ..\..\..\..\obj\windows\intern\iksolver\*.lib ..\..\..\..\..\lib\windows\iksolver\lib\*.a	ECHO Done
# End Special Build Tool

!ELSEIF  "$(CFG)" == "iksolver - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\intern\iksolver\debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\intern\iksolver\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\..\..\..\intern\moto\include" /I "..\..\..\..\intern\memutil" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x413 /d "_DEBUG"
# ADD RSC /l 0x413 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\intern\iksolver\debug\libiksolver.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO Copying header files	XCOPY /Y ..\..\extern\*.h ..\..\..\..\..\lib\windows\iksolver\include\	ECHO Copying lib	XCOPY /Y ..\..\..\..\obj\windows\intern\iksolver\debug\*.lib ..\..\..\..\..\lib\windows\iksolver\lib\debug\*.a	ECHO Done
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "iksolver - Win32 Release"
# Name "iksolver - Win32 Debug"
# Begin Group "intern"

# PROP Default_Filter ""
# Begin Group "common"

# PROP Default_Filter ""
# End Group
# Begin Group "TNT"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\intern\TNT\cholesky.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\cmat.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\fcscmat.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\fmat.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\fortran.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\fspvec.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\index.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\lapack.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\lu.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\qr.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\region1d.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\region2d.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\stopwatch.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\subscript.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\svd.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\tnt.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\tntmath.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\tntreqs.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\transv.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\triang.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\trisolve.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\vec.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\vecadaptor.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\TNT\version.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\intern\IK_QJacobian.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\IK_QJacobian.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\IK_QJacobianSolver.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\IK_QJacobianSolver.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\IK_QSegment.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\IK_QSegment.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\IK_QSolver_Class.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\IK_QTask.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\IK_QTask.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\IK_Solver.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\MT_ExpMap.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\MT_ExpMap.h
# End Source File
# End Group
# Begin Group "extern"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\extern\IK_solver.h
# End Source File
# End Group
# End Target
# End Project
