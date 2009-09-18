# Microsoft Developer Studio Project File - Name="blender" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=blender - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "blender.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "blender.mak" CFG="blender - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "blender - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "blender - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "blender - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\obj\windows\blender"
# PROP Intermediate_Dir "..\..\obj\windows\blender"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\source\blender\misc" /I "..\..\..\lib\windows\guardedalloc\include" /I "..\..\source\blender\blenlib" /I "..\..\source\kernel\gen_messaging" /I "..\..\source\blender\include" /I "..\..\source\blender" /I "..\..\source\blender\makesdna" /I "..\..\source\blender\blenkernel" /I "..\..\source\blender\blenloader" /I "..\..\source\blender\python" /I "..\..\source\blender\imbuf" /I "..\..\source\blender\render\extern\include" /I "..\..\source\blender\radiosity\extern\include" /I "..\..\source\kernel\gen_system" /I "..\..\source\blender\renderconverter\\" /I "..\..\source\blender\renderui\\" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "WITH_QUICKTIME" /YX /FD /c
# ADD BASE RSC /l 0x413 /d "NDEBUG"
# ADD RSC /l 0x413 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 solid.lib SDL.lib freetype2ST.lib ftgl_static_ST.lib gnu_gettext.lib qtmlClient.lib openal_static.lib libsoundsystem.a libopenalsoundsystem.lib libdummysoundsystem.lib libguardedalloc.a libbsp.a libbmfont.a libghost.a libstring.a ws2_32.lib dxguid.lib opengl32.lib libjpeg.a glu32.lib user32.lib shell32.lib gdi32.lib vfw32.lib advapi32.lib winmm.lib ole32.lib libdecimation.a libiksolver.a libpng.a zlib.lib libmoto.a blender_ONL.lib blender_elbeem.lib boolop.lib BLI_bullet.lib pthreadVC2.lib /nologo /subsystem:console /machine:I386 /nodefaultlib:"msvcprt.lib" /nodefaultlib:"msvcrt.lib" /nodefaultlib:"glut32.lib" /nodefaultlib:"libcd.lib" /nodefaultlib:"libc.lib" /nodefaultlib:"libcpd.lib" /nodefaultlib:"libcp.lib" /nodefaultlib:"libcmtd.lib" /out:"..\..\bin\blender.exe" /libpath:"..\..\..\lib\windows\sdl\lib" /libpath:"..\..\..\lib\windows\freetype\lib" /libpath:"..\..\..\lib\windows\ftgl\lib" /libpath:"..\..\..\lib\windows\gettext\lib" /libpath:"..\..\..\lib\windows\ode\lib" /libpath:"..\..\..\lib\windows\bsp\lib" /libpath:"..\..\..\lib\windows\moto\lib" /libpath:"..\..\..\lib\windows\bmfont\lib" /libpath:"..\..\..\lib\windows\ghost\lib" /libpath:"..\..\..\lib\windows\python\frozen" /libpath:"..\..\..\lib\windows\guardedalloc\lib" /libpath:"..\..\..\lib\windows\string\lib" /libpath:"..\..\..\lib\windows\solid\lib" /libpath:"..\..\..\lib\windows\python\lib" /libpath:"..\..\..\lib\windows\iksolver\lib" /libpath:"..\..\..\lib\windows\decimation\lib" /libpath:"..\..\..\lib\windows\openal\lib" /libpath:"..\..\..\lib\windows\jpeg\lib" /libpath:"..\..\..\lib\windows\blenkey\lib" /libpath:"..\..\..\lib\windows\openssl\lib" /libpath:"..\..\obj\windows\intern\soundsystem\dummy" /libpath:"..\..\obj\windows\intern\soundsystem\openal" /libpath:"..\..\..\lib\windows\soundsystem\lib\\" /libpath:"..\..\..\lib\windows\zlib\lib\\" /libpath:"..\..\..\lib\windows\png\lib\\" /libpath:"..\..\..\lib\windows\opennl" /libpath:"..\..\..\lib\windows\elbeem\lib" /libpath:"..\..\..\lib\windows\boolop\lib" /libpath:"..\..\..\lib\windows\bullet\lib" /libpath:"..\..\..\lib\windows\pthreads\lib"
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO Copying required 3rd party dlls...	XCOPY /Y ..\..\..\lib\windows\gettext\lib\*.dll ..\..\bin\	XCOPY /Y ..\..\..\lib\windows\sdl\lib\*dll ..\..\bin\	ECHO Done
# End Special Build Tool

!ELSEIF  "$(CFG)" == "blender - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\obj\windows\blender\debug"
# PROP Intermediate_Dir "..\..\obj\windows\blender\debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\..\source\blender\misc" /I "..\..\..\lib\windows\guardedalloc\include" /I "..\..\source\blender\blenlib" /I "..\..\source\kernel\gen_messaging" /I "..\..\source\blender\include" /I "..\..\source\blender" /I "..\..\source\blender\makesdna" /I "..\..\source\blender\blenkernel" /I "..\..\source\blender\blenloader" /I "..\..\source\blender\python" /I "..\..\source\blender\imbuf" /I "..\..\source\blender\render\extern\include" /I "..\..\source\blender\radiosity\extern\include" /I "..\..\source\kernel\gen_system" /I "..\..\source\blender\renderconverter\\" /I "..\..\source\blender\renderui" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "WITH_QUICKTIME" /YX /FD /GZ /c
# ADD BASE RSC /l 0x413 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 solid.lib SDL.lib freetype2ST.lib ftgl_static_ST.lib gnu_gettext.lib qtmlClient.lib openal_static.lib libsoundsystem.a libopenalsoundsystem.lib libdummysoundsystem.lib libguardedalloc.a libbsp.a libbmfont.a libghost.a libstring.a ws2_32.lib dxguid.lib opengl32.lib libjpeg.a glu32.lib user32.lib gdi32.lib shell32.lib vfw32.lib advapi32.lib winmm.lib ole32.lib libdecimation.a libiksolver.a libpng.a zlib.lib libmoto.a blender_ONL.lib blender_elbeem.lib boolop.lib BLI_bullet_d.lib pthreadVC2.lib /nologo /subsystem:console /debug /machine:I386 /nodefaultlib:"glut32.lib" /nodefaultlib:"libcd.lib" /nodefaultlib:"libc.lib" /nodefaultlib:"libcpd.lib" /nodefaultlib:"libcp.lib" /nodefaultlib:"libcmt.lib" /nodefaultlib:"libcpmtd.lib" /nodefaultlib:"msvcprtd.lib" /out:"..\..\bin\debug\blender.exe" /pdbtype:sept /libpath:"..\..\..\lib\windows\sdl\lib" /libpath:"..\..\..\lib\windows\freetype\lib" /libpath:"..\..\..\lib\windows\ftgl\lib" /libpath:"..\..\..\lib\windows\gettext\lib" /libpath:"..\..\..\lib\windows\ode\lib" /libpath:"..\..\..\lib\windows\bsp\lib\debug" /libpath:"..\..\..\lib\windows\moto\lib\debug" /libpath:"..\..\..\lib\windows\bmfont\lib\debug" /libpath:"..\..\..\lib\windows\ghost\lib\debug" /libpath:"..\..\..\lib\windows\python\frozen" /libpath:"..\..\..\lib\windows\guardedalloc\lib\debug" /libpath:"..\..\..\lib\windows\string\lib\debug" /libpath:"..\..\..\lib\windows\solid\lib\\" /libpath:"..\..\..\lib\windows\python\lib" /libpath:"..\..\..\lib\windows\iksolver\lib\debug" /libpath:"..\..\..\lib\windows\decimation\lib\debug" /libpath:"..\..\..\lib\windows\openal\lib" /libpath:"..\..\obj\windows\intern\soundsystem\dummy\debug" /libpath:"..\..\obj\windows\intern\soundsystem\openal\debug" /libpath:"..\..\..\lib\windows\soundsystem\lib\debug\\" /libpath:"..\..\..\lib\windows\jpeg\lib" /libpath:"..\..\..\lib\windows\blenkey\lib\debug" /libpath:"..\..\..\lib\windows\openssl\lib" /libpath:"..\..\..\lib\windows\zlib\lib\\" /libpath:"..\..\..\lib\windows\png\lib\\" /libpath:"..\..\..\lib\windows\opennl\debug" /libpath:"..\..\..\lib\windows\elbeem\lib\debug" /libpath:"..\..\..\lib\windows\boolop\lib" /libpath:"..\..\..\lib\windows\bullet\lib" /libpath:"..\..\..\lib\windows\elbeem\lib" /libpath:"..\..\..\lib\windows\pthreads\lib"
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=ECHO Copying required 3rd party dlls...	XCOPY /Y ..\..\..\lib\windows\gettext\lib\*.dll ..\..\bin\	ECHO Done
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "blender - Win32 Release"
# Name "blender - Win32 Debug"
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
