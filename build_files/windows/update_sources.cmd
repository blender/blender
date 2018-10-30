if "%SVN%" == "" (
	echo svn not found, cannot update libraries
	goto UPDATE_GIT
)
"%SVN%" up "%BLENDER_DIR%/../lib/*"

:UPDATE_GIT

if "%GIT%" == "" (
	echo Git not found, cannot update code
	goto EOF
)
"%GIT%" pull --rebase
"%GIT%" submodule update --init --recursive
rem Use blender2.8 branch for submodules that have it.
"%GIT%" submodule foreach "git checkout blender2.8 || git checkout master"
"%GIT%" submodule foreach git pull --rebase origin


:EOF