set SOURCEDIR=%BLENDER_DIR%/doc/python_api/sphinx-in
set BUILDDIR=%BLENDER_DIR%/doc/python_api/sphinx-out
if "%BF_LANG%" == "" set BF_LANG=en
set SPHINXOPTS=-j auto -D language=%BF_LANG%

call "%~dp0\find_sphinx.cmd"

if EXIST "%SPHINX_BIN%" (
    goto detect_sphinx_done
)

echo unable to locate sphinx-build, run "set sphinx_BIN=full_path_to_sphinx-build.exe"
exit /b 1

:detect_sphinx_done

call "%~dp0\find_blender.cmd"

if EXIST "%BLENDER_BIN%" (
    goto detect_blender_done
)

echo unable to locate blender, run "set BLENDER_BIN=full_path_to_blender.exe"
exit /b 1

:detect_blender_done

%BLENDER_BIN% ^
	--background -noaudio --factory-startup ^
	--python %BLENDER_DIR%/doc/python_api/sphinx_doc_gen.py

"%SPHINX_BIN%" -b html %SPHINXOPTS% %O% %SOURCEDIR% %BUILDDIR%

:EOF
