# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

if(WITH_LEGACY_OPENGL)
  add_definitions(-DWITH_LEGACY_OPENGL)
endif()

if(WITH_CLANG_TIDY)
  if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(WARNING "Currently Clang-Tidy might fail with GCC toolchain, switch to Clang toolchain if that happens")
  endif()

  find_package(ClangTidy REQUIRED)
  set(CMAKE_C_CLANG_TIDY ${CLANG_TIDY_EXECUTABLE})
  set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXECUTABLE})
endif()

add_subdirectory(blender)
