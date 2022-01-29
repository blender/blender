REM find all dependencies and set the corresponding environment variables. 
for %%X in (svn.exe) do (set SVN=%%~$PATH:X)
for %%X in (cmake.exe) do (set CMAKE=%%~$PATH:X)
for %%X in (ctest.exe) do (set CTEST=%%~$PATH:X)
for %%X in (git.exe) do (set GIT=%%~$PATH:X)
REM For python, default on 39 but if that does not exist also check
REM the 310,311 and 312 folders to see if those are there, it checks
REM this far ahead to ensure good lib folder compatiblity in the future.
set PYTHON=%BLENDER_DIR%\..\lib\win64_vc15\python\39\bin\python.exe
if EXIST %PYTHON% (
	goto detect_python_done
)
set PYTHON=%BLENDER_DIR%\..\lib\win64_vc15\python\310\bin\python.exe
if EXIST %PYTHON% (
	goto detect_python_done
)
set PYTHON=%BLENDER_DIR%\..\lib\win64_vc15\python\311\bin\python.exe
if EXIST %PYTHON% (
	goto detect_python_done
)
set PYTHON=%BLENDER_DIR%\..\lib\win64_vc15\python\312\bin\python.exe
if EXIST %PYTHON% (
	goto detect_python_done
)

if NOT EXIST %PYTHON% (
    echo Warning: Python not found, there is likely an issue with the library folder
    set PYTHON=""
)

:detect_python_done
if NOT "%verbose%" == "" (
	echo svn    : "%SVN%"
	echo cmake  : "%CMAKE%"
	echo ctest  : "%CTEST%"
	echo git    : "%GIT%"
	echo python : "%PYTHON%"
)
