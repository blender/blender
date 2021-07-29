# This is applied as an override on top of blender_linux.config
# Disables all the areas which are not needed for the player.
set(WITH_COMPOSITOR          OFF CACHE BOOL "" FORCE)
set(WITH_CYCLES              OFF CACHE BOOL "" FORCE)
set(WITH_FREESTYLE           OFF CACHE BOOL "" FORCE)
set(WITH_GHOST_XDND          OFF CACHE BOOL "" FORCE)
set(WITH_OPENCOLLADA         OFF CACHE BOOL "" FORCE)
set(WITH_OPENSUBDIV          OFF CACHE BOOL "" FORCE)
set(WITH_LIBMV               OFF CACHE BOOL "" FORCE)

set(WITH_BLENDER             OFF CACHE BOOL "" FORCE)
set(WITH_PLAYER              ON  CACHE BOOL "" FORCE)
