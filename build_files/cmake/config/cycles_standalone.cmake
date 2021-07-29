# only compile Cycles standalone, without Blender
#
# Example usage:
#   cmake -C../blender/build_files/cmake/config/cycles_standalone.cmake  ../blender
#

# disable Blender
set(WITH_BLENDER             OFF  CACHE BOOL "" FORCE)
set(WITH_PLAYER              OFF  CACHE BOOL "" FORCE)
set(WITH_CYCLES_BLENDER      OFF  CACHE BOOL "" FORCE)

# build Cycles
set(WITH_CYCLES_STANDALONE        ON CACHE BOOL "" FORCE)
set(WITH_CYCLES_STANDALONE_GUI    ON CACHE BOOL "" FORCE)
