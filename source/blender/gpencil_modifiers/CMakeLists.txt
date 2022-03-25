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

  # dna_type_offsets.h in BLO_read_write.h
  ${CMAKE_BINARY_DIR}/source/blender/makesdna/intern
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
  ${ZLIB_INCLUDE_DIRS}
)

set(SRC
  intern/MOD_gpencil_ui_common.c

  intern/MOD_gpencil_util.c
  intern/MOD_gpencilarmature.c
  intern/MOD_gpencilarray.c
  intern/MOD_gpencilbuild.c
  intern/MOD_gpencilcolor.c
  intern/MOD_gpencildash.c
  intern/MOD_gpencilhook.c
  intern/MOD_gpencillattice.c
  intern/MOD_gpencillength.c
  intern/MOD_gpencillineart.c
  intern/MOD_gpencilmirror.c
  intern/MOD_gpencilmultiply.c
  intern/MOD_gpencilnoise.c
  intern/MOD_gpenciloffset.c
  intern/MOD_gpencilopacity.c
  intern/MOD_gpencilshrinkwrap.c
  intern/MOD_gpencilsimplify.c
  intern/MOD_gpencilsmooth.c
  intern/MOD_gpencilsubdiv.c
  intern/MOD_gpenciltexture.c
  intern/MOD_gpencilthick.c
  intern/MOD_gpenciltime.c
  intern/MOD_gpenciltint.c
  intern/MOD_gpencilweight_angle.c
  intern/MOD_gpencilweight_proximity.c

  MOD_gpencil_lineart.h
  MOD_gpencil_modifiertypes.h
  intern/MOD_gpencil_ui_common.h
  intern/MOD_gpencil_util.h

  # Lineart code
  intern/lineart/lineart_chain.c
  intern/lineart/lineart_cpu.c
  intern/lineart/lineart_ops.c
  intern/lineart/lineart_util.c

  intern/lineart/MOD_lineart.h
  intern/lineart/lineart_intern.h

)

set(LIB
)

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_gpencil_modifiers "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

add_dependencies(bf_gpencil_modifiers bf_dna)
# RNA_prototypes.h
add_dependencies(bf_gpencil_modifiers bf_rna)
