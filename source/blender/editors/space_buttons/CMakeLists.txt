# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../gpu
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/glew-mx
  ../../../../intern/guardedalloc
)

set(SRC
  buttons_context.c
  buttons_ops.c
  buttons_texture.c
  space_buttons.c

  buttons_intern.h
)

set(LIB
)


if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_EXPERIMENTAL_FEATURES)
  add_definitions(-DWITH_SIMULATION_DATABLOCK)
  add_definitions(-DWITH_POINT_CLOUD)
  add_definitions(-DWITH_NEW_CURVES_TYPE)
endif()

blender_add_lib(bf_editor_space_buttons "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
