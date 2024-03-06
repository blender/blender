if "%GIT%" == "" (
	echo Git not found, cannot show hashes.
	goto EOF
)
cd "%BLENDER_DIR%"
for /f "delims=" %%i in ('"%GIT%" rev-parse --abbrev-ref HEAD') do echo Branch_name=%%i
for /f "delims=" %%i in ('"%GIT%" rev-parse HEAD') do echo Branch_hash=%%i
cd "%BLENDER_DIR%/scripts/addons"
for /f "delims=" %%i in ('"%GIT%" rev-parse --abbrev-ref HEAD') do echo Addons_Branch_name=%%i
for /f "delims=" %%i in ('"%GIT%" rev-parse HEAD') do echo Addons_Branch_hash=%%i

if "%BUILD_ARCH%" == "arm64" (
	cd "%BLENDER_DIR%/lib/windows_arm64"
) else (
	cd "%BLENDER_DIR%/lib/windows_x64"
)
for /f "delims=" %%i in ('"%GIT%" rev-parse --abbrev-ref HEAD') do echo Libs_Branch_name=%%i
for /f "delims=" %%i in ('"%GIT%" rev-parse HEAD') do echo Libs_Branch_hash=%%i

cd "%BLENDER_DIR%"
:EOF