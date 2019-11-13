@echo off

rem This is an entry point of the codesign server for Windows.
rem It makes sure that signtool.exe is within the current PATH and can be
rem used by the Python script.

SETLOCAL

set PATH=C:\Program Files (x86)\Windows Kits\10\App Certification Kit;%PATH%

codesign_server_windows.py
