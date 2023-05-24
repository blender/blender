# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenfont
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../functions
  ../../gpencil_modifiers_legacy
  ../../gpu
  ../../ikplugin
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../modifiers
  ../../python
  ../../render
  ../../shader_fx
  ../../windowmanager
  ../../../../intern/clog
  ../../../../intern/guardedalloc

  # dna_type_offsets.h in BLO_read_write.h
  ${CMAKE_BINARY_DIR}/source/blender/makesdna/intern
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  object_add.cc
  object_bake.cc
  object_bake_api.cc
  object_bake_simulation.cc
  object_collection.c
  object_constraint.c
  object_data_transfer.c
  object_data_transform.cc
  object_edit.cc
  object_facemap_ops.c
  object_gpencil_modifier.c
  object_hook.c
  object_light_linking_ops.cc
  object_modes.cc
  object_modifier.cc
  object_ops.c
  object_random.c
  object_relations.c
  object_remesh.cc
  object_select.c
  object_shader_fx.c
  object_shapekey.c
  object_transform.cc
  object_utils.c
  object_vgroup.cc
  object_volume.c
  object_warp.c

  object_intern.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
  bf_editor_mesh
  bf_render
  bf_windowmanager
)


if(WITH_PYTHON)
  add_definitions(-DWITH_PYTHON)
endif()

if(WITH_EXPERIMENTAL_FEATURES)
  add_definitions(-DWITH_SIMULATION_DATABLOCK)
  add_definitions(-DWITH_POINT_CLOUD)
endif()

blender_add_lib(bf_editor_object "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

add_dependencies(bf_editor_object bf_dna)
# RNA_prototypes.h
add_dependencies(bf_editor_object bf_rna)
