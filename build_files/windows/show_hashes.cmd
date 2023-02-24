if "%GIT%" == "" (
	echo Git not found, cannot show hashes.
	goto EOF
)
cd "%BLENDER_DIR%"
for /f "delims=" %%i in ('"%GIT%" rev-parse HEAD') do echo Branch_hash=%%i
cd "%BLENDER_DIR%/locale"
for /f "delims=" %%i in ('"%GIT%" rev-parse HEAD') do echo Locale_hash=%%i
cd "%BLENDER_DIR%/scripts/addons"
for /f "delims=" %%i in ('"%GIT%" rev-parse HEAD') do echo Addons_Hash=%%i
cd "%BLENDER_DIR%"
:EOF