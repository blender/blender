# Microsoft Developer Studio Project File - Name="ik_glut_test" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=ik_glut_test - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ik_glut_test.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ik_glut_test.mak" CFG="ik_glut_test - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ik_glut_test - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "ik_glut_test - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ik_glut_test - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\..\extern" /I "..\..\..\..\..\..\lib\windows\glut-3.7\include" /I "..\..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\..\..\lib\windows\memutil\include" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x413 /d "NDEBUG"
# ADD RSC /l 0x413 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 iksolver_rmtd.lib libmoto.a /nologo /subsystem:console /machine:I386 /libpath:"..\..\..\..\lib\windows\release" /libpath:"..\..\..\..\..\..\lib\windows\glut-3.7\lib" /libpath:"..\..\..\..\..\..\lib\windows\moto\lib"

!ELSEIF  "$(CFG)" == "ik_glut_test - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\..\..\..\extern" /I "..\..\..\..\..\..\lib\windows\glut-3.7\include" /I "..\..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\..\..\lib\windows\memutil\include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD BASE RSC /l 0x413 /d "_DEBUG"
# ADD RSC /l 0x413 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 iksolver_dmtd.lib libmoto.a /nologo /subsystem:console /debug /machine:I386 /nodefaultlib:"LIBCMTD.lib" /pdbtype:sept /libpath:"..\..\..\..\lib\windows\debug" /libpath:"..\..\..\..\..\..\lib\windows\glut-3.7\lib" /libpath:"..\..\..\..\..\..\lib\windows\moto\lib\debug"

!ENDIF 

# Begin Target

# Name "ik_glut_test - Win32 Release"
# Name "ik_glut_test - Win32 Debug"
# Begin Group "common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\common\GlutDrawer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\common\GlutDrawer.h
# End Source File
# Begin Source File

SOURCE=..\..\common\GlutKeyboardManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\common\GlutKeyboardManager.h
# End Source File
# Begin Source File

SOURCE=..\..\common\GlutMouseManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\common\GlutMouseManager.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\intern\ChainDrawer.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\main.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\MyGlutKeyHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\MyGlutMouseHandler.h
# End Source File
# End Target
# End Project
