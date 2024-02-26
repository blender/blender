# SPDX-License-Identifier: GPL-2.0-or-later

# ######## Global feature set settings ########

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/config/blender_release.cmake")

message(STATUS "Building in CentOS 7 64bit environment")

set(WITH_CXX11_ABI           OFF CACHE BOOL "" FORCE)

# ######## Linux-specific build options ########
# Options which are specific to Linux-only platforms

set(WITH_DOC_MANPAGE         OFF CACHE BOOL "" FORCE)

# ######## Official release-specific build options ########
# Options which are specific to Linux release builds only

set(WITH_JACK_DYNLOAD        ON  CACHE BOOL "" FORCE)
set(WITH_PULSEAUDIO_DYNLOAD  ON  CACHE BOOL "" FORCE)
set(WITH_SDL_DYNLOAD         ON  CACHE BOOL "" FORCE)

# ######## Release environment specific settings ########

# Platform specific configuration, to ensure static linking against everything.

# Additional linking libraries
set(CMAKE_EXE_LINKER_FLAGS   "-lrt -static-libstdc++ -no-pie"  CACHE STRING "" FORCE)
