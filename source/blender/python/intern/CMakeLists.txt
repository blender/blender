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
# Contributor(s): Jacques Beaurainm, Campbell Barton
#
# ***** END GPL LICENSE BLOCK *****

set(INC 
	..
	../../blenlib
	../../makesdna
	../../makesrna
	../../blenkernel
	../../blenloader
	../../windowmanager
	../../editors/include
	../../../../intern/guardedalloc
	../../../../intern/audaspace/intern
)

set(INC_SYS
	${PYTHON_INCLUDE_DIRS}
)

set(SRC
	bpy.c
	bpy_app.c
	bpy_driver.c
	bpy_interface.c
	bpy_library.c
	bpy_operator.c
	bpy_operator_wrap.c
	bpy_props.c
	bpy_rna.c
	bpy_rna_anim.c
	bpy_rna_array.c
	bpy_rna_callback.c
	bpy_traceback.c
	bpy_util.c
	stubs.c

	bpy.h
	bpy_app.h
	bpy_driver.h
	bpy_operator.h
	bpy_operator_wrap.h
	bpy_props.h
	bpy_rna.h
	bpy_rna_anim.h
	bpy_rna_callback.h
	bpy_traceback.h
	bpy_util.h
	../BPY_extern.h
)

# only to check if buildinfo is available
if(WITH_BUILDINFO)
	add_definitions(-DBUILD_DATE)
endif()

if(WITH_PYTHON_MODULE)
	add_definitions(-DWITH_PYTHON_MODULE)
endif()

if(WITH_PYTHON_SAFETY)
	add_definitions(-DWITH_PYTHON_SAFETY)
endif()

blender_add_lib(bf_python "${SRC}" "${INC}" "${INC_SYS}")
