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
  ../../draw
  ../../gpu
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../render/extern/include
  ../../windowmanager
  ../../../../intern/atomic
  ../../../../intern/clog
  ../../../../intern/glew-mx
  ../../../../intern/guardedalloc
)

set(INC_SYS
  ${GLEW_INCLUDE_PATH}
)

set(SRC
  paint_cursor.c
  paint_curve.c
  paint_curve_undo.c
  paint_hide.c
  paint_image.c
  paint_image_2d.c
  paint_image_proj.c
  paint_mask.c
  paint_ops.c
  paint_stroke.c
  paint_utils.c
  paint_vertex.c
  paint_vertex_color_ops.c
  paint_vertex_color_utils.c
  paint_vertex_proj.c
  paint_vertex_weight_ops.c
  paint_vertex_weight_utils.c
  sculpt.c
  sculpt_automasking.c
  sculpt_cloth.c
  sculpt_detail.c
  sculpt_dyntopo.c
  sculpt_face_set.c
  sculpt_filter_color.c
  sculpt_filter_mask.c
  sculpt_filter_mesh.c
  sculpt_mask_expand.c
  sculpt_multiplane_scrape.c
  sculpt_paint_color.c
  sculpt_pose.c
  sculpt_smooth.c
  sculpt_transform.c
  sculpt_undo.c
  sculpt_uv.c

  paint_intern.h
  sculpt_intern.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
)

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_editor_sculpt_paint "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
