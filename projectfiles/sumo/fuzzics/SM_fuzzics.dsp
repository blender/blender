# Microsoft Developer Studio Project File - Name="SM_fuzzics" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=SM_fuzzics - Win32 Profile
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "SM_fuzzics.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "SM_fuzzics.mak" CFG="SM_fuzzics - Win32 Profile"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "SM_fuzzics - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "SM_fuzzics - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "SM_fuzzics - Win32 MT DLL Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "SM_fuzzics - Win32 MT DLL Release" (based on "Win32 (x86) Static Library")
!MESSAGE "SM_fuzzics - Win32 Profile" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "SM_fuzzics - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\sumo\fuzzics"
# PROP Intermediate_Dir "..\..\..\obj\windows\sumo\fuzzics"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GR /GX /O2 /I "..\..\..\source\gameengine\physics\sumo\Fuzzics\include" /I "../../../../lib/windows/moto/include" /I "../../../extern/solid" /I "..\..\..\source\sumo\include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SM_fuzzics - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\sumo\fuzzics\debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\sumo\fuzzics\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GR /GX /Zi /Od /I "..\..\..\source\gameengine\physics\sumo\Fuzzics\include" /I "../../../../lib/windows/moto/include" /I "../../../extern/solid" /I "..\..\..\source\sumo\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SM_fuzzics - Win32 MT DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "SM_fuzzics___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "SM_fuzzics___Win32_MT_DLL_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\sumo\fuzzics\mtdll_debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\sumo\fuzzics\mtdll_debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /Gm /GX /ZI /Od /I "..\..\..\source\sumo\Fuzzics\include" /I "../../../../../lib/windows/moto/include" /I "..\..\..\source\sumo\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /I "..\..\..\source\sumo\Fuzzics\include" /I "../../../../lib/windows/moto/include" /I "..\..\..\source\sumo\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\sumo\fuzzics\debug\SM_fuzzics.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SM_fuzzics - Win32 MT DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "SM_fuzzics___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "SM_fuzzics___Win32_MT_DLL_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\sumo\fuzzics\mtdll"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\sumo\fuzzics\mtdll"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /GX /O2 /I "..\..\..\source\sumo\Fuzzics\include" /I "../../../../../lib/windows/moto/include" /I "..\..\..\source\sumo\include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD CPP /nologo /MD /GX /O2 /I "..\..\..\source\sumo\Fuzzics\include" /I "../../../../lib/windows/moto/include" /I "..\..\..\source\sumo\include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\sumo\fuzzics\SM_fuzzics.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SM_fuzzics - Win32 Profile"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "SM_fuzzics___Win32_Profile"
# PROP BASE Intermediate_Dir "SM_fuzzics___Win32_Profile"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "SM_fuzzics___Win32_Profile"
# PROP Intermediate_Dir "SM_fuzzics___Win32_Profile"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\..\..\source\sumo\Fuzzics\include" /I "../../../../../lib/windows/moto/include" /I "..\..\..\source\sumo\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /O2 /I "..\..\..\source\sumo\Fuzzics\include" /I "../../../../lib/windows/moto/include" /I "..\..\..\source\sumo\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\obj\windows\sumo\fuzzics\debug\SM_fuzzics.lib"
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\sumo\fuzzics\profile\SM_fuzzics.lib"

!ENDIF 

# Begin Target

# Name "SM_fuzzics - Win32 Release"
# Name "SM_fuzzics - Win32 Debug"
# Name "SM_fuzzics - Win32 MT DLL Debug"
# Name "SM_fuzzics - Win32 MT DLL Release"
# Name "SM_fuzzics - Win32 Profile"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\Sumo\Fuzzics\src\SM_FhObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\Sumo\Fuzzics\src\SM_Object.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\Sumo\Fuzzics\src\SM_Scene.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\Sumo\Fuzzics\include\SM_Callback.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\Sumo\Fuzzics\include\SM_ClientObjectInfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\Sumo\Fuzzics\include\SM_Debug.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\Sumo\Fuzzics\include\SM_FhObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\Sumo\Fuzzics\include\SM_MotionState.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\Sumo\Fuzzics\include\SM_Object.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\Sumo\Fuzzics\include\SM_Props.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Physics\Sumo\Fuzzics\include\SM_Scene.h
# End Source File
# End Group
# End Target
# End Project
