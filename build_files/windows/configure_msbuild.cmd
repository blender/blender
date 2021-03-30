set BUILD_GENERATOR_POST=
set BUILD_PLATFORM_SELECT=
set MSBUILD_PLATFORM=x64

if "%BUILD_WITH_SCCACHE%"=="1" (
		echo sccache is only supported with ninja as the build system. 
		exit /b 1 
)

if "%WITH_CLANG%"=="1" (
	set CLANG_CMAKE_ARGS=-T"llvm"
)

if "%WITH_ASAN%"=="1" (
		set ASAN_CMAKE_ARGS=-DWITH_COMPILER_ASAN=On
)

if "%WITH_PYDEBUG%"=="1" (
	set PYDEBUG_CMAKE_ARGS=-DWINDOWS_PYTHON_DEBUG=On
)

if "%BUILD_VS_YEAR%"=="2019" (
	set BUILD_PLATFORM_SELECT=-A %MSBUILD_PLATFORM%
) else (
	set BUILD_GENERATOR_POST=%WINDOWS_ARCH%
)

set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -G "Visual Studio %BUILD_VS_VER% %BUILD_VS_YEAR%%BUILD_GENERATOR_POST%" %BUILD_PLATFORM_SELECT% %TESTS_CMAKE_ARGS% %CLANG_CMAKE_ARGS% %ASAN_CMAKE_ARGS% %PYDEBUG_CMAKE_ARGS%

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
)

if NOT EXIST %BUILD_DIR%\Blender.sln set MUST_CONFIGURE=1
if "%NOBUILD%"=="1" set MUST_CONFIGURE=1

if "%MUST_CONFIGURE%"=="1" (

	if NOT "%verbose%" == "" (
		echo "%CMAKE% %BUILD_CMAKE_ARGS% -H%BLENDER_DIR% -B%BUILD_DIR%"
	)

	cmake ^
		%BUILD_CMAKE_ARGS% ^
		-H%BLENDER_DIR% ^
		-B%BUILD_DIR% 
	
	if errorlevel 1 (
		echo "Configuration Failed"
		exit /b 1
	)
)
echo echo off > %BUILD_DIR%\rebuild.cmd
echo if "%%VSCMD_VER%%" == "" ^( >> %BUILD_DIR%\rebuild.cmd
echo   call "%VCVARS%" %BUILD_ARCH% >> %BUILD_DIR%\rebuild.cmd
echo ^) >> %BUILD_DIR%\rebuild.cmd
echo "%CMAKE%" . >> %BUILD_DIR%\rebuild.cmd
echo echo %%TIME%% ^> buildtime.txt >> %BUILD_DIR%\rebuild.cmd
echo msbuild ^
	%BUILD_DIR%\INSTALL.vcxproj ^
	/property:Configuration=%BUILD_TYPE% ^
	/maxcpucount:2 ^
	/verbosity:minimal ^
	/p:platform=%MSBUILD_PLATFORM% ^
	/flp:Summary;Verbosity=minimal;LogFile=%BUILD_DIR%\Build.log >> %BUILD_DIR%\rebuild.cmd
echo echo %%TIME%% ^>^> buildtime.txt >> %BUILD_DIR%\rebuild.cmd