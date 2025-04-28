# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/config/blender_release.cmake")

set(WITH_CYCLES_TEST_OSL     ON CACHE BOOL "" FORCE)
set(WITH_LEGACY_MACOS_X64_LINKER ON CACHE BOOL "" FORCE)
