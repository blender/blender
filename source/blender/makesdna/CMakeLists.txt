# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

add_subdirectory(intern)
