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
  ../../blenfont
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../bmesh
  ../../gpu
  ../../ikplugin
  ../../makesdna
  ../../makesrna
  ../../render/extern/include
  ../../depsgraph
  ../../windowmanager
  ../../../../intern/glew-mx
  ../../../../intern/guardedalloc
)

set(INC_SYS
  ${GLEW_INCLUDE_PATH}
)

set(SRC
  transform.c
  transform_constraints.c
  transform_convert.c
  transform_convert_action.c
  transform_convert_armature.c
  transform_convert_cursor.c
  transform_convert_curve.c
  transform_convert_gpencil.c
  transform_convert_graph.c
  transform_convert_lattice.c
  transform_convert_mask.c
  transform_convert_mball.c
  transform_convert_mesh.c
  transform_convert_nla.c
  transform_convert_node.c
  transform_convert_object.c
  transform_convert_paintcurve.c
  transform_convert_particle.c
  transform_convert_sculpt.c
  transform_convert_sequencer.c
  transform_convert_tracking.c
  transform_generics.c
  transform_gizmo_2d.c
  transform_gizmo_3d.c
  transform_gizmo_extrude_3d.c
  transform_input.c
  transform_ops.c
  transform_orientations.c
  transform_snap.c
  transform_snap_object.c

  transform.h
  transform_convert.h
)

set(LIB
  bf_blenfont
  bf_blenkernel
  bf_blenlib
  bf_bmesh
  bf_editor_mask
  bf_gpu
)

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_editor_transform "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
