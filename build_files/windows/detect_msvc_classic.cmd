if NOT "%verbose%" == "" (
	echo Detecting msvc %BUILD_VS_YEAR%
)
set KEY_NAME="HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\%BUILD_VS_VER%.0\Setup\VC"
for /F "usebackq skip=2 tokens=1-2*" %%A IN (`REG QUERY %KEY_NAME% /v ProductDir 2^>nul`) DO set MSVC_VC_DIR=%%C
if DEFINED MSVC_VC_DIR (
	if NOT "%verbose%" == "" (
		echo Visual Studio %BUILD_VS_YEAR% on Win64 detected at "%MSVC_VC_DIR%"
	)
	goto msvc_detect_finally
)

REM Check 32 bits
set KEY_NAME="HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\VisualStudio\%BUILD_VS_VER%.0\Setup\VC"
for /F "usebackq skip=2 tokens=1-2*" %%A IN (`REG QUERY %KEY_NAME% /v ProductDir 2^>nul`) DO set MSVC_VC_DIR=%%C
if DEFINED MSVC_VC_DIR (
	if NOT "%verbose%" == "" (
		echo Visual Studio %BUILD_VS_YEAR% on Win32 detected at "%MSVC_VC_DIR%"
	)
	goto msvc_detect_finally
)
if NOT "%verbose%" == "" (
	echo Visual Studio %BUILD_VS_YEAR% not found. 
)
goto FAIL
:msvc_detect_finally
set VCVARS=%MSVC_VC_DIR%\vcvarsall.bat
if not exist "%VCVARS%" (
	echo "%VCVARS%" not found.
	goto FAIL
)

call "%vcvars%" %BUILD_ARCH%

rem try msbuild
msbuild /version > NUL 
if errorlevel 1 (
	if NOT "%verbose%" == "" (
		echo Visual Studio %BUILD_VS_YEAR% msbuild not found
	)
	goto FAIL
)

if NOT "%verbose%" == "" (
		echo Visual Studio %BUILD_VS_YEAR% msbuild found 
)

REM try the c++ compiler
cl 2> NUL 1>&2
if errorlevel 1 (
	if NOT "%verbose%" == "" (
		echo Visual Studio %BUILD_VS_YEAR% C/C++ Compiler not found
	)
	goto FAIL
)

if NOT "%verbose%" == "" (
		echo Visual Studio %BUILD_VS_YEAR% C/C++ Compiler found
)
goto DetectionComplete

:FAIL
exit /b 1

:DetectionComplete
if NOT "%verbose%" == "" (
		echo Visual Studio %BUILD_VS_YEAR% Detected successfuly 
)
exit /b 0
