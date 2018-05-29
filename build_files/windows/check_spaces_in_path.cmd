set BLENDER_DIR_NOSPACES=%BLENDER_DIR: =%

if not "%BLENDER_DIR%"=="%BLENDER_DIR_NOSPACES%" (
	echo There are spaces detected in the build path "%BLENDER_DIR%", this is currently not supported, exiting....
	exit /b 1
)