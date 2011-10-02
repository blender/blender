# headless configuration, useful in for servers or renderfarms
# builds without a windowing system (X11/Windows/Cocoa).
#
# Example usage:
#   cmake -C../blender/build_files/cmake/config/blender_headless.cmake  ../blender
#

set(WITH_HEADLESS            ON  CACHE FORCE BOOL) 
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
