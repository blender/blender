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
"%GIT%" submodule foreach git pull --rebase origin master

:EOF