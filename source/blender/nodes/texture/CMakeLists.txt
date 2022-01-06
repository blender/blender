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
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  ../
  ../intern
  ../../editors/include
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../depsgraph
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../render
  ../../windowmanager
  ../../../../intern/guardedalloc
)


set(SRC
  nodes/node_texture_at.c
  nodes/node_texture_bricks.c
  nodes/node_texture_checker.c
  nodes/node_texture_common.c
  nodes/node_texture_compose.c
  nodes/node_texture_coord.c
  nodes/node_texture_curves.c
  nodes/node_texture_decompose.c
  nodes/node_texture_distance.c
  nodes/node_texture_hueSatVal.c
  nodes/node_texture_image.c
  nodes/node_texture_invert.c
  nodes/node_texture_math.c
  nodes/node_texture_mixRgb.c
  nodes/node_texture_output.c
  nodes/node_texture_proc.c
  nodes/node_texture_rotate.c
  nodes/node_texture_scale.c
  nodes/node_texture_texture.c
  nodes/node_texture_translate.c
  nodes/node_texture_valToNor.c
  nodes/node_texture_valToRgb.c
  nodes/node_texture_viewer.c
  node_texture_tree.c
  node_texture_util.c

  node_texture_util.h
)

set(LIB
  bf_nodes
)

if(WITH_PYTHON)
  list(APPEND INC
    ../../python
  )
  list(APPEND INC_SYS
    ${PYTHON_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${PYTHON_LINKFLAGS}
    ${PYTHON_LIBRARIES}
  )
  add_definitions(-DWITH_PYTHON)
endif()

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

blender_add_lib(bf_nodes_texture "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
