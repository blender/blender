/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include <fmt/format.h>

#include "DNA_ID.h"
#include "DNA_brush_types.h"
#include "DNA_curves_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_world_types.h"

#include "BLI_listbase.h"
#include "BLI_math_numbers.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_set.hh"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_sys_types.h"

#include "BKE_animsys.h"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_colortools.hh"
#include "BKE_curves.hh"
#include "BKE_idprop.hh"
#include "BKE_image_format.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_pointcache.h"
#include "BKE_report.hh"

#include "BLT_translation.hh"

#include "BLO_read_write.hh"

#include "SEQ_iterator.hh"
#include "SEQ_modifier.hh"
#include "SEQ_sequencer.hh"

#include "readfile.hh"

#include "versioning_common.hh"

// #include "CLG_log.h"
// static CLG_LogRef LOG = {"blend.doversion"};

void version_system_idprops_generate(Main *bmain)
{
  auto idprops_process = [](IDProperty *idprops, IDProperty **system_idprops) -> void {
    BLI_assert(*system_idprops == nullptr);
    if (idprops) {
      /* Other ID pointers have not yet been relinked, do not try to access them for refcounting.
       */
      *system_idprops = IDP_CopyProperty_ex(idprops, LIB_ID_CREATE_NO_USER_REFCOUNT);
    }
  };

  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    idprops_process(id_iter->properties, &id_iter->system_properties);
  }
  FOREACH_MAIN_ID_END;

  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
      idprops_process(view_layer->id_properties, &view_layer->system_properties);
    }

    if (scene->ed != nullptr) {
      blender::seq::for_each_callback(&scene->ed->seqbase,
                                      [&idprops_process](Strip *strip) -> bool {
                                        idprops_process(strip->prop, &strip->system_properties);
                                        return true;
                                      });
    }
  }

  LISTBASE_FOREACH (Object *, object, &bmain->objects) {
    if (!object->pose) {
      continue;
    }
    LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
      idprops_process(pchan->prop, &pchan->system_properties);
    }
  }

  LISTBASE_FOREACH (bArmature *, armature, &bmain->armatures) {
    for (BoneCollection *bcoll : armature->collections_span()) {
      idprops_process(bcoll->prop, &bcoll->system_properties);
    }
    LISTBASE_FOREACH (Bone *, bone, &armature->bonebase) {
      idprops_process(bone->prop, &bone->system_properties);
    }
  }
}

static CustomDataLayer *find_old_seam_layer(CustomData &custom_data, const blender::StringRef name)
{
  for (CustomDataLayer &layer : blender::MutableSpan(custom_data.layers, custom_data.totlayer)) {
    if (layer.name == name) {
      return &layer;
    }
  }
  return nullptr;
}

static void rename_mesh_uv_seam_attribute(Mesh &mesh)
{
  using namespace blender;
  CustomDataLayer *old_seam_layer = find_old_seam_layer(mesh.edge_data, ".uv_seam");
  if (!old_seam_layer) {
    return;
  }
  Set<StringRef> names;
  for (const CustomDataLayer &layer : Span(mesh.vert_data.layers, mesh.vert_data.totlayer)) {
    if (layer.type & CD_MASK_PROP_ALL) {
      names.add(layer.name);
    }
  }
  for (const CustomDataLayer &layer : Span(mesh.edge_data.layers, mesh.edge_data.totlayer)) {
    if (layer.type & CD_MASK_PROP_ALL) {
      names.add(layer.name);
    }
  }
  for (const CustomDataLayer &layer : Span(mesh.face_data.layers, mesh.face_data.totlayer)) {
    if (layer.type & CD_MASK_PROP_ALL) {
      names.add(layer.name);
    }
  }
  for (const CustomDataLayer &layer : Span(mesh.corner_data.layers, mesh.corner_data.totlayer)) {
    if (layer.type & CD_MASK_PROP_ALL) {
      names.add(layer.name);
    }
  }
  LISTBASE_FOREACH (const bDeformGroup *, vertex_group, &mesh.vertex_group_names) {
    names.add(vertex_group->name);
  }

  /* If the new UV name is already taken, still rename the attribute so it becomes visible in the
   * list. Then the user can deal with the name conflict themselves. */
  const std::string new_name = BLI_uniquename_cb(
      [&](const StringRef name) { return names.contains(name); }, '.', "uv_seam");
  STRNCPY_UTF8(old_seam_layer->name, new_name.c_str());
}

static void initialize_closure_input_structure_types(bNodeTree &ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    if (node->type_legacy == GEO_NODE_EVALUATE_CLOSURE) {
      auto *storage = static_cast<NodeGeometryEvaluateClosure *>(node->storage);
      for (const int i : blender::IndexRange(storage->input_items.items_num)) {
        NodeGeometryEvaluateClosureInputItem &item = storage->input_items.items[i];
        if (item.structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
          item.structure_type = NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_DYNAMIC;
        }
      }
      for (const int i : blender::IndexRange(storage->output_items.items_num)) {
        NodeGeometryEvaluateClosureOutputItem &item = storage->output_items.items[i];
        if (item.structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
          item.structure_type = NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_DYNAMIC;
        }
      }
    }
  }
}

static void versioning_replace_legacy_combined_and_separate_color_nodes(bNodeTree *ntree)
{
  /* In geometry nodes, replace shader combine/separate color nodes with function nodes */
  if (ntree->type == NTREE_GEOMETRY) {
    version_node_input_socket_name(ntree, SH_NODE_COMBRGB_LEGACY, "R", "Red");
    version_node_input_socket_name(ntree, SH_NODE_COMBRGB_LEGACY, "G", "Green");
    version_node_input_socket_name(ntree, SH_NODE_COMBRGB_LEGACY, "B", "Blue");
    version_node_output_socket_name(ntree, SH_NODE_COMBRGB_LEGACY, "Image", "Color");

    version_node_output_socket_name(ntree, SH_NODE_SEPRGB_LEGACY, "R", "Red");
    version_node_output_socket_name(ntree, SH_NODE_SEPRGB_LEGACY, "G", "Green");
    version_node_output_socket_name(ntree, SH_NODE_SEPRGB_LEGACY, "B", "Blue");
    version_node_input_socket_name(ntree, SH_NODE_SEPRGB_LEGACY, "Image", "Color");

    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      switch (node->type_legacy) {
        case SH_NODE_COMBRGB_LEGACY: {
          node->type_legacy = FN_NODE_COMBINE_COLOR;
          NodeCombSepColor *storage = MEM_callocN<NodeCombSepColor>(__func__);
          storage->mode = NODE_COMBSEP_COLOR_RGB;
          STRNCPY_UTF8(node->idname, "FunctionNodeCombineColor");
          node->storage = storage;
          break;
        }
        case SH_NODE_SEPRGB_LEGACY: {
          node->type_legacy = FN_NODE_SEPARATE_COLOR;
          NodeCombSepColor *storage = MEM_callocN<NodeCombSepColor>(__func__);
          storage->mode = NODE_COMBSEP_COLOR_RGB;
          STRNCPY_UTF8(node->idname, "FunctionNodeSeparateColor");

          node->storage = storage;
          break;
        }
      }
    }
  }

  /* In compositing nodes, replace combine/separate RGBA/HSVA/YCbCrA/YCCA nodes with
   * combine/separate color */
  if (ntree->type == NTREE_COMPOSIT) {
    version_node_input_socket_name(ntree, CMP_NODE_COMBRGBA_LEGACY, "R", "Red");
    version_node_input_socket_name(ntree, CMP_NODE_COMBRGBA_LEGACY, "G", "Green");
    version_node_input_socket_name(ntree, CMP_NODE_COMBRGBA_LEGACY, "B", "Blue");
    version_node_input_socket_name(ntree, CMP_NODE_COMBRGBA_LEGACY, "A", "Alpha");

    version_node_input_socket_name(ntree, CMP_NODE_COMBHSVA_LEGACY, "H", "Red");
    version_node_input_socket_name(ntree, CMP_NODE_COMBHSVA_LEGACY, "S", "Green");
    version_node_input_socket_name(ntree, CMP_NODE_COMBHSVA_LEGACY, "V", "Blue");
    version_node_input_socket_name(ntree, CMP_NODE_COMBHSVA_LEGACY, "A", "Alpha");

    version_node_input_socket_name(ntree, CMP_NODE_COMBYCCA_LEGACY, "Y", "Red");
    version_node_input_socket_name(ntree, CMP_NODE_COMBYCCA_LEGACY, "Cb", "Green");
    version_node_input_socket_name(ntree, CMP_NODE_COMBYCCA_LEGACY, "Cr", "Blue");
    version_node_input_socket_name(ntree, CMP_NODE_COMBYCCA_LEGACY, "A", "Alpha");

    version_node_input_socket_name(ntree, CMP_NODE_COMBYUVA_LEGACY, "Y", "Red");
    version_node_input_socket_name(ntree, CMP_NODE_COMBYUVA_LEGACY, "U", "Green");
    version_node_input_socket_name(ntree, CMP_NODE_COMBYUVA_LEGACY, "V", "Blue");
    version_node_input_socket_name(ntree, CMP_NODE_COMBYUVA_LEGACY, "A", "Alpha");

    version_node_output_socket_name(ntree, CMP_NODE_SEPRGBA_LEGACY, "R", "Red");
    version_node_output_socket_name(ntree, CMP_NODE_SEPRGBA_LEGACY, "G", "Green");
    version_node_output_socket_name(ntree, CMP_NODE_SEPRGBA_LEGACY, "B", "Blue");
    version_node_output_socket_name(ntree, CMP_NODE_SEPRGBA_LEGACY, "A", "Alpha");

    version_node_output_socket_name(ntree, CMP_NODE_SEPHSVA_LEGACY, "H", "Red");
    version_node_output_socket_name(ntree, CMP_NODE_SEPHSVA_LEGACY, "S", "Green");
    version_node_output_socket_name(ntree, CMP_NODE_SEPHSVA_LEGACY, "V", "Blue");
    version_node_output_socket_name(ntree, CMP_NODE_SEPHSVA_LEGACY, "A", "Alpha");

    version_node_output_socket_name(ntree, CMP_NODE_SEPYCCA_LEGACY, "Y", "Red");
    version_node_output_socket_name(ntree, CMP_NODE_SEPYCCA_LEGACY, "Cb", "Green");
    version_node_output_socket_name(ntree, CMP_NODE_SEPYCCA_LEGACY, "Cr", "Blue");
    version_node_output_socket_name(ntree, CMP_NODE_SEPYCCA_LEGACY, "A", "Alpha");

    version_node_output_socket_name(ntree, CMP_NODE_SEPYUVA_LEGACY, "Y", "Red");
    version_node_output_socket_name(ntree, CMP_NODE_SEPYUVA_LEGACY, "U", "Green");
    version_node_output_socket_name(ntree, CMP_NODE_SEPYUVA_LEGACY, "V", "Blue");
    version_node_output_socket_name(ntree, CMP_NODE_SEPYUVA_LEGACY, "A", "Alpha");

    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      switch (node->type_legacy) {
        case CMP_NODE_COMBRGBA_LEGACY: {
          node->type_legacy = CMP_NODE_COMBINE_COLOR;
          NodeCMPCombSepColor *storage = MEM_callocN<NodeCMPCombSepColor>(__func__);
          storage->mode = CMP_NODE_COMBSEP_COLOR_RGB;
          STRNCPY_UTF8(node->idname, "CompositorNodeCombineColor");
          node->storage = storage;
          break;
        }
        case CMP_NODE_COMBHSVA_LEGACY: {
          node->type_legacy = CMP_NODE_COMBINE_COLOR;
          NodeCMPCombSepColor *storage = MEM_callocN<NodeCMPCombSepColor>(__func__);
          storage->mode = CMP_NODE_COMBSEP_COLOR_HSV;
          STRNCPY_UTF8(node->idname, "CompositorNodeCombineColor");
          node->storage = storage;
          break;
        }
        case CMP_NODE_COMBYCCA_LEGACY: {
          node->type_legacy = CMP_NODE_COMBINE_COLOR;
          NodeCMPCombSepColor *storage = MEM_callocN<NodeCMPCombSepColor>(__func__);
          storage->mode = CMP_NODE_COMBSEP_COLOR_YCC;
          storage->ycc_mode = node->custom1;
          STRNCPY_UTF8(node->idname, "CompositorNodeCombineColor");
          node->storage = storage;
          break;
        }
        case CMP_NODE_COMBYUVA_LEGACY: {
          node->type_legacy = CMP_NODE_COMBINE_COLOR;
          NodeCMPCombSepColor *storage = MEM_callocN<NodeCMPCombSepColor>(__func__);
          storage->mode = CMP_NODE_COMBSEP_COLOR_YUV;
          STRNCPY_UTF8(node->idname, "CompositorNodeCombineColor");
          node->storage = storage;
          break;
        }
        case CMP_NODE_SEPRGBA_LEGACY: {
          node->type_legacy = CMP_NODE_SEPARATE_COLOR;
          NodeCMPCombSepColor *storage = MEM_callocN<NodeCMPCombSepColor>(__func__);
          storage->mode = CMP_NODE_COMBSEP_COLOR_RGB;
          STRNCPY_UTF8(node->idname, "CompositorNodeSeparateColor");
          node->storage = storage;
          break;
        }
        case CMP_NODE_SEPHSVA_LEGACY: {
          node->type_legacy = CMP_NODE_SEPARATE_COLOR;
          NodeCMPCombSepColor *storage = MEM_callocN<NodeCMPCombSepColor>(__func__);
          storage->mode = CMP_NODE_COMBSEP_COLOR_HSV;
          STRNCPY_UTF8(node->idname, "CompositorNodeSeparateColor");
          node->storage = storage;
          break;
        }
        case CMP_NODE_SEPYCCA_LEGACY: {
          node->type_legacy = CMP_NODE_SEPARATE_COLOR;
          NodeCMPCombSepColor *storage = MEM_callocN<NodeCMPCombSepColor>(__func__);
          storage->mode = CMP_NODE_COMBSEP_COLOR_YCC;
          storage->ycc_mode = node->custom1;
          STRNCPY_UTF8(node->idname, "CompositorNodeSeparateColor");
          node->storage = storage;
          break;
        }
        case CMP_NODE_SEPYUVA_LEGACY: {
          node->type_legacy = CMP_NODE_SEPARATE_COLOR;
          NodeCMPCombSepColor *storage = MEM_callocN<NodeCMPCombSepColor>(__func__);
          storage->mode = CMP_NODE_COMBSEP_COLOR_YUV;
          STRNCPY_UTF8(node->idname, "CompositorNodeSeparateColor");
          node->storage = storage;
          break;
        }
      }
    }
  }

  /* In texture nodes, replace combine/separate RGBA with combine/separate color */
  if (ntree->type == NTREE_TEXTURE) {
    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      switch (node->type_legacy) {
        case TEX_NODE_COMPOSE_LEGACY: {
          node->type_legacy = TEX_NODE_COMBINE_COLOR;
          node->custom1 = NODE_COMBSEP_COLOR_RGB;
          STRNCPY_UTF8(node->idname, "TextureNodeCombineColor");
          break;
        }
        case TEX_NODE_DECOMPOSE_LEGACY: {
          node->type_legacy = TEX_NODE_SEPARATE_COLOR;
          node->custom1 = NODE_COMBSEP_COLOR_RGB;
          STRNCPY_UTF8(node->idname, "TextureNodeSeparateColor");
          break;
        }
      }
    }
  }

  /* In shader nodes, replace combine/separate RGB/HSV with combine/separate color */
  if (ntree->type == NTREE_SHADER) {
    version_node_input_socket_name(ntree, SH_NODE_COMBRGB_LEGACY, "R", "Red");
    version_node_input_socket_name(ntree, SH_NODE_COMBRGB_LEGACY, "G", "Green");
    version_node_input_socket_name(ntree, SH_NODE_COMBRGB_LEGACY, "B", "Blue");
    version_node_output_socket_name(ntree, SH_NODE_COMBRGB_LEGACY, "Image", "Color");

    version_node_input_socket_name(ntree, SH_NODE_COMBHSV_LEGACY, "H", "Red");
    version_node_input_socket_name(ntree, SH_NODE_COMBHSV_LEGACY, "S", "Green");
    version_node_input_socket_name(ntree, SH_NODE_COMBHSV_LEGACY, "V", "Blue");

    version_node_output_socket_name(ntree, SH_NODE_SEPRGB_LEGACY, "R", "Red");
    version_node_output_socket_name(ntree, SH_NODE_SEPRGB_LEGACY, "G", "Green");
    version_node_output_socket_name(ntree, SH_NODE_SEPRGB_LEGACY, "B", "Blue");
    version_node_input_socket_name(ntree, SH_NODE_SEPRGB_LEGACY, "Image", "Color");

    version_node_output_socket_name(ntree, SH_NODE_SEPHSV_LEGACY, "H", "Red");
    version_node_output_socket_name(ntree, SH_NODE_SEPHSV_LEGACY, "S", "Green");
    version_node_output_socket_name(ntree, SH_NODE_SEPHSV_LEGACY, "V", "Blue");

    LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
      switch (node->type_legacy) {
        case SH_NODE_COMBRGB_LEGACY: {
          node->type_legacy = SH_NODE_COMBINE_COLOR;
          NodeCombSepColor *storage = MEM_callocN<NodeCombSepColor>(__func__);
          storage->mode = NODE_COMBSEP_COLOR_RGB;
          STRNCPY_UTF8(node->idname, "ShaderNodeCombineColor");
          node->storage = storage;
          break;
        }
        case SH_NODE_COMBHSV_LEGACY: {
          node->type_legacy = SH_NODE_COMBINE_COLOR;
          NodeCombSepColor *storage = MEM_callocN<NodeCombSepColor>(__func__);
          storage->mode = NODE_COMBSEP_COLOR_HSV;
          STRNCPY_UTF8(node->idname, "ShaderNodeCombineColor");
          node->storage = storage;
          break;
        }
        case SH_NODE_SEPRGB_LEGACY: {
          node->type_legacy = SH_NODE_SEPARATE_COLOR;
          NodeCombSepColor *storage = MEM_callocN<NodeCombSepColor>(__func__);
          storage->mode = NODE_COMBSEP_COLOR_RGB;
          STRNCPY_UTF8(node->idname, "ShaderNodeSeparateColor");
          node->storage = storage;
          break;
        }
        case SH_NODE_SEPHSV_LEGACY: {
          node->type_legacy = SH_NODE_SEPARATE_COLOR;
          NodeCombSepColor *storage = MEM_callocN<NodeCombSepColor>(__func__);
          storage->mode = NODE_COMBSEP_COLOR_HSV;
          STRNCPY_UTF8(node->idname, "ShaderNodeSeparateColor");
          node->storage = storage;
          break;
        }
      }
    }
  }
}

/* "Use Nodes" was removed. */
static void do_version_scene_remove_use_nodes(Scene *scene)
{
  if (scene->nodetree == nullptr && scene->compositing_node_group == nullptr) {
    /* scene->use_nodes is set to false by default. Files saved without compositing node trees
     * should not disable compositing. */
    return;
  }
  if (scene->use_nodes == false && scene->r.scemode & R_DOCOMP) {
    /* A compositing node tree exists but users explicitly disabled compositing. */
    scene->r.scemode &= ~R_DOCOMP;
  }
  /* Ignore use_nodes otherwise. */
}

/* The Dot output of the Normal node was removed, so replace it with a dot product vector math
 * node, noting that the Dot output was actually negative the dot product of the normalized
 * node vector with the input. */
static void do_version_normal_node_dot_product(bNodeTree *node_tree, bNode *node)
{
  bNodeSocket *normal_input = blender::bke::node_find_socket(*node, SOCK_IN, "Normal");
  bNodeSocket *normal_output = blender::bke::node_find_socket(*node, SOCK_OUT, "Normal");
  bNodeSocket *dot_output = blender::bke::node_find_socket(*node, SOCK_OUT, "Dot");

  /* Find the links going into and out from the node. */
  bNodeLink *normal_input_link = nullptr;
  bool is_normal_ontput_needed = false;
  bool is_dot_output_used = false;
  LISTBASE_FOREACH (bNodeLink *, link, &node_tree->links) {
    if (link->tosock == normal_input) {
      normal_input_link = link;
    }

    if (link->fromsock == normal_output) {
      is_normal_ontput_needed = true;
    }

    if (link->fromsock == dot_output) {
      is_dot_output_used = true;
    }
  }

  /* The dot output is unused, nothing to do. */
  if (!is_dot_output_used) {
    return;
  }

  /* Take the dot product with negative the node normal. */
  bNode *dot_product_node = blender::bke::node_add_node(
      nullptr, *node_tree, "ShaderNodeVectorMath");
  dot_product_node->custom1 = NODE_VECTOR_MATH_DOT_PRODUCT;
  dot_product_node->flag |= NODE_COLLAPSED;
  dot_product_node->parent = node->parent;
  dot_product_node->location[0] = node->location[0];
  dot_product_node->location[1] = node->location[1];

  bNodeSocket *dot_product_a_input = blender::bke::node_find_socket(
      *dot_product_node, SOCK_IN, "Vector");
  bNodeSocket *dot_product_b_input = blender::bke::node_find_socket(
      *dot_product_node, SOCK_IN, "Vector_001");
  bNodeSocket *dot_product_output = blender::bke::node_find_socket(
      *dot_product_node, SOCK_OUT, "Value");

  copy_v3_v3(static_cast<bNodeSocketValueVector *>(dot_product_a_input->default_value)->value,
             static_cast<bNodeSocketValueVector *>(normal_input->default_value)->value);

  if (normal_input_link) {
    version_node_add_link(*node_tree,
                          *normal_input_link->fromnode,
                          *normal_input_link->fromsock,
                          *dot_product_node,
                          *dot_product_a_input);
    blender::bke::node_remove_link(node_tree, *normal_input_link);
  }

  /* Notice that we normalize and take the negative to reproduce the same behavior as the old
   * Normal node. */
  const blender::float3 node_normal =
      normal_output->default_value_typed<bNodeSocketValueVector>()->value;
  const blender::float3 normalized_node_normal = -blender::math::normalize(node_normal);
  copy_v3_v3(static_cast<bNodeSocketValueVector *>(dot_product_b_input->default_value)->value,
             normalized_node_normal);

  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &node_tree->links) {
    if (link->fromsock != dot_output) {
      continue;
    }

    version_node_add_link(
        *node_tree, *dot_product_node, *dot_product_output, *link->tonode, *link->tosock);
    blender::bke::node_remove_link(node_tree, *link);
  }

  /* If only the Dot output was used, remove the node, making sure to initialize the node types to
   * allow removal. */
  if (!is_normal_ontput_needed) {
    blender::bke::node_tree_set_type(*node_tree);
    version_node_remove(*node_tree, *node);
  }
}

static void do_version_transform_geometry_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Mode")) {
    return;
  }
  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Mode");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;
}

static void do_version_points_to_volume_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Resolution Mode")) {
    return;
  }
  const NodeGeometryPointsToVolume &storage = *static_cast<NodeGeometryPointsToVolume *>(
      node.storage);
  bNodeSocket &socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Resolution Mode");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.resolution_mode;
}

static void do_version_triangulate_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (!blender::bke::node_find_socket(node, SOCK_IN, "Quad Method")) {
    bNodeSocket &socket = version_node_add_socket(
        ntree, node, SOCK_IN, "NodeSocketMenu", "Quad Method");
    socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;
  }
  if (!blender::bke::node_find_socket(node, SOCK_IN, "N-gon Method")) {
    bNodeSocket &socket = version_node_add_socket(
        ntree, node, SOCK_IN, "NodeSocketMenu", "N-gon Method");
    socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom2;
  }
}

static void do_version_volume_to_mesh_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Resolution Mode")) {
    return;
  }
  const NodeGeometryVolumeToMesh &storage = *static_cast<NodeGeometryVolumeToMesh *>(node.storage);
  bNodeSocket &socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Resolution Mode");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.resolution_mode;
}

static void do_version_match_string_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Operation")) {
    return;
  }
  bNodeSocket &socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Operation");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;
}

static void version_seq_text_from_legacy(Main *bmain)
{
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    if (scene->ed != nullptr) {
      blender::seq::for_each_callback(&scene->ed->seqbase, [&](Strip *strip) -> bool {
        if (strip->type == STRIP_TYPE_TEXT && strip->effectdata != nullptr) {
          TextVars *data = static_cast<TextVars *>(strip->effectdata);
          if (data->text_ptr == nullptr) {
            data->text_ptr = BLI_strdup(data->text_legacy);
            data->text_len_bytes = strlen(data->text_ptr);
          }
        }
        return true;
      });
    }
  }
}

static void apply_unified_paint_settings_to_all_modes(Scene &scene)
{
  const UnifiedPaintSettings &scene_ups = scene.toolsettings->unified_paint_settings;
  auto apply_to_paint = [&](Paint *paint) {
    if (paint == nullptr) {
      return;
    }
    UnifiedPaintSettings &ups = paint->unified_paint_settings;

    ups.size = scene_ups.size;
    ups.unprojected_radius = scene_ups.unprojected_radius;
    ups.alpha = scene_ups.alpha;
    ups.weight = scene_ups.weight;
    copy_v3_v3(ups.rgb, scene_ups.rgb);
    copy_v3_v3(ups.secondary_rgb, scene_ups.secondary_rgb);
    ups.color_jitter_flag = scene_ups.color_jitter_flag;
    copy_v3_v3(ups.hsv_jitter, scene_ups.hsv_jitter);

    BLI_assert(ups.curve_rand_hue == nullptr);
    BLI_assert(ups.curve_rand_saturation == nullptr);
    BLI_assert(ups.curve_rand_value == nullptr);
    ups.curve_rand_hue = BKE_curvemapping_copy(scene_ups.curve_rand_hue);
    ups.curve_rand_saturation = BKE_curvemapping_copy(scene_ups.curve_rand_saturation);
    ups.curve_rand_value = BKE_curvemapping_copy(scene_ups.curve_rand_value);
    ups.flag = scene_ups.flag;
  };

  apply_to_paint(reinterpret_cast<Paint *>(scene.toolsettings->vpaint));
  apply_to_paint(reinterpret_cast<Paint *>(scene.toolsettings->wpaint));
  apply_to_paint(reinterpret_cast<Paint *>(scene.toolsettings->sculpt));
  apply_to_paint(reinterpret_cast<Paint *>(scene.toolsettings->gp_paint));
  apply_to_paint(reinterpret_cast<Paint *>(scene.toolsettings->gp_vertexpaint));
  apply_to_paint(reinterpret_cast<Paint *>(scene.toolsettings->gp_sculptpaint));
  apply_to_paint(reinterpret_cast<Paint *>(scene.toolsettings->gp_weightpaint));
  apply_to_paint(reinterpret_cast<Paint *>(scene.toolsettings->curves_sculpt));
  apply_to_paint(reinterpret_cast<Paint *>(&scene.toolsettings->imapaint));
}

/* The Use Alpha option is does not exist in the new generic Mix node, it essentially just
 * multiplied the factor by the alpha of the second input. */
static void do_version_mix_color_use_alpha(bNodeTree *node_tree, bNode *node)
{
  if (!(node->custom2 & SHD_MIXRGB_USE_ALPHA)) {
    return;
  }

  blender::bke::node_tree_set_type(*node_tree);

  bNodeSocket *factor_input = blender::bke::node_find_socket(*node, SOCK_IN, "Factor_Float");
  bNodeSocket *b_input = blender::bke::node_find_socket(*node, SOCK_IN, "B_Color");

  /* Find the links going into the factor and B input of the Mix node. */
  bNodeLink *factor_link = nullptr;
  bNodeLink *b_link = nullptr;
  LISTBASE_FOREACH (bNodeLink *, link, &node_tree->links) {
    if (link->tosock == factor_input) {
      factor_link = link;
    }
    else if (link->tosock == b_input) {
      b_link = link;
    }
  }

  /* If neither sockets are connected, just multiply the factor by the alpha of the B input. */
  if (!factor_link && !b_link) {
    static_cast<bNodeSocketValueFloat *>(factor_input->default_value)->value *=
        static_cast<bNodeSocketValueRGBA *>(b_input->default_value)->value[3];
    return;
  }

  /* Otherwise, add a multiply node to do the multiplication. */
  bNode *multiply_node = blender::bke::node_add_static_node(nullptr, *node_tree, SH_NODE_MATH);
  multiply_node->parent = node->parent;
  multiply_node->custom1 = NODE_MATH_MULTIPLY;
  multiply_node->location[0] = node->location[0] - node->width - 20.0f;
  multiply_node->location[1] = node->location[1];
  multiply_node->flag |= NODE_COLLAPSED;

  bNodeSocket *multiply_input_a = static_cast<bNodeSocket *>(
      BLI_findlink(&multiply_node->inputs, 0));
  bNodeSocket *multiply_input_b = static_cast<bNodeSocket *>(
      BLI_findlink(&multiply_node->inputs, 1));
  bNodeSocket *multiply_output = blender::bke::node_find_socket(*multiply_node, SOCK_OUT, "Value");

  /* Connect the output of the multiply node to the math node. */
  version_node_add_link(*node_tree, *multiply_node, *multiply_output, *node, *factor_input);

  if (factor_link) {
    /* The factor input is linked, so connect its origin to the first input of the multiply and
     * remove the original link. */
    version_node_add_link(*node_tree,
                          *factor_link->fromnode,
                          *factor_link->fromsock,
                          *multiply_node,
                          *multiply_input_a);
    blender::bke::node_remove_link(node_tree, *factor_link);
  }
  else {
    /* Otherwise, the factor is unlinked and we just copy the factor value to the first input in
     * the multiply. */
    static_cast<bNodeSocketValueFloat *>(multiply_input_a->default_value)->value =
        static_cast<bNodeSocketValueFloat *>(factor_input->default_value)->value;
  }

  if (b_link) {
    /* The B input is linked, so extract the alpha of its origin and connect it to the second input
     * of the multiply and remove the original link. */
    bNode *separate_color_node = blender::bke::node_add_static_node(
        nullptr, *node_tree, CMP_NODE_SEPARATE_COLOR);
    separate_color_node->parent = node->parent;
    separate_color_node->location[0] = multiply_node->location[0] - multiply_node->width - 20.0f;
    separate_color_node->location[1] = multiply_node->location[1];
    separate_color_node->flag |= NODE_COLLAPSED;

    bNodeSocket *image_input = blender::bke::node_find_socket(
        *separate_color_node, SOCK_IN, "Image");
    bNodeSocket *alpha_output = blender::bke::node_find_socket(
        *separate_color_node, SOCK_OUT, "Alpha");

    version_node_add_link(
        *node_tree, *b_link->fromnode, *b_link->fromsock, *separate_color_node, *image_input);
    version_node_add_link(
        *node_tree, *separate_color_node, *alpha_output, *multiply_node, *multiply_input_b);
  }
  else {
    /* Otherwise, the B input is unlinked and we just copy the alpha value to the second input in
     * the multiply. */
    static_cast<bNodeSocketValueFloat *>(multiply_input_b->default_value)->value =
        static_cast<bNodeSocketValueRGBA *>(b_input->default_value)->value[3];
  }

  version_socket_update_is_used(node_tree);
}

/* The Map Value node is now deprecated and should be replaced by other nodes. The node essentially
 * just computes (value + offset) * size and clamps based on min and max. */
static void do_version_map_value_node(bNodeTree *node_tree, bNode *node)
{
  blender::bke::node_tree_set_type(*node_tree);

  const TexMapping &texture_mapping = *static_cast<TexMapping *>(node->storage);
  const bool use_min = texture_mapping.flag & TEXMAP_CLIP_MIN;
  const bool use_max = texture_mapping.flag & TEXMAP_CLIP_MAX;
  const float offset = texture_mapping.loc[0];
  const float size = texture_mapping.size[0];
  const float min = texture_mapping.min[0];
  const float max = texture_mapping.max[0];

  bNodeSocket *value_input = blender::bke::node_find_socket(*node, SOCK_IN, "Value");

  /* Find the link going into the value input Map Value node. */
  bNodeLink *value_link = nullptr;
  LISTBASE_FOREACH (bNodeLink *, link, &node_tree->links) {
    if (link->tosock == value_input) {
      value_link = link;
    }
  }

  /* If the value input is not connected, add a value node with the computed value. */
  if (!value_link) {
    const float value = static_cast<bNodeSocketValueFloat *>(value_input->default_value)->value;
    const float mapped_value = (value + offset) * size;
    const float min_clamped_value = use_min ? blender::math::max(mapped_value, min) : mapped_value;
    const float clamped_value = use_max ? blender::math::min(min_clamped_value, max) :
                                          min_clamped_value;

    bNode *value_node = blender::bke::node_add_static_node(nullptr, *node_tree, SH_NODE_VALUE);
    value_node->parent = node->parent;
    value_node->location[0] = node->location[0];
    value_node->location[1] = node->location[1];

    bNodeSocket *value_output = blender::bke::node_find_socket(*value_node, SOCK_OUT, "Value");
    static_cast<bNodeSocketValueFloat *>(value_output->default_value)->value = clamped_value;

    /* Relink from the Map Value node to the value node. */
    LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &node_tree->links) {
      if (link->fromnode != node) {
        continue;
      }

      version_node_add_link(*node_tree, *value_node, *value_output, *link->tonode, *link->tosock);
      blender::bke::node_remove_link(node_tree, *link);
    }

    MEM_freeN(&texture_mapping);
    node->storage = nullptr;

    blender::bke::node_remove_node(nullptr, *node_tree, *node, false);

    version_socket_update_is_used(node_tree);
    return;
  }

  /* Otherwise, add math nodes to do the computation, starting with an add node to add the offset
   * of the range. */
  bNode *add_node = blender::bke::node_add_static_node(nullptr, *node_tree, SH_NODE_MATH);
  add_node->parent = node->parent;
  add_node->custom1 = NODE_MATH_ADD;
  add_node->location[0] = node->location[0];
  add_node->location[1] = node->location[1];
  add_node->flag |= NODE_COLLAPSED;

  bNodeSocket *add_input_a = static_cast<bNodeSocket *>(BLI_findlink(&add_node->inputs, 0));
  bNodeSocket *add_input_b = static_cast<bNodeSocket *>(BLI_findlink(&add_node->inputs, 1));
  bNodeSocket *add_output = blender::bke::node_find_socket(*add_node, SOCK_OUT, "Value");

  /* Connect the origin of the node to the first input of the add node and remove the original
   * link. */
  version_node_add_link(
      *node_tree, *value_link->fromnode, *value_link->fromsock, *add_node, *add_input_a);
  blender::bke::node_remove_link(node_tree, *value_link);

  /* Set the offset to the second input of the add node. */
  static_cast<bNodeSocketValueFloat *>(add_input_b->default_value)->value = offset;

  /* Add a multiply node to multiply by the size. */
  bNode *multiply_node = blender::bke::node_add_static_node(nullptr, *node_tree, SH_NODE_MATH);
  multiply_node->parent = node->parent;
  multiply_node->custom1 = NODE_MATH_MULTIPLY;
  multiply_node->location[0] = add_node->location[0];
  multiply_node->location[1] = add_node->location[1] - 40.0f;
  multiply_node->flag |= NODE_COLLAPSED;

  bNodeSocket *multiply_input_a = static_cast<bNodeSocket *>(
      BLI_findlink(&multiply_node->inputs, 0));
  bNodeSocket *multiply_input_b = static_cast<bNodeSocket *>(
      BLI_findlink(&multiply_node->inputs, 1));
  bNodeSocket *multiply_output = blender::bke::node_find_socket(*multiply_node, SOCK_OUT, "Value");

  /* Connect the output of the add node to the first input of the multiply node. */
  version_node_add_link(*node_tree, *add_node, *add_output, *multiply_node, *multiply_input_a);

  /* Set the size to the second input of the multiply node. */
  static_cast<bNodeSocketValueFloat *>(multiply_input_b->default_value)->value = size;

  bNode *final_node = multiply_node;
  bNodeSocket *final_output = multiply_output;

  if (use_min) {
    /* Add a maximum node to clamp by the minimum. */
    bNode *max_node = blender::bke::node_add_static_node(nullptr, *node_tree, SH_NODE_MATH);
    max_node->parent = node->parent;
    max_node->custom1 = NODE_MATH_MAXIMUM;
    max_node->location[0] = final_node->location[0];
    max_node->location[1] = final_node->location[1] - 40.0f;
    max_node->flag |= NODE_COLLAPSED;

    bNodeSocket *max_input_a = static_cast<bNodeSocket *>(BLI_findlink(&max_node->inputs, 0));
    bNodeSocket *max_input_b = static_cast<bNodeSocket *>(BLI_findlink(&max_node->inputs, 1));
    bNodeSocket *max_output = blender::bke::node_find_socket(*max_node, SOCK_OUT, "Value");

    /* Connect the output of the final node to the first input of the maximum node. */
    version_node_add_link(*node_tree, *final_node, *final_output, *max_node, *max_input_a);

    /* Set the minimum to the second input of the maximum node. */
    static_cast<bNodeSocketValueFloat *>(max_input_b->default_value)->value = min;

    final_node = max_node;
    final_output = max_output;
  }

  if (use_max) {
    /* Add a minimum node to clamp by the maximum. */
    bNode *min_node = blender::bke::node_add_static_node(nullptr, *node_tree, SH_NODE_MATH);
    min_node->parent = node->parent;
    min_node->custom1 = NODE_MATH_MINIMUM;
    min_node->location[0] = final_node->location[0];
    min_node->location[1] = final_node->location[1] - 40.0f;
    min_node->flag |= NODE_COLLAPSED;

    bNodeSocket *min_input_a = static_cast<bNodeSocket *>(BLI_findlink(&min_node->inputs, 0));
    bNodeSocket *min_input_b = static_cast<bNodeSocket *>(BLI_findlink(&min_node->inputs, 1));
    bNodeSocket *min_output = blender::bke::node_find_socket(*min_node, SOCK_OUT, "Value");

    /* Connect the output of the final node to the first input of the minimum node. */
    version_node_add_link(*node_tree, *final_node, *final_output, *min_node, *min_input_a);

    /* Set the maximum to the second input of the minimum node. */
    static_cast<bNodeSocketValueFloat *>(min_input_b->default_value)->value = max;

    final_node = min_node;
    final_output = min_output;
  }

  /* Relink from the Map Value node to the final node. */
  LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &node_tree->links) {
    if (link->fromnode != node) {
      continue;
    }

    version_node_add_link(*node_tree, *final_node, *final_output, *link->tonode, *link->tosock);
    blender::bke::node_remove_link(node_tree, *link);
  }

  MEM_freeN(&texture_mapping);
  node->storage = nullptr;

  blender::bke::node_remove_node(nullptr, *node_tree, *node, false);

  version_socket_update_is_used(node_tree);
}

/* The compositor Value, Color Ramp, Mix Color, Map Range, Map Value, Math, Combine XYZ, Separate
 * XYZ, and Vector Curves nodes are now deprecated and should be replaced by their generic Shader
 * node counterpart. */
static void do_version_convert_to_generic_nodes(bNodeTree *node_tree)
{
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &node_tree->nodes) {
    switch (node->type_legacy) {
      case CMP_NODE_VALUE_DEPRECATED:
        node->type_legacy = SH_NODE_VALUE;
        STRNCPY_UTF8(node->idname, "ShaderNodeValue");
        break;
      case CMP_NODE_MATH_DEPRECATED:
        node->type_legacy = SH_NODE_MATH;
        STRNCPY_UTF8(node->idname, "ShaderNodeMath");
        break;
      case CMP_NODE_COMBINE_XYZ_DEPRECATED:
        node->type_legacy = SH_NODE_COMBXYZ;
        STRNCPY_UTF8(node->idname, "ShaderNodeCombineXYZ");
        break;
      case CMP_NODE_SEPARATE_XYZ_DEPRECATED:
        node->type_legacy = SH_NODE_SEPXYZ;
        STRNCPY_UTF8(node->idname, "ShaderNodeSeparateXYZ");
        break;
      case CMP_NODE_CURVE_VEC_DEPRECATED:
        node->type_legacy = SH_NODE_CURVE_VEC;
        STRNCPY_UTF8(node->idname, "ShaderNodeVectorCurve");
        break;
      case CMP_NODE_VALTORGB_DEPRECATED: {
        node->type_legacy = SH_NODE_VALTORGB;
        STRNCPY_UTF8(node->idname, "ShaderNodeValToRGB");

        /* Compositor node uses "Image" as the output name while the shader node uses "Color" as
         * the output name. */
        bNodeSocket *image_output = blender::bke::node_find_socket(*node, SOCK_OUT, "Image");
        STRNCPY_UTF8(image_output->identifier, "Color");
        STRNCPY_UTF8(image_output->name, "Color");

        break;
      }
      case CMP_NODE_MAP_RANGE_DEPRECATED: {
        node->type_legacy = SH_NODE_MAP_RANGE;
        STRNCPY_UTF8(node->idname, "ShaderNodeMapRange");

        /* Transfer options from node to NodeMapRange storage. */
        NodeMapRange *data = MEM_callocN<NodeMapRange>(__func__);
        data->clamp = node->custom1;
        data->data_type = CD_PROP_FLOAT;
        data->interpolation_type = NODE_MAP_RANGE_LINEAR;
        node->storage = data;

        /* Compositor node uses "Value" as the output name while the shader node uses "Result" as
         * the output name. */
        bNodeSocket *value_output = blender::bke::node_find_socket(*node, SOCK_OUT, "Value");
        STRNCPY_UTF8(value_output->identifier, "Result");
        STRNCPY_UTF8(value_output->name, "Result");

        break;
      }
      case CMP_NODE_MIX_RGB_DEPRECATED: {
        node->type_legacy = SH_NODE_MIX;
        STRNCPY_UTF8(node->idname, "ShaderNodeMix");

        /* Transfer options from node to NodeShaderMix storage. */
        NodeShaderMix *data = MEM_callocN<NodeShaderMix>(__func__);
        data->data_type = SOCK_RGBA;
        data->factor_mode = NODE_MIX_MODE_UNIFORM;
        data->clamp_factor = 0;
        data->clamp_result = node->custom2 & SHD_MIXRGB_CLAMP ? 1 : 0;
        data->blend_type = node->custom1;
        node->storage = data;

        /* Compositor node uses "Fac", "Image", and ("Image" "Image_001") as socket names and
         * identifiers while the shader node uses ("Factor", "Factor_Float"), ("A", "A_Color"),
         * ("B", "B_Color"), and ("Result", "Result_Color") as socket names and identifiers. */
        bNodeSocket *factor_input = blender::bke::node_find_socket(*node, SOCK_IN, "Fac");
        STRNCPY_UTF8(factor_input->identifier, "Factor_Float");
        STRNCPY_UTF8(factor_input->name, "Factor");
        bNodeSocket *first_input = blender::bke::node_find_socket(*node, SOCK_IN, "Image");
        STRNCPY_UTF8(first_input->identifier, "A_Color");
        STRNCPY_UTF8(first_input->name, "A");
        bNodeSocket *second_input = blender::bke::node_find_socket(*node, SOCK_IN, "Image_001");
        STRNCPY_UTF8(second_input->identifier, "B_Color");
        STRNCPY_UTF8(second_input->name, "B");
        bNodeSocket *image_output = blender::bke::node_find_socket(*node, SOCK_OUT, "Image");
        STRNCPY_UTF8(image_output->identifier, "Result_Color");
        STRNCPY_UTF8(image_output->name, "Result");

        do_version_mix_color_use_alpha(node_tree, node);

        break;
      }
      case CMP_NODE_MAP_VALUE_DEPRECATED: {
        do_version_map_value_node(node_tree, node);
        break;
      }
      default:
        break;
    }
  }
}

/* Equivalent to do_version_convert_to_generic_nodes but performed after linking for handing things
 * like animation or node construction. */
static void do_version_convert_to_generic_nodes_after_linking(Main *bmain,
                                                              bNodeTree *node_tree,
                                                              ID *id)
{
  LISTBASE_FOREACH_MUTABLE (bNode *, node, &node_tree->nodes) {
    char escaped_node_name[sizeof(node->name) * 2 + 1];
    BLI_str_escape(escaped_node_name, node->name, sizeof(escaped_node_name));
    const std::string rna_path_prefix = fmt::format("nodes[\"{}\"].inputs", escaped_node_name);

    switch (node->type_legacy) {
      /* Notice that we use the shader type because the node is already converted in versioning
       * before linking. */
      case SH_NODE_CURVE_VEC: {
        /* The node gained a new Factor input as a first socket, so the vector socket moved to be
         * the second socket and we need to transfer its animation as well. */
        BKE_animdata_fix_paths_rename_all_ex(
            bmain, id, rna_path_prefix.c_str(), nullptr, nullptr, 0, 1, false);
        break;
      }
      /* Notice that we use the shader type because the node is already converted in versioning
       * before linking. */
      case SH_NODE_MIX: {
        /* The node gained multiple new sockets after the factor socket, so the second and third
         * sockets moved to be the 7th and 8th sockets. */
        BKE_animdata_fix_paths_rename_all_ex(
            bmain, id, rna_path_prefix.c_str(), nullptr, nullptr, 1, 6, false);
        BKE_animdata_fix_paths_rename_all_ex(
            bmain, id, rna_path_prefix.c_str(), nullptr, nullptr, 2, 7, false);
        break;
      }
      default:
        break;
    }
  }
}

static void do_version_split_node_rotation(bNodeTree *node_tree, bNode *node)
{
  using namespace blender;

  bNodeSocket *factor_input = bke::node_find_socket(*node, SOCK_IN, "Factor");
  float factor = factor_input->default_value_typed<bNodeSocketValueFloat>()->value;

  bNodeSocket *rotation_input = bke::node_find_socket(*node, SOCK_IN, "Rotation");
  if (!rotation_input) {
    rotation_input = bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_FLOAT, PROP_ANGLE, "Rotation", "Rotation");
  }

  bNodeSocket *position_input = bke::node_find_socket(*node, SOCK_IN, "Position");
  if (!position_input) {
    position_input = bke::node_add_static_socket(
        *node_tree, *node, SOCK_IN, SOCK_VECTOR, PROP_FACTOR, "Position", "Position");
  }

  constexpr int CMP_NODE_SPLIT_HORIZONTAL = 0;
  constexpr int CMP_NODE_SPLIT_VERTICAL = 1;

  switch (node->custom2) {
    case CMP_NODE_SPLIT_HORIZONTAL: {
      rotation_input->default_value_typed<bNodeSocketValueFloat>()->value =
          -math::numbers::pi_v<float> / 2.0f;
      position_input->default_value_typed<bNodeSocketValueVector>()->value[0] = factor;
      /* The y-coordinate doesn't matter in this case, so set the value to 0.5 so that the gizmo
       * appears nicely at the center. */
      position_input->default_value_typed<bNodeSocketValueVector>()->value[1] = 0.5f;
      break;
    }
    case CMP_NODE_SPLIT_VERTICAL: {
      rotation_input->default_value_typed<bNodeSocketValueFloat>()->value = 0.0f;
      position_input->default_value_typed<bNodeSocketValueVector>()->value[0] = 0.5f;
      position_input->default_value_typed<bNodeSocketValueVector>()->value[1] = factor;
      break;
    }
  }
}

static void do_version_remove_lzo_and_lzma_compression(FileData *fd, Object *object)
{
  constexpr int PTCACHE_COMPRESS_LZO = 1;
  constexpr int PTCACHE_COMPRESS_LZMA = 2;
  ListBase pidlist;

  BKE_ptcache_ids_from_object(&pidlist, object, nullptr, 0);

  LISTBASE_FOREACH (PTCacheID *, pid, &pidlist) {
    bool found_incompatible_cache = false;
    if (pid->cache->compression == PTCACHE_COMPRESS_LZO) {
      pid->cache->compression = PTCACHE_COMPRESS_ZSTD_FAST;
      found_incompatible_cache = true;
    }
    else if (pid->cache->compression == PTCACHE_COMPRESS_LZMA) {
      pid->cache->compression = PTCACHE_COMPRESS_ZSTD_SLOW;
      found_incompatible_cache = true;
    }

    if (pid->type == PTCACHE_TYPE_DYNAMICPAINT) {
      /* Dynamicpaint was hardcoded to use LZO. */
      found_incompatible_cache = true;
    }

    if (!found_incompatible_cache) {
      continue;
    }

    std::string cache_type;
    switch (pid->type) {
      case PTCACHE_TYPE_SOFTBODY:
        cache_type = RPT_("Softbody");
        break;
      case PTCACHE_TYPE_PARTICLES:
        cache_type = RPT_("Particle");
        break;
      case PTCACHE_TYPE_CLOTH:
        cache_type = RPT_("Cloth");
        break;
      case PTCACHE_TYPE_SMOKE_DOMAIN:
        cache_type = RPT_("Smoke Domain");
        break;
      case PTCACHE_TYPE_SMOKE_HIGHRES:
        cache_type = RPT_("Smoke");
        break;
      case PTCACHE_TYPE_DYNAMICPAINT:
        cache_type = RPT_("Dynamic Paint");
        break;
      case PTCACHE_TYPE_RIGIDBODY:
        /* Rigidbody caches shouldn't have any disk caches, but keep it here just in case. */
        cache_type = RPT_("Rigidbody");
        break;
    }
    BLO_reportf_wrap(
        fd->reports,
        RPT_WARNING,
        RPT_("%s Cache in object %s can not be read because it uses an "
             "outdated compression method. You need to delete the caches and re-bake."),
        cache_type.c_str(),
        pid->owner_id->name + 2);
  }

  BLI_freelistN(&pidlist);
}

static void do_version_convert_gp_jitter_values(Brush *brush)
{
  /* Because this change is backported into the 4.5 branch, we need to avoid performing versioning
   * in case the user updated their custom brush assets between using 4.5 and 5.0 to avoid
   * overwriting their changes.
   *
   * See #142104
   */
  if ((brush->flag2 & BRUSH_JITTER_COLOR) != 0 || !is_zero_v3(brush->hsv_jitter)) {
    return;
  }

  BrushGpencilSettings *settings = brush->gpencil_settings;
  float old_hsv_jitter[3] = {
      settings->random_hue, settings->random_saturation, settings->random_value};
  if (!is_zero_v3(old_hsv_jitter)) {
    brush->flag2 |= BRUSH_JITTER_COLOR;
  }
  copy_v3_v3(brush->hsv_jitter, old_hsv_jitter);
  if (brush->curve_rand_hue) {
    BKE_curvemapping_free_data(brush->curve_rand_hue);
    BKE_curvemapping_copy_data(brush->curve_rand_hue, settings->curve_rand_hue);
  }
  else {
    brush->curve_rand_hue = BKE_curvemapping_copy(settings->curve_rand_hue);
  }
  if (brush->curve_rand_saturation) {
    BKE_curvemapping_free_data(brush->curve_rand_saturation);
    BKE_curvemapping_copy_data(brush->curve_rand_saturation, settings->curve_rand_saturation);
  }
  else {
    brush->curve_rand_saturation = BKE_curvemapping_copy(settings->curve_rand_saturation);
  }
  if (brush->curve_rand_value) {
    BKE_curvemapping_free_data(brush->curve_rand_value);
    BKE_curvemapping_copy_data(brush->curve_rand_value, settings->curve_rand_value);
  }
  else {
    brush->curve_rand_value = BKE_curvemapping_copy(settings->curve_rand_value);
  }
}

/* The Composite node was removed and a Group Output node should be used instead, so we need to
 * make the replacement. But first note that the Group Output node relies on the node tree
 * interface, so we ensure a default interface with a single input and output. This is only for
 * root trees used as scene compositing node groups, for other node trees, we remove all composite
 * nodes since they are no longer supported inside groups. */
static void do_version_composite_node_in_scene_tree(bNodeTree &node_tree, bNode &node)
{
  blender::bke::node_tree_set_type(node_tree);

  /* Remove inactive nodes. */
  if (!(node.flag & NODE_DO_OUTPUT)) {
    version_node_remove(node_tree, node);
    return;
  }

  bNodeSocket *old_image_input = blender::bke::node_find_socket(node, SOCK_IN, "Image");

  /* Find the link going into the Image input of the Composite node. */
  bNodeLink *image_link = nullptr;
  LISTBASE_FOREACH (bNodeLink *, link, &node_tree.links) {
    if (link->tosock == old_image_input) {
      image_link = link;
    }
  }

  bNode *group_output_node = blender::bke::node_add_node(nullptr, node_tree, "NodeGroupOutput");
  group_output_node->parent = node.parent;
  group_output_node->location[0] = node.location[0];
  group_output_node->location[1] = node.location[1];

  bNodeSocket *image_input = static_cast<bNodeSocket *>(group_output_node->inputs.first);
  BLI_assert(blender::StringRef(image_input->name) == "Image");
  copy_v4_v4(image_input->default_value_typed<bNodeSocketValueRGBA>()->value,
             old_image_input->default_value_typed<bNodeSocketValueRGBA>()->value);

  if (image_link) {
    version_node_add_link(
        node_tree, *image_link->fromnode, *image_link->fromsock, *group_output_node, *image_input);
    blender::bke::node_remove_link(&node_tree, *image_link);
  }

  version_node_remove(node_tree, node);
}

/* Updates the media type of the given format to match its imtype. */
static void update_format_media_type(ImageFormatData *format)
{
  if (BKE_imtype_is_image(format->imtype)) {
    format->media_type = MEDIA_TYPE_IMAGE;
  }
  else if (BKE_imtype_is_multi_layer_image(format->imtype)) {
    format->media_type = MEDIA_TYPE_MULTI_LAYER_IMAGE;
  }
  else if (BKE_imtype_is_movie(format->imtype)) {
    format->media_type = MEDIA_TYPE_VIDEO;
  }
  else {
    BLI_assert_unreachable();
  }
}

static void do_version_world_remove_use_nodes(Main *bmain, World *world)
{
  if (world->use_nodes == true) {
    return;
  }

  /* Users defined a world node tree, but deactivated it by disabling "Use Nodes". So we
   * simulate the same effect by creating a new World Output node and setting it to active. */
  bNodeTree *ntree = world->nodetree;
  if (ntree == nullptr) {
    /* In case the world was defined through Python API it might have been missing a node tree. */
    ntree = blender::bke::node_tree_add_tree_embedded(
        bmain, &world->id, "World Node Tree Versioning", "ShaderNodeTree");
  }

  bNode *old_output = nullptr;
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (STREQ(node->idname, "ShaderNodeOutputWorld") && (node->flag & NODE_DO_OUTPUT)) {
      old_output = node;
      old_output->flag &= ~NODE_DO_OUTPUT;
    }
  }

  bNode &new_output = version_node_add_empty(*ntree, "ShaderNodeOutputWorld");
  bNodeSocket &output_surface_input = version_node_add_socket(
      *ntree, new_output, SOCK_IN, "NodeSocketShader", "Surface");
  version_node_add_socket(*ntree, new_output, SOCK_IN, "NodeSocketShader", "Volume");
  new_output.flag |= NODE_DO_OUTPUT;

  bNode &background = version_node_add_empty(*ntree, "ShaderNodeBackground");
  bNodeSocket &background_color_output = version_node_add_socket(
      *ntree, background, SOCK_OUT, "NodeSocketShader", "Background");
  bNodeSocket &background_color_input = version_node_add_socket(
      *ntree, background, SOCK_IN, "NodeSocketColor", "Color");
  bNodeSocket &background_strength_input = version_node_add_socket(
      *ntree, background, SOCK_IN, "NodeSocketFloat", "Strength");
  bNodeSocket &background_weight_input = version_node_add_socket(
      *ntree, background, SOCK_IN, "NodeSocketFloat", "Weight");
  background_weight_input.flag |= SOCK_UNAVAIL;

  version_node_add_link(
      *ntree, background, background_color_output, new_output, output_surface_input);

  bNodeSocketValueRGBA *rgba = background_color_input.default_value_typed<bNodeSocketValueRGBA>();
  rgba->value[0] = world->horr;
  rgba->value[1] = world->horg;
  rgba->value[2] = world->horb;
  rgba->value[3] = 1.0f;
  background_strength_input.default_value_typed<bNodeSocketValueFloat>()->value = 1.0f;

  if (old_output != nullptr) {
    /* Position the newly created node after the old output. Assume the old output node is at
     * the far right of the node tree. */
    background.location[0] = old_output->location[0] + 1.5f * old_output->width;
    background.location[1] = old_output->location[1];
  }

  new_output.location[0] = background.location[0] + 2.0f * background.width;
  new_output.location[1] = background.location[1];

  bNode *frame = blender::bke::node_add_static_node(nullptr, *ntree, NODE_FRAME);
  background.parent = frame;
  new_output.parent = frame;
}

void do_versions_after_linking_500(FileData *fd, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 9)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (STREQ(scene->r.engine, RE_engine_id_BLENDER_EEVEE_NEXT)) {
        STRNCPY_UTF8(scene->r.engine, RE_engine_id_BLENDER_EEVEE);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 27)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        do_version_convert_to_generic_nodes_after_linking(bmain, ntree, id);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 37)) {
    LISTBASE_FOREACH (Object *, object, &bmain->objects) {
      do_version_remove_lzo_and_lzma_compression(fd, object);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 41)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      bNodeTree *node_tree = version_get_scene_compositor_node_tree(bmain, scene);
      if (node_tree) {
        /* Add a default interface for the node tree. See the versioning function below for more
         * details. */
        node_tree->tree_interface.clear_items();
        node_tree->tree_interface.add_socket(
            DATA_("Image"), "", "NodeSocketColor", NODE_INTERFACE_SOCKET_INPUT, nullptr);
        node_tree->tree_interface.add_socket(
            DATA_("Image"), "", "NodeSocketColor", NODE_INTERFACE_SOCKET_OUTPUT, nullptr);

        LISTBASE_FOREACH_BACKWARD_MUTABLE (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_COMPOSITE_DEPRECATED) {
            do_version_composite_node_in_scene_tree(*node_tree, *node);
          }
        }
      }
    }
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      blender::bke::node_tree_set_type(*node_tree);
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH_BACKWARD_MUTABLE (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_COMPOSITE_DEPRECATED) {
            /* See do_version_composite_node_in_scene_tree. */
            version_node_remove(*node_tree, *node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */
}

void blo_do_versions_500(FileData * /*fd*/, Library * /*lib*/, Main *bmain)
{
  using namespace blender;
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 1)) {
    LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
      bke::mesh_sculpt_mask_to_generic(*mesh);
      bke::mesh_custom_normals_to_generic(*mesh);
      rename_mesh_uv_seam_attribute(*mesh);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 2)) {
    LISTBASE_FOREACH (PointCloud *, pointcloud, &bmain->pointclouds) {
      blender::bke::pointcloud_convert_customdata_to_storage(*pointcloud);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 3)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_GEOMETRY) {
        initialize_closure_input_structure_types(*ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 7)) {
    const int uv_select_island = 1 << 3;
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *ts = scene->toolsettings;
      if (ts->uv_selectmode & uv_select_island) {
        ts->uv_selectmode = UV_SELECT_VERTEX;
        ts->uv_flag |= UV_FLAG_ISLAND_SELECT;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 8)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_COMPOSIT) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy != CMP_NODE_DISPLACE) {
          continue;
        }
        if (node->storage != nullptr) {
          continue;
        }
        NodeDisplaceData *data = MEM_callocN<NodeDisplaceData>(__func__);
        data->interpolation = CMP_NODE_INTERPOLATION_ANISOTROPIC;
        node->storage = data;
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 10)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
        view_layer->eevee.ambient_occlusion_distance = scene->eevee.gtao_distance;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 13)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        version_node_socket_name(ntree, CMP_NODE_VIEW_LEVELS, "Std Dev", "Standard Deviation");
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 14)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      versioning_replace_legacy_combined_and_separate_color_nodes(ntree);
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 15)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        version_node_socket_name(ntree, CMP_NODE_ROTATE, "Degr", "Angle");
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 17)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      do_version_scene_remove_use_nodes(scene);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 20)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (ELEM(sl->spacetype, SPACE_ACTION, SPACE_GRAPH, SPACE_NLA, SPACE_SEQ)) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;
            ARegion *new_footer = do_versions_add_region_if_not_found(
                regionbase, RGN_TYPE_FOOTER, "footer for animation editors", RGN_TYPE_HEADER);
            if (new_footer != nullptr) {
              new_footer->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_TOP :
                                                                        RGN_ALIGN_BOTTOM;
              new_footer->flag |= RGN_FLAG_HIDDEN;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 21)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH_MUTABLE (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_NORMAL) {
            do_version_normal_node_dot_product(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 23)) {
    /* Change default Sky Texture to Nishita (after removal of old sky models) */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type_legacy == SH_NODE_TEX_SKY && node->storage) {
            NodeTexSky *tex = (NodeTexSky *)node->storage;
            tex->sky_model = 0;
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 25)) {
    version_seq_text_from_legacy(bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 26)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      apply_unified_paint_settings_to_all_modes(*scene);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 27)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        do_version_convert_to_generic_nodes(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 28)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_SPLIT) {
            do_version_split_node_rotation(node_tree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 30)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype != SPACE_FILE) {
            continue;
          }
          SpaceFile *sfile = reinterpret_cast<SpaceFile *>(sl);
          if (sfile->browse_mode != FILE_BROWSE_MODE_ASSETS) {
            continue;
          }
          sfile->asset_params->base_params.filter_id |= FILTER_ID_SCE;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 32)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_COMPOSIT) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy != CMP_NODE_TRANSLATE) {
          continue;
        }
        if (node->storage == nullptr) {
          continue;
        }
        NodeTranslateData *data = static_cast<NodeTranslateData *>(node->storage);
        /* Map old wrap axis to new extension mode. */
        switch (data->wrap_axis) {
          case CMP_NODE_TRANSLATE_REPEAT_AXIS_NONE:
            data->extension_x = CMP_NODE_EXTENSION_MODE_CLIP;
            data->extension_y = CMP_NODE_EXTENSION_MODE_CLIP;
            break;
          case CMP_NODE_TRANSLATE_REPEAT_AXIS_X:
            data->extension_x = CMP_NODE_EXTENSION_MODE_REPEAT;
            data->extension_y = CMP_NODE_EXTENSION_MODE_CLIP;
            break;
          case CMP_NODE_TRANSLATE_REPEAT_AXIS_Y:
            data->extension_x = CMP_NODE_EXTENSION_MODE_CLIP;
            data->extension_y = CMP_NODE_EXTENSION_MODE_REPEAT;
            break;
          case CMP_NODE_TRANSLATE_REPEAT_AXIS_XY:
            data->extension_x = CMP_NODE_EXTENSION_MODE_REPEAT;
            data->extension_y = CMP_NODE_EXTENSION_MODE_REPEAT;
            break;
        }
      }
      FOREACH_NODETREE_END;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 32)) {
    LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
      mesh->radial_symmetry[0] = 1;
      mesh->radial_symmetry[1] = 1;
      mesh->radial_symmetry[2] = 1;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 33)) {
    LISTBASE_FOREACH (Curves *, curves, &bmain->hair_curves) {
      blender::bke::curves_convert_customdata_to_storage(curves->geometry.wrap());
    }
    LISTBASE_FOREACH (GreasePencil *, grease_pencil, &bmain->grease_pencils) {
      blender::bke::grease_pencil_convert_customdata_to_storage(*grease_pencil);
      for (const int i : IndexRange(grease_pencil->drawing_array_num)) {
        GreasePencilDrawingBase *drawing_base = grease_pencil->drawing_array[i];
        if (drawing_base->type == GP_DRAWING) {
          GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base);
          blender::bke::curves_convert_customdata_to_storage(drawing->geometry.wrap());
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 34)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_COMPOSIT) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy != CMP_NODE_SCALE) {
          continue;
        }
        if (node->storage == nullptr) {
          continue;
        }
        NodeScaleData *data = static_cast<NodeScaleData *>(node->storage);
        data->extension_x = CMP_NODE_EXTENSION_MODE_CLIP;
        data->extension_y = CMP_NODE_EXTENSION_MODE_CLIP;
      }
      FOREACH_NODETREE_END;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 35)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_COMPOSIT) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy != CMP_NODE_TRANSFORM) {
          continue;
        }
        if (node->storage != nullptr) {
          continue;
        }
        NodeTransformData *data = MEM_callocN<NodeTransformData>(__func__);
        data->interpolation = node->custom1;
        data->extension_x = CMP_NODE_EXTENSION_MODE_CLIP;
        data->extension_y = CMP_NODE_EXTENSION_MODE_CLIP;
        node->storage = data;
      }
      FOREACH_NODETREE_END;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 36)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        version_node_input_socket_name(ntree, CMP_NODE_ZCOMBINE, "Image", "A");
        version_node_input_socket_name(ntree, CMP_NODE_ZCOMBINE, "Image_001", "B");

        version_node_input_socket_name(ntree, CMP_NODE_ZCOMBINE, "Z", "Depth A");
        version_node_input_socket_name(ntree, CMP_NODE_ZCOMBINE, "Z_001", "Depth B");

        version_node_output_socket_name(ntree, CMP_NODE_ZCOMBINE, "Image", "Result");
        version_node_output_socket_name(ntree, CMP_NODE_ZCOMBINE, "Z", "Depth");
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 38)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_GEOMETRY) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == GEO_NODE_TRANSFORM_GEOMETRY) {
            do_version_transform_geometry_options_to_inputs(*node_tree, *node);
          }
          else if (node->type_legacy == GEO_NODE_POINTS_TO_VOLUME) {
            do_version_points_to_volume_options_to_inputs(*node_tree, *node);
          }
          else if (node->type_legacy == GEO_NODE_TRIANGULATE) {
            do_version_triangulate_options_to_inputs(*node_tree, *node);
          }
          else if (node->type_legacy == GEO_NODE_VOLUME_TO_MESH) {
            do_version_volume_to_mesh_options_to_inputs(*node_tree, *node);
          }
          else if (STREQ(node->idname, "FunctionNodeMatchString")) {
            do_version_match_string_options_to_inputs(*node_tree, *node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 39)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      Editing *ed = seq::editing_get(scene);

      if (ed != nullptr) {
        seq::for_each_callback(&ed->seqbase, [](Strip *strip) -> bool {
          LISTBASE_FOREACH (StripModifierData *, smd, &strip->modifiers) {
            seq::modifier_persistent_uid_init(*strip, *smd);
          }
          return true;
        });
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 40)) {
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if (brush->gpencil_settings) {
        do_version_convert_gp_jitter_values(brush);
      }
    }
  }

  /* ImageFormatData gained a new media type which we need to be set according to the existing
   * imtype. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 42)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      update_format_media_type(&scene->r.im_format);
    }

    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type != NTREE_COMPOSIT) {
        continue;
      }

      LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
        if (node->type_legacy != CMP_NODE_OUTPUT_FILE) {
          continue;
        }

        NodeImageMultiFile *storage = static_cast<NodeImageMultiFile *>(node->storage);
        update_format_media_type(&storage->format);

        LISTBASE_FOREACH (bNodeSocket *, input, &node->inputs) {
          NodeImageMultiFileSocket *input_storage = static_cast<NodeImageMultiFileSocket *>(
              input->storage);
          update_format_media_type(&input_storage->format);
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 43)) {
    LISTBASE_FOREACH (World *, world, &bmain->worlds) {
      do_version_world_remove_use_nodes(bmain, world);
    }
  }

  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */

  /* Keep this versioning always enabled at the bottom of the function; it can only be moved behind
   * a subversion bump when the file format is changed. */
  LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
    bke::mesh_freestyle_marks_to_generic(*mesh);
  }
}
