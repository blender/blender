REM find all dependencies and set the corresponding environment variables. 
for %%X in (cmake.exe) do (set CMAKE=%%~$PATH:X)
for %%X in (ctest.exe) do (set CTEST=%%~$PATH:X)
for %%X in (git.exe) do (set GIT=%%~$PATH:X)
REM For python, default on 313 but if that does not exist also check
REM 311, to see if that is available.
set PYTHON=%BLENDER_DIR%\lib\windows_x64\python\313\bin\python.exe
if EXIST %PYTHON% (
	goto detect_python_done
)
set PYTHON=%BLENDER_DIR%\lib\windows_x64\python\311\bin\python.exe
if EXIST %PYTHON% (
	goto detect_python_done
)
rem Additionally check for the ARM64 version
set PYTHON=%BLENDER_DIR%\lib\windows_arm64\python\313\bin\python.exe
if EXIST %PYTHON% (
	goto detect_python_done
)
set PYTHON=%BLENDER_DIR%\lib\windows_arm64\python\311\bin\python.exe
if EXIST %PYTHON% (
	goto detect_python_done
)

if NOT EXIST %PYTHON% (
    if EXIST %BLENDER_DIR%\lib\windows_x64\.git (
      echo Warning: Python not found, there is likely an issue with the library folder
    )
    set PYTHON=""
)

:detect_python_done
if NOT "%verbose%" == "" (
	echo cmake  : "%CMAKE%"
	echo ctest  : "%CTEST%"
	echo git    : "%GIT%"
	echo python : "%PYTHON%"
)
