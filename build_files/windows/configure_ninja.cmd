ninja --version 1>NUL 2>&1
if %ERRORLEVEL% NEQ 0 (
		echo "Ninja not detected in the path"
		exit /b 1
	)

set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -G "Ninja" %TESTS_CMAKE_ARGS% -DCMAKE_BUILD_TYPE=%BUILD_TYPE%

if "%WITH_CLANG%" == "1" (
set LLVM_DIR=
	for /F "usebackq skip=2 tokens=1-2*" %%A IN (`REG QUERY "HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\LLVM\LLVM" /ve 2^>nul`) DO set LLVM_DIR=%%C
	if DEFINED LLVM_DIR (
		if NOT "%verbose%" == "" (
			echo LLVM Detected at "%LLVM_DIR%"
		)
	goto DetectionComplete
	)

	REM Check 32 bits
	for /F "usebackq skip=2 tokens=1-2*" %%A IN (`REG QUERY "HKEY_LOCAL_MACHINE\SOFTWARE\LLVM\LLVM" /ve 2^>nul`) DO set LLVM_DIR=%%C
	if DEFINED LLVM_DIR (
		if NOT "%verbose%" == "" (
			echo LLVM Detected at "%LLVM_DIR%"
		)
		goto DetectionComplete
	)
	echo LLVM not found 
	exit /b 1
	
:DetectionComplete	
	set CC=%LLVM_DIR%\bin\clang-cl
	set CXX=%LLVM_DIR%\bin\clang-cl
	rem build and tested against 2017 15.7
	set CFLAGS=-m64 -fmsc-version=1914
	set CXXFLAGS=-m64 -fmsc-version=1914
	if "%WITH_ASAN%"=="1" (
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -DWITH_COMPILER_ASAN=On
	)	
)

if "%WITH_ASAN%"=="1" (
	if "%WITH_CLANG%" == "" (
		echo ASAN is only supported with clang.
		exit /b 1 
	)
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
	%CMAKE% cmake --build . --config Clean
)

if NOT EXIST %BUILD_DIR%\build.ninja set MUST_CONFIGURE=1
if "%NOBUILD%"=="1" set MUST_CONFIGURE=1

if "%MUST_CONFIGURE%"=="1" (
	cmake ^
		%BUILD_CMAKE_ARGS% ^
		-H%BLENDER_DIR% ^
		-B%BUILD_DIR% 

	if %ERRORLEVEL% NEQ 0 (
		echo "Configuration Failed"
		exit /b 1
	)
)

echo call "%VCVARS%" %BUILD_ARCH% > %BUILD_DIR%\rebuild.cmd
echo echo %%TIME%% ^> buildtime.txt >> %BUILD_DIR%\rebuild.cmd
echo ninja install >> %BUILD_DIR%\rebuild.cmd 
echo echo %%TIME%% ^>^> buildtime.txt >> %BUILD_DIR%\rebuild.cmd