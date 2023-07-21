# SPDX-FileCopyrightText: 2018 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  intern
  ../bmesh
  ../editors/include
  ../makesrna
  ../render
  ../../../intern/eigen
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
  ${ZLIB_INCLUDE_DIRS}
)

set(SRC
  intern/FX_shader_util.h
  intern/FX_ui_common.cc

  intern/FX_shader_blur.cc
  intern/FX_shader_colorize.cc
  intern/FX_shader_flip.cc
  intern/FX_shader_glow.cc
  intern/FX_shader_pixel.cc
  intern/FX_shader_rim.cc
  intern/FX_shader_shadow.cc
  intern/FX_shader_swirl.cc
  intern/FX_shader_util.cc
  intern/FX_shader_wave.cc

  intern/FX_ui_common.h
  FX_shader_types.h
)

set(LIB
  PRIVATE bf::blenfont
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::windowmanager
)


blender_add_lib(bf_shader_fx "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_shader_fx bf_rna)
