# defaults for building blender as a python module 'bpy'
#
# Example usage:
#   cmake -C../blender/build_files/cmake/config/bpy_module.cmake  ../blender
#

set(WITH_PYTHON_MODULE       ON  CACHE BOOL "" FORCE)

# install into the systems python dir
set(WITH_INSTALL_PORTABLE    OFF CACHE BOOL "" FORCE)

# no point int copying python into python
set(WITH_PYTHON_INSTALL      OFF CACHE BOOL "" FORCE)

# disable audio, its possible some devs may want this but for now disable
# so the python module doesn't hold the audio device and loads quickly.
set(WITH_AUDASPACE           OFF CACHE BOOL "" FORCE)
set(WITH_CODEC_FFMPEG        OFF CACHE BOOL "" FORCE)
set(WITH_CODEC_SNDFILE       OFF CACHE BOOL "" FORCE)
set(WITH_COREAUDIO           OFF CACHE BOOL "" FORCE)
set(WITH_JACK                OFF CACHE BOOL "" FORCE)
set(WITH_OPENAL              OFF CACHE BOOL "" FORCE)
set(WITH_PULSEAUDIO          OFF CACHE BOOL "" FORCE)
set(WITH_SDL                 OFF CACHE BOOL "" FORCE)
set(WITH_WASAPI              OFF CACHE BOOL "" FORCE)

# other features which are not especially useful as a python module
set(WITH_ALEMBIC             OFF CACHE BOOL "" FORCE)
set(WITH_BULLET              OFF CACHE BOOL "" FORCE)
set(WITH_INPUT_NDOF          OFF CACHE BOOL "" FORCE)
set(WITH_INTERNATIONAL       OFF CACHE BOOL "" FORCE)
set(WITH_NANOVDB             OFF CACHE BOOL "" FORCE)
set(WITH_OPENCOLLADA         OFF CACHE BOOL "" FORCE)
set(WITH_OPENVDB             OFF CACHE BOOL "" FORCE)
set(WITH_X11_XINPUT          OFF CACHE BOOL "" FORCE)

# Depends on Python install, do this to quiet warning.
set(WITH_DRACO               OFF CACHE BOOL "" FORCE)

# Jemalloc does not work with dlopen() of Python modules:
# https://github.com/jemalloc/jemalloc/issues/1237
set(WITH_MEM_JEMALLOC        OFF CACHE BOOL "" FORCE)

if(WIN32)
  set(WITH_WINDOWS_BUNDLE_CRT  OFF CACHE BOOL "" FORCE)
endif()
