# SPDX-FileCopyrightText: 2018 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

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
  intern/MOD_gpencil_legacy_ui_common.c

  intern/MOD_gpencil_legacy_armature.c
  intern/MOD_gpencil_legacy_array.c
  intern/MOD_gpencil_legacy_build.c
  intern/MOD_gpencil_legacy_color.c
  intern/MOD_gpencil_legacy_dash.c
  intern/MOD_gpencil_legacy_envelope.c
  intern/MOD_gpencil_legacy_hook.c
  intern/MOD_gpencil_legacy_lattice.c
  intern/MOD_gpencil_legacy_length.c
  intern/MOD_gpencil_legacy_lineart.c
  intern/MOD_gpencil_legacy_mirror.c
  intern/MOD_gpencil_legacy_multiply.c
  intern/MOD_gpencil_legacy_noise.c
  intern/MOD_gpencil_legacy_offset.c
  intern/MOD_gpencil_legacy_opacity.c
  intern/MOD_gpencil_legacy_outline.c
  intern/MOD_gpencil_legacy_shrinkwrap.c
  intern/MOD_gpencil_legacy_simplify.c
  intern/MOD_gpencil_legacy_smooth.c
  intern/MOD_gpencil_legacy_subdiv.c
  intern/MOD_gpencil_legacy_texture.c
  intern/MOD_gpencil_legacy_thick.c
  intern/MOD_gpencil_legacy_time.c
  intern/MOD_gpencil_legacy_tint.c
  intern/MOD_gpencil_legacy_util.c
  intern/MOD_gpencil_legacy_weight_angle.c
  intern/MOD_gpencil_legacy_weight_proximity.c

  MOD_gpencil_legacy_lineart.h
  MOD_gpencil_legacy_modifiertypes.h
  intern/MOD_gpencil_legacy_ui_common.h
  intern/MOD_gpencil_legacy_util.h

  # Lineart code
  intern/lineart/lineart_chain.c
  intern/lineart/lineart_cpu.cc
  intern/lineart/lineart_ops.c
  intern/lineart/lineart_shadow.c
  intern/lineart/lineart_util.c

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
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

blender_add_lib(bf_gpencil_modifiers_legacy "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_gpencil_modifiers_legacy bf_rna)
