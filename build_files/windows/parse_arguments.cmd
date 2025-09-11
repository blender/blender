set BUILD_DIR=%BLENDER_DIR%..\build_windows
set BUILD_TYPE=Release
:argv_loop
if NOT "%1" == "" (

	REM Help Message
	if "%1" == "help" (
		set SHOW_HELP=1
		goto EOF
	)
	REM Build Types
	if "%1" == "debug" (
		set BUILD_TYPE=Debug
	REM Build Configurations
	) else if "%1" == "builddir" (
		REM Check if the second character is a : and interpret as an absolute path if present.
		call set BUILDDIR_ARG=%~2
		if "!BUILDDIR_ARG:~1,1!" == ":" (
			set BUILD_DIR_OVERRRIDE=%2
		) else (
			set BUILD_DIR_OVERRRIDE=%BLENDER_DIR%..\%2
		)
		shift /1
	) else if "%1" == "with_tests" (
		set TESTS_CMAKE_ARGS=%TESTS_CMAKE_ARGS% -DWITH_GTESTS=On
	) else if "%1" == "with_gpu_tests" (
		set TESTS_CMAKE_ARGS=%TESTS_CMAKE_ARGS% -DWITH_GPU_BACKEND_TESTS=On -DWITH_GPU_DRAW_TESTS=On -DWITH_GPU_RENDER_TESTS=On
	) else if "%1" == "full" (
		set TARGET=Full
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% ^
		    -C"%BLENDER_DIR%\build_files\cmake\config\blender_full.cmake"
	) else if "%1" == "lite" (
		set TARGET=Lite
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -C"%BLENDER_DIR%\build_files\cmake\config\blender_lite.cmake"
	) else if "%1" == "cycles" (
		set TARGET=Cycles
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -C"%BLENDER_DIR%\build_files\cmake\config\cycles_standalone.cmake"
	) else if "%1" == "headless" (
		set TARGET=Headless
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -C"%BLENDER_DIR%\build_files\cmake\config\blender_headless.cmake"
	) else if "%1" == "bpy" (
		set TARGET=Bpy
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -C"%BLENDER_DIR%\build_files\cmake\config\bpy_module.cmake"
	) else if "%1" == "clang" (
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS%
		set WITH_CLANG=1
	) else if "%1" == "release" (
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -C"%BLENDER_DIR%\build_files\cmake\config\blender_release.cmake"
		set TARGET=Release
	) else if "%1" == "developer" (
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -C"%BLENDER_DIR%\build_files\cmake\config\blender_developer.cmake"
	) else if "%1" == "asan" (
		set WITH_ASAN=1
	) else if "%1" == "x86" ( 
		echo Error: 32 bit builds of blender are no longer supported.
		goto ERR
	) else if "%1" == "x64" (
		set BUILD_ARCH=x64
	) else if "%1" == "arm64" (
		set BUILD_ARCH=arm64
	) else if "%1" == "2019" (
		set BUILD_VS_YEAR=2019
	) else if "%1" == "2019b" (
		set BUILD_VS_YEAR=2019
		set VSWHERE_ARGS=-products Microsoft.VisualStudio.Product.BuildTools
	) else if "%1" == "2022" (
		set BUILD_VS_YEAR=2022
	) else if "%1" == "2022pre" (
		set BUILD_VS_YEAR=2022
		set VSWHERE_ARGS=-prerelease
	) else if "%1" == "2022b" (
		set BUILD_VS_YEAR=2022
		set VSWHERE_ARGS=-products Microsoft.VisualStudio.Product.BuildTools
	) else if "%1" == "2026" (
		set BUILD_VS_YEAR=2026
	) else if "%1" == "2026i" (
		set BUILD_VS_YEAR=2026
		set VSWHERE_ARGS=-prerelease
	) else if "%1" == "2026b" (
		set BUILD_VS_YEAR=2026
		set VSWHERE_ARGS=-products Microsoft.VisualStudio.Product.BuildTools
	) else if "%1" == "packagename" (
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -DCPACK_OVERRIDE_PACKAGENAME="%2"
		shift /1
	) else if "%1" == "nobuild" (
		set NOBUILD=1
	) else if "%1" == "nobuildinfo" (
		set BUILD_CMAKE_ARGS=%BUILD_CMAKE_ARGS% -DWITH_BUILDINFO=Off
	) else if "%1" == "pydebug" (
		set WITH_PYDEBUG=1
	) else if "%1" == "showhash" (
		SET BUILD_SHOW_HASHES=1
	REM Non-Build Commands
	) else if "%1" == "update" (
		SET BUILD_UPDATE=1
        SET BUILD_UPDATE_SVN=1
		set BUILD_UPDATE_ARGS=
	) else if "%1" == "code_update" (
		SET BUILD_UPDATE=1
        SET BUILD_UPDATE_SVN=0
		set BUILD_UPDATE_ARGS="--no-libraries"
	) else if "%1" == "ninja" (
		SET BUILD_WITH_NINJA=1
	) else if "%1" == "sccache" (
		SET BUILD_WITH_SCCACHE=1
	) else if "%1" == "clean" (
		set MUST_CLEAN=1
	) else if "%1" == "verbose" (
		set VERBOSE=1
	) else if "%1" == "test" (
		set TEST=1
		set NOBUILD=1
	) else if "%1" == "license" (
		set LICENSE=1
		goto EOF
	) else if "%1" == "format" (
		set FORMAT=1
		set FORMAT_ARGS=%2 %3 %4 %5 %6 %7 %8 %9
		goto EOF
	) else if "%1" == "icons_geom" (
		set ICONS_GEOM=1
		goto EOF
	) else if "%1" == "doc_py" (
		set DOC_PY=1
		goto EOF
	) else if "%1" == "msvc" (
		set WITH_MSVC=1
	) else (
		echo Command "%1" unknown, aborting!
		goto ERR
	)
	shift /1
	goto argv_loop
)
:EOF
exit /b 0
:ERR
exit /b 1
