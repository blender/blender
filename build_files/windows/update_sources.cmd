if NOT EXIST %PYTHON% (
    echo python not found, required for this operation
    exit /b 1
)
:detect_python_done

REM Use -B to avoid writing __pycache__ in lib directory and causing update conflicts.
%PYTHON% -B %BLENDER_DIR%\build_files\utils\make_update.py --git-command "%GIT%" %BUILD_UPDATE_ARGS%

:EOF
