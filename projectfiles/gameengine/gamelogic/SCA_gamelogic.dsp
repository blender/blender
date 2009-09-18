# Microsoft Developer Studio Project File - Name="SCA_GameLogic" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=SCA_GameLogic - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "SCA_GameLogic.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "SCA_GameLogic.mak" CFG="SCA_GameLogic - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "SCA_GameLogic - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "SCA_GameLogic - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "SCA_GameLogic - Win32 MT DLL Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "SCA_GameLogic - Win32 MT DLL Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "SCA_GameLogic - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\gameengine\gamelogic"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\gamelogic"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GR /GX /O2 /I "..\..\..\..\lib\windows\python\include\python2.4" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\..\lib\windows\sdl\include" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\..\lib\windows\moto\include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "EXP_PYTHON_EMBEDDING" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SCA_GameLogic - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "SCA_GameLogic___Win32_Debug"
# PROP BASE Intermediate_Dir "SCA_GameLogic___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\gameengine\gamelogic\debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\gamelogic\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GR /GX /Zi /Od /I "..\..\..\..\lib\windows\python\include\python2.4" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\..\lib\windows\sdl\include" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\..\lib\windows\moto\include" /D "WIN32" /D "_MBCS" /D "_LIB" /D "EXP_PYTHON_EMBEDDING" /U "_DEBUG" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SCA_GameLogic - Win32 MT DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "SCA_GameLogic___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "SCA_GameLogic___Win32_MT_DLL_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\gameengine\gamelogic\mtdll_debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\gamelogic\mtdll_debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /Gm /GX /ZI /Od /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\lib\windows\python\include\python1.5" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "EXP_PYTHON_EMBEDDING" /YX /J /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /I "..\..\..\..\lib\windows\python\include\python2.4" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\..\lib\windows\sdl\include" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\..\lib\windows\moto\include" /D "WIN32" /D "_MBCS" /D "_LIB" /D "EXP_PYTHON_EMBEDDING" /U "_DEBUG" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\gameengine\gamelogic\debug\SCA_GameLogic.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SCA_GameLogic - Win32 MT DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "SCA_GameLogic___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "SCA_GameLogic___Win32_MT_DLL_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\gameengine\gamelogic\mtdll"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\gamelogic\mtdll"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /GX /O2 /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\lib\windows\python\include\python2.0" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "EXP_PYTHON_EMBEDDING" /YX /J /FD /c
# ADD CPP /nologo /MD /GX /O2 /I "..\..\..\..\lib\windows\python\include\python2.4" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\..\lib\windows\sdl\include" /I "..\..\..\source\kernel\gen_system" /I "..\..\..\source\gameengine\expressions" /I "..\..\..\..\lib\windows\moto\include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "EXP_PYTHON_EMBEDDING" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\gameengine\gamelogic\SCA_GameLogic.lib"
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "SCA_GameLogic - Win32 Release"
# Name "SCA_GameLogic - Win32 Debug"
# Name "SCA_GameLogic - Win32 MT DLL Debug"
# Name "SCA_GameLogic - Win32 MT DLL Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "JoystickImp"

# PROP Default_Filter "cpp"
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\Joystick\SCA_Joystick.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\Joystick\SCA_JoystickEvents.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_AlwaysEventManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_AlwaysSensor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_ANDController.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_EventManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_ExpressionController.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_IActuator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_IController.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_IInputDevice.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_ILogicBrick.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_IObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_IScene.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_ISensor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_JoystickManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_JoystickSensor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_KeyboardManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_KeyboardSensor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_LogicManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_MouseManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_MouseSensor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_ORController.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_PropertyActuator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_PropertyEventManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_PropertySensor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_PythonController.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_RandomActuator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_RandomEventManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_RandomNumberGenerator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_RandomSensor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_TimeEventManager.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "Joystick"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\Joystick\SCA_Joystick.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\Joystick\SCA_JoystickDefines.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\Joystick\SCA_JoystickPrivate.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_AlwaysEventManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_AlwaysSensor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_ANDController.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_EventManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_ExpressionController.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_IActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_IController.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_IInputDevice.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_ILogicBrick.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_IObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_IScene.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_ISensor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_JoystickManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_JoystickSensor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_KeyboardManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_KeyboardSensor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_LogicManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_MouseManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_MouseSensor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_ORController.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_PropertyActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_PropertyEventManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_PropertySensor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_PythonController.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_RandomActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_RandomEventManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_RandomNumberGenerator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_RandomSensor.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\GameLogic\SCA_TimeEventManager.h
# End Source File
# End Group
# End Target
# End Project
