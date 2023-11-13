# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WITH_PYTHON_MODULE)
  add_definitions(-DPy_ENABLE_SHARED)
endif()

add_subdirectory(intern)
add_subdirectory(generic)
add_subdirectory(gpu)
add_subdirectory(mathutils)
add_subdirectory(bmesh)
