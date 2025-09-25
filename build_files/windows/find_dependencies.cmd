REM find all dependencies and set the corresponding environment variables. 
for %%X in (cmake.exe) do (set CMAKE=%%~$PATH:X)
for %%X in (ctest.exe) do (set CTEST=%%~$PATH:X)
for %%X in (git.exe) do (set GIT=%%~$PATH:X)
REM For python, default on 310 but if that does not exist also check
REM the 311, 312 and finally 39 folders to see if those are there, it checks
REM this far ahead to ensure good lib folder compatibility in the future
REM it falls back to 3.9 just in case it is a very old lib folder.
set PYTHON=%BLENDER_DIR%\lib\windows_x64\python\310\bin\python.exe
if EXIST %PYTHON% (
	goto detect_python_done
)
set PYTHON=%BLENDER_DIR%\lib\windows_x64\python\311\bin\python.exe
if EXIST %PYTHON% (
	goto detect_python_done
)
set PYTHON=%BLENDER_DIR%\lib\windows_x64\python\312\bin\python.exe
if EXIST %PYTHON% (
	goto detect_python_done
)
set PYTHON=%BLENDER_DIR%\lib\windows_x64\python\39\bin\python.exe
if EXIST %PYTHON% (
	goto detect_python_done
)

rem Additionally check for the ARM64 version
set PYTHON=%BLENDER_DIR%\lib\windows_arm64\python\310\bin\python.exe
if EXIST %PYTHON% (
	goto detect_python_done
)
set PYTHON=%BLENDER_DIR%\lib\windows_arm64\python\311\bin\python.exe
if EXIST %PYTHON% (
	goto detect_python_done
)
set PYTHON=%BLENDER_DIR%\lib\windows_arm64\python\312\bin\python.exe
if EXIST %PYTHON% (
	goto detect_python_done
)
set PYTHON=%BLENDER_DIR%\lib\windows_arm64\python\39\bin\python.exe
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
