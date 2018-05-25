if NOT exist "%BLENDER_DIR%/source/tools" (
	echo Checking out sub-modules 
	if not "%GIT%" == "" (
		"%GIT%" submodule update --init --recursive --progress
		if errorlevel 1 goto FAIL
		"%GIT%" submodule foreach git checkout master
		if errorlevel 1 goto FAIL
		"%GIT%" submodule foreach git pull --rebase origin master
		if errorlevel 1 goto FAIL
		goto EOF
	) else (
		echo Blender submodules not found, and git not found in path to retrieve them.
		goto FAIL
	)
)
goto EOF

:FAIL
exit /b 1
:EOF