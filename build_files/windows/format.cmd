REM TODO(sergey): Switch to Python from libraries when available.
set PYTHON="python.exe"
set FORMAT_PATHS=%BLENDER_DIR%\source\tools\utils\clang_format_paths.py

REM The formatting script expects clang-format to be in the current PATH.
set PATH=%BUILD_VS_LIBDIR%\llvm\bin;%PATH%

%PYTHON% %FORMAT_PATHS% --expand-tabs

:EOF
