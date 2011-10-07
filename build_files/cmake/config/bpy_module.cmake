# defaults for building blender as a python module 'bpy'
#
# Example usage:
#   cmake -C../blender/build_files/cmake/config/bpy_module.cmake  ../blender
#

set(WITH_PYTHON_MODULE       ON  CACHE FORCE BOOL)

# install into the systems python dir
set(WITH_INSTALL_PORTABLE    OFF CACHE FORCE BOOL)

# no point int copying python into python
set(WITH_PYTHON_INSTALL      OFF CACHE FORCE BOOL)

# dont build the game engine
set(WITH_GAMEENGINE          OFF CACHE FORCE BOOL)

# disable audio, its possible some devs may want this but for now disable
# so the python module doesnt hold the audio device and loads quickly.
set(WITH_AUDASPACE           OFF CACHE FORCE BOOL)
set(WITH_FFTW3               OFF CACHE FORCE BOOL)
set(WITH_JACK                OFF CACHE FORCE BOOL)
set(WITH_SDL                 OFF CACHE FORCE BOOL)
set(WITH_OPENAL              OFF CACHE FORCE BOOL)
set(WITH_CODEC_FFMPEG        OFF CACHE FORCE BOOL)
set(WITH_CODEC_SNDFILE       OFF CACHE FORCE BOOL)

# other features which are not especially useful as a python module
set(WITH_X11_XINPUT          OFF CACHE FORCE BOOL)
set(WITH_INPUT_NDOF          OFF CACHE FORCE BOOL)
set(WITH_OPENCOLLADA         OFF CACHE FORCE BOOL)
set(WITH_INTERNATIONAL       OFF CACHE FORCE BOOL)
set(WITH_BULLET              OFF CACHE FORCE BOOL)
