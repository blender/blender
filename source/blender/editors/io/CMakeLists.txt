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
# ***** END GPL LICENSE BLOCK *****

set(INC
	../include
	../../blenkernel
	../../blenlib
	../../blentranslation
	../../bmesh
	../../depsgraph
	../../makesdna
	../../makesrna
	../../windowmanager
	../../collada
	../../alembic
	../../../../intern/guardedalloc
)

set(INC_SYS

)

set(SRC
	io_alembic.c
	io_cache.c
	io_collada.c
	io_ops.c

	io_alembic.h
	io_cache.h
	io_collada.h
	io_ops.h
)

if(WITH_OPENCOLLADA)
	add_definitions(-DWITH_COLLADA)
endif()

if(WITH_ALEMBIC)
	add_definitions(-DWITH_ALEMBIC)

	if(WITH_ALEMBIC_HDF5)
		add_definitions(-DWITH_ALEMBIC_HDF5)
	endif()
endif()

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

blender_add_lib(bf_editor_io "${SRC}" "${INC}" "${INC_SYS}")
