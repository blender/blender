# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenfont
  ../../blenkernel
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../functions
  ../../gpencil_modifiers_legacy
  ../../gpu
  ../../ikplugin
  ../../imbuf
  ../../makesrna
  ../../modifiers
  ../../python
  ../../render
  ../../shader_fx
  ../../windowmanager
  ../../../../intern/clog

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
  object_collection.cc
  object_constraint.cc
  object_data_transfer.cc
  object_data_transform.cc
  object_edit.cc
  object_gpencil_modifier.cc
  object_hook.cc
  object_light_linking_ops.cc
  object_modes.cc
  object_modifier.cc
  object_ops.cc
  object_random.cc
  object_relations.cc
  object_remesh.cc
  object_select.cc
  object_shader_fx.cc
  object_shapekey.cc
  object_transform.cc
  object_utils.cc
  object_vgroup.cc
  object_volume.cc
  object_warp.cc

  object_intern.h
)

set(LIB
  PRIVATE bf::animrig
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  bf_editor_mesh
  bf_editor_grease_pencil
  PRIVATE bf::intern::guardedalloc
  bf_render
  bf_windowmanager
)


if(WITH_PYTHON)
  add_definitions(-DWITH_PYTHON)
endif()

if(WITH_EXPERIMENTAL_FEATURES)
  add_definitions(-DWITH_SIMULATION_DATABLOCK)
  add_definitions(-DWITH_POINT_CLOUD)
  add_definitions(-DWITH_GREASE_PENCIL_V3)
endif()

blender_add_lib(bf_editor_object "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_editor_object bf_rna)
