set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -G "Ninja" %TESTS_CMAKE_ARGS% -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
:DetectionComplete
if NOT "%verbose%" == "" (
	echo BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% 
)

if NOT EXIST %BUILD_DIR%\nul (
	mkdir %BUILD_DIR%
)

if "%MUST_CLEAN%"=="1" (
	echo Cleaning %BUILD_DIR%
	cd %BUILD_DIR%
	%CMAKE% cmake --build . --config Clean
)

if NOT EXIST %BUILD_DIR%\Blender.sln set MUST_CONFIGURE=1
if "%NOBUILD%"=="1" set MUST_CONFIGURE=1

if "%MUST_CONFIGURE%"=="1" (
	cmake ^
		%BUILD_CMAKE_ARGS% ^
		-H%BLENDER_DIR% ^
		-B%BUILD_DIR% 

	if %ERRORLEVEL% NEQ 0 (
		echo "Configuration Failed"
		exit /b 1
	)
)

echo call "%VCVARS%" %BUILD_ARCH% > %BUILD_DIR%\rebuild.cmd
echo echo %%TIME%% ^> buildtime.txt >> %BUILD_DIR%\rebuild.cmd
echo ninja install >> %BUILD_DIR%\rebuild.cmd 
echo echo %%TIME%% ^>^> buildtime.txt >> %BUILD_DIR%\rebuild.cmd