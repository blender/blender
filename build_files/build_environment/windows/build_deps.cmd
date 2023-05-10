@echo off
if NOT "%1" == "" (
	if "%1" == "2017" (
    echo "Building for VS2017"
    set VSVER=15.0
    set VSVER_SHORT=15
    set BuildDir=VS15
    goto par2
  )
	if "%1" == "2019" (
    echo "Building for VS2019"
    set VSVER=15.0
    set VSVER_SHORT=15
    set BuildDir=VS15
    goto par2
  )
  
)
:usage

Echo Usage build_deps 2017/2019 x64
goto exit
:par2
if NOT "%2" == "" (
	if "%2" == "x64" (
    echo "Building for x64"
    set HARVESTROOT=Win64_vc
    set ARCH=64
		if "%1" == "2019" (
			set CMAKE_BUILDER=Visual Studio 16 2019
			set CMAKE_BUILD_ARCH=-A x64
		)
		if "%1" == "2017" (
			set CMAKE_BUILDER=Visual Studio 15 2017 Win64
            set CMAKE_BUILD_ARCH=
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

REM If Python is be available certain deps may try to 
REM to use this over the version we build, to prevent that
REM make sure pythonw is NOT in the path. We look for pythonw.exe
REM since windows apparently ships a python.exe that just opens up
REM the windows store but does not ship any actual python files that
REM could cause issues.  
for %%X in (pythonw.exe) do (set PYTHONW=%%~$PATH:X)
if EXIST "%PYTHONW%" (
  echo PYTHON found at %PYTHONW% dependencies cannot be build with python available in the path
  goto exit
)

set SOURCE_DIR=%~dp0\..
set BUILD_DIR=%cd%\build
set HARVEST_DIR=%BUILD_DIR%\output
set STAGING=%BUILD_DIR%\S

rem for python module build
set MSSdk=1 
set DISTUTILS_USE_SDK=1  
rem if you let pip pick its own build dirs, it'll stick it somewhere deep inside the user profile
rem and cython will refuse to link due to a path that gets too long.
set TMPDIR=c:\t\
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
set original_path=%path%
set oiio_paths=%Staging%\%BuildDir%%ARCH%R\Release\openimageio\bin;%Staging%\%BuildDir%%ARCH%D\Debug\openimageio\bin
set boost_paths=%Staging%\%BuildDir%%ARCH%R\Release\boost\lib;%Staging%\%BuildDir%%ARCH%D\Debug\boost\lib
set openexr_paths=%Staging%\%BuildDir%%ARCH%R\Release\openexr\bin;%Staging%\%BuildDir%%ARCH%D\Debug\openexr\bin
set imath_paths=%Staging%\%BuildDir%%ARCH%R\Release\imath\bin;%Staging%\%BuildDir%%ARCH%D\Debug\imath\bin
set tbb_paths=%Staging%\%BuildDir%%ARCH%R\Release\tbb\bin;%Staging%\%BuildDir%%ARCH%D\Debug\tbb\bin
set path=%BUILD_DIR%\downloads\mingw\mingw64\msys\1.0\bin\;%BUILD_DIR%\downloads\nasm-2.12.01\;%path%;%boost_paths%;%oiio_paths%;%openexr_paths%;%imath_paths%;%tbb_paths%
mkdir %STAGING%\%BuildDir%%ARCH%R
cd %Staging%\%BuildDir%%ARCH%R
echo %DATE% %TIME% : Start > %StatusFile%
cmake -G "%CMAKE_BUILDER%" %CMAKE_BUILD_ARCH% -Thost=x64  %SOURCE_DIR% -DPACKAGE_DIR=%BUILD_DIR%/packages -DDOWNLOAD_DIR=%BUILD_DIR%/downloads -DBUILD_MODE=Release -DHARVEST_TARGET=%HARVEST_DIR%/%HARVESTROOT%%VSVER_SHORT%/
echo %DATE% %TIME% : Release Configuration done >> %StatusFile%
if "%dobuild%" == "1" (
	msbuild -maxcpucount:1 "BlenderDependencies.sln" /p:Configuration=Release /fl /flp:logfile=BlenderDeps.log;Verbosity=minimal  /verbosity:minimal
	echo %DATE% %TIME% : Release Build done >> %StatusFile%
	cmake --build . --target Harvest_Release_Results  > Harvest_Release.txt
)
echo %DATE% %TIME% : Release Harvest done >> %StatusFile%
if "%NODEBUG%" == "1" goto exit
cd %BUILD_DIR%
mkdir %STAGING%\%BuildDir%%ARCH%D
cd %Staging%\%BuildDir%%ARCH%D
cmake -G "%CMAKE_BUILDER%" %CMAKE_BUILD_ARCH% -Thost=x64 %SOURCE_DIR% -DPACKAGE_DIR=%BUILD_DIR%/packages -DDOWNLOAD_DIR=%BUILD_DIR%/downloads -DCMAKE_BUILD_TYPE=Debug -DBUILD_MODE=Debug -DHARVEST_TARGET=%HARVEST_DIR%/%HARVESTROOT%%VSVER_SHORT%/  %CMAKE_DEBUG_OPTIONS%
echo %DATE% %TIME% : Debug Configuration done >> %StatusFile%
if "%dobuild%" == "1" (
	msbuild -maxcpucount:1 "BlenderDependencies.sln" /p:Configuration=Debug /verbosity:n /fl /flp:logfile=BlenderDeps.log;;Verbosity=normal
	echo %DATE% %TIME% : Debug Build done >> %StatusFile%
	cmake --build . --target Harvest_Debug_Results> Harvest_Debug.txt
)
echo %DATE% %TIME% : Debug Harvest done >> %StatusFile%
cd %BUILD_DIR%

:exit
set path=%original_path%
Echo .
