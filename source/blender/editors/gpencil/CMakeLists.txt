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
  ../../depsgraph
  ../../gpu
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/glew-mx
  ../../../../intern/guardedalloc
)

set(SRC
  annotate_draw.c
  annotate_paint.c
  drawgpencil.c
  editaction_gpencil.c
  gpencil_add_lineart.c
  gpencil_add_monkey.c
  gpencil_add_stroke.c
  gpencil_armature.c
  gpencil_convert.c
  gpencil_data.c
  gpencil_edit.c
  gpencil_edit_curve.c
  gpencil_fill.c
  gpencil_interpolate.c
  gpencil_merge.c
  gpencil_mesh.c
  gpencil_ops.c
  gpencil_ops_versioning.c
  gpencil_paint.c
  gpencil_primitive.c
  gpencil_sculpt_paint.c
  gpencil_select.c
  gpencil_undo.c
  gpencil_utils.c
  gpencil_uv.c
  gpencil_vertex_ops.c
  gpencil_vertex_paint.c
  gpencil_weight_paint.c

  gpencil_intern.h
  gpencil_trace.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
)

if(WITH_POTRACE)
  list(APPEND SRC
    gpencil_trace_ops.c
    gpencil_trace_utils.c
  )
  list(APPEND INC
    ${POTRACE_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${POTRACE_LIBRARIES}
  )
  add_definitions(-DWITH_POTRACE)
endif()

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()


blender_add_lib(bf_editor_gpencil "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
