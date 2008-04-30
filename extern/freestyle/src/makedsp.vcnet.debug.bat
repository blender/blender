@echo off
for /D %%d in (*) do call :FOR2 %%d
rem cd ..
goto :EOF

:FOR2
set componentpath=%1
set dirname=%~n1
for %%p in (%componentpath%\*.pro) do if exist %%p call :DSP %%p

goto :EOF

:DSP
set drive=%~d1
set filepath=%~p1
cd %drive%%filepath%
rem echo %drive%%filepath%
set filename=%~n1
echo creating %filename%_d.vcproj from %filename%.pro ...
if %filename% == app (
  qmake -t vcapp -win32 -o %filename%_d %filename%.pro 
) else (
  qmake -t vclib -win32 -o %filename%_d %filename%.pro
)
cd..
