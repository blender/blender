if "%BUILD_ARCH%"=="" (
	if "%PROCESSOR_ARCHITECTURE%" == "AMD64" (
		set WINDOWS_ARCH= Win64
		set BUILD_ARCH=x64
	) else if "%PROCESSOR_ARCHITEW6432%" == "AMD64" (
		set WINDOWS_ARCH= Win64
		set BUILD_ARCH=x64
	) else if "%PROCESSOR_ARCHITECTURE%" == "ARM64" (
		set WINDOWS_ARCH= arm64
		set BUILD_ARCH=arm64
	) else (
		echo Error: 32 bit builds of blender are no longer supported.
		goto ERR
	)
) else if "%BUILD_ARCH%"=="x64" (
	set WINDOWS_ARCH= Win64
) else if "%BUILD_ARCH%"=="arm64" (
	set WINDOWS_ARCH= arm64
	set BUILD_ARCH=arm64
)
:EOF
exit /b 0
:ERR 
exit /b 1