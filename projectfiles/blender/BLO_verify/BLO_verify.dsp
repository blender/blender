# Microsoft Developer Studio Project File - Name="BLO_verify" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=BLO_verify - Win32 Profile
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BLO_verify.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BLO_verify.mak" CFG="BLO_verify - Win32 Profile"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BLO_verify - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "BLO_verify - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "BLO_verify - Win32 MT DLL Release" (based on "Win32 (x86) Static Library")
!MESSAGE "BLO_verify - Win32 MT DLL Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "BLO_verify - Win32 Profile" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BLO_verify - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\blender\verify"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\..\source\kernel\gen_messaging" /I "..\..\..\lib\windows\openssl\include" /I "..\..\..\lib\windows\zlib\include" /I "..\..\..\source\blender\verify" /I "..\..\..\source\blender\readstreamglue" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\blender\verify\BLO_verify.lib"

!ELSEIF  "$(CFG)" == "BLO_verify - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\blender\verify\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\..\source\kernel\gen_messaging" /I "..\..\..\lib\windows\openssl\include" /I "..\..\..\lib\windows\zlib\include" /I "..\..\..\source\blender\verify" /I "..\..\..\source\blender\readstreamglue" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\blender\verify\debug\BLO_verify.lib"

!ELSEIF  "$(CFG)" == "BLO_verify - Win32 MT DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "BLO_verify___Win32_MT_DLL_Release"
# PROP BASE Intermediate_Dir "BLO_verify___Win32_MT_DLL_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\obj\windows\blender\verify\mtdll"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\blender\verify\mtdll"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /I "..\..\..\source\kernel\gen_messaging" /I "..\..\..\lib\windows\openssl\include" /I "..\..\..\lib\windows\zlib\include" /I "..\..\..\source\blender\verify" /I "..\..\..\source\blender\readstreamglue" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\..\..\source\kernel\gen_messaging" /I "..\..\..\lib\windows\openssl\include" /I "..\..\..\lib\windows\zlib\include" /I "..\..\..\source\blender\verify" /I "..\..\..\source\blender\readstreamglue" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BLO_verify - Win32 MT DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "BLO_verify___Win32_MT_DLL_Debug"
# PROP BASE Intermediate_Dir "BLO_verify___Win32_MT_DLL_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\obj\windows\blender\verify\mtdll_debug"
# PROP Intermediate_Dir "..\..\..\..\obj\windows\blender\verify\mtdll_debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\..\..\source\kernel\gen_messaging" /I "..\..\..\lib\windows\openssl\include" /I "..\..\..\lib\windows\zlib\include" /I "..\..\..\source\blender\verify" /I "..\..\..\source\blender\readstreamglue" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\..\..\source\kernel\gen_messaging" /I "..\..\..\lib\windows\openssl\include" /I "..\..\..\lib\windows\zlib\include" /I "..\..\..\source\blender\verify" /I "..\..\..\source\blender\readstreamglue" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BLO_verify - Win32 Profile"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "BLO_verify___Win32_Profile"
# PROP BASE Intermediate_Dir "BLO_verify___Win32_Profile"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "BLO_verify___Win32_Profile"
# PROP Intermediate_Dir "BLO_verify___Win32_Profile"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\..\..\source\kernel\gen_messaging" /I "..\..\..\lib\windows\openssl\include" /I "..\..\..\lib\windows\zlib\include" /I "..\..\..\source\blender\verify" /I "..\..\..\source\blender\readstreamglue" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FR /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /I "..\..\..\source\kernel\gen_messaging" /I "..\..\..\lib\windows\openssl\include" /I "..\..\..\lib\windows\zlib\include" /I "..\..\..\source\blender\verify" /I "..\..\..\source\blender\readstreamglue" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\obj\windows\blender\verify\debug\BLO_verify.lib"
# ADD LIB32 /nologo /out:"..\..\..\..\obj\windows\blender\verify\debug\BLO_verify.lib"

!ENDIF 

# Begin Target

# Name "BLO_verify - Win32 Release"
# Name "BLO_verify - Win32 Debug"
# Name "BLO_verify - Win32 MT DLL Release"
# Name "BLO_verify - Win32 MT DLL Debug"
# Name "BLO_verify - Win32 Profile"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\blender\verify\intern\BLO_verify.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# End Target
# End Project
