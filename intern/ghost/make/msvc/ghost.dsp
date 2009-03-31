# Microsoft Developer Studio Project File - Name="ghost" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=ghost - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ghost.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ghost.mak" CFG="ghost - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ghost - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "ghost - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ghost - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\intern\ghost"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\intern\ghost"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../.." /I "../../../../../lib/windows/string/include" /I "..\..\..\..\intern\string" /I "../../../../../lib/windows/wintab/include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\intern\ghost\libghost.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Desc=Copying GHOST files library (release target) to lib tree.
PostBuild_Cmds=ECHO Copying header files	XCOPY /Y ..\..\*.h ..\..\..\..\..\lib\windows\ghost\include\	ECHO Copying lib	XCOPY /Y ..\..\..\..\obj\windows\intern\ghost\*.lib ..\..\..\..\..\lib\windows\ghost\lib\*.a	ECHO Done
# End Special Build Tool

!ELSEIF  "$(CFG)" == "ghost - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\intern\ghost\debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\intern\ghost\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "../.." /I "../../../../../lib/windows/string/include" /I "..\..\..\..\intern\string" /I "../../../../../lib/windows/wintab/include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\intern\ghost\debug\libghost.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Desc=Copying GHOST files library (debug target) to lib tree.
PostBuild_Cmds=ECHO Copying header files	XCOPY /Y ..\..\*.h ..\..\..\..\..\lib\windows\ghost\include\	ECHO Copying lib	XCOPY /Y ..\..\..\..\obj\windows\intern\ghost\debug\*.lib ..\..\..\..\..\lib\windows\ghost\lib\debug\*.a	ECHO Done
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "ghost - Win32 Release"
# Name "ghost - Win32 Debug"
# Begin Group "Header Files"

# PROP Default_Filter ""
# Begin Group "intern"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\intern\GHOST_Buttons.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_CallbackEventConsumer.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_Debug.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_DisplayManager.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_DisplayManagerWin32.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_Event.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_EventButton.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_EventCursor.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_EventKey.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_EventManager.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_EventPrinter.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_EventWheel.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_EventWindow.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_ModifierKeys.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_System.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_SystemWin32.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_TimerManager.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_TimerTask.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_Window.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_WindowManager.h
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_WindowWin32.h
# End Source File
# End Group
# Begin Group "extern"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\GHOST_C-api.h"
# End Source File
# Begin Source File

SOURCE=..\..\GHOST_IEvent.h
# End Source File
# Begin Source File

SOURCE=..\..\GHOST_IEventConsumer.h
# End Source File
# Begin Source File

SOURCE=..\..\GHOST_ISystem.h
# End Source File
# Begin Source File

SOURCE=..\..\GHOST_ITimerTask.h
# End Source File
# Begin Source File

SOURCE=..\..\GHOST_IWindow.h
# End Source File
# Begin Source File

SOURCE=..\..\GHOST_Rect.h
# End Source File
# Begin Source File

SOURCE=..\..\GHOST_Types.h
# End Source File
# End Group
# End Group
# Begin Group "Source Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\intern\GHOST_Buttons.cpp
# End Source File
# Begin Source File

SOURCE="..\..\intern\GHOST_C-api.cpp"
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_CallbackEventConsumer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_DisplayManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_DisplayManagerWin32.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_EventManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_EventPrinter.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_ISystem.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_ModifierKeys.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_Rect.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_System.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_SystemWin32.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_TimerManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_Window.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_WindowManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\intern\GHOST_WindowWin32.cpp
# End Source File
# End Group
# End Target
# End Project
