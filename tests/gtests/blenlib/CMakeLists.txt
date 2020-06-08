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
# The Original Code is Copyright (C) 2014, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  ..
  ../../../source/blender/blenlib
  ../../../source/blender/makesdna
  ../../../intern/guardedalloc
  ../../../intern/atomic
)

setup_libdirs()
include_directories(${INC})

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${PLATFORM_LINKFLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${PLATFORM_LINKFLAGS_DEBUG}")

if(WIN32)
  set(BLI_path_util_extra_libs "bf_blenlib;bf_intern_utfconv;extern_wcwidth;${ZLIB_LIBRARIES}")
else()
  set(BLI_path_util_extra_libs "bf_blenlib;extern_wcwidth;${ZLIB_LIBRARIES}")
endif()

BLENDER_TEST(BLI_array "bf_blenlib")
BLENDER_TEST(BLI_array_ref "bf_blenlib")
BLENDER_TEST(BLI_array_store "bf_blenlib")
BLENDER_TEST(BLI_array_utils "bf_blenlib")
BLENDER_TEST(BLI_delaunay_2d "bf_blenlib")
BLENDER_TEST(BLI_edgehash "bf_blenlib")
BLENDER_TEST(BLI_expr_pylike_eval "bf_blenlib")
BLENDER_TEST(BLI_ghash "bf_blenlib")
BLENDER_TEST(BLI_hash_mm2a "bf_blenlib")
BLENDER_TEST(BLI_heap "bf_blenlib")
BLENDER_TEST(BLI_heap_simple "bf_blenlib")
BLENDER_TEST(BLI_index_mask "bf_blenlib")
BLENDER_TEST(BLI_index_range "bf_blenlib")
BLENDER_TEST(BLI_kdopbvh "bf_blenlib;bf_intern_numaapi")
BLENDER_TEST(BLI_linear_allocator "bf_blenlib")
BLENDER_TEST(BLI_linklist_lockfree "bf_blenlib;bf_intern_numaapi")
BLENDER_TEST(BLI_listbase "bf_blenlib")
BLENDER_TEST(BLI_map "bf_blenlib")
BLENDER_TEST(BLI_math_base "bf_blenlib")
BLENDER_TEST(BLI_math_color "bf_blenlib")
BLENDER_TEST(BLI_math_geom "bf_blenlib")
BLENDER_TEST(BLI_math_vector "bf_blenlib")
BLENDER_TEST(BLI_memiter "bf_blenlib")
BLENDER_TEST(BLI_optional "bf_blenlib")
BLENDER_TEST(BLI_path_util "${BLI_path_util_extra_libs}")
BLENDER_TEST(BLI_polyfill_2d "bf_blenlib")
BLENDER_TEST(BLI_set "bf_blenlib")
BLENDER_TEST(BLI_stack "bf_blenlib")
BLENDER_TEST(BLI_stack_cxx "bf_blenlib")
BLENDER_TEST(BLI_string "bf_blenlib")
BLENDER_TEST(BLI_string_map "bf_blenlib")
BLENDER_TEST(BLI_string_ref "bf_blenlib")
BLENDER_TEST(BLI_string_utf8 "bf_blenlib")
BLENDER_TEST(BLI_task "bf_blenlib;bf_intern_numaapi")
BLENDER_TEST(BLI_task_graph "bf_blenlib;bf_intern_numaapi")
BLENDER_TEST(BLI_vector "bf_blenlib")
BLENDER_TEST(BLI_vector_set "bf_blenlib")

BLENDER_TEST_PERFORMANCE(BLI_ghash_performance "bf_blenlib")
BLENDER_TEST_PERFORMANCE(BLI_task_performance "bf_blenlib")

unset(BLI_path_util_extra_libs)
