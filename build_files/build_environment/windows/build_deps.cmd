@echo off
if NOT "%1" == "" (
	if "%1" == "2013" (
    echo "Building for VS2013"
    set VSVER=12.0
    set VSVER_SHORT=12
    set BuildDir=VS12
    goto par2
  )
	if "%1" == "2015" (
    echo "Building for VS2015"
    set VSVER=14.0
    set VSVER_SHORT=14
    set BuildDir=VS14
    goto par2
  )
	if "%1" == "2017" (
    echo "Building for VS2017"
    set VSVER=15.0
    set VSVER_SHORT=15
    set BuildDir=VS15
    goto par2
  )
  
)
:usage

Echo Usage build_deps 2013/2015/2017 x64/x86
goto exit
:par2
if NOT "%2" == "" (
	if "%2" == "x86" (
    echo "Building for x86"
    set HARVESTROOT=Windows_vc
    set ARCH=86
		if "%1" == "2013" (
			set CMAKE_BUILDER=Visual Studio 12 2013
		)
		if "%1" == "2015" (
			set CMAKE_BUILDER=Visual Studio 14 2015
		)
		if "%1" == "2017" (
			set CMAKE_BUILDER=Visual Studio 15 2017
		)
		
    goto start
  )
	if "%2" == "x64" (
    echo "Building for x64"
    set HARVESTROOT=Win64_vc
    set ARCH=64
		if "%1" == "2013" (
			set CMAKE_BUILDER=Visual Studio 12 2013 Win64
		)
		if "%1" == "2015" (
			set CMAKE_BUILDER=Visual Studio 14 2015 Win64
		)
		if "%1" == "2017" (
			set CMAKE_BUILDER=Visual Studio 15 2017 Win64
		)
		
    goto start
  )
)
goto usage

:start
setlocal ENABLEEXTENSIONS
set CMAKE_DEBUG_OPTIONS=-DWITH_OPTIMIZED_DEBUG=On
if "%3" == "debug" set CMAKE_DEBUG_OPTIONS=-DWITH_OPTIMIZED_DEBUG=Off
set dobuild=1
if "%4" == "nobuild" set dobuild=0

set SOURCE_DIR=%~dp0\..
set BUILD_DIR=%cd%\build
set HARVEST_DIR=%BUILD_DIR%\output
set STAGING=%BUILD_DIR%\S

rem for python module build
set MSSdk=1 
set DISTUTILS_USE_SDK=1  
rem for python externals source to be shared between the various archs and compilers
mkdir %BUILD_DIR%\downloads\externals

REM Detect MSVC Installation
if DEFINED VisualStudioVersion goto msvc_detect_finally
set VALUE_NAME=ProductDir
REM Check 64 bits
set KEY_NAME="HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\%VSVER%\Setup\VC"
for /F "usebackq skip=2 tokens=1-2*" %%A IN (`REG QUERY %KEY_NAME% /v %VALUE_NAME% 2^>nul`) DO set MSVC_VC_DIR=%%C
if DEFINED MSVC_VC_DIR goto msvc_detect_finally
REM Check 32 bits
set KEY_NAME="HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\VisualStudio\%VSVER%\Setup\VC"
for /F "usebackq skip=2 tokens=1-2*" %%A IN (`REG QUERY %KEY_NAME% /v %VALUE_NAME% 2^>nul`) DO set MSVC_VC_DIR=%%C
if DEFINED MSVC_VC_DIR goto msvc_detect_finally
:msvc_detect_finally
if DEFINED MSVC_VC_DIR call "%MSVC_VC_DIR%\vcvarsall.bat"
echo on

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

set StatusFile=%BUILD_DIR%\%1_%2.log
set path=%BUILD_DIR%\downloads\mingw\mingw64\msys\1.0\bin\;%BUILD_DIR%\downloads\nasm-2.12.01\;%path%
mkdir %STAGING%\%BuildDir%%ARCH%R
cd %Staging%\%BuildDir%%ARCH%R
echo %DATE% %TIME% : Start > %StatusFile%
cmake -G "%CMAKE_BUILDER%" %SOURCE_DIR% -DDOWNLOAD_DIR=%BUILD_DIR%/downloads -DBUILD_MODE=Release -DHARVEST_TARGET=%HARVEST_DIR%/%HARVESTROOT%%VSVER_SHORT%/
echo %DATE% %TIME% : Release Configuration done >> %StatusFile%
if "%dobuild%" == "1" (
	msbuild /m "ll.vcxproj" /p:Configuration=Release /fl /flp:logfile=BlenderDeps_llvm.log;Verbosity=normal
	msbuild /m "BlenderDependencies.sln" /p:Configuration=Release /fl /flp:logfile=BlenderDeps.log;Verbosity=minimal  /verbosity:minimal
	echo %DATE% %TIME% : Release Build done >> %StatusFile%
	cmake --build . --target Harvest_Release_Results  > Harvest_Release.txt
)
echo %DATE% %TIME% : Release Harvest done >> %StatusFile%
cd %BUILD_DIR%
mkdir %STAGING%\%BuildDir%%ARCH%D
cd %Staging%\%BuildDir%%ARCH%D
cmake -G "%CMAKE_BUILDER%" %SOURCE_DIR% -DDOWNLOAD_DIR=%BUILD_DIR%/downloads -DCMAKE_BUILD_TYPE=Debug -DBUILD_MODE=Debug -DHARVEST_TARGET=%HARVEST_DIR%/%HARVESTROOT%%VSVER_SHORT%/  %CMAKE_DEBUG_OPTIONS%
echo %DATE% %TIME% : Debug Configuration done >> %StatusFile%
if "%dobuild%" == "1" (
	msbuild /m "ll.vcxproj" /p:Configuration=Debug /fl /flp:logfile=BlenderDeps_llvm.log;;Verbosity=normal 
	msbuild /m "BlenderDependencies.sln" /p:Configuration=Debug /verbosity:n /fl /flp:logfile=BlenderDeps.log;;Verbosity=normal
	echo %DATE% %TIME% : Debug Build done >> %StatusFile%
	cmake --build . --target Harvest_Debug_Results> Harvest_Debug.txt
)
echo %DATE% %TIME% : Debug Harvest done >> %StatusFile%
cd %BUILD_DIR%

:exit
Echo .
