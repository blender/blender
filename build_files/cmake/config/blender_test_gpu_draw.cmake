# SPDX-FileCopyrightText: 2019-2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Configuration for building with EEVEE, Workbench, Grease Pencil, Compositor, GPU module and UI tests.
#
# Example usage:
#   cmake -C../blender/build_files/cmake/config/blender_test_gpu_draw.cmake  ../blender
#

set(WITH_GTESTS                       ON  CACHE BOOL "" FORCE)
set(WITH_GPU_BACKEND_TESTS            ON  CACHE BOOL "" FORCE)
set(WITH_GPU_DRAW_TESTS               ON  CACHE BOOL "" FORCE)
set(WITH_GPU_RENDER_TESTS             ON  CACHE BOOL "" FORCE)
set(WITH_GPU_RENDER_TESTS_HEADED      ON  CACHE BOOL "" FORCE)
set(WITH_GPU_COMPOSITOR_TESTS         ON  CACHE BOOL "" FORCE)
set(WITH_UI_TESTS                     ON  CACHE BOOL "" FORCE)
set(WITH_RENDERDOC                    ON  CACHE BOOL "" FORCE)
