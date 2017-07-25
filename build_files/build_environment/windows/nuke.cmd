@echo off
if "%1"=="" goto EOF:
set ROOT=%~dp0\..\..\..\..\build_windows\deps

set CurPath=%ROOT%\s\vs1264D\debug\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1264D\build\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1264R\release\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1264R\build\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\output\win64_vc12\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 

set CurPath=%ROOT%\s\vs1464D\debug\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1464D\build\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1464R\release\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1464R\build\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\output\win64_vc14\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 

set CurPath=%ROOT%\s\vs1286D\debug\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1286D\build\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1286R\release\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1286R\build\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\output\windows_vc12\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 

set CurPath=%ROOT%\s\vs1486D\debug\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1486D\build\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1486R\release\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1486R\build\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\output\windows_vc14\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 


:EOF


