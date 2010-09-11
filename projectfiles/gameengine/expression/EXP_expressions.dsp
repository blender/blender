# Microsoft Developer Studio Project File - Name="EXP_expressions" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=EXP_expressions - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "EXP_expressions.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "EXP_expressions.mak" CFG="EXP_expressions - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "EXP_expressions - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "EXP_expressions - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "EXP_expressions - Win32 MT DLL Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "EXP_expressions - Win32 MT DLL Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "EXP_expressions - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\gameengine\expressions"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\expressions"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GR /GX /O2 /I "..\..\..\intern\moto\include" /I "..\..\..\..\lib\windows\python\include\python2.4" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\source\kernel\gen_system" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "EXP_PYTHON_EMBEDDING" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "EXP_expressions - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\gameengine\expressions\debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\expressions\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GR /GX /Zi /Od /I "..\..\..\intern\moto\include" /I "..\..\..\..\lib\windows\python\include\python2.4" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\source\kernel\gen_system" /D "WIN32" /D "_MBCS" /D "_LIB" /D "EXP_PYTHON_EMBEDDING" /U "_DEBUG" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "EXP_expressions - Win32 MT DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "EXP_expressions___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "EXP_expressions___Win32_MT_DLL_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\gameengine\expressions\mtdll_debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\expressions\mtdll_debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /MDd /Gm /GX /ZI /Od /I "..\..\..\source\kernel\gen_system" /I "..\..\..\..\lib\windows\python\include\python1.5" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "EXP_PYTHON_EMBEDDING" /YX /J /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /I "..\..\..\..\lib\windows\python\include\python2.0" /I "..\..\..\..\..\lib\windows\python\include\python2.0" /I "..\..\..\..\lib\windows\python\include\python2.2" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\source\kernel\gen_system" /D "WIN32" /D "_MBCS" /D "_LIB" /D "EXP_PYTHON_EMBEDDING" /U "_DEBUG" /YX /J /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\gameengine\expressions\debug\EXP_expressions.lib"
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "EXP_expressions - Win32 MT DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "EXP_expressions___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "EXP_expressions___Win32_MT_DLL_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\gameengine\expressions\mtdll"
# PROP Intermediate_Dir "..\..\..\obj\windows\gameengine\expressions\mtdll"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /GX /O2 /I "..\..\..\source\kernel\gen_system" /I "..\..\..\..\lib\windows\python\include\python2.0" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "EXP_PYTHON_EMBEDDING" /YX /J /FD /c
# ADD CPP /nologo /MD /GX /O2 /I "..\..\..\..\lib\windows\python\include\python2.0" /I "..\..\..\..\..\lib\windows\python\include\python2.0" /I "..\..\..\..\lib\windows\python\include\python2.2" /I "..\..\..\..\lib\windows\string\include" /I "..\..\..\source\kernel\gen_system" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "EXP_PYTHON_EMBEDDING" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\..\obj\windows\gameengine\expressions\EXP_expressions.lib"
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "EXP_expressions - Win32 Release"
# Name "EXP_expressions - Win32 Debug"
# Name "EXP_expressions - Win32 MT DLL Debug"
# Name "EXP_expressions - Win32 MT DLL Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\BoolValue.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\ConstExpr.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\EmptyValue.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\ErrorValue.cpp
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Expressions\EXP_C-Api.cpp"
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\Expression.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\FloatValue.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\IdentifierExpr.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\IfExpr.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\InputParser.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\IntValue.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\KX_HashedPtr.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\ListValue.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\Operator1Expr.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\Operator2Expr.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\PyObjectPlus.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\StringValue.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\Value.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\VectorValue.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\BoolValue.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\ConstExpr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\EmptyValue.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\ErrorValue.h
# End Source File
# Begin Source File

SOURCE="..\..\..\source\gameengine\Expressions\EXP_C-Api.h"
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\Expression.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\FloatValue.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\IdentifierExpr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\IfExpr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\InputParser.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\IntValue.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\KX_HashedPtr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\ListValue.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\Operator1Expr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\Operator2Expr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\PyObjectPlus.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\StringValue.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\Value.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\VectorValue.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\gameengine\Expressions\VoidValue.h
# End Source File
# End Group
# End Target
# End Project
