ninja --version 1>NUL 2>&1
if %ERRORLEVEL% NEQ 0 (
		echo "Ninja not detected in the path"
		exit /b 1
	)

set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -G "Ninja" %TESTS_CMAKE_ARGS% -DCMAKE_BUILD_TYPE=%BUILD_TYPE%

if "%BUILD_WITH_SCCACHE%"=="1" (
	set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -DWITH_WINDOWS_SCCACHE=On
	if NOT "%verbose%" == "" (
		echo Enabling sccache
	)
)

if "%WITH_CLANG%" == "1" (
	REM We want to use an external manifest with Clang
	set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -DWITH_WINDOWS_EXTERNAL_MANIFEST=On

	REM We can assume that we have a working copy via find_llvm.cmd
	set CC=%LLVM_DIR%\bin\clang-cl
	set CXX=%LLVM_DIR%\bin\clang-cl
	set CFLAGS=-m64
	set CXXFLAGS=-m64
)

if "%WITH_ASAN%"=="1" (
	set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -DWITH_COMPILER_ASAN=On
)

if NOT "%verbose%" == "" (
	echo BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% 
)

if NOT EXIST %BUILD_DIR%\nul (
	mkdir %BUILD_DIR%
)

if "%MUST_CLEAN%"=="1" (
	echo Cleaning %BUILD_DIR%
	cd %BUILD_DIR%
	"%CMAKE%" --build . --config Clean
)

if NOT EXIST %BUILD_DIR%\build.ninja set MUST_CONFIGURE=1
if "%NOBUILD%"=="1" set MUST_CONFIGURE=1

if "%MUST_CONFIGURE%"=="1" (
	"%CMAKE%" ^
		%BUILD_CMAKE_ARGS% ^
		-H%BLENDER_DIR% ^
		-B%BUILD_DIR% 

	if %ERRORLEVEL% NEQ 0 (
		echo "Configuration Failed"
		exit /b 1
	)
)

echo echo off > %BUILD_DIR%\rebuild.cmd
echo if "%%VSCMD_VER%%" == "" ^( >> %BUILD_DIR%\rebuild.cmd
echo   call "%VCVARS%" %BUILD_ARCH% >> %BUILD_DIR%\rebuild.cmd
echo ^) >> %BUILD_DIR%\rebuild.cmd
echo echo %%TIME%% ^> buildtime.txt >> %BUILD_DIR%\rebuild.cmd
echo ninja install %%* >> %BUILD_DIR%\rebuild.cmd
echo echo %%TIME%% ^>^> buildtime.txt >> %BUILD_DIR%\rebuild.cmd