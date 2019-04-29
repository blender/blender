# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2014, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  ../blenkernel
  ../blenlib
  ../bmesh
  ../draw
  ../makesdna
  ../makesrna
  ../modifiers
  ../windowmanager
  ../../../intern/atomic
  ../../../intern/guardedalloc
)

set(INC_SYS
)

set(SRC
  intern/builder/deg_builder.cc
  intern/builder/deg_builder_cache.cc
  intern/builder/deg_builder_cycle.cc
  intern/builder/deg_builder_map.cc
  intern/builder/deg_builder_nodes.cc
  intern/builder/deg_builder_nodes_rig.cc
  intern/builder/deg_builder_nodes_view_layer.cc
  intern/builder/deg_builder_pchanmap.cc
  intern/builder/deg_builder_relations.cc
  intern/builder/deg_builder_relations_keys.cc
  intern/builder/deg_builder_relations_rig.cc
  intern/builder/deg_builder_relations_view_layer.cc
  intern/builder/deg_builder_rna.cc
  intern/builder/deg_builder_transitive.cc
  intern/debug/deg_debug.cc
  intern/debug/deg_debug_relations_graphviz.cc
  intern/debug/deg_debug_stats_gnuplot.cc
  intern/eval/deg_eval.cc
  intern/eval/deg_eval_copy_on_write.cc
  intern/eval/deg_eval_flush.cc
  intern/eval/deg_eval_stats.cc
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
  intern/depsgraph_physics.cc
  intern/depsgraph_query.cc
  intern/depsgraph_query_filter.cc
  intern/depsgraph_query_foreach.cc
  intern/depsgraph_query_iter.cc
  intern/depsgraph_tag.cc
  intern/depsgraph_type.cc
  intern/depsgraph_update.cc

  DEG_depsgraph.h
  DEG_depsgraph_build.h
  DEG_depsgraph_debug.h
  DEG_depsgraph_physics.h
  DEG_depsgraph_query.h

  intern/builder/deg_builder.h
  intern/builder/deg_builder_cache.h
  intern/builder/deg_builder_cycle.h
  intern/builder/deg_builder_map.h
  intern/builder/deg_builder_nodes.h
  intern/builder/deg_builder_pchanmap.h
  intern/builder/deg_builder_relations.h
  intern/builder/deg_builder_relations_impl.h
  intern/builder/deg_builder_rna.h
  intern/builder/deg_builder_transitive.h
  intern/debug/deg_debug.h
  intern/eval/deg_eval.h
  intern/eval/deg_eval_copy_on_write.h
  intern/eval/deg_eval_flush.h
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
  intern/depsgraph_tag.h
  intern/depsgraph_type.h
  intern/depsgraph_update.h
)

set(LIB
)

if(WITH_BOOST)
  list(APPEND INC_SYS
    ${BOOST_INCLUDE_DIR}
  )
  add_definitions(-DHAVE_BOOST_FUNCTION_BINDINGS)
endif()

blender_add_lib(bf_depsgraph "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
