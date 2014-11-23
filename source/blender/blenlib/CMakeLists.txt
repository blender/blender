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
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

# XXX allowing blenkernel and RNA includes in blenlib is a hack,
# but needed in a few places atm (bpath.c for instance)
set(INC
	.
	# ../blenkernel  # dont add this back!
	../makesdna
	../../../intern/guardedalloc
	../../../extern/wcwidth
)

set(INC_SYS
	${ZLIB_INCLUDE_DIRS}
	${FREETYPE_INCLUDE_DIRS}
)

set(SRC
	intern/BLI_args.c
	intern/BLI_array.c
	intern/BLI_dial.c
	intern/BLI_dynstr.c
	intern/BLI_ghash.c
	intern/BLI_heap.c
	intern/BLI_kdopbvh.c
	intern/BLI_kdtree.c
	intern/BLI_linklist.c
	intern/BLI_memarena.c
	intern/BLI_mempool.c
	intern/DLRB_tree.c
	intern/boxpack2d.c
	intern/buffer.c
	intern/callbacks.c
	intern/convexhull2d.c
	intern/dynlib.c
	intern/easing.c
	intern/edgehash.c
	intern/endian_switch.c
	intern/fileops.c
	intern/fnmatch.c
	intern/freetypefont.c
	intern/graph.c
	intern/gsqueue.c
	intern/hash_md5.c
	intern/hash_mm2a.c
	intern/jitter.c
	intern/lasso.c
	intern/listbase.c
	intern/math_base.c
	intern/math_base_inline.c
	intern/math_color.c
	intern/math_color_blend_inline.c
	intern/math_color_inline.c
	intern/math_geom.c
	intern/math_geom_inline.c
	intern/math_interp.c
	intern/math_matrix.c
	intern/math_rotation.c
	intern/math_vector.c
	intern/math_vector_inline.c
	intern/noise.c
	intern/path_util.c
	intern/polyfill2d.c
	intern/quadric.c
	intern/rand.c
	intern/rct.c
	intern/scanfill.c
	intern/scanfill_utils.c
	intern/smallhash.c
	intern/sort.c
	intern/sort_utils.c
	intern/stack.c
	intern/storage.c
	intern/string.c
	intern/string_cursor_utf8.c
	intern/string_utf8.c
	intern/system.c
	intern/task.c
	intern/threads.c
	intern/time.c
	intern/timecode.c
	intern/uvproject.c
	intern/voronoi.c
	intern/voxel.c
	intern/winstuff.c
	intern/winstuff_dir.c

	BLI_alloca.h
	BLI_args.h
	BLI_array.h
	BLI_bitmap.h
	BLI_blenlib.h
	BLI_boxpack2d.h
	BLI_buffer.h
	BLI_callbacks.h
	BLI_compiler_attrs.h
	BLI_compiler_compat.h
	BLI_compiler_typecheck.h
	BLI_convexhull2d.h
	BLI_dial.h
	BLI_dlrbTree.h
	BLI_dynlib.h
	BLI_dynstr.h
	BLI_easing.h
	BLI_edgehash.h
	BLI_endian_switch.h
	BLI_endian_switch_inline.h
	BLI_fileops.h
	BLI_fileops_types.h
	BLI_fnmatch.h
	BLI_ghash.h
	BLI_graph.h
	BLI_gsqueue.h
	BLI_hash_md5.h
	BLI_hash_mm2a.h
	BLI_heap.h
	BLI_jitter.h
	BLI_kdopbvh.h
	BLI_kdtree.h
	BLI_lasso.h
	BLI_link_utils.h
	BLI_linklist.h
	BLI_linklist_stack.h
	BLI_listbase.h
	BLI_math.h
	BLI_math_base.h
	BLI_math_color.h
	BLI_math_color_blend.h
	BLI_math_geom.h
	BLI_math_inline.h
	BLI_math_interp.h
	BLI_math_matrix.h
	BLI_math_rotation.h
	BLI_math_vector.h
	BLI_memarena.h
	BLI_mempool.h
	BLI_noise.h
	BLI_path_util.h
	BLI_polyfill2d.h
	BLI_quadric.h
	BLI_rand.h
	BLI_rect.h
	BLI_scanfill.h
	BLI_smallhash.h
	BLI_sort.h
	BLI_sort_utils.h
	BLI_stack.h
	BLI_stackdefines.h
	BLI_strict_flags.h
	BLI_string.h
	BLI_string_cursor_utf8.h
	BLI_string_utf8.h
	BLI_sys_types.h
	BLI_system.h
	BLI_task.h
	BLI_threads.h
	BLI_timecode.h
	BLI_utildefines.h
	BLI_uvproject.h
	BLI_vfontdata.h
	BLI_voronoi.h
	BLI_voxel.h
	BLI_winstuff.h
	PIL_time.h
	PIL_time_utildefines.h
)

if(WITH_BINRELOC)
	list(APPEND INC_SYS
		${BINRELOC_INCLUDE_DIRS}
	)
	add_definitions(-DWITH_BINRELOC)
endif()

if(WITH_MEM_VALGRIND)
	add_definitions(-DWITH_MEM_VALGRIND)
endif()

if(WIN32)
	list(APPEND INC
		../../../intern/utfconv
	)
endif()

blender_add_lib(bf_blenlib "${SRC}" "${INC}" "${INC_SYS}")

if(MSVC)
	# Quiet warning about inline math library files that do not export symbols.
	# (normally you'd exclude from project, but we still want to see the files in MSVC)
	set_target_properties(bf_blenlib PROPERTIES STATIC_LIBRARY_FLAGS /ignore:4221)
endif()
