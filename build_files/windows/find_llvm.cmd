REM Find a copy of LLVM on the system
set LLVM_DIR=

REM First, we try and find the copy on the PATH (unless already specified, in which case we use that)
if "%LLVM_EXE%" == "" (
  for %%X in (clang-cl.exe) do (set "LLVM_EXE=%%~$PATH:X")
) else (
    echo LLVM EXE manually specified, using that
)

if NOT "%LLVM_EXE%" == "" (
	REM We have found LLVM on the path
    for %%X in ("%LLVM_EXE%\..\..") do set "LLVM_DIR=%%~fX"
	if NOT "%verbose%" == "" (
		echo LLVM detected via path
	)
	goto detect_llvm_done
)

REM If that fails, we try and get it from the registry
REM Check 64-bit path
for /F "usebackq skip=2 tokens=1-2*" %%A IN (`REG QUERY "HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\LLVM\LLVM" /ve 2^>nul`) DO set LLVM_DIR=%%C
if NOT "%LLVM_DIR%" == "" (
	if NOT "%verbose%" == "" (
		echo LLVM Detected via 64-bit registry
	)
	goto detect_llvm_done
)

REM Check 32-bit path
for /F "usebackq skip=2 tokens=1-2*" %%A IN (`REG QUERY "HKEY_LOCAL_MACHINE\SOFTWARE\LLVM\LLVM" /ve 2^>nul`) DO set LLVM_DIR=%%C
if NOT "%LLVM_DIR%" == "" (
	if NOT "%verbose%" == "" (
		echo LLVM Detected via 32-bit registry
	)
	goto detect_llvm_done
)

rem No copy has been found, so error out
if "%LLVM_DIR%" == "" (
	echo LLVM not found on the path, or in the registry. Please verify your installation.
	goto ERR
)

:detect_llvm_done
for /F "usebackq tokens=3" %%X in (`CALL "%LLVM_DIR%\bin\clang-cl.exe" --version ^| findstr "clang version"`) do set CLANG_VERSION=%%X

if NOT "%verbose%" == "" (
	echo Using Clang/LLVM
	echo Version  : %CLANG_VERSION%
	echo Location : "%LLVM_DIR%"
)

:EOF
exit /b 0
:ERR
exit /b 1