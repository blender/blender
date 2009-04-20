# Microsoft Developer Studio Project File - Name="SM_moto" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=SM_moto - Win32 Profile
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "SM_moto.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "SM_moto.mak" CFG="SM_moto - Win32 Profile"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "SM_moto - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "SM_moto - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "SM_moto - Win32 MT DLL Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "SM_moto - Win32 MT DLL Release" (based on "Win32 (x86) Static Library")
!MESSAGE "SM_moto - Win32 Profile" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "SM_moto - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\sumo\moto"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\..\..\..\lib\windows\moto\include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\sumo\moto\SM_moto.lib"

!ELSEIF  "$(CFG)" == "SM_moto - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\sumo\moto\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /I "..\..\..\source\sumo\MoTo\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /FR /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\sumo\moto\debug\SM_moto.lib"

!ELSEIF  "$(CFG)" == "SM_moto - Win32 MT DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "SM_moto___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "SM_moto___Win32_MT_DLL_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\sumo\moto\mtdll_debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\sumo\moto\mtdll_debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /Gm /GX /ZI /Od /I "..\..\..\source\sumo\MoTo\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /I "..\..\..\source\sumo\MoTo\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /FR /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\sumo\moto\debug\SM_moto.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SM_moto - Win32 MT DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "SM_moto___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "SM_moto___Win32_MT_DLL_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\sumo\moto\mtdll"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\sumo\moto\mtdll"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /GX /O2 /I "..\..\..\source\sumo\MoTo\include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD CPP /nologo /MD /GX /O2 /I "..\..\..\source\sumo\MoTo\include" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\sumo\moto\SM_moto.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "SM_moto - Win32 Profile"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "SM_moto___Win32_Profile"
# PROP BASE Intermediate_Dir "SM_moto___Win32_Profile"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "SM_moto___Win32_Profile"
# PROP Intermediate_Dir "SM_moto___Win32_Profile"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /O2 /I "..\..\..\source\sumo\MoTo\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /O2 /I "..\..\..\source\sumo\MoTo\include" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\obj\windows\sumo\moto\debug\SM_moto.lib"
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\sumo\moto\profile\SM_moto.lib"

!ENDIF 

# Begin Target

# Name "SM_moto - Win32 Release"
# Name "SM_moto - Win32 Debug"
# Name "SM_moto - Win32 MT DLL Debug"
# Name "SM_moto - Win32 MT DLL Release"
# Name "SM_moto - Win32 Profile"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\src\MT_CmMatrix4x4.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\src\MT_Matrix3x3.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\src\MT_Matrix4x4.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\src\MT_Point3.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\src\MT_Quaternion.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\src\MT_random.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\src\MT_Transform.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\src\MT_Vector2.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\src\MT_Vector3.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\src\MT_Vector4.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\GEN_List.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\GEN_Map.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_assert.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_CmMatrix4x4.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Matrix3x3.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Matrix3x3.inl
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Matrix4x4.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Matrix4x4.inl
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_MinMax.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Optimize.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Point2.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Point2.inl
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Point3.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Point3.inl
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Quaternion.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Quaternion.inl
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_random.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Scalar.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Stream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Transform.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Tuple2.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Tuple3.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Tuple4.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Vector2.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Vector2.inl
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Vector3.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Vector3.inl
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Vector4.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\MT_Vector4.inl
# End Source File
# Begin Source File

SOURCE=..\..\..\source\sumo\MoTo\include\NM_Scalar.h
# End Source File
# End Group
# End Target
# End Project
