REM First see if there is an environment variable set
if EXIST "%INKSCAPE_BIN%" (
    goto detect_inkscape_done
)

REM Then see if inkscape is available in the path
for %%X in (inkscape.exe) do (set INKSCAPE_BIN=%%~$PATH:X)
if EXIST "%INKSCAPE_BIN%" (
    goto detect_inkscape_done
)

REM Finally see if it is perhaps installed at the default location
set INKSCAPE_BIN=%ProgramFiles%\Inkscape\bin\inkscape.exe
if EXIST "%INKSCAPE_BIN%" (
    goto detect_inkscape_done
)

REM If still not found clear the variable
set INKSCAPE_BIN=

:detect_inkscape_done
