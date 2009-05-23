# Microsoft Developer Studio Project File - Name="MoTo" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=MoTo - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "MoTo.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "MoTo.mak" CFG="MoTo - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "MoTo - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "MoTo - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "MoTo - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\intern\moto\"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\intern\moto\"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W4 /GX /O2 /Ob2 /I "..\..\include\\" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x413 /d "NDEBUG"
# ADD RSC /l 0x413 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\intern\moto\libmoto.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO Copying header files	XCOPY /Y ..\..\include\*.h ..\..\..\..\..\lib\windows\moto\include\	ECHO Copying lib	XCOPY /Y ..\..\..\..\obj\windows\intern\moto\*.lib ..\..\..\..\..\lib\windows\moto\lib\*.a	ECHO Done
# End Special Build Tool

!ELSEIF  "$(CFG)" == "MoTo - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\intern\moto\debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\intern\moto\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\..\include\\" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x413 /d "_DEBUG"
# ADD RSC /l 0x413 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\intern\moto\debug\libmoto.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO Copying header files	XCOPY /Y ..\..\include\*.h ..\..\..\..\..\lib\windows\moto\include\	ECHO Copying lib	XCOPY /Y ..\..\..\..\obj\windows\intern\moto\debug\*.lib ..\..\..\..\..\lib\windows\moto\lib\debug\*.a	ECHO Copying Debug info.	XCOPY /Y ..\..\..\..\obj\windows\intern\moto\debug\vc60.* ..\..\..\..\..\lib\windows\moto\lib\debug\	ECHO Done
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "MoTo - Win32 Release"
# Name "MoTo - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\intern\MT_CmMatrix4x4.cpp

!IF  "$(CFG)" == "MoTo - Win32 Release"

# ADD CPP /W3 /I "../../include"

!ELSEIF  "$(CFG)" == "MoTo - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\intern\MT_Matrix3x3.cpp

!IF  "$(CFG)" == "MoTo - Win32 Release"

# ADD CPP /W3 /I "../../include"

!ELSEIF  "$(CFG)" == "MoTo - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\intern\MT_Matrix4x4.cpp

!IF  "$(CFG)" == "MoTo - Win32 Release"

# ADD CPP /W3 /I "../../include"

!ELSEIF  "$(CFG)" == "MoTo - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\intern\MT_Plane3.cpp

!IF  "$(CFG)" == "MoTo - Win32 Release"

# ADD CPP /W3

!ELSEIF  "$(CFG)" == "MoTo - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\intern\MT_Point3.cpp

!IF  "$(CFG)" == "MoTo - Win32 Release"

# ADD CPP /W3 /I "../../include"

!ELSEIF  "$(CFG)" == "MoTo - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\intern\MT_Quaternion.cpp

!IF  "$(CFG)" == "MoTo - Win32 Release"

# ADD CPP /W3 /I "../../include"

!ELSEIF  "$(CFG)" == "MoTo - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\intern\MT_random.cpp

!IF  "$(CFG)" == "MoTo - Win32 Release"

# ADD CPP /W3 /I "../../include"

!ELSEIF  "$(CFG)" == "MoTo - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\intern\MT_Transform.cpp

!IF  "$(CFG)" == "MoTo - Win32 Release"

# ADD CPP /W3 /I "../../include"

!ELSEIF  "$(CFG)" == "MoTo - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\intern\MT_Vector2.cpp

!IF  "$(CFG)" == "MoTo - Win32 Release"

# ADD CPP /W3 /I "../../include"

!ELSEIF  "$(CFG)" == "MoTo - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\intern\MT_Vector3.cpp

!IF  "$(CFG)" == "MoTo - Win32 Release"

# ADD CPP /W3 /I "../../include"

!ELSEIF  "$(CFG)" == "MoTo - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\intern\MT_Vector4.cpp

!IF  "$(CFG)" == "MoTo - Win32 Release"

# ADD CPP /W3 /I "../../include"

!ELSEIF  "$(CFG)" == "MoTo - Win32 Debug"

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "inlines"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\MT_Matrix3x3.inl
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Matrix4x4.inl
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Plane3.inl
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Point2.inl
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Point3.inl
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Quaternion.inl
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Vector2.inl
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Vector3.inl
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Vector4.inl
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\include\GEN_List.h
# End Source File
# Begin Source File

SOURCE=..\..\include\GEN_Map.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_assert.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_CmMatrix4x4.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Matrix3x3.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Matrix4x4.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_MinMax.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Optimize.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Plane3.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Point2.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Point3.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Quaternion.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_random.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Scalar.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Stream.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Transform.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Tuple2.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Tuple3.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Tuple4.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Vector2.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Vector3.h
# End Source File
# Begin Source File

SOURCE=..\..\include\MT_Vector4.h
# End Source File
# Begin Source File

SOURCE=..\..\include\NM_Scalar.h
# End Source File
# End Group
# End Target
# End Project
