# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  gizmo
  gizmo/intern
  ../asset_system
  ../blenkernel
  ../blenloader
  ../blentranslation
  ../compositor
  ../editors/include
  ../draw
  ../gpu
  ../imbuf
  ../makesrna
  ../nodes
  ../render
  ../sequencer
  ../../../intern/memutil
  ../bmesh

  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)
set(INC_SYS
)

set(SRC
  intern/wm.cc
  intern/wm_cursors.cc
  intern/wm_dragdrop.cc
  intern/wm_draw.cc
  intern/wm_event_query.cc
  intern/wm_event_system.cc
  intern/wm_files.cc
  intern/wm_files_link.cc
  intern/wm_gesture.cc
  intern/wm_gesture_ops.cc
  intern/wm_init_exit.cc
  intern/wm_jobs.cc
  intern/wm_keymap.cc
  intern/wm_keymap_utils.cc
  intern/wm_menu_type.cc
  intern/wm_operator_props.cc
  intern/wm_operator_type.cc
  intern/wm_operator_utils.cc
  intern/wm_operators.cc
  intern/wm_panel_type.cc
  intern/wm_platform_support.cc
  intern/wm_playanim.cc
  intern/wm_splash_screen.cc
  intern/wm_stereo.cc
  intern/wm_subwindow.cc
  intern/wm_surface.cc
  intern/wm_toolsystem.cc
  intern/wm_tooltip.cc
  intern/wm_uilist_type.cc
  intern/wm_utils.cc
  intern/wm_window.cc
  gizmo/intern/wm_gizmo.cc
  gizmo/intern/wm_gizmo_group.cc
  gizmo/intern/wm_gizmo_group_type.cc
  gizmo/intern/wm_gizmo_map.cc
  gizmo/intern/wm_gizmo_target_props.cc
  gizmo/intern/wm_gizmo_type.cc
  message_bus/intern/wm_message_bus.cc
  message_bus/intern/wm_message_bus_rna.cc
  message_bus/intern/wm_message_bus_static.cc

  WM_api.hh
  WM_keymap.hh
  WM_message.hh
  WM_toolsystem.hh
  WM_types.hh
  wm.hh
  wm_cursors.hh
  wm_draw.hh
  wm_event_system.hh
  wm_event_types.hh
  wm_files.hh
  wm_surface.hh
  wm_window.hh
  intern/wm_platform_support.hh
  intern/wm_window_private.hh
  gizmo/WM_gizmo_api.hh
  gizmo/WM_gizmo_types.hh
  gizmo/wm_gizmo_fn.hh
  gizmo/wm_gizmo_wmapi.hh
  gizmo/intern/wm_gizmo_intern.hh
  message_bus/intern/wm_message_bus_intern.hh
  message_bus/wm_message_bus.hh
)

set(LIB
  PRIVATE bf::blenfont
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  bf_editor_screen
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::animrig
  bf_sequencer
  bf_intern_ghost
)

if(WIN32)
  add_definitions(-DNOMINMAX)
endif()

if(WITH_AUDASPACE)
  list(APPEND INC_SYS
    ${AUDASPACE_C_INCLUDE_DIRS}
  )
  if(WITH_SYSTEM_AUDASPACE)
    list(APPEND LIB
      ${AUDASPACE_C_LIBRARIES}
      ${AUDASPACE_PY_LIBRARIES}
    )
  endif()
  add_definitions(-DWITH_AUDASPACE)
endif()

if(WITH_CYCLES)
  add_definitions(-DWITH_CYCLES)
endif()

if(WITH_OPENGL_BACKEND)
  add_definitions(-DWITH_OPENGL_BACKEND)
endif()

if(WITH_VULKAN_BACKEND)
  add_definitions(-DWITH_VULKAN_BACKEND)
endif()

if(WITH_OPENCOLLADA)
  add_definitions(-DWITH_COLLADA)
endif()

if(WITH_CODEC_FFMPEG)
  list(APPEND INC_SYS
    ${FFMPEG_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${FFMPEG_LIBRARIES}
  )
  add_definitions(-DWITH_FFMPEG)
endif()

if(WITH_HEADLESS)
  add_definitions(-DWITH_HEADLESS)
elseif(WITH_GHOST_X11)
  add_definitions(-DWITH_GHOST_X11)
endif()

if(WITH_PYTHON)
  list(APPEND INC
    ../python
  )
  add_definitions(-DWITH_PYTHON)
  if(WITH_PYTHON_MODULE)
    add_definitions(-DWITH_PYTHON_MODULE)
  endif()
endif()

if(WITH_BUILDINFO)
  add_definitions(-DWITH_BUILDINFO)
endif()

if(WITH_INPUT_NDOF)
  add_definitions(-DWITH_INPUT_NDOF)
endif()

if(WITH_INPUT_IME)
  add_definitions(-DWITH_INPUT_IME)
endif()

if(WITH_COMPOSITOR_CPU)
  list(APPEND LIB
    bf_compositor
  )
  add_definitions(-DWITH_COMPOSITOR_CPU)
endif()

if(WITH_XR_OPENXR)
  add_definitions(-DWITH_XR_OPENXR)

  list(APPEND INC
    xr
  )

  list(APPEND SRC
    xr/intern/wm_xr.cc
    xr/intern/wm_xr_action.cc
    xr/intern/wm_xr_actionmap.cc
    xr/intern/wm_xr_draw.cc
    xr/intern/wm_xr_operators.cc
    xr/intern/wm_xr_session.cc

    xr/wm_xr.hh
    xr/intern/wm_xr_intern.hh
  )
endif()

blender_add_lib_nolist(bf_windowmanager "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_windowmanager bf_rna)

if(WITH_GTESTS)
  set(TEST_INC
  )
  set(TEST_SRC
    intern/wm_dragdrop_test.cc
  )
  blender_add_test_suite_lib(windowmanager "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB}")
endif()
