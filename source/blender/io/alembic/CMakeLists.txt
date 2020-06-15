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
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../bmesh
  ../../depsgraph
  ../../editors/include
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/guardedalloc
  ../../../../intern/utfconv
)

set(INC_SYS
  ${ALEMBIC_INCLUDE_DIRS}
  ${BOOST_INCLUDE_DIR}
  ${OPENEXR_INCLUDE_DIRS}
)

set(SRC
  intern/abc_axis_conversion.cc
  intern/abc_customdata.cc
  intern/abc_exporter.cc
  intern/abc_reader_archive.cc
  intern/abc_reader_camera.cc
  intern/abc_reader_curves.cc
  intern/abc_reader_mesh.cc
  intern/abc_reader_nurbs.cc
  intern/abc_reader_object.cc
  intern/abc_reader_points.cc
  intern/abc_reader_transform.cc
  intern/abc_util.cc
  intern/abc_writer_archive.cc
  intern/abc_writer_camera.cc
  intern/abc_writer_curves.cc
  intern/abc_writer_hair.cc
  intern/abc_writer_mball.cc
  intern/abc_writer_mesh.cc
  intern/abc_writer_nurbs.cc
  intern/abc_writer_object.cc
  intern/abc_writer_points.cc
  intern/abc_writer_transform.cc
  intern/alembic_capi.cc

  ABC_alembic.h
  intern/abc_axis_conversion.h
  intern/abc_customdata.h
  intern/abc_exporter.h
  intern/abc_reader_archive.h
  intern/abc_reader_camera.h
  intern/abc_reader_curves.h
  intern/abc_reader_mesh.h
  intern/abc_reader_nurbs.h
  intern/abc_reader_object.h
  intern/abc_reader_points.h
  intern/abc_reader_transform.h
  intern/abc_util.h
  intern/abc_writer_archive.h
  intern/abc_writer_camera.h
  intern/abc_writer_curves.h
  intern/abc_writer_hair.h
  intern/abc_writer_mball.h
  intern/abc_writer_mesh.h
  intern/abc_writer_nurbs.h
  intern/abc_writer_object.h
  intern/abc_writer_points.h
  intern/abc_writer_transform.h
)

set(LIB
  bf_blenkernel
  bf_blenlib

  ${ALEMBIC_LIBRARIES}
  ${OPENEXR_LIBRARIES}
)

list(APPEND LIB
  ${BOOST_LIBRARIES}
)

blender_add_lib(bf_alembic "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
