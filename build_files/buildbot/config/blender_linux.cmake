# SPDX-FileCopyrightText: 2015-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# ######## Global feature set settings ########

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/config/blender_release.cmake")

message(STATUS "Building in Rocky 8 Linux 64bit environment")

# ######## Linux-specific build options ########
# Options which are specific to Linux-only platforms

set(WITH_DOC_MANPAGE         OFF CACHE BOOL "" FORCE)
