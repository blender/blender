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

set(INC
  .
  # ../blenkernel  # don't add this back!
  ../makesdna
  ../../../intern/atomic
  ../../../intern/eigen
  ../../../intern/guardedalloc
  ../../../intern/numaapi/include
  ../../../extern/wcwidth
  ../../../extern/json/include
)

set(INC_SYS
  ${ZLIB_INCLUDE_DIRS}
  ${ZSTD_INCLUDE_DIRS}
  ${GMP_INCLUDE_DIRS}
)

set(SRC
  intern/BLI_args.c
  intern/BLI_array.c
  intern/BLI_assert.c
  intern/BLI_color.cc
  intern/BLI_dial_2d.c
  intern/BLI_dynstr.c
  intern/BLI_filelist.c
  intern/BLI_ghash.c
  intern/BLI_ghash_utils.c
  intern/BLI_heap.c
  intern/BLI_heap_simple.c
  intern/BLI_index_range.cc
  intern/BLI_kdopbvh.c
  intern/BLI_linklist.c
  intern/BLI_linklist_lockfree.c
  intern/BLI_memarena.c
  intern/BLI_memblock.c
  intern/BLI_memiter.c
  intern/BLI_mempool.c
  intern/BLI_mmap.c
  intern/BLI_timer.c
  intern/DLRB_tree.c
  intern/array_store.c
  intern/array_store_utils.c
  intern/array_utils.c
  intern/astar.c
  intern/bitmap.c
  intern/bitmap_draw_2d.c
  intern/boxpack_2d.c
  intern/buffer.c
  intern/convexhull_2d.c
  intern/delaunay_2d.cc
  intern/dot_export.cc
  intern/dynlib.c
  intern/easing.c
  intern/edgehash.c
  intern/endian_switch.c
  intern/expr_pylike_eval.c
  intern/fileops.c
  intern/fileops.cc
  intern/filereader_file.c
  intern/filereader_gzip.c
  intern/filereader_memory.c
  intern/filereader_zstd.c
  intern/fnmatch.c
  intern/gsqueue.c
  intern/hash_md5.c
  intern/hash_mm2a.c
  intern/hash_mm3.c
  intern/index_mask.cc
  intern/jitter_2d.c
  intern/kdtree_1d.c
  intern/kdtree_2d.c
  intern/kdtree_3d.c
  intern/kdtree_4d.c
  intern/lasso_2d.c
  intern/listbase.c
  intern/math_base.c
  intern/math_base_inline.c
  intern/math_base_safe_inline.c
  intern/math_bits_inline.c
  intern/math_boolean.cc
  intern/math_color.c
  intern/math_color_blend_inline.c
  intern/math_color_inline.c
  intern/math_geom.c
  intern/math_geom_inline.c
  intern/math_interp.c
  intern/math_matrix.c
  intern/math_rotation.c
  intern/math_solvers.c
  intern/math_statistics.c
  intern/math_time.c
  intern/math_vec.cc
  intern/math_vector.c
  intern/math_vector_inline.c
  intern/memory_utils.c
  intern/mesh_boolean.cc
  intern/mesh_intersect.cc
  intern/noise.c
  intern/noise.cc
  intern/path_util.c
  intern/polyfill_2d.c
  intern/polyfill_2d_beautify.c
  intern/quadric.c
  intern/rand.cc
  intern/rct.c
  intern/resource_scope.cc
  intern/scanfill.c
  intern/scanfill_utils.c
  intern/session_uuid.c
  intern/serialize.cc
  intern/smallhash.c
  intern/sort.c
  intern/sort_utils.c
  intern/stack.c
  intern/storage.c
  intern/string.c
  intern/string_cursor_utf8.c
  intern/string_search.cc
  intern/string_utf8.c
  intern/string_utils.c
  intern/system.c
  intern/task_graph.cc
  intern/task_iterator.c
  intern/task_pool.cc
  intern/task_range.cc
  intern/task_scheduler.cc
  intern/threads.cc
  intern/time.c
  intern/timecode.c
  intern/timeit.cc
  intern/uuid.cc
  intern/uvproject.c
  intern/voronoi_2d.c
  intern/voxel.c
  intern/winstuff.c
  intern/winstuff_dir.c

  # Private headers.
  intern/BLI_mempool_private.h

  # Header as source (included in C files above).
  intern/kdtree_impl.h
  intern/list_sort_impl.h


  BLI_alloca.h
  BLI_allocator.hh
  BLI_any.hh
  BLI_args.h
  BLI_array.h
  BLI_array.hh
  BLI_array_store.h
  BLI_array_store_utils.h
  BLI_array_utils.h
  BLI_asan.h
  BLI_assert.h
  BLI_astar.h
  BLI_bitmap.h
  BLI_bitmap_draw_2d.h
  BLI_blenlib.h
  BLI_boxpack_2d.h
  BLI_buffer.h
  BLI_color.hh
  BLI_compiler_attrs.h
  BLI_compiler_compat.h
  BLI_compiler_typecheck.h
  BLI_console.h
  BLI_convexhull_2d.h
  BLI_delaunay_2d.h
  BLI_dial_2d.h
  BLI_disjoint_set.hh
  BLI_dlrbTree.h
  BLI_dot_export.hh
  BLI_dot_export_attribute_enums.hh
  BLI_double2.hh
  BLI_double3.hh
  BLI_dynlib.h
  BLI_dynstr.h
  BLI_easing.h
  BLI_edgehash.h
  BLI_endian_defines.h
  BLI_endian_switch.h
  BLI_endian_switch_inline.h
  BLI_enumerable_thread_specific.hh
  BLI_expr_pylike_eval.h
  BLI_fileops.h
  BLI_fileops.hh
  BLI_fileops_types.h
  BLI_filereader.h
  BLI_float2.hh
  BLI_float3.hh
  BLI_float4.hh
  BLI_float4x4.hh
  BLI_fnmatch.h
  BLI_function_ref.hh
  BLI_ghash.h
  BLI_gsqueue.h
  BLI_hash.h
  BLI_hash.hh
  BLI_hash_md5.h
  BLI_hash_mm2a.h
  BLI_hash_mm3.h
  BLI_hash_tables.hh
  BLI_heap.h
  BLI_heap_simple.h
  BLI_index_mask.hh
  BLI_index_range.hh
  BLI_inplace_priority_queue.hh
  BLI_iterator.h
  BLI_jitter_2d.h
  BLI_kdopbvh.h
  BLI_kdtree.h
  BLI_kdtree_impl.h
  BLI_lasso_2d.h
  BLI_linear_allocator.hh
  BLI_link_utils.h
  BLI_linklist.h
  BLI_linklist_lockfree.h
  BLI_linklist_stack.h
  BLI_listbase.h
  BLI_listbase_wrapper.hh
  BLI_map.hh
  BLI_map_slots.hh
  BLI_math.h
  BLI_math_base.h
  BLI_math_base_safe.h
  BLI_math_bits.h
  BLI_math_boolean.hh
  BLI_math_color.h
  BLI_math_color_blend.h
  BLI_math_geom.h
  BLI_math_inline.h
  BLI_math_interp.h
  BLI_math_matrix.h
  BLI_math_mpq.hh
  BLI_math_rotation.h
  BLI_math_solvers.h
  BLI_math_statistics.h
  BLI_math_time.h
  BLI_math_vector.h
  BLI_memarena.h
  BLI_memblock.h
  BLI_memiter.h
  BLI_memory_utils.h
  BLI_memory_utils.hh
  BLI_mempool.h
  BLI_mesh_boolean.hh
  BLI_mesh_intersect.hh
  BLI_mmap.h
  BLI_mpq2.hh
  BLI_mpq3.hh
  BLI_multi_value_map.hh
  BLI_noise.h
  BLI_noise.hh
  BLI_path_util.h
  BLI_polyfill_2d.h
  BLI_polyfill_2d_beautify.h
  BLI_probing_strategies.hh
  BLI_quadric.h
  BLI_rand.h
  BLI_rand.hh
  BLI_range.h
  BLI_rect.h
  BLI_resource_scope.hh
  BLI_scanfill.h
  BLI_session_uuid.h
  BLI_set.hh
  BLI_set_slots.hh
  BLI_serialize.hh
  BLI_simd.h
  BLI_smallhash.h
  BLI_sort.h
  BLI_sort_utils.h
  BLI_span.hh
  BLI_stack.h
  BLI_stack.hh
  BLI_strict_flags.h
  BLI_string.h
  BLI_string_cursor_utf8.h
  BLI_string_ref.hh
  BLI_string_search.h
  BLI_string_utf8.h
  BLI_string_utils.h
  BLI_sys_types.h
  BLI_system.h
  BLI_task.h
  BLI_task.hh
  BLI_threads.h
  BLI_timecode.h
  BLI_timeit.hh
  BLI_timer.h
  BLI_user_counter.hh
  BLI_utildefines.h
  BLI_utildefines_iter.h
  BLI_utildefines_stack.h
  BLI_utildefines_variadic.h
  BLI_utility_mixins.hh
  BLI_uuid.h
  BLI_uvproject.h
  BLI_vector.hh
  BLI_vector_adaptor.hh
  BLI_vector_set.hh
  BLI_vector_set_slots.hh
  BLI_virtual_array.hh
  BLI_virtual_vector_array.hh
  BLI_voronoi_2d.h
  BLI_voxel.h
  BLI_winstuff.h
  PIL_time.h
  PIL_time_utildefines.h
)

set(LIB
  bf_intern_eigen
  bf_intern_guardedalloc
  bf_intern_numaapi
  extern_wcwidth

  ${ZLIB_LIBRARIES}
  ${ZSTD_LIBRARIES}
)

if(WITH_MEM_VALGRIND)
  add_definitions(-DWITH_MEM_VALGRIND)
endif()

if(WITH_TBB)
  add_definitions(-DWITH_TBB)

  list(APPEND INC_SYS
    ${TBB_INCLUDE_DIRS}
  )

  list(APPEND LIB
    ${TBB_LIBRARIES}
  )
endif()

if(WITH_GMP)
  add_definitions(-DWITH_GMP)

  list(APPEND INC_SYS
    ${GMP_INCLUDE_DIRS}
  )

  list(APPEND LIB
    ${GMP_LIBRARIES}
  )
endif()

if(WIN32)
  if (WITH_BLENDER_THUMBNAILER)
    # Needed for querying the thumbnailer .dll in winstuff.c
    add_definitions(-DWITH_BLENDER_THUMBNAILER)
  endif()
  list(APPEND INC
    ../../../intern/utfconv
  )
  list(APPEND LIB
    bf_intern_utfconv
  )
  list(APPEND SRC
    intern/system_win32.c
  )
endif()


if(APPLE)
  list(APPEND SRC
    intern/storage_apple.mm
  )
endif()

if(UNIX AND NOT APPLE)
  list(APPEND LIB
    bf_intern_libc_compat
  )
endif()

# no need to compile object files for inline headers.
set_source_files_properties(
  intern/math_base_inline.c
  intern/math_base_safe_inline.c
  intern/math_bits_inline.c
  intern/math_color_blend_inline.c
  intern/math_color_inline.c
  intern/math_geom_inline.c
  intern/math_vector_inline.c
  PROPERTIES HEADER_FILE_ONLY TRUE
)

blender_add_lib(bf_blenlib "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

if(WITH_GTESTS)
  set(TEST_SRC
    tests/BLI_any_test.cc
    tests/BLI_array_store_test.cc
    tests/BLI_array_test.cc
    tests/BLI_array_utils_test.cc
    tests/BLI_color_test.cc
    tests/BLI_delaunay_2d_test.cc
    tests/BLI_disjoint_set_test.cc
    tests/BLI_edgehash_test.cc
    tests/BLI_expr_pylike_eval_test.cc
    tests/BLI_function_ref_test.cc
    tests/BLI_fileops_test.cc
    tests/BLI_ghash_test.cc
    tests/BLI_hash_mm2a_test.cc
    tests/BLI_heap_simple_test.cc
    tests/BLI_heap_test.cc
    tests/BLI_index_mask_test.cc
    tests/BLI_index_range_test.cc
    tests/BLI_inplace_priority_queue_test.cc
    tests/BLI_kdopbvh_test.cc
    tests/BLI_linear_allocator_test.cc
    tests/BLI_linklist_lockfree_test.cc
    tests/BLI_listbase_test.cc
    tests/BLI_map_test.cc
    tests/BLI_math_base_safe_test.cc
    tests/BLI_math_base_test.cc
    tests/BLI_math_bits_test.cc
    tests/BLI_math_color_test.cc
    tests/BLI_math_geom_test.cc
    tests/BLI_math_matrix_test.cc
    tests/BLI_math_rotation_test.cc
    tests/BLI_math_solvers_test.cc
    tests/BLI_math_time_test.cc
    tests/BLI_math_vector_test.cc
    tests/BLI_memiter_test.cc
    tests/BLI_memory_utils_test.cc
    tests/BLI_mesh_boolean_test.cc
    tests/BLI_mesh_intersect_test.cc
    tests/BLI_multi_value_map_test.cc
    tests/BLI_path_util_test.cc
    tests/BLI_polyfill_2d_test.cc
    tests/BLI_ressource_strings.h
    tests/BLI_session_uuid_test.cc
    tests/BLI_set_test.cc
    tests/BLI_span_test.cc
    tests/BLI_stack_cxx_test.cc
    tests/BLI_stack_test.cc
    tests/BLI_serialize_test.cc
    tests/BLI_string_ref_test.cc
    tests/BLI_string_search_test.cc
    tests/BLI_string_test.cc
    tests/BLI_string_utf8_test.cc
    tests/BLI_task_graph_test.cc
    tests/BLI_task_test.cc
    tests/BLI_uuid_test.cc
    tests/BLI_vector_set_test.cc
    tests/BLI_vector_test.cc
    tests/BLI_virtual_array_test.cc

    tests/BLI_exception_safety_test_utils.hh
  )
  set(TEST_INC
    ../imbuf
  )
  set(TEST_LIB
    bf_blenlib
  )
  include(GTestTesting)
  blender_add_test_executable(blenlib "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")

  add_subdirectory(tests/performance)
endif()
