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

set(INC 
	.
	../blenkernel
	../blenloader
	../gpu
	../makesdna
	../../../intern/ghost
	../../../intern/guardedalloc
)

set(INC_SYS
	${ZLIB_INCLUDE_DIRS}
	${FREETYPE_INCLUDE_DIRS}
)

set(SRC
	intern/BLI_args.c
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
	intern/bpath.c
	intern/callbacks.c
	intern/cpu.c
	intern/dynlib.c
	intern/edgehash.c
	intern/fileops.c
	intern/fnmatch.c
	intern/freetypefont.c
	intern/graph.c
	intern/gsqueue.c
	intern/jitter.c
	intern/listbase.c
	intern/math_base.c
	intern/math_base_inline.c
	intern/math_color.c
	intern/math_color_inline.c
	intern/math_geom.c
	intern/math_geom_inline.c
	intern/math_matrix.c
	intern/math_rotation.c
	intern/math_vector.c
	intern/math_vector_inline.c
	intern/md5.c
	intern/noise.c
	intern/path_util.c
	intern/pbvh.c
	intern/rand.c
	intern/rct.c
	intern/scanfill.c
	intern/smallhash.c
	intern/storage.c
	intern/string.c
	intern/string_cursor_utf8.c
	intern/string_utf8.c
	intern/threads.c
	intern/time.c
	intern/uvproject.c
	intern/voxel.c
	intern/winstuff.c

	BLI_array.h
	BLI_bitmap.h
	BLI_smallhash.h
	BLI_args.h
	BLI_blenlib.h
	BLI_boxpack2d.h
	BLI_bpath.h
	BLI_callbacks.h
	BLI_cpu.h
	BLI_dlrbTree.h
	BLI_dynlib.h
	BLI_dynstr.h
	BLI_edgehash.h
	BLI_editVert.h
	BLI_fileops.h
	BLI_fileops_types.h
	BLI_fnmatch.h
	BLI_ghash.h
	BLI_graph.h
	BLI_gsqueue.h
	BLI_heap.h
	BLI_jitter.h
	BLI_kdopbvh.h
	BLI_kdtree.h
	BLI_linklist.h
	BLI_listbase.h
	BLI_math.h
	BLI_math_base.h
	BLI_math_color.h
	BLI_math_geom.h
	BLI_math_inline.h
	BLI_math_matrix.h
	BLI_math_rotation.h
	BLI_math_vector.h
	BLI_md5.h
	BLI_memarena.h
	BLI_mempool.h
	BLI_noise.h
	BLI_path_util.h
	BLI_pbvh.h
	BLI_rand.h
	BLI_rect.h
	BLI_scanfill.h
	BLI_string.h
	BLI_string_cursor_utf8.h
	BLI_string_utf8.h
	BLI_threads.h
	BLI_utildefines.h
	BLI_uvproject.h
	BLI_vfontdata.h
	BLI_voxel.h
	BLI_winstuff.h
	PIL_time.h
)

if(WITH_BINRELOC)
	list(APPEND INC_SYS
		${BINRELOC_INCLUDE_DIRS}
	)
	add_definitions(-DWITH_BINRELOC)
endif()

if(WITH_OPENMP)
	add_definitions(-DPARALLEL=1)
endif()

if(WITH_XDG_USER_DIRS)
	add_definitions(-DWITH_XDG_USER_DIRS)
endif()

blender_add_lib(bf_blenlib "${SRC}" "${INC}" "${INC_SYS}")
