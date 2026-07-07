if NOT EXIST %PYTHON% (
    echo python not found, required for this operation
    exit /b 1
)

set FORMAT_PATHS=%BLENDER_DIR%\tools\utils_maintenance\autopep8_format_paths.py

for %%a in (%PYTHON%) do (
    set PEP8_LOCATION=%%~dpa\..\lib\site-packages\autopep8.py
)

REM Use -B to avoid writing __pycache__ in lib directory and causing update conflicts.
REM While we run with --no-subprocess a sub process is still used to get the version
REM information, so we still have to supply a valid --autopep8-command here.
%PYTHON% -B %FORMAT_PATHS%  --autopep8-command "%PEP8_LOCATION%" --no-subprocess %FORMAT_ARGS%

:EOF
