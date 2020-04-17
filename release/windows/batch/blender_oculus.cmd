@echo off

REM Helper setting hints to get the OpenXR preview support enabled for Oculus.
REM Of course this is not meant as a permanent solution. Oculus will likely provide a better setup at some point.

echo Starting Blender with Oculus OpenXR support. This assumes the Oculus runtime
echo is installed in the default location. If this is not the case, please adjust
echo the path inside oculus.json.
echo.
echo Note that OpenXR support in Oculus is considered a preview. Use with care!
echo.
pause
set XR_RUNTIME_JSON=%~dp0oculus.json
"%~dp0\blender"
