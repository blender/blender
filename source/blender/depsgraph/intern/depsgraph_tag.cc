/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 *
 * Core routines for how the Depsgraph works.
 */

#include "intern/depsgraph_tag.h"

#include <stdio.h>
#include <cstring> /* required for memset */
#include <queue>

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_math_bits.h"
#include "BLI_task.h"

extern "C" {
#include "DNA_anim_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_animsys.h"
#include "BKE_global.h"
#include "BKE_idcode.h"
#include "BKE_node.h"
#include "BKE_scene.h"
#include "BKE_workspace.h"

#define new new_
#include "BKE_screen.h"
#undef new
} /* extern "C" */

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_debug.h"
#include "DEG_depsgraph_query.h"

#include "intern/builder/deg_builder.h"
#include "intern/depsgraph.h"
#include "intern/depsgraph_update.h"
#include "intern/eval/deg_eval_copy_on_write.h"
#include "intern/eval/deg_eval_flush.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_factory.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"

/* *********************** */
/* Update Tagging/Flushing */

namespace DEG {

namespace {

void depsgraph_geometry_tag_to_component(const ID *id, NodeType *component_type)
{
  const NodeType result = geometry_tag_to_component(id);
  if (result != NodeType::UNDEFINED) {
    *component_type = result;
  }
}

bool is_selectable_data_id_type(const ID_Type id_type)
{
  return ELEM(id_type, ID_ME, ID_CU, ID_MB, ID_LT, ID_GD);
}

void depsgraph_select_tag_to_component_opcode(const ID *id,
                                              NodeType *component_type,
                                              OperationCode *operation_code)
{
  const ID_Type id_type = GS(id->name);
  if (id_type == ID_SCE) {
    /* We need to flush base flags to all objects in a scene since we
     * don't know which ones changed. However, we don't want to update
     * the whole scene, so pick up some operation which will do as less
     * as possible.
     *
     * TODO(sergey): We can introduce explicit exit operation which
     * does nothing and which is only used to cascade flush down the
     * road. */
    *component_type = NodeType::LAYER_COLLECTIONS;
    *operation_code = OperationCode::VIEW_LAYER_EVAL;
  }
  else if (id_type == ID_OB) {
    *component_type = NodeType::OBJECT_FROM_LAYER;
    *operation_code = OperationCode::OBJECT_BASE_FLAGS;
  }
  else if (id_type == ID_MC) {
    *component_type = NodeType::BATCH_CACHE;
    *operation_code = OperationCode::MOVIECLIP_SELECT_UPDATE;
  }
  else if (is_selectable_data_id_type(id_type)) {
    *component_type = NodeType::BATCH_CACHE;
    *operation_code = OperationCode::GEOMETRY_SELECT_UPDATE;
  }
  else {
    *component_type = NodeType::COPY_ON_WRITE;
    *operation_code = OperationCode::COPY_ON_WRITE;
  }
}

void depsgraph_base_flags_tag_to_component_opcode(const ID *id,
                                                  NodeType *component_type,
                                                  OperationCode *operation_code)
{
  const ID_Type id_type = GS(id->name);
  if (id_type == ID_SCE) {
    *component_type = NodeType::LAYER_COLLECTIONS;
    *operation_code = OperationCode::VIEW_LAYER_EVAL;
  }
  else if (id_type == ID_OB) {
    *component_type = NodeType::OBJECT_FROM_LAYER;
    *operation_code = OperationCode::OBJECT_BASE_FLAGS;
  }
}

OperationCode psysTagToOperationCode(IDRecalcFlag tag)
{
  if (tag == ID_RECALC_PSYS_RESET) {
    return OperationCode::PARTICLE_SETTINGS_RESET;
  }
  return OperationCode::OPERATION;
}

void depsgraph_tag_to_component_opcode(const ID *id,
                                       IDRecalcFlag tag,
                                       NodeType *component_type,
                                       OperationCode *operation_code)
{
  const ID_Type id_type = GS(id->name);
  *component_type = NodeType::UNDEFINED;
  *operation_code = OperationCode::OPERATION;
  /* Special case for now, in the future we should get rid of this. */
  if (tag == 0) {
    *component_type = NodeType::ID_REF;
    *operation_code = OperationCode::OPERATION;
    return;
  }
  switch (tag) {
    case ID_RECALC_TRANSFORM:
      *component_type = NodeType::TRANSFORM;
      break;
    case ID_RECALC_GEOMETRY:
      depsgraph_geometry_tag_to_component(id, component_type);
      break;
    case ID_RECALC_ANIMATION:
      *component_type = NodeType::ANIMATION;
      break;
    case ID_RECALC_PSYS_REDO:
    case ID_RECALC_PSYS_RESET:
    case ID_RECALC_PSYS_CHILD:
    case ID_RECALC_PSYS_PHYS:
      if (id_type == ID_PA) {
        /* NOTES:
         * - For particle settings node we need to use different
         *   component. Will be nice to get this unified with object,
         *   but we can survive for now with single exception here.
         *   Particles needs reconsideration anyway, */
        *component_type = NodeType::PARTICLE_SETTINGS;
        *operation_code = psysTagToOperationCode(tag);
      }
      else {
        *component_type = NodeType::PARTICLE_SYSTEM;
      }
      break;
    case ID_RECALC_COPY_ON_WRITE:
      *component_type = NodeType::COPY_ON_WRITE;
      break;
    case ID_RECALC_SHADING:
      if (id_type == ID_NT) {
        *component_type = NodeType::SHADING_PARAMETERS;
      }
      else {
        *component_type = NodeType::SHADING;
      }
      break;
    case ID_RECALC_SELECT:
      depsgraph_select_tag_to_component_opcode(id, component_type, operation_code);
      break;
    case ID_RECALC_BASE_FLAGS:
      depsgraph_base_flags_tag_to_component_opcode(id, component_type, operation_code);
      break;
    case ID_RECALC_POINT_CACHE:
      *component_type = NodeType::POINT_CACHE;
      break;
    case ID_RECALC_EDITORS:
      /* There is no such node in depsgraph, this tag is to be handled
       * separately. */
      break;
    case ID_RECALC_SEQUENCER:
      *component_type = NodeType::SEQUENCER;
      break;
    case ID_RECALC_AUDIO_JUMP:
      *component_type = NodeType::AUDIO;
      break;
    case ID_RECALC_ALL:
    case ID_RECALC_PSYS_ALL:
      BLI_assert(!"Should not happen");
      break;
  }
}

void id_tag_update_ntree_special(
    Main *bmain, Depsgraph *graph, ID *id, int flag, eUpdateSource update_source)
{
  bNodeTree *ntree = ntreeFromID(id);
  if (ntree == NULL) {
    return;
  }
  graph_id_tag_update(bmain, graph, &ntree->id, flag, update_source);
}

void depsgraph_update_editors_tag(Main *bmain, Depsgraph *graph, ID *id)
{
  /* NOTE: We handle this immediately, without delaying anything, to be
   * sure we don't cause threading issues with OpenGL. */
  /* TODO(sergey): Make sure this works for CoW-ed datablocks as well. */
  DEGEditorUpdateContext update_ctx = {NULL};
  update_ctx.bmain = bmain;
  update_ctx.depsgraph = (::Depsgraph *)graph;
  update_ctx.scene = graph->scene;
  update_ctx.view_layer = graph->view_layer;
  deg_editors_id_update(&update_ctx, id);
}

void depsgraph_tag_component(Depsgraph *graph,
                             IDNode *id_node,
                             NodeType component_type,
                             OperationCode operation_code,
                             eUpdateSource update_source)
{
  ComponentNode *component_node = id_node->find_component(component_type);
  if (component_node == NULL) {
    return;
  }
  if (operation_code == OperationCode::OPERATION) {
    component_node->tag_update(graph, update_source);
  }
  else {
    OperationNode *operation_node = component_node->find_operation(operation_code);
    if (operation_node != NULL) {
      operation_node->tag_update(graph, update_source);
    }
  }
  /* If component depends on copy-on-write, tag it as well. */
  if (component_node->need_tag_cow_before_update()) {
    ComponentNode *cow_comp = id_node->find_component(NodeType::COPY_ON_WRITE);
    cow_comp->tag_update(graph, update_source);
    id_node->id_orig->recalc |= ID_RECALC_COPY_ON_WRITE;
  }
}

/* This is a tag compatibility with legacy code.
 *
 * Mainly, old code was tagging object with ID_RECALC_GEOMETRY tag to inform
 * that object's data datablock changed. Now API expects that ID is given
 * explicitly, but not all areas are aware of this yet. */
void deg_graph_id_tag_legacy_compat(
    Main *bmain, Depsgraph *depsgraph, ID *id, IDRecalcFlag tag, eUpdateSource update_source)
{
  if (tag == ID_RECALC_GEOMETRY || tag == 0) {
    switch (GS(id->name)) {
      case ID_OB: {
        Object *object = (Object *)id;
        ID *data_id = (ID *)object->data;
        if (data_id != NULL) {
          graph_id_tag_update(bmain, depsgraph, data_id, 0, update_source);
        }
        break;
      }
      /* TODO(sergey): Shape keys are annoying, maybe we should find a
       * way to chain geometry evaluation to them, so we don't need extra
       * tagging here. */
      case ID_ME: {
        Mesh *mesh = (Mesh *)id;
        ID *key_id = &mesh->key->id;
        if (key_id != NULL) {
          graph_id_tag_update(bmain, depsgraph, key_id, 0, update_source);
        }
        break;
      }
      case ID_LT: {
        Lattice *lattice = (Lattice *)id;
        ID *key_id = &lattice->key->id;
        if (key_id != NULL) {
          graph_id_tag_update(bmain, depsgraph, key_id, 0, update_source);
        }
        break;
      }
      case ID_CU: {
        Curve *curve = (Curve *)id;
        ID *key_id = &curve->key->id;
        if (key_id != NULL) {
          graph_id_tag_update(bmain, depsgraph, key_id, 0, update_source);
        }
        break;
      }
      default:
        break;
    }
  }
}

static void graph_id_tag_update_single_flag(Main *bmain,
                                            Depsgraph *graph,
                                            ID *id,
                                            IDNode *id_node,
                                            IDRecalcFlag tag,
                                            eUpdateSource update_source)
{
  if (tag == ID_RECALC_EDITORS) {
    if (graph != NULL) {
      depsgraph_update_editors_tag(bmain, graph, id);
    }
    return;
  }
  /* Get description of what is to be tagged. */
  NodeType component_type;
  OperationCode operation_code;
  depsgraph_tag_to_component_opcode(id, tag, &component_type, &operation_code);
  /* Check whether we've got something to tag. */
  if (component_type == NodeType::UNDEFINED) {
    /* Given ID does not support tag. */
    /* TODO(sergey): Shall we raise some panic here? */
    return;
  }
  /* Tag ID recalc flag. */
  DepsNodeFactory *factory = type_get_factory(component_type);
  BLI_assert(factory != NULL);
  id->recalc |= factory->id_recalc_tag();
  /* Some sanity checks before moving forward. */
  if (id_node == NULL) {
    /* Happens when object is tagged for update and not yet in the
     * dependency graph (but will be after relations update). */
    return;
  }
  /* Tag corresponding dependency graph operation for update. */
  if (component_type == NodeType::ID_REF) {
    id_node->tag_update(graph, update_source);
  }
  else {
    depsgraph_tag_component(graph, id_node, component_type, operation_code, update_source);
  }
  /* TODO(sergey): Get rid of this once all areas are using proper data ID
   * for tagging. */
  deg_graph_id_tag_legacy_compat(bmain, graph, id, tag, update_source);
}

string stringify_append_bit(const string &str, IDRecalcFlag tag)
{
  string result = str;
  if (!result.empty()) {
    result += ", ";
  }
  result += DEG_update_tag_as_string(tag);
  return result;
}

string stringify_update_bitfield(int flag)
{
  if (flag == 0) {
    return "LEGACY_0";
  }
  string result = "";
  int current_flag = flag;
  /* Special cases to avoid ALL flags form being split into
   * individual bits. */
  if ((current_flag & ID_RECALC_PSYS_ALL) == ID_RECALC_PSYS_ALL) {
    result = stringify_append_bit(result, ID_RECALC_PSYS_ALL);
  }
  /* Handle all the rest of the flags. */
  while (current_flag != 0) {
    IDRecalcFlag tag = (IDRecalcFlag)(1 << bitscan_forward_clear_i(&current_flag));
    result = stringify_append_bit(result, tag);
  }
  return result;
}

const char *update_source_as_string(eUpdateSource source)
{
  switch (source) {
    case DEG_UPDATE_SOURCE_TIME:
      return "TIME";
    case DEG_UPDATE_SOURCE_USER_EDIT:
      return "USER_EDIT";
    case DEG_UPDATE_SOURCE_RELATIONS:
      return "RELATIONS";
    case DEG_UPDATE_SOURCE_VISIBILITY:
      return "VISIBILITY";
  }
  BLI_assert(!"Should never happen.");
  return "UNKNOWN";
}

/* Special tag function which tags all components which needs to be tagged
 * for update flag=0.
 *
 * TODO(sergey): This is something to be avoid in the future, make it more
 * explicit and granular for users to tag what they really need. */
void deg_graph_node_tag_zero(Main *bmain,
                             Depsgraph *graph,
                             IDNode *id_node,
                             eUpdateSource update_source)
{
  if (id_node == NULL) {
    return;
  }
  ID *id = id_node->id_orig;
  /* TODO(sergey): Which recalc flags to set here? */
  id->recalc |= ID_RECALC_ALL & ~(ID_RECALC_PSYS_ALL | ID_RECALC_ANIMATION);
  GHASH_FOREACH_BEGIN (ComponentNode *, comp_node, id_node->components) {
    if (comp_node->type == NodeType::ANIMATION) {
      continue;
    }
    comp_node->tag_update(graph, update_source);
  }
  GHASH_FOREACH_END();
  deg_graph_id_tag_legacy_compat(bmain, graph, id, (IDRecalcFlag)0, update_source);
}

void deg_graph_on_visible_update(Main *bmain, Depsgraph *graph)
{
  for (DEG::IDNode *id_node : graph->id_nodes) {
    if (!id_node->visible_components_mask) {
      /* ID has no components which affects anything visible.
       * No need bother with it to tag or anything. */
      continue;
    }
    if (id_node->visible_components_mask == id_node->previously_visible_components_mask) {
      /* The ID was already visible and evaluated, all the subsequent
       * updates and tags are to be done explicitly. */
      continue;
    }
    int flag = 0;
    if (!DEG::deg_copy_on_write_is_expanded(id_node->id_cow)) {
      flag |= ID_RECALC_COPY_ON_WRITE;
      /* TODO(sergey): Shouldn't be needed, but currently we are lackign
       * some flushing of evaluated data to the original one, which makes,
       * for example, files saved with the rest pose.
       * Need to solve those issues carefully, for until then we evaluate
       * animation for datablocks which appears in the graph for the first
       * time. */
      flag |= ID_RECALC_ANIMATION;
    }
    /* We only tag components which needs an update. Tagging everything is
     * not a good idea because that might reset particles cache (or any
     * other type of cache).
     *
     * TODO(sergey): Need to generalize this somehow. */
    const ID_Type id_type = GS(id_node->id_orig->name);
    if (id_type == ID_OB) {
      flag |= ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY;
    }
    graph_id_tag_update(bmain, graph, id_node->id_orig, flag, DEG_UPDATE_SOURCE_VISIBILITY);
    if (id_type == ID_SCE) {
      /* Make sure collection properties are up to date. */
      id_node->tag_update(graph, DEG_UPDATE_SOURCE_VISIBILITY);
    }
    /* Now when ID is updated to the new visibility state, prevent it from
     * being re-tagged again. Simplest way to do so is to pretend that it
     * was already updated by the "previous" dependency graph.
     *
     * NOTE: Even if the on_visible_update() is called from the state when
     * dependency graph is tagged for relations update, it will be fine:
     * since dependency graph builder re-schedules entry tags, all the
     * tags we request from here will be applied in the updated state of
     * dependency graph. */
    id_node->previously_visible_components_mask = id_node->visible_components_mask;
  }
}

} /* namespace */

NodeType geometry_tag_to_component(const ID *id)
{
  const ID_Type id_type = GS(id->name);
  switch (id_type) {
    case ID_OB: {
      const Object *object = (Object *)id;
      switch (object->type) {
        case OB_MESH:
        case OB_CURVE:
        case OB_SURF:
        case OB_FONT:
        case OB_LATTICE:
        case OB_MBALL:
        case OB_GPENCIL:
          return NodeType::GEOMETRY;
        case OB_ARMATURE:
          return NodeType::EVAL_POSE;
          /* TODO(sergey): More cases here? */
      }
      break;
    }
    case ID_ME:
    case ID_CU:
    case ID_LT:
    case ID_MB:
      return NodeType::GEOMETRY;
    case ID_PA: /* Particles */
      return NodeType::UNDEFINED;
    case ID_LP:
      return NodeType::PARAMETERS;
    case ID_GD:
      return NodeType::GEOMETRY;
    case ID_PAL: /* Palettes */
      return NodeType::PARAMETERS;
    default:
      break;
  }
  return NodeType::UNDEFINED;
}

void id_tag_update(Main *bmain, ID *id, int flag, eUpdateSource update_source)
{
  graph_id_tag_update(bmain, NULL, id, flag, update_source);
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
      Depsgraph *depsgraph = (Depsgraph *)BKE_scene_get_depsgraph(scene, view_layer, false);
      if (depsgraph != NULL) {
        graph_id_tag_update(bmain, depsgraph, id, flag, update_source);
      }
    }
  }
}

void graph_id_tag_update(
    Main *bmain, Depsgraph *graph, ID *id, int flag, eUpdateSource update_source)
{
  const int debug_flags = (graph != NULL) ? DEG_debug_flags_get((::Depsgraph *)graph) : G.debug;
  if (debug_flags & G_DEBUG_DEPSGRAPH_TAG) {
    printf("%s: id=%s flags=%s source=%s\n",
           __func__,
           id->name,
           stringify_update_bitfield(flag).c_str(),
           update_source_as_string(update_source));
  }
  IDNode *id_node = (graph != NULL) ? graph->find_id_node(id) : NULL;
  if (graph != NULL) {
    DEG_graph_id_type_tag(reinterpret_cast<::Depsgraph *>(graph), GS(id->name));
  }
  if (flag == 0) {
    deg_graph_node_tag_zero(bmain, graph, id_node, update_source);
  }
  id->recalc |= flag;
  int current_flag = flag;
  while (current_flag != 0) {
    IDRecalcFlag tag = (IDRecalcFlag)(1 << bitscan_forward_clear_i(&current_flag));
    graph_id_tag_update_single_flag(bmain, graph, id, id_node, tag, update_source);
  }
  /* Special case for nested node tree datablocks. */
  id_tag_update_ntree_special(bmain, graph, id, flag, update_source);
  /* Direct update tags means that something outside of simulated/cached
   * physics did change and that cache is to be invalidated.
   * This is only needed if data changes. If it's just a drawing, we keep the
   * point cache. */
  if (update_source == DEG_UPDATE_SOURCE_USER_EDIT && flag != ID_RECALC_SHADING) {
    graph_id_tag_update_single_flag(
        bmain, graph, id, id_node, ID_RECALC_POINT_CACHE, update_source);
  }
}

}  // namespace DEG

const char *DEG_update_tag_as_string(IDRecalcFlag flag)
{
  switch (flag) {
    case ID_RECALC_TRANSFORM:
      return "TRANSFORM";
    case ID_RECALC_GEOMETRY:
      return "GEOMETRY";
    case ID_RECALC_ANIMATION:
      return "ANIMATION";
    case ID_RECALC_PSYS_REDO:
      return "PSYS_REDO";
    case ID_RECALC_PSYS_RESET:
      return "PSYS_RESET";
    case ID_RECALC_PSYS_CHILD:
      return "PSYS_CHILD";
    case ID_RECALC_PSYS_PHYS:
      return "PSYS_PHYS";
    case ID_RECALC_PSYS_ALL:
      return "PSYS_ALL";
    case ID_RECALC_COPY_ON_WRITE:
      return "COPY_ON_WRITE";
    case ID_RECALC_SHADING:
      return "SHADING";
    case ID_RECALC_SELECT:
      return "SELECT";
    case ID_RECALC_BASE_FLAGS:
      return "BASE_FLAGS";
    case ID_RECALC_POINT_CACHE:
      return "POINT_CACHE";
    case ID_RECALC_EDITORS:
      return "EDITORS";
    case ID_RECALC_SEQUENCER:
      return "SEQUENCER";
    case ID_RECALC_AUDIO_JUMP:
      return "AUDIO_JUMP";
    case ID_RECALC_ALL:
      return "ALL";
  }
  BLI_assert(!"Unhandled update flag, should never happen!");
  return "UNKNOWN";
}

/* Data-Based Tagging  */

/* Tag given ID for an update in all the dependency graphs. */
void DEG_id_tag_update(ID *id, int flag)
{
  DEG_id_tag_update_ex(G.main, id, flag);
}

void DEG_id_tag_update_ex(Main *bmain, ID *id, int flag)
{
  if (id == NULL) {
    /* Ideally should not happen, but old depsgraph allowed this. */
    return;
  }
  DEG::id_tag_update(bmain, id, flag, DEG::DEG_UPDATE_SOURCE_USER_EDIT);
}

void DEG_graph_id_tag_update(struct Main *bmain,
                             struct Depsgraph *depsgraph,
                             struct ID *id,
                             int flag)
{
  DEG::Depsgraph *graph = (DEG::Depsgraph *)depsgraph;
  DEG::graph_id_tag_update(bmain, graph, id, flag, DEG::DEG_UPDATE_SOURCE_USER_EDIT);
}

/* Mark a particular datablock type as having changing. */
void DEG_graph_id_type_tag(Depsgraph *depsgraph, short id_type)
{
  if (id_type == ID_NT) {
    /* Stupid workaround so parent datablocks of nested nodetree get looped
     * over when we loop over tagged datablock types. */
    DEG_graph_id_type_tag(depsgraph, ID_MA);
    DEG_graph_id_type_tag(depsgraph, ID_TE);
    DEG_graph_id_type_tag(depsgraph, ID_LA);
    DEG_graph_id_type_tag(depsgraph, ID_WO);
    DEG_graph_id_type_tag(depsgraph, ID_SCE);
  }
  const int id_type_index = BKE_idcode_to_index(id_type);
  DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(depsgraph);
  deg_graph->id_type_updated[id_type_index] = 1;
}

void DEG_id_type_tag(Main *bmain, short id_type)
{
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
      Depsgraph *depsgraph = (Depsgraph *)BKE_scene_get_depsgraph(scene, view_layer, false);
      if (depsgraph != NULL) {
        DEG_graph_id_type_tag(depsgraph, id_type);
      }
    }
  }
}

void DEG_graph_flush_update(Main *bmain, Depsgraph *depsgraph)
{
  if (depsgraph == NULL) {
    return;
  }
  DEG::deg_graph_flush_updates(bmain, (DEG::Depsgraph *)depsgraph);
}

/* Update dependency graph when visible scenes/layers changes. */
void DEG_graph_on_visible_update(Main *bmain, Depsgraph *depsgraph)
{
  DEG::Depsgraph *graph = (DEG::Depsgraph *)depsgraph;
  DEG::deg_graph_on_visible_update(bmain, graph);
}

void DEG_on_visible_update(Main *bmain, const bool UNUSED(do_time))
{
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
      Depsgraph *depsgraph = (Depsgraph *)BKE_scene_get_depsgraph(scene, view_layer, false);
      if (depsgraph != NULL) {
        DEG_graph_on_visible_update(bmain, depsgraph);
      }
    }
  }
}

/* Check if something was changed in the database and inform
 * editors about this. */
void DEG_ids_check_recalc(
    Main *bmain, Depsgraph *depsgraph, Scene *scene, ViewLayer *view_layer, bool time)
{
  bool updated = time || DEG_id_type_any_updated(depsgraph);

  DEGEditorUpdateContext update_ctx = {NULL};
  update_ctx.bmain = bmain;
  update_ctx.depsgraph = depsgraph;
  update_ctx.scene = scene;
  update_ctx.view_layer = view_layer;
  DEG::deg_editors_scene_update(&update_ctx, updated);
}

static void deg_graph_clear_id_node_func(void *__restrict data_v,
                                         const int i,
                                         const ParallelRangeTLS *__restrict /*tls*/)
{
  /* TODO: we clear original ID recalc flags here, but this may not work
   * correctly when there are multiple depsgraph with others still using
   * the recalc flag. */
  DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(data_v);
  DEG::IDNode *id_node = deg_graph->id_nodes[i];
  id_node->id_cow->recalc &= ~ID_RECALC_ALL;
  id_node->id_orig->recalc &= ~ID_RECALC_ALL;

  /* Clear embedded node trees too. */
  bNodeTree *ntree_cow = ntreeFromID(id_node->id_cow);
  if (ntree_cow) {
    ntree_cow->id.recalc &= ~ID_RECALC_ALL;
  }
  bNodeTree *ntree_orig = ntreeFromID(id_node->id_orig);
  if (ntree_orig) {
    ntree_orig->id.recalc &= ~ID_RECALC_ALL;
  }
}

void DEG_ids_clear_recalc(Main *UNUSED(bmain), Depsgraph *depsgraph)
{
  DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(depsgraph);
  /* TODO(sergey): Re-implement POST_UPDATE_HANDLER_WORKAROUND using entry_tags
   * and id_tags storage from the new dependency graph. */
  if (!DEG_id_type_any_updated(depsgraph)) {
    return;
  }
  /* Go over all ID nodes nodes, clearing tags. */
  const int num_id_nodes = deg_graph->id_nodes.size();
  ParallelRangeSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.min_iter_per_thread = 1024;
  BLI_task_parallel_range(0, num_id_nodes, deg_graph, deg_graph_clear_id_node_func, &settings);
  memset(deg_graph->id_type_updated, 0, sizeof(deg_graph->id_type_updated));
}
