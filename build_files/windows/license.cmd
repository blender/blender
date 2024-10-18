if EXIST %PYTHON% (
	goto detect_python_done
)

echo python not found in lib folder
exit /b 1

:detect_python_done

REM Use -B to avoid writing __pycache__ in lib directory and causing update conflicts.
%PYTHON% -B %BLENDER_DIR%\tools\utils_maintenance\make_license.py

:EOF
