REM find all dependencies and set the corresponding environment variables. 
for %%X in (svn.exe) do (set SVN=%%~$PATH:X)
for %%X in (cmake.exe) do (set CMAKE=%%~$PATH:X)
for %%X in (ctest.exe) do (set CTEST=%%~$PATH:X)
for %%X in (git.exe) do (set GIT=%%~$PATH:X)
REM For python, default on 310 but if that does not exist also check
REM the 311, 312 and finally 39 folders to see if those are there, it checks
REM this far ahead to ensure good lib folder compatibility in the future
REM it falls back to 3.9 just incase it is a very old lib folder.
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
set PYTHON=%BLENDER_DIR%\..\lib\win64_vc15\python\39\bin\python.exe
if EXIST %PYTHON% (
	goto detect_python_done
)

if NOT EXIST %PYTHON% (
    REM Only emit this warning when the library folder exists but the
    REM python folder does not. As we don't want to concern people that
    REM run make update for the first time. 
    if EXIST %BLENDER_DIR%\..\lib\win64_vc15 (
      echo Warning: Python not found, there is likely an issue with the library folder
    )
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
