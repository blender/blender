# Microsoft Developer Studio Project File - Name="gpplugin" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=gpplugin - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "gpplugin.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "gpplugin.mak" CFG="gpplugin - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "gpplugin - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "gpplugin - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "gpplugin - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\..\obj\windows\gameengine\gameplayer\qt\gpplugin"
# PROP Intermediate_Dir "..\..\..\..\..\obj\windows\gameengine\gameplayer\qt\gpplugin"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GPPLUGIN_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "h:\qtwin\include" /I "..\..\..\..\source\gameengine\Expressions" /I "..\..\..\..\source\gameengine\GameLogic" /I "..\..\..\..\source\gameengine\Ketsji" /I "..\..\..\..\source\gameengine\Ketsji\KXNetwork" /I "..\..\..\..\source\gameengine\Network" /I "..\..\..\..\source\gameengine\Network\LoopBackNetwork" /I "..\..\..\..\source\gameengine\Rasterizer" /I "..\..\..\..\source\gameengine\Rasterizer\RAS_OpenGLRasterizer" /I "..\..\..\..\source\gameengine\SceneGraph" /I "..\..\..\..\source\sumo\Fuzzics\include" /I "..\..\..\..\source\sumo\include" /I "..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\..\lib\windows\python\include\python2.0" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GPPLUGIN_EXPORTS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x413 /d "NDEBUG"
# ADD RSC /l 0x413 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib imm32.lib winmm.lib wsock32.lib h:\qtwin\lib\qnp.lib h:\qtwin\lib\qt.lib opengl32.lib glu32.lib python20.lib /nologo /dll /machine:I386 /out:"..\..\..\..\..\obj\windows\npWebGP.dll" /libpath:"..\..\..\..\..\lib\windows\python\lib\\"

!ELSEIF  "$(CFG)" == "gpplugin - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\..\obj\windows\gameengine\gameplayer\qt\gpplugin\debug"
# PROP Intermediate_Dir "..\..\..\..\..\obj\windows\gameengine\gameplayer\qt\gpplugin\debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GPPLUGIN_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /GX /Zi /Od /I "h:\qtwin\include" /I "..\..\..\..\source\gameengine\Expressions" /I "..\..\..\..\source\gameengine\GameLogic" /I "..\..\..\..\source\gameengine\Ketsji" /I "..\..\..\..\source\gameengine\Ketsji\KXNetwork" /I "..\..\..\..\source\gameengine\Network" /I "..\..\..\..\source\gameengine\Network\LoopBackNetwork" /I "..\..\..\..\source\gameengine\Rasterizer" /I "..\..\..\..\source\gameengine\Rasterizer\RAS_OpenGLRasterizer" /I "..\..\..\..\source\gameengine\SceneGraph" /I "..\..\..\..\source\sumo\Fuzzics\include" /I "..\..\..\..\source\sumo\include" /I "..\..\..\..\..\lib\windows\moto\include" /I "..\..\..\..\source\kernel\gen_system" /I "..\..\..\..\..\lib\windows\python\include\python1.5" /D "WIN32" /D "PLUGIN" /D "_DEBUG"
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x413 /d "_DEBUG"
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib imm32.lib winmm.lib wsock32.lib h:\qtwin\lib\qnp.lib h:\qtwin\lib\qt.lib opengl32.lib glu32.lib python15_d.lib /nologo /subsystem:windows /dll /debug /machine:I386 /def:"..\..\..\..\source\gameengine\GamePlayer\Qt\GP.def" /out:"..\..\..\..\..\obj\windows\debug\npWebGP.dll" /libpath:"..\..\..\..\..\lib\windows\python\lib"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "gpplugin - Win32 Release"
# Name "gpplugin - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Qt\GP.cpp
DEP_CPP_GP_CP=\
	"..\..\..\..\..\qtwin\include\qapplication.h"\
	"..\..\..\..\..\qtwin\include\qarray.h"\
	"..\..\..\..\..\qtwin\include\qasciidict.h"\
	"..\..\..\..\..\qtwin\include\qbrush.h"\
	"..\..\..\..\..\qtwin\include\qcollection.h"\
	"..\..\..\..\..\qtwin\include\qcolor.h"\
	"..\..\..\..\..\qtwin\include\qconfig-large.h"\
	"..\..\..\..\..\qtwin\include\qconfig-medium.h"\
	"..\..\..\..\..\qtwin\include\qconfig-minimal.h"\
	"..\..\..\..\..\qtwin\include\qconfig-small.h"\
	"..\..\..\..\..\qtwin\include\qconfig.h"\
	"..\..\..\..\..\qtwin\include\qcstring.h"\
	"..\..\..\..\..\qtwin\include\qcursor.h"\
	"..\..\..\..\..\qtwin\include\qdatastream.h"\
	"..\..\..\..\..\qtwin\include\qdialog.h"\
	"..\..\..\..\..\qtwin\include\qevent.h"\
	"..\..\..\..\..\qtwin\include\qfeatures.h"\
	"..\..\..\..\..\qtwin\include\qfont.h"\
	"..\..\..\..\..\qtwin\include\qfontinfo.h"\
	"..\..\..\..\..\qtwin\include\qfontmetrics.h"\
	"..\..\..\..\..\qtwin\include\qframe.h"\
	"..\..\..\..\..\qtwin\include\qgarray.h"\
	"..\..\..\..\..\qtwin\include\qgdict.h"\
	"..\..\..\..\..\qtwin\include\qgl.h"\
	"..\..\..\..\..\qtwin\include\qglist.h"\
	"..\..\..\..\..\qtwin\include\qglobal.h"\
	"..\..\..\..\..\qtwin\include\qiconset.h"\
	"..\..\..\..\..\qtwin\include\qimage.h"\
	"..\..\..\..\..\qtwin\include\qintdict.h"\
	"..\..\..\..\..\qtwin\include\qiodevice.h"\
	"..\..\..\..\..\qtwin\include\qlist.h"\
	"..\..\..\..\..\qtwin\include\qmenudata.h"\
	"..\..\..\..\..\qtwin\include\qmessagebox.h"\
	"..\..\..\..\..\qtwin\include\qmime.h"\
	"..\..\..\..\..\qtwin\include\qnamespace.h"\
	"..\..\..\..\..\qtwin\include\qnp.h"\
	"..\..\..\..\..\qtwin\include\qobject.h"\
	"..\..\..\..\..\qtwin\include\qobjectdefs.h"\
	"..\..\..\..\..\qtwin\include\qpaintdevice.h"\
	"..\..\..\..\..\qtwin\include\qpainter.h"\
	"..\..\..\..\..\qtwin\include\qpalette.h"\
	"..\..\..\..\..\qtwin\include\qpen.h"\
	"..\..\..\..\..\qtwin\include\qpixmap.h"\
	"..\..\..\..\..\qtwin\include\qpngio.h"\
	"..\..\..\..\..\qtwin\include\qpoint.h"\
	"..\..\..\..\..\qtwin\include\qpointarray.h"\
	"..\..\..\..\..\qtwin\include\qpopupmenu.h"\
	"..\..\..\..\..\qtwin\include\qrect.h"\
	"..\..\..\..\..\qtwin\include\qregexp.h"\
	"..\..\..\..\..\qtwin\include\qregion.h"\
	"..\..\..\..\..\qtwin\include\qshared.h"\
	"..\..\..\..\..\qtwin\include\qsignal.h"\
	"..\..\..\..\..\qtwin\include\qsize.h"\
	"..\..\..\..\..\qtwin\include\qsizepolicy.h"\
	"..\..\..\..\..\qtwin\include\qstring.h"\
	"..\..\..\..\..\qtwin\include\qstringlist.h"\
	"..\..\..\..\..\qtwin\include\qstrlist.h"\
	"..\..\..\..\..\qtwin\include\qstyle.h"\
	"..\..\..\..\..\qtwin\include\qt_windows.h"\
	"..\..\..\..\..\qtwin\include\qtranslator.h"\
	"..\..\..\..\..\qtwin\include\qvaluelist.h"\
	"..\..\..\..\..\qtwin\include\qwidget.h"\
	"..\..\..\..\..\qtwin\include\qwindowdefs.h"\
	"..\..\..\..\..\qtwin\include\qwindowdefs_win.h"\
	"..\..\..\..\..\qtwin\include\qwmatrix.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\abstract.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\bufferobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\ceval.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\classobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\cobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\complexobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\config.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\dictobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\fileobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\floatobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\funcobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\import.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\intobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\intrcheck.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\listobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\longobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\methodobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\modsupport.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\moduleobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\mymalloc.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\myproto.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\object.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\objimpl.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\patchlevel.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pydebug.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pyerrors.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pyfpe.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pystate.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\Python.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pythonrun.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\rangeobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\sliceobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\stringobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\sysmodule.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\traceback.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\tupleobject.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_IInputDevice.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_ISystem.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\GP_Init.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\QtInputDevice.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\QtKeyboardDevice.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\QtOpenGLWidget.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\QtRenderTools.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\QtSystem.h"\
	"..\..\..\..\source\gameengine\Network\LoopBackNetwork\NG_LoopBackNetworkDeviceInterface.h"\
	"..\..\..\..\source\gameengine\Network\NG_NetworkDeviceInterface.h"\
	"..\..\..\..\source\gameengine\Network\NG_NetworkScene.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_ICanvas.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_IPolygonMaterial.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_IRasterizer.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_IRenderTools.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_MaterialBucket.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_TexVert.h"\
	"..\..\..\..\source\kernel\gen_system\GEN_HashedString.h"\
	"..\..\..\..\source\kernel\gen_system\GEN_Matrix4x4.h"\
	"..\..\..\..\source\kernel\gen_system\GEN_StdString.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_Map.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_MinMax.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_Optimize.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_random.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_Stream.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_assert.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Matrix3x3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Matrix3x3.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point2.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point2.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point3.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Quaternion.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Quaternion.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Scalar.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Transform.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Tuple2.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Tuple3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Tuple4.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector2.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector2.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector3.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector4.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector4.inl"\
	"..\..\..\..\..\lib\windows\moto\include\NM_Scalar.h"\
	
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Qt\GP_Init.cpp
DEP_CPP_GP_IN=\
	"..\..\..\..\..\lib\windows\python\include\python1.5\abstract.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\bufferobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\ceval.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\classobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\cobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\complexobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\config.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\dictobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\fileobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\floatobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\funcobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\import.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\intobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\intrcheck.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\listobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\longobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\methodobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\modsupport.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\moduleobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\mymalloc.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\myproto.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\object.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\objimpl.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\patchlevel.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pydebug.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pyerrors.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pyfpe.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pystate.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\Python.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pythonrun.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\rangeobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\sliceobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\stringobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\sysmodule.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\traceback.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\tupleobject.h"\
	"..\..\..\..\source\gameengine\Expressions\BoolValue.h"\
	"..\..\..\..\source\gameengine\Expressions\ListValue.h"\
	"..\..\..\..\source\gameengine\Expressions\PyObjectPlus.h"\
	"..\..\..\..\source\gameengine\Expressions\Value.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_AlwaysEventManager.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_AlwaysSensor.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_ANDController.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_EventManager.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_IActuator.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_IController.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_IInputDevice.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_ILogicBrick.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_IObject.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_ISensor.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_ISystem.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_KeyboardManager.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_KeyboardSensor.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_LogicManager.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_PythonController.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\DebugActuator.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\QtExampleEngine.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\SamplePolygonMaterial.h"\
	"..\..\..\..\source\gameengine\Ketsji\KX_GameObject.h"\
	"..\..\..\..\source\gameengine\Ketsji\KX_ObjectActuator.h"\
	"..\..\..\..\source\gameengine\Ketsji\KX_PythonInit.h"\
	"..\..\..\..\source\gameengine\Ketsji\KX_SoundActuator.h"\
	"..\..\..\..\source\gameengine\Ketsji\KXNetwork\KX_NetworkEventManager.h"\
	"..\..\..\..\source\gameengine\Network\LoopBackNetwork\NG_LoopBackNetworkDeviceInterface.h"\
	"..\..\..\..\source\gameengine\Network\NG_NetworkDeviceInterface.h"\
	"..\..\..\..\source\gameengine\Network\NG_NetworkScene.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_BucketManager.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_ICanvas.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_IPolygonMaterial.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_IRasterizer.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_MaterialBucket.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_MeshObject.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_OpenGLRasterizer\RAS_OpenGLRasterizer.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_Polygon.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_TexVert.h"\
	"..\..\..\..\source\gameengine\SceneGraph\SG_IObject.h"\
	"..\..\..\..\source\gameengine\SceneGraph\SG_Node.h"\
	"..\..\..\..\source\gameengine\SceneGraph\SG_Spatial.h"\
	"..\..\..\..\source\kernel\gen_system\GEN_HashedPtr.h"\
	"..\..\..\..\source\kernel\gen_system\GEN_HashedString.h"\
	"..\..\..\..\source\kernel\gen_system\GEN_Matrix4x4.h"\
	"..\..\..\..\source\kernel\gen_system\GEN_StdString.h"\
	"..\..\..\..\source\sumo\include\solid.h"\
	"..\..\..\..\source\sumo\include\solid_types.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_Map.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_MinMax.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_Optimize.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_random.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_Stream.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_assert.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Matrix3x3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Matrix3x3.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point2.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point2.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point3.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Quaternion.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Quaternion.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Scalar.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Transform.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Tuple2.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Tuple3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Tuple4.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector2.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector2.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector3.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector4.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector4.inl"\
	"..\..\..\..\..\lib\windows\moto\include\NM_Scalar.h"\
	
NODEP_CPP_GP_IN=\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\GPC_OpenALWaveCache.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\OpenALScene.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\SND_SoundObject.h"\
	
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Qt\QtExampleEngine.cpp
DEP_CPP_QTEXA=\
	"..\..\..\..\..\lib\windows\python\include\python1.5\abstract.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\bufferobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\ceval.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\classobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\cobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\complexobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\config.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\dictobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\fileobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\floatobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\funcobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\import.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\intobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\intrcheck.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\listobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\longobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\methodobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\modsupport.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\moduleobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\mymalloc.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\myproto.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\object.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\objimpl.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\patchlevel.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pydebug.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pyerrors.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pyfpe.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pystate.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\Python.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pythonrun.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\rangeobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\sliceobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\stringobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\sysmodule.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\traceback.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\tupleobject.h"\
	"..\..\..\..\source\gameengine\Expressions\BoolValue.h"\
	"..\..\..\..\source\gameengine\Expressions\IntValue.h"\
	"..\..\..\..\source\gameengine\Expressions\ListValue.h"\
	"..\..\..\..\source\gameengine\Expressions\PyObjectPlus.h"\
	"..\..\..\..\source\gameengine\Expressions\Value.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_IActuator.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_IInputDevice.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_ILogicBrick.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_IObject.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_ISystem.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_LogicManager.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\QtExampleEngine.h"\
	"..\..\..\..\source\gameengine\Ketsji\KX_Camera.h"\
	"..\..\..\..\source\gameengine\Ketsji\KX_GameObject.h"\
	"..\..\..\..\source\gameengine\Ketsji\KX_ObjectActuator.h"\
	"..\..\..\..\source\gameengine\Network\NG_NetworkScene.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_BucketManager.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_CameraData.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_IPolygonMaterial.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_IRasterizer.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_IRenderTools.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_MaterialBucket.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_MeshObject.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_Polygon.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_TexVert.h"\
	"..\..\..\..\source\gameengine\SceneGraph\SG_IObject.h"\
	"..\..\..\..\source\gameengine\SceneGraph\SG_Node.h"\
	"..\..\..\..\source\gameengine\SceneGraph\SG_Spatial.h"\
	"..\..\..\..\source\kernel\gen_system\GEN_HashedPtr.h"\
	"..\..\..\..\source\kernel\gen_system\GEN_HashedString.h"\
	"..\..\..\..\source\kernel\gen_system\GEN_Matrix4x4.h"\
	"..\..\..\..\source\kernel\gen_system\GEN_StdString.h"\
	"..\..\..\..\source\sumo\Fuzzics\include\SM_Scene.h"\
	"..\..\..\..\source\sumo\include\solid.h"\
	"..\..\..\..\source\sumo\include\solid_types.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_Map.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_MinMax.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_Optimize.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_random.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_Stream.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_assert.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Matrix3x3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Matrix3x3.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point2.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point2.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point3.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Quaternion.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Quaternion.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Scalar.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Transform.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Tuple2.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Tuple3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Tuple4.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector2.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector2.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector3.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector4.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector4.inl"\
	"..\..\..\..\..\lib\windows\moto\include\NM_Scalar.h"\
	
NODEP_CPP_QTEXA=\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\OpenALScene.h"\
	
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Qt\QtKeyboardDevice.cpp
DEP_CPP_QTKEY=\
	"..\..\..\..\source\gameengine\GameLogic\SCA_IInputDevice.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\QtInputDevice.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\QtKeyboardDevice.h"\
	
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Qt\QtOpenGLWidget.cpp
DEP_CPP_QTOPE=\
	"..\..\..\..\..\qtwin\include\qapplication.h"\
	"..\..\..\..\..\qtwin\include\qarray.h"\
	"..\..\..\..\..\qtwin\include\qasciidict.h"\
	"..\..\..\..\..\qtwin\include\qbrush.h"\
	"..\..\..\..\..\qtwin\include\qcollection.h"\
	"..\..\..\..\..\qtwin\include\qcolor.h"\
	"..\..\..\..\..\qtwin\include\qconfig-large.h"\
	"..\..\..\..\..\qtwin\include\qconfig-medium.h"\
	"..\..\..\..\..\qtwin\include\qconfig-minimal.h"\
	"..\..\..\..\..\qtwin\include\qconfig-small.h"\
	"..\..\..\..\..\qtwin\include\qconfig.h"\
	"..\..\..\..\..\qtwin\include\qcstring.h"\
	"..\..\..\..\..\qtwin\include\qcursor.h"\
	"..\..\..\..\..\qtwin\include\qdatastream.h"\
	"..\..\..\..\..\qtwin\include\qevent.h"\
	"..\..\..\..\..\qtwin\include\qfeatures.h"\
	"..\..\..\..\..\qtwin\include\qfont.h"\
	"..\..\..\..\..\qtwin\include\qfontinfo.h"\
	"..\..\..\..\..\qtwin\include\qfontmetrics.h"\
	"..\..\..\..\..\qtwin\include\qframe.h"\
	"..\..\..\..\..\qtwin\include\qgarray.h"\
	"..\..\..\..\..\qtwin\include\qgdict.h"\
	"..\..\..\..\..\qtwin\include\qgl.h"\
	"..\..\..\..\..\qtwin\include\qglist.h"\
	"..\..\..\..\..\qtwin\include\qglobal.h"\
	"..\..\..\..\..\qtwin\include\qiconset.h"\
	"..\..\..\..\..\qtwin\include\qintdict.h"\
	"..\..\..\..\..\qtwin\include\qiodevice.h"\
	"..\..\..\..\..\qtwin\include\qlist.h"\
	"..\..\..\..\..\qtwin\include\qmenudata.h"\
	"..\..\..\..\..\qtwin\include\qmime.h"\
	"..\..\..\..\..\qtwin\include\qnamespace.h"\
	"..\..\..\..\..\qtwin\include\qobject.h"\
	"..\..\..\..\..\qtwin\include\qobjectdefs.h"\
	"..\..\..\..\..\qtwin\include\qpaintdevice.h"\
	"..\..\..\..\..\qtwin\include\qpalette.h"\
	"..\..\..\..\..\qtwin\include\qpixmap.h"\
	"..\..\..\..\..\qtwin\include\qpoint.h"\
	"..\..\..\..\..\qtwin\include\qpopupmenu.h"\
	"..\..\..\..\..\qtwin\include\qrect.h"\
	"..\..\..\..\..\qtwin\include\qregexp.h"\
	"..\..\..\..\..\qtwin\include\qregion.h"\
	"..\..\..\..\..\qtwin\include\qshared.h"\
	"..\..\..\..\..\qtwin\include\qsignal.h"\
	"..\..\..\..\..\qtwin\include\qsize.h"\
	"..\..\..\..\..\qtwin\include\qsizepolicy.h"\
	"..\..\..\..\..\qtwin\include\qstring.h"\
	"..\..\..\..\..\qtwin\include\qstringlist.h"\
	"..\..\..\..\..\qtwin\include\qstyle.h"\
	"..\..\..\..\..\qtwin\include\qt_windows.h"\
	"..\..\..\..\..\qtwin\include\qtranslator.h"\
	"..\..\..\..\..\qtwin\include\qvaluelist.h"\
	"..\..\..\..\..\qtwin\include\qwidget.h"\
	"..\..\..\..\..\qtwin\include\qwindowdefs.h"\
	"..\..\..\..\..\qtwin\include\qwindowdefs_win.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\abstract.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\bufferobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\ceval.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\classobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\cobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\complexobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\config.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\dictobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\fileobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\floatobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\funcobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\import.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\intobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\intrcheck.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\listobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\longobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\methodobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\modsupport.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\moduleobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\mymalloc.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\myproto.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\object.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\objimpl.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\patchlevel.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pydebug.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pyerrors.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pyfpe.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pystate.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\Python.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pythonrun.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\rangeobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\sliceobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\stringobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\sysmodule.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\traceback.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\tupleobject.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_IInputDevice.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_ISystem.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\GP_Init.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\QtInputDevice.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\QtKeyboardDevice.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\QtOpenGLWidget.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\QtRenderTools.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\QtSystem.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_ICanvas.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_IPolygonMaterial.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_IRasterizer.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_IRenderTools.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_MaterialBucket.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_TexVert.h"\
	"..\..\..\..\source\kernel\gen_system\GEN_HashedString.h"\
	"..\..\..\..\source\kernel\gen_system\GEN_Matrix4x4.h"\
	"..\..\..\..\source\kernel\gen_system\GEN_StdString.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_Map.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_MinMax.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_Optimize.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_random.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_Stream.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_assert.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Matrix3x3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Matrix3x3.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point2.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point2.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point3.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Quaternion.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Quaternion.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Scalar.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Transform.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Tuple2.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Tuple3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Tuple4.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector2.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector2.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector3.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector4.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector4.inl"\
	"..\..\..\..\..\lib\windows\moto\include\NM_Scalar.h"\
	
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Qt\QtSystem.cpp
DEP_CPP_QTSYS=\
	"..\..\..\..\..\qtwin\include\qapplication.h"\
	"..\..\..\..\..\qtwin\include\qarray.h"\
	"..\..\..\..\..\qtwin\include\qasciidict.h"\
	"..\..\..\..\..\qtwin\include\qbrush.h"\
	"..\..\..\..\..\qtwin\include\qcollection.h"\
	"..\..\..\..\..\qtwin\include\qcolor.h"\
	"..\..\..\..\..\qtwin\include\qconfig-large.h"\
	"..\..\..\..\..\qtwin\include\qconfig-medium.h"\
	"..\..\..\..\..\qtwin\include\qconfig-minimal.h"\
	"..\..\..\..\..\qtwin\include\qconfig-small.h"\
	"..\..\..\..\..\qtwin\include\qconfig.h"\
	"..\..\..\..\..\qtwin\include\qcstring.h"\
	"..\..\..\..\..\qtwin\include\qcursor.h"\
	"..\..\..\..\..\qtwin\include\qdatastream.h"\
	"..\..\..\..\..\qtwin\include\qevent.h"\
	"..\..\..\..\..\qtwin\include\qfeatures.h"\
	"..\..\..\..\..\qtwin\include\qfont.h"\
	"..\..\..\..\..\qtwin\include\qfontinfo.h"\
	"..\..\..\..\..\qtwin\include\qfontmetrics.h"\
	"..\..\..\..\..\qtwin\include\qframe.h"\
	"..\..\..\..\..\qtwin\include\qgarray.h"\
	"..\..\..\..\..\qtwin\include\qgdict.h"\
	"..\..\..\..\..\qtwin\include\qgl.h"\
	"..\..\..\..\..\qtwin\include\qglist.h"\
	"..\..\..\..\..\qtwin\include\qglobal.h"\
	"..\..\..\..\..\qtwin\include\qiconset.h"\
	"..\..\..\..\..\qtwin\include\qintdict.h"\
	"..\..\..\..\..\qtwin\include\qiodevice.h"\
	"..\..\..\..\..\qtwin\include\qlist.h"\
	"..\..\..\..\..\qtwin\include\qmenudata.h"\
	"..\..\..\..\..\qtwin\include\qmime.h"\
	"..\..\..\..\..\qtwin\include\qnamespace.h"\
	"..\..\..\..\..\qtwin\include\qobject.h"\
	"..\..\..\..\..\qtwin\include\qobjectdefs.h"\
	"..\..\..\..\..\qtwin\include\qpaintdevice.h"\
	"..\..\..\..\..\qtwin\include\qpalette.h"\
	"..\..\..\..\..\qtwin\include\qpixmap.h"\
	"..\..\..\..\..\qtwin\include\qpoint.h"\
	"..\..\..\..\..\qtwin\include\qpopupmenu.h"\
	"..\..\..\..\..\qtwin\include\qrect.h"\
	"..\..\..\..\..\qtwin\include\qregexp.h"\
	"..\..\..\..\..\qtwin\include\qregion.h"\
	"..\..\..\..\..\qtwin\include\qshared.h"\
	"..\..\..\..\..\qtwin\include\qsignal.h"\
	"..\..\..\..\..\qtwin\include\qsize.h"\
	"..\..\..\..\..\qtwin\include\qsizepolicy.h"\
	"..\..\..\..\..\qtwin\include\qstring.h"\
	"..\..\..\..\..\qtwin\include\qstringlist.h"\
	"..\..\..\..\..\qtwin\include\qstyle.h"\
	"..\..\..\..\..\qtwin\include\qt_windows.h"\
	"..\..\..\..\..\qtwin\include\qtranslator.h"\
	"..\..\..\..\..\qtwin\include\qvaluelist.h"\
	"..\..\..\..\..\qtwin\include\qwidget.h"\
	"..\..\..\..\..\qtwin\include\qwindowdefs.h"\
	"..\..\..\..\..\qtwin\include\qwindowdefs_win.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\abstract.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\bufferobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\ceval.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\classobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\cobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\complexobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\config.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\dictobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\fileobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\floatobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\funcobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\import.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\intobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\intrcheck.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\listobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\longobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\methodobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\modsupport.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\moduleobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\mymalloc.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\myproto.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\object.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\objimpl.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\patchlevel.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pydebug.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pyerrors.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pyfpe.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pystate.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\Python.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\pythonrun.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\rangeobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\sliceobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\stringobject.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\sysmodule.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\traceback.h"\
	"..\..\..\..\..\lib\windows\python\include\python1.5\tupleobject.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_IInputDevice.h"\
	"..\..\..\..\source\gameengine\GameLogic\SCA_ISystem.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\GP_Init.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\QtInputDevice.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\QtKeyboardDevice.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\QtOpenGLWidget.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\QtRenderTools.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\QtSystem.h"\
	"..\..\..\..\source\gameengine\Network\LoopBackNetwork\NG_LoopBackNetworkDeviceInterface.h"\
	"..\..\..\..\source\gameengine\Network\NG_NetworkDeviceInterface.h"\
	"..\..\..\..\source\gameengine\Network\NG_NetworkScene.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_ICanvas.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_IPolygonMaterial.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_IRasterizer.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_IRenderTools.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_MaterialBucket.h"\
	"..\..\..\..\source\gameengine\Rasterizer\RAS_TexVert.h"\
	"..\..\..\..\source\kernel\gen_system\GEN_HashedString.h"\
	"..\..\..\..\source\kernel\gen_system\GEN_Matrix4x4.h"\
	"..\..\..\..\source\kernel\gen_system\GEN_StdString.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_Map.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_MinMax.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_Optimize.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_random.h"\
	"..\..\..\..\..\lib\windows\moto\include\GEN_Stream.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_assert.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Matrix3x3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Matrix3x3.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point2.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point2.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Point3.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Quaternion.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Quaternion.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Scalar.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Transform.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Tuple2.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Tuple3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Tuple4.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector2.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector2.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector3.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector3.inl"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector4.h"\
	"..\..\..\..\..\lib\windows\moto\include\MT_Vector4.inl"\
	"..\..\..\..\..\lib\windows\moto\include\NM_Scalar.h"\
	
NODEP_CPP_QTSYS=\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\BKE_bad_level_calls.h"\
	"..\..\..\..\source\gameengine\GamePlayer\Qt\BLO_readfile.h"\
	
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Qt\DebugActuator.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Qt\GP_Init.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Qt\QtExampleEngine.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Qt\QtInputDevice.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Qt\QtKeyboardDevice.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Qt\QtOpenGLWidget.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Qt\QtRenderTools.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Qt\QtSystem.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Qt\resource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Qt\SamplePolygonMaterial.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\..\..\..\source\gameengine\GamePlayer\Qt\GP.rc
# End Source File
# End Group
# End Target
# End Project
