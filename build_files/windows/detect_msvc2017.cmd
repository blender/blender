if NOT "%verbose%" == "" (
	echo Detecting msvc 2017
)
set BUILD_VS_VER=15
set BUILD_VS_YEAR=2017
set ProgramFilesX86=%ProgramFiles(x86)%
if not exist "%ProgramFilesX86%" set ProgramFilesX86=%ProgramFiles%

set vs_where=%ProgramFilesX86%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%vs_where%" (
	if NOT "%verbose%" == "" (
		echo Visual Studio 2017 ^(15.2 or newer^) is not detected
	)
	goto FAIL
)

if NOT "%verbose%" == "" (
		echo "%vs_where%" -latest %VSWHERE_ARGS% -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64`
	)

for /f "usebackq tokens=1* delims=: " %%i in (`"%vs_where%" -latest %VSWHERE_ARGS% -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64`) do (
	if /i "%%i"=="installationPath" set VS_InstallDir=%%j
)

if "%VS_InstallDir%"=="" (
	if NOT "%verbose%" == "" (
		echo Visual Studio is detected but the "Desktop development with C++" workload has not been instlled
		goto FAIL
	)
)

set VCVARS=%VS_InstallDir%\VC\Auxiliary\Build\vcvarsall.bat
if exist "%VCVARS%" (
	call "%VCVARS%" %BUILD_ARCH%
) else (
	if NOT "%verbose%" == "" (
		echo "%VCVARS%" not found
	)
	goto FAIL
)

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

if NOT "%verbose%" == "" (
	echo Visual Studio 2017 is detected successfully  
)
goto EOF

:FAIL
exit /b 1 

:EOF
