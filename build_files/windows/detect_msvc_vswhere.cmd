if NOT "%verbose%" == "" (
	echo Detecting msvc %BUILD_VS_YEAR%
)

set ProgramFilesX86=%ProgramFiles(x86)%
if not exist "%ProgramFilesX86%" set ProgramFilesX86=%ProgramFiles%

set vs_where=%ProgramFilesX86%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "%vs_where%" (
	if NOT "%verbose%" == "" (
		echo Visual Studio %BUILD_VS_YEAR% is not detected
	)
	goto FAIL
)

if "%BUILD_ARCH%" == "arm64" (
	set VSWHERE_ARCH=ARM64
) else (
	set VSWHERE_ARCH=x86.x64
)

if NOT "%verbose%" == "" (
	echo "%vs_where%" -latest %VSWHERE_ARGS% -version ^[%BUILD_VS_VER%.0^,%BUILD_VS_VER%.99^) -requires Microsoft.VisualStudio.Component.VC.Tools.%VSWHERE_ARCH%
)
for /f "usebackq tokens=1* delims=: " %%i in (`"%vs_where%" -latest -version ^[%BUILD_VS_VER%.0^,%BUILD_VS_VER%.99^) %VSWHERE_ARGS% -requires Microsoft.VisualStudio.Component.VC.Tools.%VSWHERE_ARCH%`) do (
	if /i "%%i"=="installationPath" set VS_InstallDir=%%j
)

if NOT "%verbose%" == "" (
	echo VS_Installdir="%VS_InstallDir%"
)

if "%VS_InstallDir%"=="" (
	if NOT "%verbose%" == "" (
		echo.
		echo Visual Studio is detected but no suitable installation was found. 
		echo.
		echo Check the "Desktop development with C++" workload has been installed. 
		echo. 
		echo If you are attempting to use either Visual Studio Preview version or the Visual C++ Build tools, Please see 'make help' on how to opt in to those toolsets.
		echo. 
		goto FAIL
	)
)

rem If we are using Clang + MSBuild, check to make sure the clang tools are installed
if "%WITH_CLANG%" == "1" (
	if NOT "%BUILD_WITH_NINJA%" == "1" (
		if NOT "%verbose%" == "" (
			echo "%vs_where%" -latest %VSWHERE_ARGS% -version ^[%BUILD_VS_VER%.0^,%BUILD_VS_VER%.99^) -requires Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset
		)

		for /f "usebackq tokens=1* delims=: " %%i in (`"%vs_where%" -latest -version ^[%BUILD_VS_VER%.0^,%BUILD_VS_VER%.99^) %VSWHERE_ARGS% -requires Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset`) do (
			if /i "%%i"=="installationName" set VSWhere_ClangFound=%%j
		)
	)
)

REM This needs to be in a separate block, due to no delayed expansion (which breaks other things if enabled)
if "%WITH_CLANG%" == "1" (
	if NOT "%BUILD_WITH_NINJA%" == "1" (
		if "%VSWhere_ClangFound%"=="" (
			echo.
			echo Clang was specified whilst using the Visual Studio CMake generator, but the Clang Toolset was not found in Visual Studio.
			echo.
			echo Check the "MSBuild support for LLVM (clang-cl) toolset" component has been installed.
			echo.
			echo Alternatively use the Ninja generator via the "ninja" switch when calling make.bat
			goto FAIL
		)
	)
)

set VCVARS=%VS_InstallDir%\VC\Auxiliary\Build\vcvarsall.bat
if exist "%VCVARS%" (
	if NOT "%verbose%" == "" (
		echo calling "%VCVARS%" %BUILD_ARCH%
	)
	call "%VCVARS%" %BUILD_ARCH%
) else (
	if NOT "%verbose%" == "" (
		echo "%VCVARS%" not found
	)
	goto FAIL
)

rem try msbuild
if NOT "%verbose%" == "" (
	echo Testing for MSBuild 
)
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
if NOT "%verbose%" == "" (
	echo Testing for the C/C++ Compiler
)
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
	echo Visual Studio %BUILD_VS_YEAR% is detected successfully  
)
goto EOF

:FAIL
exit /b 1 

:EOF
