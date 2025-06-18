# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../ikplugin
  ../../makesrna
  ../../modifiers
  ../../nodes
  ../../python
  ../../shader_fx

  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  add_modifier_assets.cc
  interface_template_shape_key_tree.cc
  object_add.cc
  object_bake.cc
  object_bake_api.cc
  object_bake_simulation.cc
  object_camera.cc
  object_collection.cc
  object_constraint.cc
  object_data_transfer.cc
  object_data_transform.cc
  object_edit.cc
  object_hook.cc
  object_light_linking_ops.cc
  object_modes.cc
  object_modifier.cc
  object_multires_modifier.cc
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
  object_visual_geometry_to_objects.cc
  object_volume.cc
  object_warp.cc

  object_intern.hh
)

set(LIB
  PRIVATE bf::animrig
  PRIVATE bf::asset_system
  PRIVATE bf::blenfont
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  bf_editor_mesh
  bf_editor_metaball
  bf_editor_grease_pencil
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::functions
  PRIVATE bf::geometry
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::render
  PRIVATE bf::windowmanager
)


if(WITH_PYTHON)
  add_definitions(-DWITH_PYTHON)
endif()

if(WITH_EXPERIMENTAL_FEATURES)
  add_definitions(-DWITH_SIMULATION_DATABLOCK)
endif()

blender_add_lib(bf_editor_object "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.hh
add_dependencies(bf_editor_object bf_rna)
