REM First see if there is an environment variable set
if EXIST "%SPHINX_BIN%" (
    goto detect_sphinx_done
)

REM Then see if inkscape is available in the path
for %%X in (sphinx-build.exe) do (set SPHINX_BIN=%%~$PATH:X)
if EXIST "%SPHINX_BIN%" (
    goto detect_sphinx_done
)

echo.The 'sphinx-build' command was not found. Make sure you have Sphinx
echo.installed, then set the SPHINX_BIN environment variable to point
echo.to the full path of the 'sphinx-build' executable. Alternatively you
echo.may add the Sphinx directory to PATH.
echo.
echo.If you don't have Sphinx installed, grab it from
echo.http://sphinx-doc.org/

REM If still not found clear the variable
set SPHINX_BIN=

:detect_sphinx_done
