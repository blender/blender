if "%NOBUILD%"=="1" goto EOF
set HAS_ERROR=
cd %BUILD_DIR%
echo %TIME% > buildtime.txt
ninja install
if errorlevel 1 (
		set HAS_ERROR=1
	)
echo %TIME% >>buildtime.txt
cd %BLENDER_DIR%

if "%HAS_ERROR%" == "1" (
		echo Error during build
		exit /b 1
)
:EOF