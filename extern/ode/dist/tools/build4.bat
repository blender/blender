@echo off
rem build all four precision/release configurations and log the build messages
rem (used for debugging).

setlocal

set PLATFORM=cygwin
set SETTINGS=config\user-settings

echo SINGLE debug > BUILD_LOG
echo PLATFORM=%PLATFORM%> %SETTINGS%
echo PRECISION=SINGLE>> %SETTINGS%
echo BUILD=debug>> %SETTINGS%
make clean
make >> BUILD_LOG
echo --------------------------------------------- >> BUILD_LOG

echo DOUBLE debug >> BUILD_LOG
echo PLATFORM=%PLATFORM%> %SETTINGS%
echo PRECISION=DOUBLE>> %SETTINGS%
echo BUILD=debug>> %SETTINGS%
make clean
make >> BUILD_LOG
echo --------------------------------------------- >> BUILD_LOG

echo SINGLE release >> BUILD_LOG
echo PLATFORM=%PLATFORM%> %SETTINGS%
echo PRECISION=SINGLE>> %SETTINGS%
echo BUILD=release>> %SETTINGS%
make clean
make >> BUILD_LOG
echo --------------------------------------------- >> BUILD_LOG

echo DOUBLE release >> BUILD_LOG
echo PLATFORM=%PLATFORM%> %SETTINGS%
echo PRECISION=DOUBLE>> %SETTINGS%
echo BUILD=release>> %SETTINGS%
make clean
make >> BUILD_LOG
echo --------------------------------------------- >> BUILD_LOG

make clean
del %SETTINGS%
