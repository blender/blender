if "%GIT%" == "" (
	echo Git not found, cannot show hashes.
	goto EOF
)
call "%~dp0show_hashes_impl.cmd" "Blender" "%BLENDER_DIR%"
if "%BUILD_ARCH%" == "arm64" (
	set "LIB_FOLDER=%BLENDER_DIR%/lib/windows_arm64"
) else (
	set "LIB_FOLDER=%BLENDER_DIR%/lib/windows_x64"
)
REM Check if the library folder is a valid git repository
if not exist "%LIB_FOLDER%\.git" goto :lib_not_found
call "%~dp0show_hashes_impl.cmd" "Libraries" "%LIB_FOLDER%"
goto EOF

:lib_not_found
echo.
echo [Libraries]
echo   Folder:   %LIB_FOLDER%
echo   Status:   Not a valid git repository
echo.
echo   The library submodule has not been initialized.
echo   Run 'make update' to download the required libraries.

:EOF
