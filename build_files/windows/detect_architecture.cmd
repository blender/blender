if "%BUILD_ARCH%"=="" (
	if "%PROCESSOR_ARCHITECTURE%" == "AMD64" (
		set WINDOWS_ARCH= Win64
		set BUILD_ARCH=x64
	) else if "%PROCESSOR_ARCHITEW6432%" == "AMD64" (
		set WINDOWS_ARCH= Win64
		set BUILD_ARCH=x64
	) else (
		set WINDOWS_ARCH=
		set BUILD_ARCH=x86
	)
) else if "%BUILD_ARCH%"=="x64" (
	set WINDOWS_ARCH= Win64
) else if "%BUILD_ARCH%"=="x86" (
	set WINDOWS_ARCH=
)
