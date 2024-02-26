set BUILD_VS_LIBDIR=lib/windows_x64

if NOT "%verbose%" == "" (
	echo Library Directory = "%BUILD_VS_LIBDIR%"
)
if NOT EXIST "%BUILD_VS_LIBDIR%\.git" (
	rem libs not found, but git is on the system
	if not "%GIT%"=="" (
		echo.
		echo The required external libraries in %BUILD_VS_LIBDIR% are missing
		echo.
		set /p GetLibs= "Would you like to download them? (y/n)"
		if /I "!GetLibs!"=="Y" (
			echo.
			echo Downloading %BUILD_VS_LIBDIR% libraries, please wait.
			echo.
			echo *********************************************************
			echo *                                                       *
			echo * Note: Once the initial download finishes and you see  *
			echo *       "Resolving deltas: 100%% (nnn/nnn) done"         *
			echo *       a second, much larger, update will occur with   *
			echo *       no visible updates. Please do not interrupt     *
			echo *       this process. It may take over an hour to       *
			echo *       complete depending on your internet connection. *
			echo *                                                       *
			echo *********************************************************
:RETRY
			"%GIT%" -C "%BLENDER_DIR%\" config --local "submodule.%BUILD_VS_LIBDIR%.update" "checkout"
			set GIT_LFS_SKIP_SMUDGE=1
			"%GIT%" -C "%BLENDER_DIR%\" submodule update --progress --init "%BUILD_VS_LIBDIR%"
			set GIT_LFS_SKIP_SMUDGE=
			"%GIT%" -C "./%BUILD_VS_LIBDIR%" lfs pull
			if errorlevel 1 (
				set /p LibRetry= "Error during download, retry? y/n"
				if /I "!LibRetry!"=="Y" (
					goto RETRY
				)
				echo.
				echo Error: Download of external libraries failed. 
				echo Until this is resolved you CANNOT make a successful blender build.
				echo.
				exit /b 1
			)
		) else (
           echo Not downloading libraries, until this is resolved you CANNOT make a successful blender build.
           exit /b 1
        )
	)
) else (
	if NOT EXIST %PYTHON% (
		if not "%GIT%"=="" (
			echo.
			echo Python not found in external libraries, updating to latest version
			echo.
			"%GIT%" -C "%BLENDER_DIR%" submodule update "%BUILD_VS_LIBDIR%"
		)
	)
)

if NOT EXIST %BUILD_VS_LIBDIR% (
	echo.
	echo Error: Required libraries not found at "%BUILD_VS_LIBDIR%"
	echo This is needed for building, aborting!
	echo.
	if "%GIT%"=="" (
		echo This is most likely caused by git.exe not being available.
	)
	exit /b 1
)