REM find all dependencies and set the corresponding environment variables. 
for %%X in (svn.exe) do (set SVN=%%~$PATH:X)
for %%X in (cmake.exe) do (set CMAKE=%%~$PATH:X)
for %%X in (git.exe) do (set GIT=%%~$PATH:X)
if NOT "%verbose%" == "" (
	echo svn   : "%SVN%"
	echo cmake : "%CMAKE%"
	echo git   : "%GIT%"
)
if "%CMAKE%" == "" (
	echo Cmake not found in path, required for building, exiting...
	exit /b 1
)