@echo off
REM This batch file does an out-of-source CMake build in ../build_windows
REM This is for users who like to configure & build Blender with a single command.
setlocal EnableDelayedExpansion
setlocal ENABLEEXTENSIONS
set BLENDER_DIR=%~dp0

call "%BLENDER_DIR%\build_files\windows\reset_variables.cmd"

call "%BLENDER_DIR%\build_files\windows\check_spaces_in_path.cmd"
if errorlevel 1 goto EOF

call "%BLENDER_DIR%\build_files\windows\parse_arguments.cmd" %*
if errorlevel 1 goto EOF

REM if it is one of the convenience targets and BLENDER_BIN is set
REM skip compiler detection
if "%ICONS%%ICONS_GEOM%%DOC_PY%" == "1" (
	if EXIST "%BLENDER_BIN%" (
		goto convenience_targets
	)
)

call "%BLENDER_DIR%\build_files\windows\find_dependencies.cmd"
if errorlevel 1 goto EOF

if "%BUILD_SHOW_HASHES%" == "1" (
	call "%BLENDER_DIR%\build_files\windows\show_hashes.cmd"
	goto EOF
)

if "%SHOW_HELP%" == "1" (
	call "%BLENDER_DIR%\build_files\windows\show_help.cmd"
	goto EOF
)

if "%FORMAT%" == "1" (
	call "%BLENDER_DIR%\build_files\windows\format.cmd"
	goto EOF
)

call "%BLENDER_DIR%\build_files\windows\detect_architecture.cmd"
if errorlevel 1 goto EOF

if "%BUILD_VS_YEAR%" == "" (
	call "%BLENDER_DIR%\build_files\windows\autodetect_msvc.cmd"
	if errorlevel 1 (
		echo Visual Studio not found ^(try with the 'verbose' switch for more information^)
		goto EOF
	)
) else (
	call "%BLENDER_DIR%\build_files\windows\detect_msvc%BUILD_VS_YEAR%.cmd"
	if errorlevel 1 (
		echo Visual Studio %BUILD_VS_YEAR% not found ^(try with the 'verbose' switch for more information^)
		goto EOF
	)
)

if "%SVN_FIX%" == "1" (
	call "%BLENDER_DIR%\build_files\windows\svn_fix.cmd"
	goto EOF
)

if "%BUILD_UPDATE%" == "1" (
	call "%BLENDER_DIR%\build_files\windows\check_libraries.cmd"
	if errorlevel 1 goto EOF

	call "%BLENDER_DIR%\build_files\windows\update_sources.cmd"
	goto EOF
)

call "%BLENDER_DIR%\build_files\windows\set_build_dir.cmd"

:convenience_targets

if "%ICONS%" == "1" (
	call "%BLENDER_DIR%\build_files\windows\icons.cmd"
	goto EOF
)

if "%ICONS_GEOM%" == "1" (
	call "%BLENDER_DIR%\build_files\windows\icons_geom.cmd"
	goto EOF
)

if "%DOC_PY%" == "1" (
	call "%BLENDER_DIR%\build_files\windows\doc_py.cmd"
	goto EOF
)

echo Building blender with VS%BUILD_VS_YEAR% for %BUILD_ARCH% in %BUILD_DIR%

call "%BLENDER_DIR%\build_files\windows\check_libraries.cmd"
if errorlevel 1 goto EOF

if "%TEST%" == "1" (
	call "%BLENDER_DIR%\build_files\windows\test.cmd"
	goto EOF
)

call "%BLENDER_DIR%\build_files\windows\check_submodules.cmd"
if errorlevel 1 goto EOF

if "%BUILD_WITH_NINJA%" == "" (
	call "%BLENDER_DIR%\build_files\windows\configure_msbuild.cmd"
	if errorlevel 1 goto EOF

	call "%BLENDER_DIR%\build_files\windows\build_msbuild.cmd"
	if errorlevel 1 goto EOF
) else (
	call "%BLENDER_DIR%\build_files\windows\configure_ninja.cmd"
	if errorlevel 1 goto EOF

	call "%BLENDER_DIR%\build_files\windows\build_ninja.cmd"
	if errorlevel 1 goto EOF
)

:EOF
