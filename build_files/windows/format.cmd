if EXIST %BLENDER_DIR%\..\lib\win64_vc14\llvm\bin\clang-format.exe (
    set CF_PATH=..\lib\win64_vc14\llvm\bin
    goto detect_done
)
if EXIST %BLENDER_DIR%\..\lib\windows_vc14\llvm\bin\clang-format.exe (
    set CF_PATH=..\lib\windows_vc14\llvm\bin
    goto detect_done
)

echo clang-format not found
exit /b 1

:detect_done
echo found clang-format in %CF_PATH%

REM TODO(sergey): Switch to Python from libraries when available.
set PYTHON="python.exe"
set FORMAT_PATHS=%BLENDER_DIR%\source\tools\utils_maintenance\clang_format_paths.py

REM The formatting script expects clang-format to be in the current PATH.
set PATH=%CF_PATH%;%PATH%

%PYTHON% %FORMAT_PATHS% %FORMAT_ARGS%

:EOF
