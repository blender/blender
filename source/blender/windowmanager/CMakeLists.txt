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
  gizmo
  gizmo/intern
  ../blenfont
  ../blenkernel
  ../blenlib
  ../blenloader
  ../blentranslation
  ../compositor
  ../depsgraph
  ../editors/include
  ../draw
  ../gpu
  ../imbuf
  ../makesdna
  ../makesrna
  ../nodes
  ../render/extern/include
  ../../../intern/clog
  ../../../intern/ghost
  ../../../intern/glew-mx
  ../../../intern/guardedalloc
  ../../../intern/memutil
)

set(INC_SYS
  ${ZLIB_INCLUDE_DIRS}
  ${GLEW_INCLUDE_PATH}
)

set(SRC
  intern/wm.c
  intern/wm_cursors.c
  intern/wm_dragdrop.c
  intern/wm_draw.c
  intern/wm_event_system.c
  intern/wm_files.c
  intern/wm_files_link.c
  intern/wm_gesture.c
  intern/wm_gesture_ops.c
  intern/wm_init_exit.c
  intern/wm_jobs.c
  intern/wm_keymap.c
  intern/wm_keymap_utils.c
  intern/wm_menu_type.c
  intern/wm_operator_props.c
  intern/wm_operator_type.c
  intern/wm_operators.c
  intern/wm_panel_type.c
  intern/wm_playanim.c
  intern/wm_stereo.c
  intern/wm_subwindow.c
  intern/wm_toolsystem.c
  intern/wm_tooltip.c
  intern/wm_uilist_type.c
  intern/wm_utils.c
  intern/wm_window.c
  gizmo/intern/wm_gizmo.c
  gizmo/intern/wm_gizmo_group.c
  gizmo/intern/wm_gizmo_group_type.c
  gizmo/intern/wm_gizmo_map.c
  gizmo/intern/wm_gizmo_target_props.c
  gizmo/intern/wm_gizmo_type.c
  message_bus/intern/wm_message_bus.c
  message_bus/intern/wm_message_bus_rna.c
  message_bus/intern/wm_message_bus_static.c

  WM_api.h
  WM_keymap.h
  WM_message.h
  WM_toolsystem.h
  WM_types.h
  wm.h
  wm_cursors.h
  wm_draw.h
  wm_event_system.h
  wm_event_types.h
  wm_files.h
  wm_window.h
  gizmo/WM_gizmo_api.h
  gizmo/WM_gizmo_types.h
  gizmo/wm_gizmo_fn.h
  gizmo/wm_gizmo_wmapi.h
  gizmo/intern/wm_gizmo_intern.h
  message_bus/intern/wm_message_bus_intern.h
  message_bus/wm_message_bus.h
)

set(LIB
  bf_editor_screen
)

if(WITH_AUDASPACE)
  add_definitions(-DWITH_AUDASPACE)

  list(APPEND INC_SYS
    ${AUDASPACE_C_INCLUDE_DIRS}
  )
endif()

add_definitions(${GL_DEFINITIONS})

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_OPENCOLLADA)
  add_definitions(-DWITH_COLLADA)
endif()

if(WITH_CODEC_FFMPEG)
  list(APPEND INC_SYS
    ${FFMPEG_INCLUDE_DIRS}
  )
  add_definitions(-DWITH_FFMPEG)
endif()

if(WITH_HEADLESS)
  add_definitions(-DWITH_HEADLESS)
elseif(WITH_X11)
  add_definitions(-DWITH_X11)
endif()

if(WITH_PYTHON)
  list(APPEND INC
    ../python
  )
  add_definitions(-DWITH_PYTHON)
endif()

if(WITH_BUILDINFO)
  add_definitions(-DWITH_BUILDINFO)
endif()

if(WITH_OPENSUBDIV)
  add_definitions(-DWITH_OPENSUBDIV)
endif()

if(WITH_INPUT_NDOF)
  add_definitions(-DWITH_INPUT_NDOF)
endif()

if(WIN32)
  if(WITH_INPUT_IME)
    add_definitions(-DWITH_INPUT_IME)
  endif()
endif()

if(WITH_COMPOSITOR)
  list(APPEND LIB
    bf_compositor
  )
  add_definitions(-DWITH_COMPOSITOR)
endif()

blender_add_lib_nolist(bf_windowmanager "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
