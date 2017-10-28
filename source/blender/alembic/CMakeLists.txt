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
# Contributor(s): Kevin Dietrich.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	../blenkernel
	../blenlib
	../blenloader
	../bmesh
	../editors/include
	../makesdna
	../makesrna
	../windowmanager
	../depsgraph
	../../../intern/guardedalloc
	../../../intern/utfconv
)

set(INC_SYS
	${ALEMBIC_INCLUDE_DIRS}
	${BOOST_INCLUDE_DIR}
	${HDF5_INCLUDE_DIRS}
	${OPENEXR_INCLUDE_DIRS}
)

set(SRC
	intern/abc_archive.cc
	intern/abc_camera.cc
	intern/abc_customdata.cc
	intern/abc_curves.cc
	intern/abc_exporter.cc
	intern/abc_hair.cc
	intern/abc_mball.cc
	intern/abc_mesh.cc
	intern/abc_nurbs.cc
	intern/abc_object.cc
	intern/abc_points.cc
	intern/abc_transform.cc
	intern/abc_util.cc
	intern/alembic_capi.cc

	ABC_alembic.h
	intern/abc_archive.h
	intern/abc_camera.h
	intern/abc_customdata.h
	intern/abc_curves.h
	intern/abc_exporter.h
	intern/abc_hair.h
	intern/abc_mball.h
	intern/abc_mesh.h
	intern/abc_nurbs.h
	intern/abc_object.h
	intern/abc_points.h
	intern/abc_transform.h
	intern/abc_util.h
)

if(WITH_ALEMBIC_HDF5)
	add_definitions(-DWITH_ALEMBIC_HDF5)
endif()

blender_add_lib(bf_alembic "${SRC}" "${INC}" "${INC_SYS}")
