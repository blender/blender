# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2006 Blender Foundation. All rights reserved.

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

add_subdirectory(intern)
