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
  ../uvedit
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../gpu
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../render/extern/include
  ../../windowmanager
  ../../../../intern/clog
  ../../../../intern/glew-mx
  ../../../../intern/guardedalloc
)

set(INC_SYS
  ${GLEW_INCLUDE_PATH}
)

set(SRC
  editface.c
  editmesh_add.c
  editmesh_add_gizmo.c
  editmesh_bevel.c
  editmesh_bisect.c
  editmesh_extrude.c
  editmesh_extrude_screw.c
  editmesh_extrude_spin.c
  editmesh_extrude_spin_gizmo.c
  editmesh_inset.c
  editmesh_intersect.c
  editmesh_knife.c
  editmesh_knife_project.c
  editmesh_loopcut.c
  editmesh_path.c
  editmesh_polybuild.c
  editmesh_preselect_edgering.c
  editmesh_preselect_elem.c
  editmesh_rip.c
  editmesh_rip_edge.c
  editmesh_select.c
  editmesh_select_similar.c
  editmesh_tools.c
  editmesh_undo.c
  editmesh_utils.c
  mesh_data.c
  mesh_mirror.c
  mesh_ops.c
  meshtools.c

  mesh_intern.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
)

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_BULLET)
  add_definitions(-DWITH_BULLET)
endif()

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_editor_mesh "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
