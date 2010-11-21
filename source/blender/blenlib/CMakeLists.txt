# $Id$
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

SET(INC 
	.
	../makesdna
	../blenkernel
	../gpu
	../../../intern/ghost
	../../../intern/guardedalloc
	${ZLIB_INC}
	${FREETYPE_INCLUDE_DIRS}
)

SET(SRC
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
	intern/math_geom.c
	intern/math_geom_inline.c
	intern/math_matrix.c
	intern/math_rotation.c
	intern/math_vector.c
	intern/math_vector_inline.c
	intern/noise.c
	intern/path_util.c
	intern/pbvh.c
	intern/rand.c
	intern/rct.c
	intern/scanfill.c
	intern/storage.c
	intern/string.c
	intern/threads.c
	intern/time.c
	intern/uvproject.c
	intern/voxel.c
	intern/winstuff.c
)

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
	LIST(APPEND INC "${BINRELOC_INC}")
ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")

IF(WITH_OPENMP)
	ADD_DEFINITIONS(-DPARALLEL=1)
ENDIF(WITH_OPENMP)

BLENDERLIB(bf_blenlib "${SRC}" "${INC}")
