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
# The Original Code is Copyright (C) 2019 by Blender Foundation.
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  ..
  ../../../source/blender/blenkernel
  ../../../source/blender/blenlib
  ../../../source/blender/blenloader
  ../../../source/blender/depsgraph
  ../../../source/blender/imbuf
  ../../../source/blender/makesdna
  ../../../source/blender/makesrna
  ../../../source/blender/windowmanager
  ../../../intern/guardedalloc
  ${GLOG_INCLUDE_DIRS}
  ${GFLAGS_INCLUDE_DIRS}
  ../../../extern/gtest/include
)

set(SRC
  blendfile_loading_base_test.cc
  blendfile_loading_base_test.h
)

set(LIB
)

blender_add_lib(bf_blenloader_test "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")


set(INC
  .
  ..
  ../../../source/blender/blenlib
  ../../../source/blender/blenloader
  ../../../source/blender/blenkernel
  ../../../source/blender/makesdna
  ../../../source/blender/makesrna
  ../../../source/blender/depsgraph
  ../../../intern/guardedalloc
)

set(LIB
  bf_blenloader_test
  bf_blenloader

  # Should not be needed but gives windows linker errors if the ocio libs are linked before this:
  bf_intern_opencolorio
  bf_gpu
)

include_directories(${INC})

setup_libdirs()
get_property(BLENDER_SORTED_LIBS GLOBAL PROPERTY BLENDER_SORTED_LIBS_PROP)


set(SRC
  blendfile_load_test.cc
)
if(WITH_BUILDINFO)
  list(APPEND SRC
    "$<TARGET_OBJECTS:buildinfoobj>"
  )
endif()

BLENDER_SRC_GTEST_EX(
  NAME blenloader
  SRC "${SRC}"
  EXTRA_LIBS "${LIB}"
  COMMAND_ARGS --test-assets-dir "${CMAKE_SOURCE_DIR}/../lib/tests")

unset(_buildinfo_src)

setup_liblinks(blenloader_test)
