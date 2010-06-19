# Microsoft Developer Studio Project File - Name="GP_ps2" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=GP_ps2 - Win32 Profile
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "GP_ps2.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "GP_ps2.mak" CFG="GP_ps2 - Win32 Profile"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "GP_ps2 - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "GP_ps2 - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "GP_ps2 - Win32 Profile" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "GP_ps2 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\gameplayer\ps2"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /GX /O2 /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\rasterizer" /I "..\..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\sumo\include" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "GP_ps2 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\gameengine\gameplayer\ps2\debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\rasterizer" /I "..\..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\sumo\include" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ELSEIF  "$(CFG)" == "GP_ps2 - Win32 Profile"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "GP_ps2___Win32_Profile"
# PROP BASE Intermediate_Dir "GP_ps2___Win32_Profile"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "GP_ps2___Win32_Profile"
# PROP Intermediate_Dir "GP_ps2___Win32_Profile"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\rasterizer" /I "..\..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\sumo\include" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /O2 /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\source\gameengine\rasterizer" /I "..\..\..\..\source\gameengine\gamelogic" /I "..\..\..\..\source\gameengine\expressions" /I "..\..\..\..\source\sumo\include" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "GP_ps2 - Win32 Release"
# Name "GP_ps2 - Win32 Debug"
# Name "GP_ps2 - Win32 Profile"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\PS2\DebugActuator.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\PS2\GPC_Init.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\PS2\PS2DualShockDevice.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\PS2\PS2GamePlayer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\PS2\PS2System.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\PS2\DebugActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\PS2\ExampleEngine.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\PS2\GPC_Init.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\PS2\PS2Canvas.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\PS2\PS2DualShockDevice.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\PS2\PS2InputDevice.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\PS2\PS2Rasterizer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\PS2\PS2RenderTools.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\PS2\PS2System.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\PS2\SamplePolygonMaterial.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
