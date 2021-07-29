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
	../atomic
)

set(INC_SYS

)

set(SRC
	./intern/mallocn.c
	./intern/mallocn_guarded_impl.c
	./intern/mallocn_lockfree_impl.c

	MEM_guardedalloc.h
	./intern/mallocn_intern.h

	# only so the header is known by cmake
	../atomic/atomic_ops.h
)

if(WIN32 AND NOT UNIX)
	list(APPEND SRC
		intern/mmap_win.c

		mmap_win.h
	)
endif()

blender_add_lib(bf_intern_guardedalloc "${SRC}" "${INC}" "${INC_SYS}")

# Override C++ alloc, optional.
if(WITH_CXX_GUARDEDALLOC)
	set(SRC
		cpp/mallocn.cpp
	)
	blender_add_lib(bf_intern_guardedalloc_cpp "${SRC}" "${INC}" "${INC_SYS}")
endif()
