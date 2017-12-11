@echo off
REM This batch file does an out-of-source CMake build in ../build_windows
REM This is for users who like to configure & build Blender with a single command.
setlocal EnableDelayedExpansion
setlocal ENABLEEXTENSIONS
set BLENDER_DIR=%~dp0
set BLENDER_DIR_NOSPACES=%BLENDER_DIR: =%
for %%X in (svn.exe) do (set HAS_SVN=%%~$PATH:X)
if not "%BLENDER_DIR%"=="%BLENDER_DIR_NOSPACES%" (
	echo There are spaces detected in the build path "%BLENDER_DIR%", this is currently not supported, exiting....
	goto EOF
)
set BUILD_DIR=%BLENDER_DIR%..\build_windows
set BUILD_TYPE=Release
rem reset all variables so they do not get accidentally get carried over from previous builds
set BUILD_DIR_OVERRRIDE=
set BUILD_CMAKE_ARGS=
set BUILD_ARCH=
set BUILD_VS_VER=
set BUILD_VS_YEAR=
set BUILD_VS_LIBDIRPOST=
set BUILD_VS_LIBDIR=
set BUILD_VS_SVNDIR=
set BUILD_NGE=
set KEY_NAME=
set MSBUILD_PLATFORM=
set MUST_CLEAN=
set NOBUILD=
set TARGET=
set WINDOWS_ARCH=
set TESTS_CMAKE_ARGS=
:argv_loop
if NOT "%1" == "" (

	REM Help Message
	if "%1" == "help" (
		goto HELP
	)

	REM Build Types
	if "%1" == "debug" (
		set BUILD_TYPE=Debug
	REM Build Configurations
	) else if "%1" == "noge" (
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -DWITH_GAMEENGINE=OFF -DWITH_PLAYER=OFF
		set BUILD_NGE=_noge
	) else if "%1" == "builddir" (
		set BUILD_DIR_OVERRRIDE="%BLENDER_DIR%..\%2"
		shift /1
	) else if "%1" == "with_tests" (
		set TESTS_CMAKE_ARGS=-DWITH_GTESTS=On
	) else if "%1" == "full" (
		set TARGET=Full
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% ^
		    -C"%BLENDER_DIR%\build_files\cmake\config\blender_full.cmake"
	) else if "%1" == "lite" (
		set TARGET=Lite
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% ^
		    -C"%BLENDER_DIR%\build_files\cmake\config\blender_lite.cmake"
	) else if "%1" == "cycles" (
		set TARGET=Cycles
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% ^
		    -C"%BLENDER_DIR%\build_files\cmake\config\cycles_standalone.cmake"
	) else if "%1" == "headless" (
		set TARGET=Headless
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% ^
		    -C"%BLENDER_DIR%\build_files\cmake\config\blender_headless.cmake"
	) else if "%1" == "bpy" (
		set TARGET=Bpy
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% ^
		    -C"%BLENDER_DIR%\build_files\cmake\config\bpy_module.cmake"
	) else if "%1" == "release" (
		set TARGET=Release
	) else if "%1" == "x86" (
		set BUILD_ARCH=x86
	) else if "%1" == "x64" (
		set BUILD_ARCH=x64
	) else if "%1" == "2017" (
		set BUILD_VS_VER=15
		set BUILD_VS_YEAR=2017
		set BUILD_VS_LIBDIRPOST=vc14
	) else if "%1" == "2015" (
		set BUILD_VS_VER=14
		set BUILD_VS_YEAR=2015
		set BUILD_VS_LIBDIRPOST=vc14
	) else if "%1" == "2013" (
		set BUILD_VS_VER=12
		set BUILD_VS_YEAR=2013
		set BUILD_VS_LIBDIRPOST=vc12
	) else if "%1" == "packagename" (
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -DCPACK_OVERRIDE_PACKAGENAME="%2"
		shift /1
	) else if "%1" == "nobuild" (
		set NOBUILD=1
	) else if "%1" == "showhash" (
		for /f "delims=" %%i in ('git rev-parse HEAD') do echo Branch_hash=%%i
		cd release/datafiles/locale
		for /f "delims=" %%i in ('git rev-parse HEAD') do echo Locale_hash=%%i
		cd %~dp0
		cd release/scripts/addons
		for /f "delims=" %%i in ('git rev-parse HEAD') do echo Addons_Hash=%%i
		cd %~dp0
		goto EOF
	REM Non-Build Commands
	) else if "%1" == "update" (
		svn up ../lib/*
		git pull --rebase
		git submodule foreach git pull --rebase origin master
		goto EOF
	) else if "%1" == "clean" (
		set MUST_CLEAN=1
	) else (
		echo Command "%1" unknown, aborting!
		goto EOF
	)

	shift /1
	goto argv_loop
)
if "%BUILD_ARCH%"=="" (
	if "%PROCESSOR_ARCHITECTURE%" == "AMD64" (
		set WINDOWS_ARCH= Win64
		set BUILD_ARCH=x64
	) else if "%PROCESSOR_ARCHITEW6432%" == "AMD64" (
		set WINDOWS_ARCH= Win64
		set BUILD_ARCH=x64
	) else (
		set WINDOWS_ARCH=
		set BUILD_ARCH=x86
	)
) else if "%BUILD_ARCH%"=="x64" (
	set WINDOWS_ARCH= Win64
) else if "%BUILD_ARCH%"=="x86" (
	set WINDOWS_ARCH=
)

if "%BUILD_VS_VER%"=="" (
	set BUILD_VS_VER=12
	set BUILD_VS_YEAR=2013
	set BUILD_VS_LIBDIRPOST=vc12
)

if "%BUILD_ARCH%"=="x64" (
	set MSBUILD_PLATFORM=x64
) else if "%BUILD_ARCH%"=="x86" (
	set MSBUILD_PLATFORM=win32
)


if "%target%"=="Release" (
	rem for vc12 check for both cuda 7.5 and 8
	if "%CUDA_PATH%"=="" (
		echo Cuda Not found, aborting!
		goto EOF
	)
	set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% ^
	-C"%BLENDER_DIR%\build_files\cmake\config\blender_release.cmake"
)

:DetectMSVC
REM Detect MSVC Installation for 2013-2015
if DEFINED VisualStudioVersion goto msvc_detect_finally
set VALUE_NAME=ProductDir
REM Check 64 bits
set KEY_NAME="HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\%BUILD_VS_VER%.0\Setup\VC"
for /F "usebackq skip=2 tokens=1-2*" %%A IN (`REG QUERY %KEY_NAME% /v %VALUE_NAME% 2^>nul`) DO set MSVC_VC_DIR=%%C
if DEFINED MSVC_VC_DIR goto msvc_detect_finally
REM Check 32 bits
set KEY_NAME="HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\VisualStudio\%BUILD_VS_VER%.0\Setup\VC"
for /F "usebackq skip=2 tokens=1-2*" %%A IN (`REG QUERY %KEY_NAME% /v %VALUE_NAME% 2^>nul`) DO set MSVC_VC_DIR=%%C
if DEFINED MSVC_VC_DIR goto msvc_detect_finally
:msvc_detect_finally
if DEFINED MSVC_VC_DIR call "%MSVC_VC_DIR%\vcvarsall.bat"
if DEFINED MSVC_VC_DIR goto sanity_checks

rem MSVC Build environment 2017 and up.
for /F "usebackq skip=2 tokens=1-2*" %%A IN (`REG QUERY "HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\SXS\VS7" /v %BUILD_VS_VER%.0 2^>nul`) DO set MSVC_VS_DIR=%%C
if DEFINED MSVC_VS_DIR goto msvc_detect_finally_2017
REM Check 32 bits
for /F "usebackq skip=2 tokens=1-2*" %%A IN (`REG QUERY "HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\VisualStudio\sxs\vs7" /v %BUILD_VS_VER%.0 2^>nul`) DO set MSVC_VS_DIR=%%C
if DEFINED MSVC_VS_DIR goto msvc_detect_finally_2017
:msvc_detect_finally_2017
if DEFINED MSVC_VS_DIR call "%MSVC_VS_DIR%\Common7\Tools\VsDevCmd.bat"

:sanity_checks
REM Sanity Checks
where /Q msbuild
if %ERRORLEVEL% NEQ 0 (
	if "%BUILD_VS_VER%"=="12" (
		rem vs12 not found, try vs14
		echo Visual Studio 2013 not found, trying Visual Studio 2015.
		set BUILD_VS_VER=14
		set BUILD_VS_YEAR=2015
		set BUILD_VS_LIBDIRPOST=vc14
		goto DetectMSVC
	) else (
		echo Error: "MSBuild" command not in the PATH.
		echo You must have MSVC installed and run this from the "Developer Command Prompt"
		echo ^(available from Visual Studio's Start menu entry^), aborting!
		goto EOF
	)
)


set BUILD_DIR=%BUILD_DIR%_%TARGET%%BUILD_NGE%_%BUILD_ARCH%_vc%BUILD_VS_VER%_%BUILD_TYPE%
if NOT "%BUILD_DIR_OVERRRIDE%"=="" (
	set BUILD_DIR=%BUILD_DIR_OVERRRIDE%
)

where /Q cmake
if %ERRORLEVEL% NEQ 0 (
	echo Error: "CMake" command not in the PATH.
	echo You must have CMake installed and added to your PATH, aborting!
	goto EOF
)

if "%BUILD_ARCH%"=="x64" (
	set BUILD_VS_SVNDIR=win64_%BUILD_VS_LIBDIRPOST%
) else if "%BUILD_ARCH%"=="x86" (
	set BUILD_VS_SVNDIR=windows_%BUILD_VS_LIBDIRPOST%
)
set BUILD_VS_LIBDIR="%BLENDER_DIR%..\lib\%BUILD_VS_SVNDIR%"

if NOT EXIST %BUILD_VS_LIBDIR% (
	rem libs not found, but svn is on the system
	if not "%HAS_SVN%"=="" (
		echo.
		echo The required external libraries in %BUILD_VS_LIBDIR% are missing
		echo.
		set /p GetLibs= "Would you like to download them? (y/n)"
		if /I "!GetLibs!"=="Y" (
			echo.
			echo Downloading %BUILD_VS_SVNDIR% libraries, please wait.
			echo.
			svn checkout https://svn.blender.org/svnroot/bf-blender/trunk/lib/%BUILD_VS_SVNDIR% %BUILD_VS_LIBDIR%
		)
	)
)

if NOT EXIST %BUILD_VS_LIBDIR% (
	echo Error: Path to libraries not found "%BUILD_VS_LIBDIR%"
	echo This is needed for building, aborting!
	goto EOF
)

if "%TARGET%"=="" (
	echo Error: Convenience target not set
	echo This is required for building, aborting!
	echo .
	goto HELP
)

set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -G "Visual Studio %BUILD_VS_VER% %BUILD_VS_YEAR%%WINDOWS_ARCH%" %TESTS_CMAKE_ARGS%
if NOT EXIST %BUILD_DIR%\nul (
	mkdir %BUILD_DIR%
)
if "%MUST_CLEAN%"=="1" (
	echo Cleaning %BUILD_DIR%
	msbuild ^
		%BUILD_DIR%\Blender.sln ^
		/target:clean ^
		/property:Configuration=%BUILD_TYPE% ^
		/verbosity:minimal ^
		/p:platform=%MSBUILD_PLATFORM%

	if %ERRORLEVEL% NEQ 0 (
		echo Cleaned "%BUILD_DIR%"
	)
	goto EOF
)
REM Only configure on first run or when called with nobuild
if NOT EXIST %BUILD_DIR%\Blender.sln set MUST_CONFIGURE=1
if "%NOBUILD%"=="1" set MUST_CONFIGURE=1

if "%MUST_CONFIGURE%"=="1" (
	cmake ^
		%BUILD_CMAKE_ARGS% ^
		-H%BLENDER_DIR% ^
		-B%BUILD_DIR% ^
		%BUILD_CMAKE_ARGS%

	if %ERRORLEVEL% NEQ 0 (
		echo "Configuration Failed"
		goto EOF
	)
)
if DEFINED MSVC_VC_DIR echo call "%MSVC_VC_DIR%\vcvarsall.bat" > %BUILD_DIR%\rebuild.cmd
if DEFINED MSVC_VS_DIR echo call "%MSVC_VS_DIR%\Common7\Tools\VsDevCmd.bat" > %BUILD_DIR%\rebuild.cmd
echo cmake . >> %BUILD_DIR%\rebuild.cmd
echo msbuild ^
	%BUILD_DIR%\Blender.sln ^
	/target:build ^
	/property:Configuration=%BUILD_TYPE% ^
	/maxcpucount:2 ^
	/verbosity:minimal ^
	/p:platform=%MSBUILD_PLATFORM% ^
	/flp:Summary;Verbosity=minimal;LogFile=%BUILD_DIR%\Build.log >> %BUILD_DIR%\rebuild.cmd
echo msbuild ^
	%BUILD_DIR%\INSTALL.vcxproj ^
	/property:Configuration=%BUILD_TYPE% ^
	/verbosity:minimal ^
	/p:platform=%MSBUILD_PLATFORM% >> %BUILD_DIR%\rebuild.cmd

if "%NOBUILD%"=="1" goto EOF

msbuild ^
	%BUILD_DIR%\Blender.sln ^
	/target:build ^
	/property:Configuration=%BUILD_TYPE% ^
	/maxcpucount:2 ^
	/verbosity:minimal ^
	/p:platform=%MSBUILD_PLATFORM% ^
	/flp:Summary;Verbosity=minimal;LogFile=%BUILD_DIR%\Build.log

if %ERRORLEVEL% NEQ 0 (
	echo "Build Failed"
	goto EOF
)

msbuild ^
	%BUILD_DIR%\INSTALL.vcxproj ^
	/property:Configuration=%BUILD_TYPE% ^
	/verbosity:minimal ^
	/p:platform=%MSBUILD_PLATFORM%

echo.
echo At any point you can optionally modify your build configuration by editing:
echo "%BUILD_DIR%\CMakeCache.txt", then run "rebuild.cmd" in the build folder to build with the changes applied.
echo.
echo Blender successfully built, run from: "%BUILD_DIR%\bin\%BUILD_TYPE%\blender.exe"
echo.
goto EOF
:HELP
	echo.
	echo Convenience targets
	echo - release ^(identical to the official blender.org builds^)
	echo - full ^(same as release minus the cuda kernels^)
	echo - lite
	echo - headless
	echo - cycles
	echo - bpy
	echo.
	echo Utilities ^(not associated with building^)
	echo - clean ^(Target must be set^)
	echo - update
	echo - nobuild ^(only generate project files^)
	echo - showhash ^(Show git hashes of source tree^)
	echo.
	echo Configuration options
	echo - with_tests ^(enable building unit tests^)
	echo - noge ^(disable building game enginge and player^)
	echo - debug ^(Build an unoptimized debuggable build^)
	echo - packagename [newname] ^(override default cpack package name^)
	echo - buildir [newdir] ^(override default build folder^)
	echo - x86 ^(override host auto-detect and build 32 bit code^)
	echo - x64 ^(override host auto-detect and build 64 bit code^)
	echo - 2013 ^(build with visual studio 2013^)
	echo - 2015 ^(build with visual studio 2015^) [EXPERIMENTAL]
	echo - 2017 ^(build with visual studio 2017^) [EXPERIMENTAL]
	echo.

:EOF
