set BUILD_VS_LIBDIR="lib\windows_x64"

:RETRY
"%GIT%" -C "%BLENDER_DIR%" config --local "submodule.%BUILD_VS_LIBDIR%.update" "checkout"
"%GIT%" -C "%BLENDER_DIR%" submodule update --init "%BUILD_VS_LIBDIR%"
if errorlevel 1 (
		set /p LibRetry= "Error during update, retry? y/n"
		if /I "!LibRetry!"=="Y" (
			goto RETRY
		)
		echo.
		echo Error: Download of external libraries failed. 
		echo Until this is resolved you CANNOT make a successful blender build.
		echo.
		exit /b 1
)
