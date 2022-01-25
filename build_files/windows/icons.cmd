if NOT EXIST %PYTHON% (
    echo python not found, required for this operation
    exit /b 1
)

call "%~dp0\find_inkscape.cmd"

if EXIST "%INKSCAPE_BIN%" (
    goto detect_inkscape_done
)

echo unable to locate inkscape, run "set inkscape_BIN=full_path_to_inkscape.exe"
exit /b 1

:detect_inkscape_done

call "%~dp0\find_blender.cmd"

if EXIST "%BLENDER_BIN%" (
    goto detect_blender_done
)

echo unable to locate blender, run "set BLENDER_BIN=full_path_to_blender.exe"
exit /b 1

:detect_blender_done

%PYTHON% -B %BLENDER_DIR%\release\datafiles\blender_icons_update.py
%PYTHON% -B %BLENDER_DIR%\release\datafiles\prvicons_update.py
%PYTHON% -B %BLENDER_DIR%\release\datafiles\alert_icons_update.py

:EOF
