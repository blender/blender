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
if "%SVN%" == "" (
	echo SVN not found, cannot library information.
	goto EOF
)
set BUILD_VS_LIBDIR=%BLENDER_DIR%..\lib\win64_vc15
for /f "delims=" %%i in ('"%SVN%" info --show-item=url --no-newline %BUILD_VS_LIBDIR% ') do echo Libs_URL=%%i
for /f "delims=" %%i in ('"%SVN%" info --show-item=revision --no-newline %BUILD_VS_LIBDIR% ') do echo Libs_Revision=%%i
for /f "delims=" %%i in ('"%SVN%" info --show-item=last-changed-date --no-newline %BUILD_VS_LIBDIR% ') do echo Libs_LastChange=%%i
cd "%BLENDER_DIR%"
:EOF