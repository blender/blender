# Microsoft Developer Studio Project File - Name="PHY_Sumo" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=PHY_Sumo - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "PHY_Sumo.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "PHY_Sumo.mak" CFG="PHY_Sumo - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "PHY_Sumo - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "PHY_Sumo - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "PHY_Sumo - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GR /GX /O2 /I "../../../../../extern/solid" /I "../../../../../source/gameengine/physics" /I "../../../../../source/gameengine/physics/common" /I "../../../../../source/gameengine/physics/sumo" /I "../../../../../source/gameengine/physics/sumo/include" /I "../../../../../source/gameengine/physics/sumo/fuzzics/include" /I "../../../../../../lib/windows/Moto/include" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\gameengine\physics\sumo\PHY_Sumo.lib"

!ELSEIF  "$(CFG)" == "PHY_Sumo - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GR /GX /ZI /Od /I "../../../../../extern/solid" /I "../../../../../source/gameengine/physics" /I "../../../../../source/gameengine/physics/common" /I "../../../../../source/gameengine/physics/sumo" /I "../../../../../source/gameengine/physics/sumo/include" /I "../../../../../source/gameengine/physics/sumo/fuzzics/include" /I "../../../../../../lib/windows/Moto/include" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\gameengine\physics\sumo\debug\PHY_Sumo.lib"

!ENDIF 

# Begin Target

# Name "PHY_Sumo - Win32 Release"
# Name "PHY_Sumo - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\Physics\Sumo\Fuzzics\src\SM_FhObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\Physics\Sumo\Fuzzics\src\SM_MotionState.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\Physics\Sumo\Fuzzics\src\SM_Object.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\Physics\Sumo\Fuzzics\src\SM_Scene.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\Physics\Sumo\SumoPHYCallbackBridge.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\Physics\Sumo\SumoPhysicsController.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\Physics\Sumo\SumoPhysicsEnvironment.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\Physics\Sumo\Fuzzics\include\SM_Callback.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\Physics\Sumo\Fuzzics\include\SM_ClientObjectInfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\Physics\Sumo\Fuzzics\include\SM_FhObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\Physics\Sumo\Fuzzics\include\SM_MotionState.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\Physics\Sumo\Fuzzics\include\SM_Object.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\Physics\Sumo\Fuzzics\include\SM_Props.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\Physics\Sumo\Fuzzics\include\SM_Scene.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\Physics\Sumo\SumoPHYCallbackBridge.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\Physics\Sumo\SumoPhysicsController.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\..\source\gameengine\Physics\Sumo\SumoPhysicsEnvironment.h
# End Source File
# End Group
# End Target
# End Project
