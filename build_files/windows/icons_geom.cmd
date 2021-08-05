if EXIST %PYTHON% (
    goto detect_python_done
)

set PYTHON=%BLENDER_DIR%\..\lib\win64_vc15\python\39\bin\python.exe
if EXIST %PYTHON% (
    goto detect_python_done
)

echo python not found at %PYTHON%
exit /b 1

:detect_python_done
echo found python (%PYTHON%)

call "%~dp0\find_blender.cmd"

if EXIST "%BLENDER_BIN%" (
    goto detect_blender_done
)

echo unable to locate blender, run "set BLENDER_BIN=full_path_to_blender.exe"
exit /b 1

:detect_blender_done

%PYTHON% -B %BLENDER_DIR%\release\datafiles\blender_icons_geom_update.py 

:EOF
