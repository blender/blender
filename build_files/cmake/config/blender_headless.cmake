# headless configuration, useful in for servers or renderfarms
# builds without a windowing system (X11/Windows/Cocoa).
#
# Example usage:
#   cmake -C../blender/build_files/cmake/config/blender_headless.cmake  ../blender
#

set(WITH_HEADLESS            ON  CACHE BOOL "" FORCE) 
set(WITH_GAMEENGINE          OFF CACHE BOOL "" FORCE)

# disable audio, its possible some devs may want this but for now disable
# so the python module doesnt hold the audio device and loads quickly.
set(WITH_AUDASPACE           OFF CACHE BOOL "" FORCE)
set(WITH_FFTW3               OFF CACHE BOOL "" FORCE)
set(WITH_JACK                OFF CACHE BOOL "" FORCE)
set(WITH_SDL                 OFF CACHE BOOL "" FORCE)
set(WITH_OPENAL              OFF CACHE BOOL "" FORCE)
set(WITH_CODEC_FFMPEG        OFF CACHE BOOL "" FORCE)
set(WITH_CODEC_SNDFILE       OFF CACHE BOOL "" FORCE)

# other features which are not especially useful as a python module
set(WITH_X11_XINPUT          OFF CACHE BOOL "" FORCE)
set(WITH_INPUT_NDOF          OFF CACHE BOOL "" FORCE)
