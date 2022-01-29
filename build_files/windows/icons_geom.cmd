if NOT EXIST %PYTHON% (
    echo python not found, required for this operation
    exit /b 1
)

call "%~dp0\find_blender.cmd"

if EXIST "%BLENDER_BIN%" (
    goto detect_blender_done
)

echo unable to locate blender, run "set BLENDER_BIN=full_path_to_blender.exe"
exit /b 1

:detect_blender_done

%PYTHON% -B %BLENDER_DIR%\release\datafiles\blender_icons_geom_update.py 

:EOF
