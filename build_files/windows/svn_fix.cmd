if "%BUILD_VS_YEAR%"=="2017" set BUILD_VS_LIBDIRPOST=vc15
if "%BUILD_VS_YEAR%"=="2019" set BUILD_VS_LIBDIRPOST=vc15
if "%BUILD_VS_YEAR%"=="2022" set BUILD_VS_LIBDIRPOST=vc15

set BUILD_VS_SVNDIR=win64_%BUILD_VS_LIBDIRPOST%
set BUILD_VS_LIBDIR="%BLENDER_DIR%..\lib\%BUILD_VS_SVNDIR%"

echo Starting cleanup in %BUILD_VS_LIBDIR%.
cd %BUILD_VS_LIBDIR%
:RETRY
"%SVN%" cleanup
"%SVN%" update
if errorlevel 1 (
		set /p LibRetry= "Error during update, retry? y/n"
		if /I "!LibRetry!"=="Y" (
			goto RETRY
		)
		echo.
		echo Error: Download of external libraries failed. 
		echo This is needed for building, please manually run 'svn cleanup' and 'svn update' in
		echo %BUILD_VS_LIBDIR% , until this is resolved you CANNOT make a successful blender build
		echo.
		exit /b 1
)
echo Cleanup complete

