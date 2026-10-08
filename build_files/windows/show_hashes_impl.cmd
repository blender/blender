REM Usage: show_hashes_impl.cmd <title> <folder>
REM   title: Section title (e.g., "Blender" or "Libraries")
REM   folder: Git repository folder path
setlocal enabledelayedexpansion
set "TITLE=%~1"
set "FOLDER=%~2"

cd "%FOLDER%"

echo.
echo [%TITLE%]
echo   Folder:   %FOLDER%
for /f "delims=" %%i in ('"%GIT%" rev-parse --abbrev-ref HEAD') do echo   Branch:   %%i
for /f "delims=" %%i in ('"%GIT%" rev-parse HEAD') do echo   Commit:   %%i
for /f "delims=" %%i in ('"%GIT%" log -1 --date^=local --format^=%%cd') do echo   Date:     %%i
for /f "delims=" %%i in ('"%GIT%" log -1 --format^=%%s') do echo   Title:    %%i
for /f "delims=" %%i in ('"%GIT%" rev-parse --abbrev-ref --symbolic-full-name @{u} 2^>^&1') do (
	set "UPSTREAM=%%i"
)
if "!UPSTREAM:~0,6!" == "fatal:" (
	echo   Upstream: !UPSTREAM:fatal: =!
) else (
	echo   Upstream: !UPSTREAM!
)
for /f "delims=" %%i in ('"%GIT%" remote get-url origin 2^>^&1') do (
	set "REMOTE_URL=%%i"
)
if "!REMOTE_URL:~0,6!" == "fatal:" (
	echo   Remote:   !REMOTE_URL:fatal: =!
) else (
	echo   Remote:   !REMOTE_URL!
)
endlocal
