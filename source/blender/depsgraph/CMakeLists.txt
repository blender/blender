# SPDX-FileCopyrightText: 2014 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../blenkernel
  ../blenlib
  ../bmesh
  ../draw
  ../functions
  ../makesrna
  ../modifiers
  ../sequencer
  ../windowmanager
  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  intern/builder/deg_builder.cc
  intern/builder/deg_builder_cache.cc
  intern/builder/deg_builder_cycle.cc
  intern/builder/deg_builder_key.cc
  intern/builder/deg_builder_key.h
  intern/builder/deg_builder_map.cc
  intern/builder/deg_builder_nodes.cc
  intern/builder/deg_builder_nodes_rig.cc
  intern/builder/deg_builder_nodes_scene.cc
  intern/builder/deg_builder_nodes_view_layer.cc
  intern/builder/deg_builder_pchanmap.cc
  intern/builder/deg_builder_relations.cc
  intern/builder/deg_builder_relations_drivers.cc
  intern/builder/deg_builder_relations_rig.cc
  intern/builder/deg_builder_relations_scene.cc
  intern/builder/deg_builder_relations_view_layer.cc
  intern/builder/deg_builder_remove_noop.cc
  intern/builder/deg_builder_rna.cc
  intern/builder/deg_builder_stack.cc
  intern/builder/deg_builder_transitive.cc
  intern/builder/pipeline.cc
  intern/builder/pipeline_all_objects.cc
  intern/builder/pipeline_compositor.cc
  intern/builder/pipeline_from_ids.cc
  intern/builder/pipeline_render.cc
  intern/builder/pipeline_view_layer.cc
  intern/debug/deg_debug.cc
  intern/debug/deg_debug_relations_graphviz.cc
  intern/debug/deg_debug_stats_gnuplot.cc
  intern/eval/deg_eval.cc
  intern/eval/deg_eval_copy_on_write.cc
  intern/eval/deg_eval_flush.cc
  intern/eval/deg_eval_runtime_backup.cc
  intern/eval/deg_eval_runtime_backup_animation.cc
  intern/eval/deg_eval_runtime_backup_gpencil.cc
  intern/eval/deg_eval_runtime_backup_modifier.cc
  intern/eval/deg_eval_runtime_backup_movieclip.cc
  intern/eval/deg_eval_runtime_backup_object.cc
  intern/eval/deg_eval_runtime_backup_pose.cc
  intern/eval/deg_eval_runtime_backup_scene.cc
  intern/eval/deg_eval_runtime_backup_sequence.cc
  intern/eval/deg_eval_runtime_backup_sequencer.cc
  intern/eval/deg_eval_runtime_backup_sound.cc
  intern/eval/deg_eval_runtime_backup_volume.cc
  intern/eval/deg_eval_stats.cc
  intern/eval/deg_eval_visibility.cc
  intern/eval/deg_eval_visibility.h
  intern/node/deg_node.cc
  intern/node/deg_node_component.cc
  intern/node/deg_node_factory.cc
  intern/node/deg_node_id.cc
  intern/node/deg_node_operation.cc
  intern/node/deg_node_time.cc
  intern/depsgraph.cc
  intern/depsgraph_build.cc
  intern/depsgraph_debug.cc
  intern/depsgraph_eval.cc
  intern/depsgraph_light_linking.cc
  intern/depsgraph_light_linking.h
  intern/depsgraph_physics.cc
  intern/depsgraph_query.cc
  intern/depsgraph_query_foreach.cc
  intern/depsgraph_query_iter.cc
  intern/depsgraph_registry.cc
  intern/depsgraph_relation.cc
  intern/depsgraph_tag.cc
  intern/depsgraph_type.cc
  intern/depsgraph_update.cc

  DEG_depsgraph.h
  DEG_depsgraph_build.h
  DEG_depsgraph_debug.h
  DEG_depsgraph_light_linking.h
  DEG_depsgraph_light_linking.hh
  DEG_depsgraph_physics.h
  DEG_depsgraph_query.h

  intern/builder/deg_builder.h
  intern/builder/deg_builder_cache.h
  intern/builder/deg_builder_cycle.h
  intern/builder/deg_builder_map.h
  intern/builder/deg_builder_nodes.h
  intern/builder/deg_builder_pchanmap.h
  intern/builder/deg_builder_relations.h
  intern/builder/deg_builder_relations_drivers.h
  intern/builder/deg_builder_relations_impl.h
  intern/builder/deg_builder_remove_noop.h
  intern/builder/deg_builder_rna.h
  intern/builder/deg_builder_stack.h
  intern/builder/deg_builder_transitive.h
  intern/builder/pipeline.h
  intern/builder/pipeline_all_objects.h
  intern/builder/pipeline_compositor.h
  intern/builder/pipeline_from_ids.h
  intern/builder/pipeline_render.h
  intern/builder/pipeline_view_layer.h
  intern/debug/deg_debug.h
  intern/debug/deg_time_average.h
  intern/eval/deg_eval.h
  intern/eval/deg_eval_copy_on_write.h
  intern/eval/deg_eval_flush.h
  intern/eval/deg_eval_runtime_backup.h
  intern/eval/deg_eval_runtime_backup_animation.h
  intern/eval/deg_eval_runtime_backup_gpencil.h
  intern/eval/deg_eval_runtime_backup_modifier.h
  intern/eval/deg_eval_runtime_backup_movieclip.h
  intern/eval/deg_eval_runtime_backup_object.h
  intern/eval/deg_eval_runtime_backup_pose.h
  intern/eval/deg_eval_runtime_backup_scene.h
  intern/eval/deg_eval_runtime_backup_sequence.h
  intern/eval/deg_eval_runtime_backup_sequencer.h
  intern/eval/deg_eval_runtime_backup_sound.h
  intern/eval/deg_eval_runtime_backup_volume.h
  intern/eval/deg_eval_stats.h
  intern/node/deg_node.h
  intern/node/deg_node_component.h
  intern/node/deg_node_factory.h
  intern/node/deg_node_factory_impl.h
  intern/node/deg_node_id.h
  intern/node/deg_node_operation.h
  intern/node/deg_node_time.h
  intern/depsgraph.h
  intern/depsgraph_physics.h
  intern/depsgraph_registry.h
  intern/depsgraph_relation.h
  intern/depsgraph_tag.h
  intern/depsgraph_type.h
  intern/depsgraph_update.h
)

set(LIB
  bf_blenkernel
  PRIVATE bf::dna
  PRIVATE bf::intern::atomic
  PRIVATE bf::intern::guardedalloc
)

if(WITH_PYTHON)
  add_definitions(-DWITH_PYTHON)
  list(APPEND INC
    ../python
  )
endif()

blender_add_lib(bf_depsgraph "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_depsgraph bf_rna)

if(WITH_GTESTS)
  set(TEST_SRC
    intern/builder/deg_builder_rna_test.cc
  )
  set(TEST_LIB
    bf_depsgraph
  )
  include(GTestTesting)
  blender_add_test_lib(bf_depsgraph_tests "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB}")
endif()
