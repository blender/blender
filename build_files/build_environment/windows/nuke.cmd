@echo off
REM This is a helper script to easily force a rebuild of a single dependency
REM calling nuke depname in c:\db will remove all build artifacts of the 
REM dependency and the next time you call vmbuild the dep will be build from 
REM scratch. 
if "%1"=="" goto EOF:
set ROOT=%~dp0\build\

set CurPath=%ROOT%\s\vs1564D\debug\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1564D\x64\debug\external_%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1564D\x64\debug\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1564D\build\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1564R\release\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1564R\x64\Release\external_%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1564R\x64\Release\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1564R\build\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\output\win64_vc15\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 

set CurPath=%ROOT%\s\vs1664D\debug\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1664D\x64\debug\external_%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1664D\x64\debug\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1664D\build\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1664R\release\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1664R\x64\Release\external_%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1664R\x64\Release\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1664R\build\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\output\win64_vc16\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 

set CurPath=%ROOT%\s\vs1764D\debug\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1764D\x64\debug\external_%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1764D\x64\debug\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1764D\build\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1764R\release\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1764R\x64\Release\external_%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1764R\x64\Release\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\s\vs1764R\build\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 
set CurPath=%ROOT%\output\win64_vc17\%1\
if EXIST %CurPath%\nul ( echo removing "%CurPath%" && rd /s /q "%CurPath%" ) 

