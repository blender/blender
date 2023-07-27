# SPDX-FileCopyrightText: 2018 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  intern
  ../blenfont
  ../blenkernel
  ../blentranslation
  ../bmesh
  ../depsgraph
  ../editors/include
  ../makesrna
  ../render
  ../windowmanager
  ../../../intern/eigen

  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
  ${ZLIB_INCLUDE_DIRS}
)

set(SRC
  intern/MOD_gpencil_legacy_ui_common.cc

  intern/MOD_gpencil_legacy_armature.cc
  intern/MOD_gpencil_legacy_array.cc
  intern/MOD_gpencil_legacy_build.cc
  intern/MOD_gpencil_legacy_color.cc
  intern/MOD_gpencil_legacy_dash.cc
  intern/MOD_gpencil_legacy_envelope.cc
  intern/MOD_gpencil_legacy_hook.cc
  intern/MOD_gpencil_legacy_lattice.cc
  intern/MOD_gpencil_legacy_length.cc
  intern/MOD_gpencil_legacy_lineart.cc
  intern/MOD_gpencil_legacy_mirror.cc
  intern/MOD_gpencil_legacy_multiply.cc
  intern/MOD_gpencil_legacy_noise.cc
  intern/MOD_gpencil_legacy_offset.cc
  intern/MOD_gpencil_legacy_opacity.cc
  intern/MOD_gpencil_legacy_outline.cc
  intern/MOD_gpencil_legacy_shrinkwrap.cc
  intern/MOD_gpencil_legacy_simplify.cc
  intern/MOD_gpencil_legacy_smooth.cc
  intern/MOD_gpencil_legacy_subdiv.cc
  intern/MOD_gpencil_legacy_texture.cc
  intern/MOD_gpencil_legacy_thick.cc
  intern/MOD_gpencil_legacy_time.cc
  intern/MOD_gpencil_legacy_tint.cc
  intern/MOD_gpencil_legacy_util.cc
  intern/MOD_gpencil_legacy_weight_angle.cc
  intern/MOD_gpencil_legacy_weight_proximity.cc

  MOD_gpencil_legacy_lineart.h
  MOD_gpencil_legacy_modifiertypes.h
  intern/MOD_gpencil_legacy_ui_common.h
  intern/MOD_gpencil_legacy_util.h

  # Lineart code
  intern/lineart/lineart_chain.cc
  intern/lineart/lineart_cpu.cc
  intern/lineart/lineart_ops.cc
  intern/lineart/lineart_shadow.cc
  intern/lineart/lineart_util.cc

  intern/lineart/MOD_lineart.h
  intern/lineart/lineart_intern.h
)

if(WITH_TBB)
add_definitions(-DWITH_TBB)
if(WIN32)
  # TBB includes Windows.h which will define min/max macros
  # that will collide with the stl versions.
  add_definitions(-DNOMINMAX)
endif()
list(APPEND INC_SYS
  ${TBB_INCLUDE_DIRS}
)

list(APPEND LIB
  ${TBB_LIBRARIES}
)
endif()

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

blender_add_lib(bf_gpencil_modifiers_legacy "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_gpencil_modifiers_legacy bf_rna)
