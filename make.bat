@echo off
REM This batch file does an out-of-source CMake build in ../build_windows
REM This is for users who like to configure & build Blender with a single command.

setlocal ENABLEEXTENSIONS
set BLENDER_DIR=%~dp0
set BUILD_DIR=%BLENDER_DIR%..\build_windows
set BUILD_TYPE=Release
set BUILD_CMAKE_ARGS=

:argv_loop
if NOT "%1" == "" (

	REM Help Message
	if "%1" == "help" (
		goto HELP
	)

	REM Build Types
	if "%1" == "debug" (
		set BUILD_DIR=%BUILD_DIR%_debug
		set BUILD_TYPE=Debug

	REM Build Configurations
	) else if "%1" == "full" (
		set TARGET_SET=1
		set BUILD_DIR=%BUILD_DIR%_full
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% ^
		    -C"%BLENDER_DIR%\build_files\cmake\config\blender_full.cmake"
	) else if "%1" == "lite" (
		set TARGET_SET=1
		set BUILD_DIR=%BUILD_DIR%_lite
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% ^
		    -C"%BLENDER_DIR%\build_files\cmake\config\blender_lite.cmake"
	) else if "%1" == "cycles" (
		set TARGET_SET=1
		set BUILD_DIR=%BUILD_DIR%_cycles
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% ^
		    -C"%BLENDER_DIR%\build_files\cmake\config\cycles_standalone.cmake"
	) else if "%1" == "headless" (
		set TARGET_SET=1
		set BUILD_DIR=%BUILD_DIR%_headless
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% ^
		    -C"%BLENDER_DIR%\build_files\cmake\config\blender_headless.cmake"
	) else if "%1" == "bpy" (
		set TARGET_SET=1
		set BUILD_DIR=%BUILD_DIR%_bpy
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% ^
		    -C"%BLENDER_DIR%\build_files\cmake\config\bpy_module.cmake"
	) else if "%1" == "release" (
		set TARGET_SET=1
		if "%CUDA_PATH_V7_5%"=="" (
			echo Cuda 7.5 Not found, aborting!
			goto EOF
		)
		if "%CUDA_PATH_V8_0%"=="" (
			echo Cuda 8.0 Not found, aborting!
			goto EOF
		)
		set BUILD_DIR=%BUILD_DIR%_Release
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% ^
		    -C"%BLENDER_DIR%\build_files\cmake\config\blender_release.cmake" -DCUDA_NVCC_EXECUTABLE:FILEPATH=%CUDA_PATH_V7_5%/bin/nvcc.exe -DCUDA_NVCC8_EXECUTABLE:FILEPATH=%CUDA_PATH_V8_0%/bin/nvcc.exe  
	)	else if "%1" == "x86" (
		set BUILD_ARCH=x86
		set BUILD_DIR=%BUILD_DIR%_x86
	)	else if "%1" == "x64" (
		set BUILD_ARCH=x64
		set BUILD_DIR=%BUILD_DIR%_x64
	)	else if "%1" == "2015" (
	set BUILD_VS_VER=14
	set BUILD_VS_YEAR=2015
	)	else if "%1" == "2013" (
	set BUILD_VS_VER=12
	set BUILD_VS_YEAR=2013
	)	else if "%1" == "packagename" (
	set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -DCPACK_OVERRIDE_PACKAGENAME="%2"
	shift /1
	)	else if "%1" == "nobuild" (
	set NOBUILD=1
	)	else if "%1" == "showhash" (
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
	) else if "%PROCESSOR_ARCHITEW6432%" == "AMD64" (
		set WINDOWS_ARCH= Win64
	) else (
		set WINDOWS_ARCH=
	)
) else if "%BUILD_ARCH%"=="x64" (
		set WINDOWS_ARCH= Win64
	) else if "%BUILD_ARCH%"=="x86" (
		set WINDOWS_ARCH=
	)

if "%BUILD_VS_VER%"=="" (
	set BUILD_VS_VER=12
	set BUILD_VS_YEAR=2013
)

set BUILD_DIR=%BUILD_DIR%_vc%BUILD_VS_VER%

REM Detect MSVC Installation
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

REM Sanity Checks
where /Q msbuild
if %ERRORLEVEL% NEQ 0 (
	echo Error: "MSBuild" command not in the PATH.
	echo You must have MSVC installed and run this from the "Developer Command Prompt"
	echo ^(available from Visual Studio's Start menu entry^), aborting!
	goto EOF
)
where /Q cmake
if %ERRORLEVEL% NEQ 0 (
	echo Error: "CMake" command not in the PATH.
	echo You must have CMake installed and added to your PATH, aborting!
	goto EOF
)
if NOT EXIST %BLENDER_DIR%..\lib\nul (
	echo Error: Path to libraries not found "%BLENDER_DIR%..\lib\"
	echo This is needed for building, aborting!
	goto EOF
)
if NOT "%TARGET_SET%"=="1" (
	echo Error: Convenience target not set
	echo This is required for building, aborting!
	echo . 
	goto HELP
)

set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -G "Visual Studio %BUILD_VS_VER% %BUILD_VS_YEAR%%WINDOWS_ARCH%"
if NOT EXIST %BUILD_DIR%\nul (
	mkdir %BUILD_DIR%
)
if "%MUST_CLEAN%"=="1" (
		echo Cleaning %BUILD_DIR%
		msbuild ^
			%BUILD_DIR%\Blender.sln ^
			/target:clean ^
			/property:Configuration=%BUILD_TYPE% ^
			/verbosity:minimal
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
if "%NOBUILD%"=="1" goto EOF

msbuild ^
	%BUILD_DIR%\Blender.sln ^
	/target:build ^
	/property:Configuration=%BUILD_TYPE% ^
	/maxcpucount ^
	/verbosity:minimal

if %ERRORLEVEL% NEQ 0 (
	echo "Build Failed"
	goto EOF
)

msbuild ^
	%BUILD_DIR%\INSTALL.vcxproj ^
	/property:Configuration=%BUILD_TYPE% ^
	/verbosity:minimal

echo.
echo At any point you can optionally modify your build configuration by editing:
echo "%BUILD_DIR%\CMakeCache.txt", then run "make" again to build with the changes applied.
echo.
echo Blender successfully built, run from: "%BUILD_DIR%\bin\%BUILD_TYPE%"
echo.
goto EOF
:HELP
		echo.
		echo Convenience targets
		echo - release 
		echo - debug
		echo - full
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
		echo - packagename [newname] ^(override default cpack package name^)
		echo - x86 ^(override host autodetect and build 32 bit code^)
		echo - x64 ^(override host autodetect and build 64 bit code^)
		echo - 2013 ^(build with visual studio 2013^)
		echo - 2015 ^(build with visual studio 2015^) [EXPERIMENTAL]
		echo.

:EOF

