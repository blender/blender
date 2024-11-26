# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/config/blender_release.cmake")

set(WITH_CYCLES_TEST_OSL     ON CACHE BOOL "" FORCE)

set(HIPRT_COMPILER_PARALLEL_JOBS        6 CACHE STRING "" FORCE)
set(SYCL_OFFLINE_COMPILER_PARALLEL_JOBS 6 CACHE STRING "" FORCE)
