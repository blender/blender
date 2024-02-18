@echo off
echo Starting blender with GPU debugging options, log files will be created
echo in your temp folder, windows explorer will open after you close blender
echo to help you find them.
echo.
echo If you report a bug on https://projects.blender.org you can attach these files
echo by dragging them into the text area of your bug report, please include both
echo blender_debug_output.txt and blender_system_info.txt in your report. 
echo.
pause
echo.
echo Starting blender and waiting for it to exit....
setlocal

set PYTHONPATH=
set DEBUGLOGS="%temp%\blender\debug_logs"
mkdir "%DEBUGLOGS%" > NUL 2>&1

"%~dp0\blender" --debug --debug-gpu --debug-cycles --python-expr "import bpy; bpy.context.preferences.filepaths.temporary_directory=r'%DEBUGLOGS%'; bpy.ops.wm.sysinfo(filepath=r'%DEBUGLOGS%\blender_system_info.txt')" > "%DEBUGLOGS%\blender_debug_output.txt" 2>&1 < %0
explorer "%DEBUGLOGS%"
