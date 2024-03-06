if EXIST %BLENDER_DIR%\lib\windows_x64\llvm\bin\clang-format.exe (
    set CF_PATH=lib\windows_x64\llvm\bin
    goto detect_done
)

if EXIST %BLENDER_DIR%\lib\windows_arm64\llvm\bin\clang-format.exe (
    set CF_PATH=lib\windows_arm64\llvm\bin
    goto detect_done
)

echo clang-format not found
exit /b 1

:detect_done
echo found clang-format in %CF_PATH%

if NOT EXIST %PYTHON% (
    echo python not found, required for this operation
    exit /b 1
)

set FORMAT_PATHS=%BLENDER_DIR%\tools\utils_maintenance\clang_format_paths.py

REM The formatting script expects clang-format to be in the current PATH.
set PATH=%CF_PATH%;%PATH%

REM Use -B to avoid writing __pycache__ in lib directory and causing update conflicts.
%PYTHON% -B %FORMAT_PATHS% %FORMAT_ARGS%

:EOF
