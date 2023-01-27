# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ..
  ../intern
  ../../editors/include
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../depsgraph
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../render
  ../../windowmanager
  ../../../../intern/guardedalloc
  ../../bmesh
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  nodes/node_texture_at.cc
  nodes/node_texture_bricks.cc
  nodes/node_texture_checker.cc
  nodes/node_texture_combine_color.cc
  nodes/node_texture_common.cc
  nodes/node_texture_compose.cc
  nodes/node_texture_coord.cc
  nodes/node_texture_curves.cc
  nodes/node_texture_decompose.cc
  nodes/node_texture_distance.cc
  nodes/node_texture_hueSatVal.cc
  nodes/node_texture_image.cc
  nodes/node_texture_invert.cc
  nodes/node_texture_math.cc
  nodes/node_texture_mixRgb.cc
  nodes/node_texture_output.cc
  nodes/node_texture_proc.cc
  nodes/node_texture_rotate.cc
  nodes/node_texture_scale.cc
  nodes/node_texture_separate_color.cc
  nodes/node_texture_texture.cc
  nodes/node_texture_translate.cc
  nodes/node_texture_valToNor.cc
  nodes/node_texture_valToRgb.cc
  nodes/node_texture_viewer.cc

  node_texture_register.cc
  node_texture_tree.cc
  node_texture_util.cc

  node_texture_register.hh
  node_texture_util.hh
)

set(LIB
  bf_nodes
)

blender_add_lib(bf_nodes_texture "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_nodes_texture bf_rna)
