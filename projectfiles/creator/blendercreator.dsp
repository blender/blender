# Microsoft Developer Studio Project File - Name="blendercreator" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=blendercreator - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "blendercreator.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "blendercreator.mak" CFG="blendercreator - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "blendercreator - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "blendercreator - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "blendercreator - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\obj\windows\creator"
# PROP Intermediate_Dir "..\..\obj\windows\creator"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\lib\windows\guardedalloc\include" /I "..\..\source\blender\misc" /I "..\..\source\blender\renderui" /I "..\..\source\blender\renderconverter" /I "..\..\source\kernel\gen_messaging" /I "..\..\source\blender\readstreamglue" /I "..\..\source\blender\imbuf" /I "..\..\source\kernel\gen_system" /I "..\..\source\blender\radiosity\extern\include" /I "..\..\source\blender\render\extern\include" /I "..\..\source\blender\blenlib" /I "..\..\source\blender\makesdna" /I "..\..\source\blender\blenkernel" /I "..\..\source\blender\include" /I "..\..\source\blender" /I "..\..\source\blender\blenloader" /I "..\..\source\blender\bpython\include" /I "..\..\source\gameengine\SoundSystem" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /YX /J /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 odelib.lib fmodvc.lib libbsp.a libguardedalloc.a libbmfont.a libstring.a libghost.a ws2_32.lib openal_static.lib dxguid.lib opengl32.lib libjpeg-static.a glu32.lib user32.lib gdi32.lib vfw32.lib advapi32.lib ole32.lib libdecimation.a libblenkey.a libeay32.lib libiksolver.a libpng.a libz.a libmoto.a /nologo /subsystem:console /machine:I386 /nodefaultlib:"glut32.lib" /nodefaultlib:"libcd.lib" /nodefaultlib:"libc.lib" /nodefaultlib:"libcpd.lib" /nodefaultlib:"libcp.lib" /nodefaultlib:"libcmtd.lib" /out:"..\..\obj\windows\blendercreator.exe" /libpath:"..\..\lib\windows\ode\lib" /libpath:"..\..\lib\windows\bsp\lib" /libpath:"..\..\lib\windows\ghost\lib" /libpath:"..\..\lib\windows\bmfont\lib" /libpath:"..\..\lib\windows\openal\lib" /libpath:"..\..\lib\windows\iksolver\lib\\" /libpath:"..\..\lib\windows\ode-0.03\lib" /libpath:"..\..\lib\windows\python\frozen" /libpath:"..\..\lib\windows\guardedalloc\lib" /libpath:"..\..\lib\windows\string\lib" /libpath:"..\..\lib\windows\moto\lib" /libpath:"..\..\lib\windows\python\lib" /libpath:"..\..\lib\windows\jpeg\lib" /libpath:"..\..\lib\windows\decimation\lib" /libpath:"..\..\lib\windows\blenkey\lib" /libpath:"..\..\lib\windows\openssl\lib" /libpath:"..\..\lib\windows\zlib\lib\\" /libpath:"..\..\lib\windows\png\lib\\" /libpath:"..\..\lib\windows\fmod\lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "blendercreator - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\obj\windows\creator\debug"
# PROP Intermediate_Dir "..\..\obj\windows\creator\debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\lib\windows\guardedalloc\include" /I "..\..\source\blender\misc" /I "..\..\source\blender\renderui" /I "..\..\source\blender\renderconverter" /I "..\..\source\kernel\gen_messaging" /I "..\..\source\blender\readstreamglue" /I "..\..\source\blender\imbuf" /I "..\..\source\kernel\gen_system" /I "..\..\source\blender\radiosity\extern\include" /I "..\..\source\blender\render\extern\include" /I "..\..\source\blender\blenlib" /I "..\..\source\blender\makesdna" /I "..\..\source\blender\blenkernel" /I "..\..\source\blender\include" /I "..\..\source\blender" /I "..\..\source\blender\blenloader" /I "..\..\source\blender\bpython\include" /I "..\..\source\gameengine\SoundSystem" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /YX /J /FD /GZ /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 odelib.lib fmodvc.lib libbsp.a libguardedalloc.a libbmfont.a libstring.a libghost.a ws2_32.lib openal_static.lib dxguid.lib opengl32.lib libjpeg-static.a glu32.lib user32.lib gdi32.lib vfw32.lib advapi32.lib ole32.lib libdecimation.a libblenkey.a libeay32.lib libiksolver.a libpng.a libz.a libmoto.a /nologo /subsystem:console /debug /machine:I386 /nodefaultlib:"glut32.lib" /nodefaultlib:"libcd.lib" /nodefaultlib:"libc.lib" /nodefaultlib:"libcpd.lib" /nodefaultlib:"libcp.lib" /nodefaultlib:"libcmt.lib" /out:"..\..\obj\windows\debug\blendercreator.exe" /pdbtype:sept /libpath:"..\..\lib\windows\ode\lib" /libpath:"..\..\lib\windows\bsp\lib" /libpath:"..\..\lib\windows\ghost\lib\debug" /libpath:"..\..\lib\windows\bmfont\lib\debug" /libpath:"..\..\lib\windows\openal\lib\debug" /libpath:"..\..\lib\windows\iksolver\lib" /libpath:"..\..\lib\windows\ode-0.03\lib" /libpath:"..\..\lib\windows\python\frozen" /libpath:"..\..\lib\windows\guardedalloc\lib" /libpath:"..\..\lib\windows\string\lib" /libpath:"..\..\lib\windows\moto\lib" /libpath:"..\..\lib\windows\python\lib" /libpath:"..\..\lib\windows\jpeg\lib" /libpath:"..\..\lib\windows\decimation\lib" /libpath:"..\..\lib\windows\blenkey\lib" /libpath:"..\..\lib\windows\openssl\lib" /libpath:"..\..\lib\windows\zlib\lib\\" /libpath:"..\..\lib\windows\png\lib\\" /libpath:"..\..\lib\windows\fmod\lib"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "blendercreator - Win32 Release"
# Name "blendercreator - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\source\creator\creator.c
# End Source File
# Begin Source File

SOURCE=..\..\source\icons\winblender.rc
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\..\source\icons\winblender.ico
# End Source File
# Begin Source File

SOURCE=..\..\source\icons\winblenderfile.ico
# End Source File
# End Group
# End Target
# End Project
