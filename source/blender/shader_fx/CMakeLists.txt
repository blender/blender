# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2018 Blender Foundation. All rights reserved.

set(INC
  .
  intern
  ../blenfont
  ../blenkernel
  ../blenlib
  ../blentranslation
  ../bmesh
  ../depsgraph
  ../editors/include
  ../makesdna
  ../makesrna
  ../render
  ../windowmanager
  ../../../intern/eigen
  ../../../intern/guardedalloc
)

set(INC_SYS
  ${ZLIB_INCLUDE_DIRS}
)

set(SRC
  intern/FX_shader_util.h
  intern/FX_ui_common.c

  intern/FX_shader_blur.c
  intern/FX_shader_colorize.c
  intern/FX_shader_flip.c
  intern/FX_shader_glow.c
  intern/FX_shader_pixel.c
  intern/FX_shader_rim.c
  intern/FX_shader_shadow.c
  intern/FX_shader_swirl.c
  intern/FX_shader_util.c
  intern/FX_shader_wave.c

  intern/FX_ui_common.h
  FX_shader_types.h
)

set(LIB
)


blender_add_lib(bf_shader_fx "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
