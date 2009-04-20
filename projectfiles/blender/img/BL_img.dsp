# Microsoft Developer Studio Project File - Name="BL_img" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=BL_IMG - WIN32 DEBUG
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "BL_img.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "BL_img.mak" CFG="BL_IMG - WIN32 DEBUG"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "BL_img - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "BL_img - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "BL_img - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\obj\windows\blender\img"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\img"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "BL_img - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\obj\windows\blender\img\debug"
# PROP Intermediate_Dir "..\..\..\obj\windows\blender\img\debug"
# PROP Target_Dir ""
LINK32=link.exe -lib
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /YX /J /FD /GZ /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "BL_img - Win32 Release"
# Name "BL_img - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\source\blender\img\intern\IMG_Api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\img\intern\IMG_BrushRGBA32.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\img\intern\IMG_CanvasRGBA32.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\img\intern\IMG_Line.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\img\intern\IMG_Pixmap.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\img\intern\IMG_PixmapRGBA32.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\img\intern\IMG_Rect.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "intern"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\source\blender\img\intern\IMG_BrushRGBA32.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\img\intern\IMG_CanvasRGBA32.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\img\intern\IMG_Color.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\img\intern\IMG_Line.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\img\intern\IMG_MemPtr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\img\intern\IMG_Pixmap.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\img\intern\IMG_PixmapRGBA32.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\img\intern\IMG_Rect.h
# End Source File
# Begin Source File

SOURCE=..\..\..\source\blender\img\intern\IMG_Types.h
# End Source File
# End Group
# Begin Group "extern"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\source\blender\img\IMG_Api.h
# End Source File
# End Group
# End Group
# End Target
# End Project
