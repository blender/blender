# SPDX-FileCopyrightText: 2015-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# ######## Global feature set settings ########

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/config/blender_release.cmake")

message(STATUS "Building in Rocky 8 Linux 64bit environment")

set(LIBDIR_NAME "linux_x86_64_glibc_228")

# ######## Linux-specific build options ########
# Options which are specific to Linux-only platforms

set(WITH_DOC_MANPAGE         OFF CACHE BOOL "" FORCE)

# ######## Official release-specific build options ########
# Options which are specific to Linux release builds only

set(WITH_JACK_DYNLOAD        ON  CACHE BOOL "" FORCE)
set(WITH_PULSEAUDIO_DYNLOAD  ON  CACHE BOOL "" FORCE)
set(WITH_SDL_DYNLOAD         ON  CACHE BOOL "" FORCE)

# ######## Release environment specific settings ########

set(LIBDIR "${CMAKE_CURRENT_LIST_DIR}/../../../../lib/${LIBDIR_NAME}" CACHE STRING "" FORCE)

# Platform specific configuration, to ensure static linking against everything.

# Additional linking libraries
set(CMAKE_EXE_LINKER_FLAGS "-lrt -no-pie"  CACHE STRING "" FORCE)
