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
  ../../gpu
  ../../depsgraph
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/atomic
  ../../../../intern/glew-mx
  ../../../../intern/guardedalloc
)

set(INC_SYS
  ${GLEW_INCLUDE_PATH}
)

set(SRC
  sequencer_add.c
  sequencer_buttons.c
  sequencer_draw.c
  sequencer_edit.c
  sequencer_modifier.c
  sequencer_ops.c
  sequencer_preview.c
  sequencer_scopes.c
  sequencer_select.c
  sequencer_view.c
  space_sequencer.c

  sequencer_intern.h
)

set(LIB
  bf_editor_interface
  bf_editor_util
)

if(WITH_AUDASPACE)
  add_definitions(-DWITH_AUDASPACE)

  list(APPEND INC_SYS
    ${AUDASPACE_C_INCLUDE_DIRS}
  )
endif()

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_editor_space_sequencer "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
