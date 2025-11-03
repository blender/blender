/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include <fmt/format.h>

#include "MEM_guardedalloc.h"

/* Define macros in `DNA_genfile.h`. */
#define DNA_GENFILE_VERSIONING_MACROS

#include "DNA_ID.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_curves_types.h"
#include "DNA_defaults.h"
#include "DNA_genfile.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"
#include "DNA_world_types.h"

#include "BLI_function_ref.hh"
#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_numbers.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_sys_types.h"

#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_armature.hh"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_colortools.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_idprop.hh"
#include "BKE_image_format.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_paint.hh"
#include "BKE_pointcache.h"
#include "BKE_report.hh"

#include "BLT_translation.hh"

#include "BLO_read_write.hh"

#include "SEQ_edit.hh"
#include "SEQ_effects.hh"
#include "SEQ_iterator.hh"
#include "SEQ_modifier.hh"
#include "SEQ_relations.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_utils.hh"

#include "WM_api.hh"

#include "AS_asset_library.hh"

#include "readfile.hh"

#include "versioning_common.hh"

// #include "CLG_log.h"
// static CLG_LogRef LOG = {"blend.doversion"};

static void idprops_process(IDProperty *idprops, IDProperty **system_idprops)
{
  BLI_assert(*system_idprops == nullptr);
  if (idprops) {
    /* Other ID pointers have not yet been relinked, do not try to access them for refcounting. */
    *system_idprops = IDP_CopyProperty_ex(idprops, LIB_ID_CREATE_NO_USER_REFCOUNT);
  }
}

void version_system_idprops_generate(Main *bmain)
{
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
      blender::seq::foreach_strip(&scene->ed->seqbase, [](Strip *strip) -> bool {
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
    /* There is no way to iterate directly over all bones of an armature currently, use a recursive
     * approach instead. */
    auto process_bone_recursive = [](const auto &process_bone_recursive, Bone *bone) -> void {
      idprops_process(bone->prop, &bone->system_properties);
      LISTBASE_FOREACH (Bone *, bone_it, &bone->childbase) {
        process_bone_recursive(process_bone_recursive, bone_it);
      }
    };
    LISTBASE_FOREACH (Bone *, bone_it, &armature->bonebase) {
      process_bone_recursive(process_bone_recursive, bone_it);
    }
  }
}
/* Separate callback for nodes, because they had the split implemented later. */
void version_system_idprops_nodes_generate(Main *bmain)
{
  FOREACH_NODETREE_BEGIN (bmain, node_tree, id_owner) {
    LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
      idprops_process(node->prop, &node->system_properties);
    }
  }
  FOREACH_NODETREE_END;
}
/* Separate callback for non-root bones, because they were missed in the initial implementation. */
void version_system_idprops_children_bones_generate(Main *bmain)
{
  LISTBASE_FOREACH (bArmature *, armature, &bmain->armatures) {
    /* There is no way to iterate directly over all bones of an armature currently, use a recursive
     * approach instead. */
    auto process_bone_recursive = [](const auto &process_bone_recursive, Bone *bone) -> void {
      /* Do not overwrite children bones' system properties if they were already defined by some
       * scripts or add-on e.g. */
      if (bone->system_properties == nullptr) {
        idprops_process(bone->prop, &bone->system_properties);
      }
      LISTBASE_FOREACH (Bone *, bone_it, &bone->childbase) {
        process_bone_recursive(process_bone_recursive, bone_it);
      }
    };
    LISTBASE_FOREACH (Bone *, bone_it, &armature->bonebase) {
      LISTBASE_FOREACH (Bone *, bone_child_it, &bone_it->childbase) {
        process_bone_recursive(process_bone_recursive, bone_child_it);
      }
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

static void update_brush_sizes(Main &bmain)
{
  /* This conversion was originally done in 582c7d94b8, between subversion 1 (84bee96757) and
   * subversion 2 (fa03c53d4a). The original change should have come with a subversion bump to be
   * filled in later, but since it didn't, the best we can do is use subversion 1 for this check.
   * Thankfully, this only results in a single day window in which a user would have had to
   * download the build where this versioning was not correctly applied. */
  LISTBASE_FOREACH (Brush *, brush, &bmain.brushes) {
    brush->size *= 2;
    brush->unprojected_size *= 2.0f;
  }

  auto apply_to_paint = [&](Paint *paint) {
    if (paint == nullptr) {
      return;
    }
    UnifiedPaintSettings &ups = paint->unified_paint_settings;

    ups.size *= 2;
    ups.unprojected_size *= 2.0f;
  };

  LISTBASE_FOREACH (Scene *, scene, &bmain.scenes) {
    scene->toolsettings->unified_paint_settings.size *= 2;
    scene->toolsettings->unified_paint_settings.unprojected_size *= 2.0f;
    apply_to_paint(reinterpret_cast<Paint *>(scene->toolsettings->vpaint));
    apply_to_paint(reinterpret_cast<Paint *>(scene->toolsettings->wpaint));
    apply_to_paint(reinterpret_cast<Paint *>(scene->toolsettings->sculpt));
    apply_to_paint(reinterpret_cast<Paint *>(scene->toolsettings->gp_paint));
    apply_to_paint(reinterpret_cast<Paint *>(scene->toolsettings->gp_vertexpaint));
    apply_to_paint(reinterpret_cast<Paint *>(scene->toolsettings->gp_sculptpaint));
    apply_to_paint(reinterpret_cast<Paint *>(scene->toolsettings->gp_weightpaint));
    apply_to_paint(reinterpret_cast<Paint *>(scene->toolsettings->curves_sculpt));
    apply_to_paint(reinterpret_cast<Paint *>(&scene->toolsettings->imapaint));
  }
}

static void initialize_closure_input_structure_types(bNodeTree &ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    if (node->type_legacy == NODE_EVALUATE_CLOSURE) {
      auto *storage = static_cast<NodeEvaluateClosure *>(node->storage);
      if (!storage) {
        /* Can happen with certain files saved in 4.5 which did not officially support closures
         * yet. */
        continue;
      }
      for (const int i : blender::IndexRange(storage->input_items.items_num)) {
        NodeEvaluateClosureInputItem &item = storage->input_items.items[i];
        if (item.structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO) {
          item.structure_type = NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_DYNAMIC;
        }
      }
      for (const int i : blender::IndexRange(storage->output_items.items_num)) {
        NodeEvaluateClosureOutputItem &item = storage->output_items.items[i];
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

static void do_version_fill_curve_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Mode")) {
    return;
  }
  const auto &storage = *static_cast<NodeGeometryCurveFill *>(node.storage);
  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Mode");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.mode;
}

static void do_version_fillet_curve_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Mode")) {
    return;
  }
  const auto &storage = *static_cast<NodeGeometryCurveFillet *>(node.storage);
  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Mode");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.mode;
}

static void do_version_resample_curve_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Mode")) {
    return;
  }
  const auto &storage = *static_cast<NodeGeometryCurveResample *>(node.storage);
  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Mode");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.mode;
}

static void do_version_distribute_points_in_volume_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Mode")) {
    return;
  }
  const auto &storage = *static_cast<NodeGeometryDistributePointsInVolume *>(node.storage);
  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Mode");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.mode;
}

static void do_version_merge_by_distance_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Mode")) {
    return;
  }
  const auto &storage = *static_cast<NodeGeometryMergeByDistance *>(node.storage);
  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Mode");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.mode;
}

static void do_version_mesh_to_volume_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Resolution Mode")) {
    return;
  }
  const auto &storage = *static_cast<NodeGeometryMeshToVolume *>(node.storage);
  bNodeSocket &socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Resolution Mode");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.resolution_mode;
}

static void do_version_raycast_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Interpolation")) {
    return;
  }
  const auto &storage = *static_cast<NodeGeometryRaycast *>(node.storage);
  bNodeSocket &socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Interpolation");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.mapping;
}

static void do_version_remove_attribute_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Pattern Mode")) {
    return;
  }
  bNodeSocket &socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Pattern Mode");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;
}

static void do_version_sample_grid_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Interpolation")) {
    return;
  }
  bNodeSocket &socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Interpolation");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom2;
}

static void do_version_scale_elements_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Scale Mode")) {
    return;
  }
  bNodeSocket &socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Scale Mode");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom2;
}

static void do_version_set_curve_normal_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Mode")) {
    return;
  }
  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Mode");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;
}

static void do_version_subdivision_surface_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  auto &storage = *static_cast<NodeGeometrySubdivisionSurface *>(node.storage);
  if (!blender::bke::node_find_socket(node, SOCK_IN, "UV Smooth")) {
    bNodeSocket &socket = version_node_add_socket(
        ntree, node, SOCK_IN, "NodeSocketMenu", "UV Smooth");
    socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.uv_smooth;
  }
  if (!blender::bke::node_find_socket(node, SOCK_IN, "Boundary Smooth")) {
    bNodeSocket &socket = version_node_add_socket(
        ntree, node, SOCK_IN, "NodeSocketMenu", "Boundary Smooth");
    socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.boundary_smooth;
  }
}

static void do_version_uv_pack_islands_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Method")) {
    return;
  }
  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Method");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;
}

static void do_version_uv_unwrap_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Method")) {
    return;
  }
  const auto &storage = *static_cast<NodeGeometryUVUnwrap *>(node.storage);
  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Method");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.method;
}

static void version_seq_text_from_legacy(Main *bmain)
{
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    if (scene->ed != nullptr) {
      blender::seq::foreach_strip(&scene->ed->seqbase, [&](Strip *strip) -> bool {
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

static void for_each_mode_paint_settings(
    Scene &scene, blender::FunctionRef<void(Scene &scene, Paint *paint)> func)
{
  func(scene, reinterpret_cast<Paint *>(scene.toolsettings->vpaint));
  func(scene, reinterpret_cast<Paint *>(scene.toolsettings->wpaint));
  func(scene, reinterpret_cast<Paint *>(scene.toolsettings->sculpt));
  func(scene, reinterpret_cast<Paint *>(scene.toolsettings->gp_paint));
  func(scene, reinterpret_cast<Paint *>(scene.toolsettings->gp_vertexpaint));
  func(scene, reinterpret_cast<Paint *>(scene.toolsettings->gp_sculptpaint));
  func(scene, reinterpret_cast<Paint *>(scene.toolsettings->gp_weightpaint));
  func(scene, reinterpret_cast<Paint *>(scene.toolsettings->curves_sculpt));
  func(scene, reinterpret_cast<Paint *>(&scene.toolsettings->imapaint));
}

static void copy_unified_paint_settings(Scene &scene, Paint *paint)
{
  if (paint == nullptr) {
    return;
  }

  const UnifiedPaintSettings &scene_ups = scene.toolsettings->unified_paint_settings;
  UnifiedPaintSettings &ups = paint->unified_paint_settings;

  ups.size = scene_ups.size;
  ups.unprojected_size = scene_ups.unprojected_size;
  ups.alpha = scene_ups.alpha;
  ups.weight = scene_ups.weight;
  copy_v3_v3(ups.color, scene_ups.color);
  copy_v3_v3(ups.rgb, scene_ups.rgb);
  copy_v3_v3(ups.secondary_color, scene_ups.secondary_color);
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

  bNode *frame = blender::bke::node_add_static_node(nullptr, *node_tree, NODE_FRAME);
  frame->parent = node->parent;
  STRNCPY(frame->label, RPT_("Versioning: Map Value node was removed"));
  NodeFrame *frame_data = static_cast<NodeFrame *>(frame->storage);
  frame_data->label_size = 10;

  /* If the value input is not connected, add a value node with the computed value. */
  if (!value_link) {
    const float value = static_cast<bNodeSocketValueFloat *>(value_input->default_value)->value;
    const float mapped_value = (value + offset) * size;
    const float min_clamped_value = use_min ? blender::math::max(mapped_value, min) : mapped_value;
    const float clamped_value = use_max ? blender::math::min(min_clamped_value, max) :
                                          min_clamped_value;

    bNode &value_node = version_node_add_empty(*node_tree, "ShaderNodeValue");
    bNodeSocket &value_output = version_node_add_socket(
        *node_tree, value_node, SOCK_OUT, "NodeSocketFloat", "Value");

    value_node.parent = frame;
    value_node.location[0] = node->location[0];
    value_node.location[1] = node->location[1];

    static_cast<bNodeSocketValueFloat *>(value_output.default_value)->value = clamped_value;

    /* Relink from the Map Value node to the value node. */
    LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &node_tree->links) {
      if (link->fromnode != node) {
        continue;
      }

      version_node_add_link(*node_tree, value_node, value_output, *link->tonode, *link->tosock);
      blender::bke::node_remove_link(node_tree, *link);
    }

    MEM_freeN(&texture_mapping);
    node->storage = nullptr;
    version_node_remove(*node_tree, *node);

    version_socket_update_is_used(node_tree);
    return;
  }

  /* Otherwise, add math nodes to do the computation, starting with an add node to add the offset
   * of the range. */
  bNode &add_node = version_node_add_empty(*node_tree, "ShaderNodeMath");
  bNodeSocket &add_input_a = version_node_add_socket(
      *node_tree, add_node, SOCK_IN, "NodeSocketFloat", "Value");
  bNodeSocket &add_input_b = version_node_add_socket(
      *node_tree, add_node, SOCK_IN, "NodeSocketFloat", "Value_001");
  version_node_add_socket(*node_tree, add_node, SOCK_IN, "NodeSocketFloat", "Value_002");

  bNodeSocket &add_output = version_node_add_socket(
      *node_tree, add_node, SOCK_OUT, "NodeSocketFloat", "Value");

  add_node.parent = frame;
  add_node.custom1 = NODE_MATH_ADD;
  add_node.location[0] = node->location[0];
  add_node.location[1] = node->location[1];
  add_node.flag |= NODE_COLLAPSED;

  /* Connect the origin of the node to the first input of the add node and remove the original
   * link. */
  version_node_add_link(
      *node_tree, *value_link->fromnode, *value_link->fromsock, add_node, add_input_a);
  blender::bke::node_remove_link(node_tree, *value_link);

  /* Set the offset to the second input of the add node. */
  static_cast<bNodeSocketValueFloat *>(add_input_b.default_value)->value = offset;

  /* Add a multiply node to multiply by the size. */
  bNode &multiply_node = version_node_add_empty(*node_tree, "ShaderNodeMath");
  multiply_node.parent = frame;
  multiply_node.custom1 = NODE_MATH_MULTIPLY;
  multiply_node.location[0] = add_node.location[0];
  multiply_node.location[1] = add_node.location[1] - 40.0f;
  multiply_node.flag |= NODE_COLLAPSED;

  bNodeSocket &multiply_input_a = version_node_add_socket(
      *node_tree, multiply_node, SOCK_IN, "NodeSocketFloat", "Value");
  bNodeSocket &multiply_input_b = version_node_add_socket(
      *node_tree, multiply_node, SOCK_IN, "NodeSocketFloat", "Value_001");
  version_node_add_socket(*node_tree, multiply_node, SOCK_IN, "NodeSocketFloat", "Value_002");

  bNodeSocket &multiply_output = version_node_add_socket(
      *node_tree, multiply_node, SOCK_OUT, "NodeSocketFloat", "Value");

  /* Connect the output of the add node to the first input of the multiply node. */
  version_node_add_link(*node_tree, add_node, add_output, multiply_node, multiply_input_a);

  /* Set the size to the second input of the multiply node. */
  static_cast<bNodeSocketValueFloat *>(multiply_input_b.default_value)->value = size;

  bNode *final_node = &multiply_node;
  bNodeSocket *final_output = &multiply_output;

  if (use_min) {
    /* Add a maximum node to clamp by the minimum. */
    bNode &max_node = version_node_add_empty(*node_tree, "ShaderNodeMath");
    max_node.parent = frame;
    max_node.custom1 = NODE_MATH_MAXIMUM;
    max_node.location[0] = final_node->location[0];
    max_node.location[1] = final_node->location[1] - 40.0f;
    max_node.flag |= NODE_COLLAPSED;

    bNodeSocket &max_input_a = version_node_add_socket(
        *node_tree, max_node, SOCK_IN, "NodeSocketFloat", "Value");
    bNodeSocket &max_input_b = version_node_add_socket(
        *node_tree, max_node, SOCK_IN, "NodeSocketFloat", "Value_001");
    version_node_add_socket(*node_tree, max_node, SOCK_IN, "NodeSocketFloat", "Value_002");
    bNodeSocket &max_output = version_node_add_socket(
        *node_tree, max_node, SOCK_OUT, "NodeSocketFloat", "Value");

    /* Connect the output of the final node to the first input of the maximum node. */
    version_node_add_link(*node_tree, *final_node, *final_output, max_node, max_input_a);

    /* Set the minimum to the second input of the maximum node. */
    static_cast<bNodeSocketValueFloat *>(max_input_b.default_value)->value = min;

    final_node = &max_node;
    final_output = &max_output;
  }

  if (use_max) {
    /* Add a minimum node to clamp by the maximum. */
    bNode &min_node = version_node_add_empty(*node_tree, "ShaderNodeMath");
    min_node.parent = frame;
    min_node.custom1 = NODE_MATH_MINIMUM;
    min_node.location[0] = final_node->location[0];
    min_node.location[1] = final_node->location[1] - 40.0f;
    min_node.flag |= NODE_COLLAPSED;

    bNodeSocket &min_input_a = version_node_add_socket(
        *node_tree, min_node, SOCK_IN, "NodeSocketFloat", "Value");
    bNodeSocket &min_input_b = version_node_add_socket(
        *node_tree, min_node, SOCK_IN, "NodeSocketFloat", "Value_001");
    version_node_add_socket(*node_tree, min_node, SOCK_IN, "NodeSocketFloat", "Value_002");
    bNodeSocket &min_output = version_node_add_socket(
        *node_tree, min_node, SOCK_OUT, "NodeSocketFloat", "Value");

    /* Connect the output of the final node to the first input of the minimum node. */
    version_node_add_link(*node_tree, *final_node, *final_output, min_node, min_input_a);

    /* Set the maximum to the second input of the minimum node. */
    static_cast<bNodeSocketValueFloat *>(min_input_b.default_value)->value = max;

    final_node = &min_node;
    final_output = &min_output;
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

  version_node_remove(*node_tree, *node);

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
  ListBase pidlist;

  BKE_ptcache_ids_from_object(&pidlist, object, nullptr, 0);

  LISTBASE_FOREACH (PTCacheID *, pid, &pidlist) {
    bool found_incompatible_cache = false;
    if (ELEM(pid->cache->compression,
             PTCACHE_COMPRESS_LZO_DEPRECATED,
             PTCACHE_COMPRESS_LZMA_DEPRECATED))
    {
      pid->cache->compression = PTCACHE_COMPRESS_ZSTD_FILTERED;
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

/* The Sun beams node was removed and the Glare node should be used instead, so we need to
 * make the replacement. */
static void do_version_sun_beams(bNodeTree &node_tree, bNode &node)
{
  blender::bke::node_tree_set_type(node_tree);

  bNodeSocket *old_image_input = blender::bke::node_find_socket(node, SOCK_IN, "Image");
  bNodeSocket *old_source_input = blender::bke::node_find_socket(node, SOCK_IN, "Source");
  bNodeSocket *old_length_input = blender::bke::node_find_socket(node, SOCK_IN, "Length");
  bNodeSocket *old_image_output = blender::bke::node_find_socket(node, SOCK_OUT, "Image");

  bNode *glare_node = blender::bke::node_add_node(nullptr, node_tree, "CompositorNodeGlare");
  glare_node->parent = node.parent;
  glare_node->location[0] = node.location[0];
  glare_node->location[1] = node.location[1];

  bNodeSocket *image_input = blender::bke::node_find_socket(*glare_node, SOCK_IN, "Image");
  bNodeSocket *type_input = blender::bke::node_find_socket(*glare_node, SOCK_IN, "Type");
  bNodeSocket *quality_input = blender::bke::node_find_socket(*glare_node, SOCK_IN, "Quality");
  bNodeSocket *threshold_input = blender::bke::node_find_socket(
      *glare_node, SOCK_IN, "Highlights Threshold");
  bNodeSocket *size_input = blender::bke::node_find_socket(*glare_node, SOCK_IN, "Size");
  bNodeSocket *source_input = blender::bke::node_find_socket(*glare_node, SOCK_IN, "Sun Position");
  bNodeSocket *glare_output = blender::bke::node_find_socket(*glare_node, SOCK_OUT, "Glare");

  type_input->default_value_typed<bNodeSocketValueMenu>()->value = CMP_NODE_GLARE_SUN_BEAMS;
  quality_input->default_value_typed<bNodeSocketValueMenu>()->value = CMP_NODE_GLARE_QUALITY_HIGH;
  copy_v4_v4(image_input->default_value_typed<bNodeSocketValueRGBA>()->value,
             old_image_input->default_value_typed<bNodeSocketValueRGBA>()->value);
  threshold_input->default_value_typed<bNodeSocketValueFloat>()->value = 0.0f;
  size_input->default_value_typed<bNodeSocketValueFloat>()->value =
      old_length_input->default_value_typed<bNodeSocketValueFloat>()->value;
  copy_v2_v2(source_input->default_value_typed<bNodeSocketValueVector>()->value,
             old_source_input->default_value_typed<bNodeSocketValueVector>()->value);

  LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &node_tree.links) {
    if (link->tosock == old_image_input) {
      version_node_add_link(
          node_tree, *link->fromnode, *link->fromsock, *glare_node, *image_input);
      blender::bke::node_remove_link(&node_tree, *link);
      continue;
    }

    if (link->tosock == old_source_input) {
      version_node_add_link(
          node_tree, *link->fromnode, *link->fromsock, *glare_node, *source_input);
      blender::bke::node_remove_link(&node_tree, *link);
      continue;
    }

    if (link->tosock == old_length_input) {
      version_node_add_link(node_tree, *link->fromnode, *link->fromsock, *glare_node, *size_input);
      blender::bke::node_remove_link(&node_tree, *link);
      continue;
    }

    if (link->fromsock == old_image_output) {
      version_node_add_link(node_tree, *link->tonode, *link->tosock, *glare_node, *glare_output);
      blender::bke::node_remove_link(&node_tree, *link);
    }
  }

  version_node_remove(node_tree, node);
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

/* The file output node started using item accessors, so we need to free socket storage and copy
 * them to the new items members. Additionally, the base path was split into a directory and a file
 * name, so we need to split it. */
static void do_version_file_output_node(bNode &node)
{
  if (node.storage == nullptr) {
    return;
  }

  NodeCompositorFileOutput *data = static_cast<NodeCompositorFileOutput *>(node.storage);

  /* The directory previously stored both the directory and the file name. */
  char directory[FILE_MAX] = "";
  char file_name[FILE_MAX] = "";
  BLI_path_split_dir_file(data->directory, directory, FILE_MAX, file_name, FILE_MAX);
  STRNCPY(data->directory, directory);
  data->file_name = BLI_strdup_null(file_name);

  data->items_count = BLI_listbase_count(&node.inputs);
  data->items = MEM_calloc_arrayN<NodeCompositorFileOutputItem>(data->items_count, __func__);
  int i = 0;
  LISTBASE_FOREACH_INDEX (bNodeSocket *, input, &node.inputs, i) {
    NodeImageMultiFileSocket *old_item_data = static_cast<NodeImageMultiFileSocket *>(
        input->storage);
    NodeCompositorFileOutputItem *item_data = &data->items[i];

    item_data->identifier = i;
    BKE_image_format_copy(&item_data->format, &old_item_data->format);
    item_data->save_as_render = old_item_data->save_as_render;
    item_data->override_node_format = !bool(old_item_data->use_node_format);

    item_data->socket_type = input->type;
    if (item_data->socket_type == SOCK_VECTOR) {
      item_data->vector_socket_dimensions =
          input->default_value_typed<bNodeSocketValueVector>()->dimensions;
    }

    if (data->format.imtype == R_IMF_IMTYPE_MULTILAYER) {
      item_data->name = BLI_strdup(old_item_data->layer);
    }
    else {
      item_data->name = BLI_strdup(old_item_data->path);
    }

    const std::string identifier = "Item_" + std::to_string(item_data->identifier);
    STRNCPY(input->identifier, identifier.c_str());

    BKE_image_format_free(&old_item_data->format);
    MEM_freeN(old_item_data);
    input->storage = nullptr;
  }
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
  if (world->use_nodes) {
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
  STRNCPY(frame->label, RPT_("Versioning: Use Nodes was removed"));
  background.parent = frame;
  new_output.parent = frame;
}

static void do_version_blur_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Type")) {
    return;
  }
  const auto &storage = *static_cast<NodeBlurData *>(node.storage);
  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Type");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.filtertype;
}

static void do_version_filter_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Type")) {
    return;
  }
  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Type");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;
}

static void do_version_levels_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Channel")) {
    return;
  }
  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Channel");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;
}

static void do_version_dilate_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Type")) {
    return;
  }
  bNodeSocket &type_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Type");
  type_socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;

  const auto &storage = *static_cast<NodeDilateErode *>(node.storage);
  bNodeSocket &falloff_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Falloff");
  falloff_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.falloff;
}

static void do_version_tone_map_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Type")) {
    return;
  }

  const auto &storage = *static_cast<NodeTonemap *>(node.storage);
  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Type");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.type;
}

static void do_version_lens_distortion_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Type")) {
    return;
  }

  const auto &storage = *static_cast<NodeLensDist *>(node.storage);
  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Type");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.distortion_type;
}

static void do_version_kuwahara_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Type")) {
    return;
  }

  const auto &storage = *static_cast<NodeKuwaharaData *>(node.storage);
  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Type");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.variation;
}

static void do_version_denoise_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Prefilter")) {
    return;
  }

  const auto &storage = *static_cast<NodeDenoise *>(node.storage);
  bNodeSocket &prefilter_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Prefilter");
  prefilter_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.prefilter;
  bNodeSocket &quality_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Quality");
  quality_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.quality;
}

static void do_version_translate_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Interpolation")) {
    return;
  }

  const auto &storage = *static_cast<NodeTranslateData *>(node.storage);
  bNodeSocket &interpolation_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Interpolation");
  interpolation_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.interpolation;
  bNodeSocket &extension_x_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Extension X");
  extension_x_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.extension_x;
  bNodeSocket &extension_y_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Extension Y");
  extension_y_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.extension_y;
}

static void do_version_transform_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Interpolation")) {
    return;
  }

  const auto &storage = *static_cast<NodeTransformData *>(node.storage);
  bNodeSocket &interpolation_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Interpolation");
  interpolation_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.interpolation;
  bNodeSocket &extension_x_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Extension X");
  extension_x_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.extension_x;
  bNodeSocket &extension_y_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Extension Y");
  extension_y_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.extension_y;
}

static void do_version_corner_pin_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Interpolation")) {
    return;
  }

  const auto &storage = *static_cast<NodeCornerPinData *>(node.storage);
  bNodeSocket &interpolation_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Interpolation");
  interpolation_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.interpolation;
  bNodeSocket &extension_x_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Extension X");
  extension_x_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.extension_x;
  bNodeSocket &extension_y_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Extension Y");
  extension_y_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.extension_y;
}

static void do_version_map_uv_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Interpolation")) {
    return;
  }

  const auto &storage = *static_cast<NodeMapUVData *>(node.storage);
  bNodeSocket &interpolation_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Interpolation");
  interpolation_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.interpolation;
  bNodeSocket &extension_x_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Extension X");
  extension_x_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.extension_x;
  bNodeSocket &extension_y_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Extension Y");
  extension_y_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.extension_y;
}

static void do_version_scale_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Interpolation")) {
    return;
  }

  bNodeSocket &type_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Type");
  type_socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;
  bNodeSocket &frame_type_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Frame Type");
  frame_type_socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom2;

  const auto &storage = *static_cast<NodeScaleData *>(node.storage);
  bNodeSocket &interpolation_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Interpolation");
  interpolation_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.interpolation;
  bNodeSocket &extension_x_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Extension X");
  extension_x_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.extension_x;
  bNodeSocket &extension_y_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Extension Y");
  extension_y_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.extension_y;
}

static void do_version_rotate_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Interpolation")) {
    return;
  }

  const auto &storage = *static_cast<NodeRotateData *>(node.storage);
  bNodeSocket &interpolation_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Interpolation");
  interpolation_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.interpolation;
  bNodeSocket &extension_x_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Extension X");
  extension_x_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.extension_x;
  bNodeSocket &extension_y_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Extension Y");
  extension_y_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.extension_y;
}

static void do_version_displace_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Interpolation")) {
    return;
  }

  const auto &storage = *static_cast<NodeDisplaceData *>(node.storage);
  bNodeSocket &interpolation_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Interpolation");
  interpolation_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.interpolation;
  bNodeSocket &extension_x_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Extension X");
  extension_x_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.extension_x;
  bNodeSocket &extension_y_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Extension Y");
  extension_y_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.extension_y;
}

static void do_version_stabilize_2d_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Interpolation")) {
    return;
  }

  bNodeSocket &interpolation_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Interpolation");
  interpolation_socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;
}

static void do_version_box_mask_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Operation")) {
    return;
  }

  bNodeSocket &operation_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Operation");
  operation_socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;
}

static void do_version_ellipse_mask_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Operation")) {
    return;
  }

  bNodeSocket &operation_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Operation");
  operation_socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;
}

static void do_version_track_position_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Mode")) {
    return;
  }

  bNodeSocket &mode_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Mode");
  mode_socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;
  bNodeSocket &frame_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketInt", "Frame");
  frame_socket.default_value_typed<bNodeSocketValueInt>()->value = node.custom2;
}

static void do_version_keying_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Feather Falloff")) {
    return;
  }

  const auto &storage = *static_cast<NodeKeyingData *>(node.storage);
  bNodeSocket &feather_falloff_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Feather Falloff");
  feather_falloff_socket.default_value_typed<bNodeSocketValueMenu>()->value =
      storage.feather_falloff;
}

static void do_version_mask_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Size Source")) {
    return;
  }

  bNodeSocket &size_source_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Size Source");
  size_source_socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;
}

static void do_version_movie_distortion_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Type")) {
    return;
  }

  bNodeSocket &type_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Type");
  type_socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;
}

static void do_version_glare_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Type")) {
    return;
  }

  const auto &storage = *static_cast<NodeGlare *>(node.storage);
  bNodeSocket &type_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Type");
  type_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.type;
  bNodeSocket &quality_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Quality");
  quality_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.quality;
}

static void initialize_missing_closure_and_bundle_node_storage(bNodeTree &ntree)
{
  /* When opening and saving 5.0 files with bundle/closure nodes in 4.5, the storage is lost, since
   * Blender 4.5 does not officially support these features yet (they were experimental features
   * though). This versioning code just adds back the storage so that it does not crash further
   * down the line. */
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    if (node->storage) {
      continue;
    }
    switch (node->type_legacy) {
      case NODE_CLOSURE_INPUT: {
        node->storage = MEM_callocN<NodeClosureInput>(__func__);
        break;
      }
      case NODE_CLOSURE_OUTPUT: {
        node->storage = MEM_callocN<NodeClosureOutput>(__func__);
        break;
      }
      case NODE_EVALUATE_CLOSURE: {
        node->storage = MEM_callocN<NodeEvaluateClosure>(__func__);
        break;
      }
      case NODE_COMBINE_BUNDLE: {
        node->storage = MEM_callocN<NodeCombineBundle>(__func__);
        break;
      }
      case NODE_SEPARATE_BUNDLE: {
        node->storage = MEM_callocN<NodeSeparateBundle>(__func__);
        break;
      }
    }
  }
}

static void do_version_material_remove_use_nodes(Main *bmain, Material *material)
{
  if (material->use_nodes) {
    return;
  }

  /* Users defined a material node tree, but deactivated it by disabling "Use Nodes". So we
   * simulate the same effect by creating a new Material Output node and setting it to active. */
  bNodeTree *ntree = material->nodetree;
  if (ntree == nullptr) {
    /* In case the material was created in Python API it might have been missing a node tree. */
    ntree = blender::bke::node_tree_add_tree_embedded(
        bmain, &material->id, "Material Node Tree Versioning", "ShaderNodeTree");
  }

  bNode *old_output = nullptr;
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (STREQ(node->idname, "ShaderNodeOutputMaterial") && (node->flag & NODE_DO_OUTPUT)) {
      old_output = node;
      old_output->flag &= ~NODE_DO_OUTPUT;
    }
  }

  bNode *frame = blender::bke::node_add_static_node(nullptr, *ntree, NODE_FRAME);
  STRNCPY(frame->label, RPT_("Versioning: Use Nodes was removed"));

  {
    /* For EEVEE, we use a principled BSDF shader because we need to recreate the metallic,
     * specular and roughness properties of the material for use_nodes = false.*/
    bNode &new_output_eevee = version_node_add_empty(*ntree, "ShaderNodeOutputMaterial");
    bNodeSocket &output_surface_input = version_node_add_socket(
        *ntree, new_output_eevee, SOCK_IN, "NodeSocketShader", "Surface");
    version_node_add_socket(*ntree, new_output_eevee, SOCK_IN, "NodeSocketShader", "Volume");
    version_node_add_socket(*ntree, new_output_eevee, SOCK_IN, "NodeSocketVector", "Displacement");
    version_node_add_socket(*ntree, new_output_eevee, SOCK_IN, "NodeSocketFloat", "Thickness");

    version_node_add_socket(*ntree, new_output_eevee, SOCK_IN, "NodeSocketShader", "Volume");
    new_output_eevee.flag |= NODE_DO_OUTPUT;
    new_output_eevee.custom1 = SHD_OUTPUT_EEVEE;

    bNode &shader_eevee = *blender::bke::node_add_static_node(
        nullptr, *ntree, SH_NODE_BSDF_PRINCIPLED);
    bNodeSocket &shader_bsdf_output = *blender::bke::node_find_socket(
        shader_eevee, SOCK_OUT, "BSDF");
    bNodeSocket &shader_color_input = *blender::bke::node_find_socket(
        shader_eevee, SOCK_IN, "Base Color");
    bNodeSocket &specular_input = *blender::bke::node_find_socket(
        shader_eevee, SOCK_IN, "Specular IOR Level");
    bNodeSocket &metallic_input = *blender::bke::node_find_socket(
        shader_eevee, SOCK_IN, "Metallic");
    bNodeSocket &roughness_input = *blender::bke::node_find_socket(
        shader_eevee, SOCK_IN, "Roughness");

    version_node_add_link(
        *ntree, shader_eevee, shader_bsdf_output, new_output_eevee, output_surface_input);

    bNodeSocketValueRGBA *rgba = shader_color_input.default_value_typed<bNodeSocketValueRGBA>();
    rgba->value[0] = material->r;
    rgba->value[1] = material->g;
    rgba->value[2] = material->b;
    rgba->value[3] = material->a;
    roughness_input.default_value_typed<bNodeSocketValueFloat>()->value = material->roughness;
    metallic_input.default_value_typed<bNodeSocketValueFloat>()->value = material->metallic;
    specular_input.default_value_typed<bNodeSocketValueFloat>()->value = material->spec;

    if (old_output != nullptr) {
      /* Position the newly created node after the old output. Assume the old output node is at
       * the far right of the node tree. */
      shader_eevee.location[0] = old_output->location[0] + 1.5f * old_output->width;
      shader_eevee.location[1] = old_output->location[1];
    }

    new_output_eevee.location[0] = shader_eevee.location[0] + 2.0f * shader_eevee.width;
    new_output_eevee.location[1] = shader_eevee.location[1];

    shader_eevee.parent = frame;
    new_output_eevee.parent = frame;
  }

  {
    /* For Cycles, a simple diffuse BSDF is sufficient. */
    bNode &new_output_cycles = version_node_add_empty(*ntree, "ShaderNodeOutputMaterial");
    bNodeSocket &output_surface_input = version_node_add_socket(
        *ntree, new_output_cycles, SOCK_IN, "NodeSocketShader", "Surface");
    version_node_add_socket(*ntree, new_output_cycles, SOCK_IN, "NodeSocketShader", "Volume");
    version_node_add_socket(
        *ntree, new_output_cycles, SOCK_IN, "NodeSocketVector", "Displacement");
    version_node_add_socket(*ntree, new_output_cycles, SOCK_IN, "NodeSocketFloat", "Thickness");
    /* We don't activate the output explicitly to avoid having two active outputs. We assume
     * `node_tree.get_output_node('Cycles')` will return this node. */
    new_output_cycles.custom1 = SHD_OUTPUT_CYCLES;

    bNode &shader_cycles = *blender::bke::node_add_static_node(
        nullptr, *ntree, SH_NODE_BSDF_DIFFUSE);
    bNodeSocket &shader_bsdf_output = *blender::bke::node_find_socket(
        shader_cycles, SOCK_OUT, "BSDF");
    bNodeSocket &shader_color_input = *blender::bke::node_find_socket(
        shader_cycles, SOCK_IN, "Color");

    version_node_add_link(
        *ntree, shader_cycles, shader_bsdf_output, new_output_cycles, output_surface_input);

    bNodeSocketValueRGBA *rgba = shader_color_input.default_value_typed<bNodeSocketValueRGBA>();
    rgba->value[0] = material->r;
    rgba->value[1] = material->g;
    rgba->value[2] = material->b;
    rgba->value[3] = material->a;

    if (old_output != nullptr) {
      shader_cycles.location[0] = old_output->location[0] + 1.5f * old_output->width;
      shader_cycles.location[1] = old_output->location[1] + 2.0f * old_output->height;
    }

    new_output_cycles.location[0] = shader_cycles.location[0] + 3.0f * shader_cycles.width;
    new_output_cycles.location[1] = shader_cycles.location[1];

    shader_cycles.parent = frame;
    new_output_cycles.parent = frame;
  }
}

static void do_version_set_alpha_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Type")) {
    return;
  }

  const auto &storage = *static_cast<NodeSetAlpha *>(node.storage);
  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Type");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.mode;
}

static void do_version_channel_matte_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Color Space")) {
    return;
  }

  const auto &storage = *static_cast<NodeChroma *>(node.storage);
  bNodeSocket &color_space_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Color Space");
  color_space_socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1 - 1;
  bNodeSocket &rgb_key_channel_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "RGB Key Channel");
  rgb_key_channel_socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom2 - 1;
  bNodeSocket &hsv_key_channel_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "HSV Key Channel");
  hsv_key_channel_socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom2 - 1;
  bNodeSocket &yuv_key_channel_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "YUV Key Channel");
  yuv_key_channel_socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom2 - 1;
  bNodeSocket &ycc_key_channel_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "YCbCr Key Channel");
  ycc_key_channel_socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom2 - 1;

  bNodeSocket &limit_method_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Limit Method");
  limit_method_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.algorithm;
  bNodeSocket &rgb_limit_channel_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "RGB Limit Channel");
  rgb_limit_channel_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.channel -
                                                                                1;
  bNodeSocket &hsv_limit_channel_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "HSV Limit Channel");
  hsv_limit_channel_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.channel -
                                                                                1;
  bNodeSocket &yuv_limit_channel_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "YUV Limit Channel");
  yuv_limit_channel_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.channel -
                                                                                1;
  bNodeSocket &ycc_limit_channel_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "YCbCr Limit Channel");
  ycc_limit_channel_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.channel -
                                                                                1;
}

static void do_version_color_balance_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Type")) {
    return;
  }

  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Type");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;
}

static void do_version_convert_alpha_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Type")) {
    return;
  }

  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Type");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;
}

static void do_version_distance_matte_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Color Space")) {
    return;
  }

  auto &storage = *static_cast<NodeChroma *>(node.storage);
  bNodeSocket &socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Color Space");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.channel - 1;
}

static void do_version_color_spill_menus_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Spill Channel")) {
    return;
  }

  bNodeSocket &spill_channel_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Spill Channel");
  spill_channel_socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1 - 1;
  bNodeSocket &limit_method_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Limit Method");
  limit_method_socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom2;

  auto &storage = *static_cast<NodeColorspill *>(node.storage);
  bNodeSocket &limit_channel_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Limit Channel");
  limit_channel_socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.limchan;
}

static void do_version_double_edge_mask_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Image Edges")) {
    return;
  }

  bNodeSocket &image_edges_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketBool", "Image Edges");
  image_edges_socket.default_value_typed<bNodeSocketValueBoolean>()->value = bool(node.custom2);
  bNodeSocket &only_inside_outer_socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketBool", "Only Inside Outer");
  only_inside_outer_socket.default_value_typed<bNodeSocketValueBoolean>()->value = bool(
      node.custom1);
}

static void version_dynamic_viewer_node_items(bNodeTree &ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree.nodes) {
    if (node->type_legacy != GEO_NODE_VIEWER) {
      continue;
    }
    NodeGeometryViewer *storage = static_cast<NodeGeometryViewer *>(node->storage);
    const int input_sockets_num = BLI_listbase_count(&node->inputs);
    if (input_sockets_num == storage->items_num + 1) {
      /* Make versioning idempotent. */
      continue;
    }
    storage->items_num = 2;
    storage->items = MEM_calloc_arrayN<NodeGeometryViewerItem>(2, __func__);
    NodeGeometryViewerItem &geometry_item = storage->items[0];
    geometry_item.name = BLI_strdup("Geometry");
    geometry_item.socket_type = SOCK_GEOMETRY;
    geometry_item.identifier = 0;
    NodeGeometryViewerItem &value_item = storage->items[1];
    value_item.name = BLI_strdup("Value");
    value_item.socket_type = blender::bke::custom_data_type_to_socket_type(
                                 eCustomDataType(storage->data_type_legacy))
                                 .value_or(SOCK_FLOAT);
    value_item.identifier = 1;
    storage->next_identifier = 2;
  }
}

static void do_version_displace_node_remove_xy_scale(bNodeTree &node_tree, bNode &node)
{
  blender::bke::node_tree_set_type(node_tree);

  bNodeSocket *displacement_input = blender::bke::node_find_socket(node, SOCK_IN, "Displacement");
  bNodeSocket *x_scale_input = blender::bke::node_find_socket(node, SOCK_IN, "X Scale");
  bNodeSocket *y_scale_input = blender::bke::node_find_socket(node, SOCK_IN, "Y Scale");

  /* Find the link going into the inputs of the node. */
  bNodeLink *displacement_link = nullptr;
  bNodeLink *x_scale_link = nullptr;
  bNodeLink *y_scale_link = nullptr;
  LISTBASE_FOREACH (bNodeLink *, link, &node_tree.links) {
    if (link->tosock == displacement_input) {
      displacement_link = link;
    }
    if (link->tosock == x_scale_input) {
      x_scale_link = link;
    }
    if (link->tosock == y_scale_input) {
      y_scale_link = link;
    }
  }

  bNode *multiply_node = blender::bke::node_add_node(nullptr, node_tree, "ShaderNodeVectorMath");
  multiply_node->parent = node.parent;
  multiply_node->location[0] = node.location[0] - node.width - 20.0f;
  multiply_node->location[1] = node.location[1];
  multiply_node->custom1 = NODE_VECTOR_MATH_MULTIPLY;

  bNodeSocket *multiply_a_input = blender::bke::node_find_socket(
      *multiply_node, SOCK_IN, "Vector");
  bNodeSocket *multiply_b_input = blender::bke::node_find_socket(
      *multiply_node, SOCK_IN, "Vector_001");
  bNodeSocket *multiply_output = blender::bke::node_find_socket(
      *multiply_node, SOCK_OUT, "Vector");

  copy_v2_v2(multiply_a_input->default_value_typed<bNodeSocketValueVector>()->value,
             displacement_input->default_value_typed<bNodeSocketValueVector>()->value);
  if (displacement_link) {
    version_node_add_link(node_tree,
                          *displacement_link->fromnode,
                          *displacement_link->fromsock,
                          *multiply_node,
                          *multiply_a_input);
    blender::bke::node_remove_link(&node_tree, *displacement_link);
  }

  version_node_add_link(node_tree, *multiply_node, *multiply_output, node, *displacement_input);

  bNode *combine_node = blender::bke::node_add_node(nullptr, node_tree, "ShaderNodeCombineXYZ");
  combine_node->parent = node.parent;
  combine_node->location[0] = multiply_node->location[0] - multiply_node->width - 20.0f;
  combine_node->location[1] = multiply_node->location[1];

  bNodeSocket *combine_x_input = blender::bke::node_find_socket(*combine_node, SOCK_IN, "X");
  bNodeSocket *combine_y_input = blender::bke::node_find_socket(*combine_node, SOCK_IN, "Y");
  bNodeSocket *combine_output = blender::bke::node_find_socket(*combine_node, SOCK_OUT, "Vector");

  version_node_add_link(
      node_tree, *combine_node, *combine_output, *multiply_node, *multiply_b_input);

  combine_x_input->default_value_typed<bNodeSocketValueFloat>()->value =
      x_scale_input->default_value_typed<bNodeSocketValueFloat>()->value;
  if (x_scale_link) {
    version_node_add_link(node_tree,
                          *x_scale_link->fromnode,
                          *x_scale_link->fromsock,
                          *combine_node,
                          *combine_x_input);
    blender::bke::node_remove_link(&node_tree, *x_scale_link);
  }

  combine_y_input->default_value_typed<bNodeSocketValueFloat>()->value =
      y_scale_input->default_value_typed<bNodeSocketValueFloat>()->value;
  if (y_scale_link) {
    version_node_add_link(node_tree,
                          *y_scale_link->fromnode,
                          *y_scale_link->fromsock,
                          *combine_node,
                          *combine_y_input);
    blender::bke::node_remove_link(&node_tree, *y_scale_link);
  }
}

/* The Size input is now in pixels, while previously, it was relative to 0.01 of the greater image
 * dimension. */
static void do_version_bokeh_blur_pixel_size(bNodeTree &node_tree, bNode &node)
{
  blender::bke::node_tree_set_type(node_tree);

  bNodeSocket *image_input = blender::bke::node_find_socket(node, SOCK_IN, "Image");
  bNodeSocket *size_input = blender::bke::node_find_socket(node, SOCK_IN, "Size");

  /* Find the link going into the inputs of the node. */
  bNodeLink *image_link = nullptr;
  bNodeLink *size_link = nullptr;
  LISTBASE_FOREACH (bNodeLink *, link, &node_tree.links) {
    if (link->tosock == size_input) {
      size_link = link;
    }
    if (link->tosock == image_input) {
      image_link = link;
    }
  }

  bNode &multiply_node = version_node_add_empty(node_tree, "ShaderNodeMath");
  multiply_node.parent = node.parent;
  multiply_node.location[0] = node.location[0] - node.width - 20.0f;
  multiply_node.location[1] = node.location[1];
  multiply_node.custom1 = NODE_MATH_MULTIPLY;

  bNodeSocket &multiply_a_input = version_node_add_socket(
      node_tree, multiply_node, SOCK_IN, "NodeSocketFloat", "Value");
  bNodeSocket &multiply_b_input = version_node_add_socket(
      node_tree, multiply_node, SOCK_IN, "NodeSocketFloat", "Value_001");
  bNodeSocket &multiply_output = version_node_add_socket(
      node_tree, multiply_node, SOCK_OUT, "NodeSocketFloat", "Value");

  multiply_a_input.default_value_typed<bNodeSocketValueFloat>()->value =
      size_input->default_value_typed<bNodeSocketValueFloat>()->value;
  if (size_link) {
    version_node_add_link(
        node_tree, *size_link->fromnode, *size_link->fromsock, multiply_node, multiply_a_input);
    blender::bke::node_remove_link(&node_tree, *size_link);
  }

  version_node_add_link(node_tree, multiply_node, multiply_output, node, *size_input);

  bNode *relative_to_pixel_node = blender::bke::node_add_node(
      nullptr, node_tree, "CompositorNodeRelativeToPixel");
  relative_to_pixel_node->parent = node.parent;
  relative_to_pixel_node->location[0] = multiply_node.location[0] - multiply_node.width - 20.0f;
  relative_to_pixel_node->location[1] = multiply_node.location[1];
  relative_to_pixel_node->custom1 = CMP_NODE_RELATIVE_TO_PIXEL_DATA_TYPE_FLOAT;
  relative_to_pixel_node->custom2 = CMP_NODE_RELATIVE_TO_PIXEL_REFERENCE_DIMENSION_GREATER;

  bNodeSocket *relative_to_pixel_image_input = blender::bke::node_find_socket(
      *relative_to_pixel_node, SOCK_IN, "Image");
  bNodeSocket *relative_to_pixel_value_input = blender::bke::node_find_socket(
      *relative_to_pixel_node, SOCK_IN, "Float Value");
  bNodeSocket *relative_to_pixel_value_output = blender::bke::node_find_socket(
      *relative_to_pixel_node, SOCK_OUT, "Float Value");

  version_node_add_link(node_tree,
                        *relative_to_pixel_node,
                        *relative_to_pixel_value_output,
                        multiply_node,
                        multiply_b_input);

  relative_to_pixel_value_input->default_value_typed<bNodeSocketValueFloat>()->value = 0.01f;
  if (image_link) {
    version_node_add_link(node_tree,
                          *image_link->fromnode,
                          *image_link->fromsock,
                          *relative_to_pixel_node,
                          *relative_to_pixel_image_input);
  }
}

static bool window_has_sequence_editor_open(const wmWindow *win)
{
  bScreen *screen = WM_window_get_active_screen(win);
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
      if (sl->spacetype == SPACE_SEQ) {
        return true;
      }
    }
  }
  return false;
}

/* Merge transform effect properties with strip transform. Because this effect could use modifiers,
 * change its type to gaussian blur with 0 radius. */
static void sequencer_substitute_transform_effects(Scene *scene)
{
  blender::seq::foreach_strip(&scene->ed->seqbase, [&](Strip *strip) -> bool {
    if (strip->type == STRIP_TYPE_TRANSFORM_LEGACY && strip->effectdata != nullptr) {
      TransformVarsLegacy *tv = static_cast<TransformVarsLegacy *>(strip->effectdata);
      StripTransform *transform = strip->data->transform;
      blender::float2 offset(tv->xIni, tv->yIni);
      if (tv->percent == 1) {
        blender::float2 scene_resolution(scene->r.xsch, scene->r.ysch);
        offset *= scene_resolution;
      }
      transform->xofs += offset.x;
      transform->yofs += offset.y;
      transform->scale_x *= tv->ScalexIni;
      transform->scale_y *= tv->ScaleyIni;
      transform->rotation += tv->rotIni;
      blender::seq::effect_free(strip);
      strip->type = STRIP_TYPE_GAUSSIAN_BLUR;
      blender::seq::effect_ensure_initialized(strip);
      GaussianBlurVars *gv = static_cast<GaussianBlurVars *>(strip->effectdata);
      gv->size_x = gv->size_y = 0.0f;
      blender::seq::edit_strip_name_set(scene, strip, "Transform Placeholder (Migrated)");
      blender::seq::ensure_unique_name(strip, scene);
    }
    return true;
  });
}

/* The LGG mode of the Color Balance node was being done in sRGB space, while now it is done in
 * linear space. So a Gamma node will be added before and after the node to perform the adjustment
 * in sRGB space. */
static void do_version_lift_gamma_gain_srgb_to_linear(bNodeTree &node_tree, bNode &node)
{
  bNodeSocket *image_input = blender::bke::node_find_socket(node, SOCK_IN, "Image");
  bNodeSocket *type_input = blender::bke::node_find_socket(node, SOCK_IN, "Type");
  bNodeSocket *image_output = blender::bke::node_find_socket(node, SOCK_OUT, "Image");

  /* Find the links going into and out of the node. */
  bNodeLink *image_input_link = nullptr;
  bNodeLink *type_input_link = nullptr;
  LISTBASE_FOREACH (bNodeLink *, link, &node_tree.links) {
    if (link->tosock == image_input) {
      image_input_link = link;
    }
    if (link->tosock == type_input) {
      type_input_link = link;
    }
  }

  if (type_input_link || !type_input ||
      type_input->default_value_typed<bNodeSocketValueMenu>()->value != CMP_NODE_COLOR_BALANCE_LGG)
  {
    return;
  }

  bNode *inverse_gamma_node = blender::bke::node_add_static_node(
      nullptr, node_tree, SH_NODE_GAMMA);
  inverse_gamma_node->parent = node.parent;
  inverse_gamma_node->location[0] = node.location[0];
  inverse_gamma_node->location[1] = node.location[1];

  bNodeSocket *inverse_gamma_color_input = blender::bke::node_find_socket(
      *inverse_gamma_node, SOCK_IN, "Color");
  copy_v4_v4(inverse_gamma_color_input->default_value_typed<bNodeSocketValueRGBA>()->value,
             image_input->default_value_typed<bNodeSocketValueRGBA>()->value);
  bNodeSocket *inverse_gamma_color_output = blender::bke::node_find_socket(
      *inverse_gamma_node, SOCK_OUT, "Color");

  bNodeSocket *inverse_gamma_input = blender::bke::node_find_socket(
      *inverse_gamma_node, SOCK_IN, "Gamma");
  inverse_gamma_input->default_value_typed<bNodeSocketValueFloat>()->value = 1.0f / 2.2f;

  version_node_add_link(
      node_tree, *inverse_gamma_node, *inverse_gamma_color_output, node, *image_input);
  if (image_input_link) {
    version_node_add_link(node_tree,
                          *image_input_link->fromnode,
                          *image_input_link->fromsock,
                          *inverse_gamma_node,
                          *inverse_gamma_color_input);
    blender::bke::node_remove_link(&node_tree, *image_input_link);
  }

  bNode *gamma_node = blender::bke::node_add_static_node(nullptr, node_tree, SH_NODE_GAMMA);
  gamma_node->parent = node.parent;
  gamma_node->location[0] = node.location[0];
  gamma_node->location[1] = node.location[1];

  bNodeSocket *gamma_color_input = blender::bke::node_find_socket(*gamma_node, SOCK_IN, "Color");
  bNodeSocket *gamma_color_output = blender::bke::node_find_socket(*gamma_node, SOCK_OUT, "Color");

  bNodeSocket *gamma_input = blender::bke::node_find_socket(*gamma_node, SOCK_IN, "Gamma");
  gamma_input->default_value_typed<bNodeSocketValueFloat>()->value = 2.2f;

  LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &node_tree.links) {
    if (link->fromsock != image_output) {
      continue;
    }

    version_node_add_link(
        node_tree, *gamma_node, *gamma_color_output, *link->tonode, *link->tosock);
    blender::bke::node_remove_link(&node_tree, *link);
  }

  version_node_add_link(node_tree, node, *image_output, *gamma_node, *gamma_color_input);
}

static void version_bone_hide_property_driver(AnimData *arm_adt, blender::Vector<Object *> &users)
{
  using namespace blender::animrig;
  constexpr char const *hide_prop_prefix = "bones[";
  constexpr char const *hide_prop_suffix = "\"].hide";

  blender::Vector<FCurve *> drivers_to_fix;
  LISTBASE_FOREACH (FCurve *, fcurve, &arm_adt->drivers) {
    const blender::StringRef rna_path(fcurve->rna_path);
    int quoted_bone_name_start = 0;
    int quoted_bone_name_end = 0;
    const bool is_prefix_found = BLI_str_quoted_substr_range(
        fcurve->rna_path, hide_prop_prefix, &quoted_bone_name_start, &quoted_bone_name_end);
    if (is_prefix_found && STREQ(fcurve->rna_path + quoted_bone_name_end, hide_prop_suffix)) {
      drivers_to_fix.append(fcurve);
    }
  }

  if (drivers_to_fix.is_empty()) {
    return;
  }

  for (Object *ob : users) {
    AnimData *ob_adt = BKE_animdata_ensure_id(&ob->id);
    for (FCurve *original : drivers_to_fix) {
      /* Has to be a copy in case there is more than 1 object using the armature. */
      FCurve *copy = BKE_fcurve_copy(original);
      char *fixed_path = BLI_string_joinN("pose.", copy->rna_path);
      MEM_SAFE_FREE(copy->rna_path);
      copy->rna_path = fixed_path;
      BLI_addtail(&ob_adt->drivers, copy);
    }
  }

  for (FCurve *original : drivers_to_fix) {
    BLI_remlink(&arm_adt->drivers, original);
    BKE_fcurve_free(original);
  }
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 54)) {
    LISTBASE_FOREACH (Object *, object, &bmain->objects) {
      if (object->type != OB_ARMATURE || !object->data) {
        continue;
      }
      BKE_pose_rebuild(nullptr, object, static_cast<bArmature *>(object->data), false);
      LISTBASE_FOREACH (bPoseChannel *, pose_bone, &object->pose->chanbase) {
        if (pose_bone->bone->flag & BONE_HIDDEN_P) {
          pose_bone->drawflag |= PCHAN_DRAW_HIDDEN;
        }
        else {
          pose_bone->drawflag &= ~PCHAN_DRAW_HIDDEN;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 57)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH_BACKWARD_MUTABLE (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_SUNBEAMS_DEPRECATED) {
            do_version_sun_beams(*node_tree, *node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 63)) {
    LISTBASE_FOREACH (wmWindowManager *, wm, &bmain->wm) {
      LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
        if (window_has_sequence_editor_open(win)) {
          Scene *scene = WM_window_get_active_scene(win);
          if (scene->ed != nullptr) {
            WorkSpace *workspace = WM_window_get_active_workspace(win);
            workspace->sequencer_scene = scene;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 97)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed != nullptr) {
        sequencer_substitute_transform_effects(scene);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 101)) {
    const uint8_t default_flags = DNA_struct_default_get(ToolSettings)->fix_to_cam_flag;
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (!scene->toolsettings) {
        continue;
      }
      scene->toolsettings->fix_to_cam_flag = default_flags;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 110)) {
    /* Build map of armature->object to quickly find out afterwards which armature is used by which
     * objects. */
    blender::Map<bArmature *, blender::Vector<Object *>> armature_usage_map;
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      if (ob->type != OB_ARMATURE || !ob->data) {
        continue;
      }
      bArmature *arm = reinterpret_cast<bArmature *>(ob->data);
      blender::Vector<Object *> &users = armature_usage_map.lookup_or_add_default(arm);
      users.append(ob);
    }

    LISTBASE_FOREACH (bArmature *, armature, &bmain->armatures) {
      AnimData *arm_adt = BKE_animdata_from_id(&armature->id);

      if (!arm_adt || BLI_listbase_is_empty(&arm_adt->drivers)) {
        continue;
      }

      blender::Vector<Object *> *users = armature_usage_map.lookup_ptr(armature);
      if (!users) {
        /* If `users` is a nullptr that means there is no user of that armature. That means the
         * property won't be fixed for armatures that are not used by an object during versioning.
         * However since the driver has to be moved to an object there is no way to fix it in this
         * case. */
        continue;
      }

      version_bone_hide_property_driver(arm_adt, *users);
    }
  }

  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */
}

static void remove_in_and_out_node_panel_recursive(bNodeTreeInterfacePanel &panel)
{
  using namespace blender;
  const Span old_sockets(panel.items_array, panel.items_num);

  Vector<bNodeTreeInterfaceItem *> new_sockets;
  for (bNodeTreeInterfaceItem *item : old_sockets) {
    if (item->item_type == NODE_INTERFACE_PANEL) {
      remove_in_and_out_node_panel_recursive(*reinterpret_cast<bNodeTreeInterfacePanel *>(item));
      continue;
    }
    bNodeTreeInterfaceSocket *socket = reinterpret_cast<bNodeTreeInterfaceSocket *>(item);
    constexpr int in_and_out = NODE_INTERFACE_SOCKET_INPUT | NODE_INTERFACE_SOCKET_OUTPUT;
    if ((socket->flag & in_and_out) != in_and_out) {
      continue;
    }

    bNodeTreeInterfaceSocket *new_output = MEM_callocN<bNodeTreeInterfaceSocket>(__func__);
    new_output->item.item_type = NODE_INTERFACE_SOCKET;
    new_output->name = BLI_strdup_null(socket->name);
    new_output->description = BLI_strdup_null(socket->description);
    new_output->socket_type = BLI_strdup_null(socket->socket_type);
    new_output->flag = socket->flag & ~NODE_INTERFACE_SOCKET_INPUT;
    new_output->attribute_domain = socket->attribute_domain;
    new_output->default_input = socket->default_input;
    new_output->default_attribute_name = BLI_strdup_null(socket->default_attribute_name);
    new_output->identifier = BLI_strdup(socket->identifier);
    if (socket->properties) {
      new_output->properties = IDP_CopyProperty_ex(socket->properties,
                                                   LIB_ID_CREATE_NO_USER_REFCOUNT);
    }
    new_output->structure_type = socket->structure_type;
    new_sockets.append(reinterpret_cast<bNodeTreeInterfaceItem *>(new_output));

    socket->flag &= ~NODE_INTERFACE_SOCKET_OUTPUT;
  }

  if (new_sockets.is_empty()) {
    return;
  }

  new_sockets.extend(old_sockets);
  VectorData new_socket_data = new_sockets.release();
  MEM_freeN(panel.items_array);
  panel.items_array = new_socket_data.data;
  panel.items_num = new_socket_data.size;
}

/**
 * Fix node interface sockest that could become both inputs and outputs before the current design
 * was settled on.
 */
static void remove_in_and_out_node_interface(bNodeTree &node_tree)
{
  remove_in_and_out_node_panel_recursive(node_tree.tree_interface.root_panel);
}

static void repair_node_link_node_pointers(FileData &fd, bNodeTree &node_tree)
{
  using namespace blender;
  Map<bNodeSocket *, bNode *> socket_to_node;
  LISTBASE_FOREACH (bNode *, node, &node_tree.nodes) {
    LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
      socket_to_node.add(socket, node);
    }
    LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
      socket_to_node.add(socket, node);
    }
  }
  LISTBASE_FOREACH (bNodeLink *, link, &node_tree.links) {
    bool fixed = false;
    bNode *to_node = socket_to_node.lookup(link->tosock);
    if (to_node != link->tonode) {
      link->tonode = to_node;
      fixed = true;
    }
    bNode *from_node = socket_to_node.lookup(link->fromsock);
    if (from_node != link->fromnode) {
      link->fromnode = from_node;
      fixed = true;
    }
    if (fixed) {
      BLO_reportf_wrap(fd.reports,
                       RPT_WARNING,
                       "Repairing invalid state in node link from %s:%s to %s:%s",
                       link->fromnode->name,
                       link->fromsock->identifier,
                       link->tonode->name,
                       link->tosock->identifier);
    }
  }
}

static void sequencer_remove_listbase_pointers(Scene &scene)
{
  Editing *ed = scene.ed;
  if (!ed) {
    return;
  }
  const MetaStack *last_meta_stack = blender::seq::meta_stack_active_get(ed);
  if (!last_meta_stack) {
    return;
  }
  ed->current_meta_strip = last_meta_stack->parent_strip;
  blender::seq::meta_stack_set(&scene, last_meta_stack->parent_strip);
}

static void do_version_adaptive_subdivision(Main *bmain)
{
  /* Move cycles properties natively into subdivision surface modifier. */
  bool experimental_features = false;
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    IDProperty *idprop = version_cycles_properties_from_ID(&scene->id);
    if (idprop) {
      experimental_features |= version_cycles_property_boolean(idprop, "feature_set", false);
    }
  }

  LISTBASE_FOREACH (Object *, object, &bmain->objects) {
    bool use_adaptive_subdivision = false;
    float dicing_rate = 1.0f;

    IDProperty *idprop = version_cycles_properties_from_ID(&object->id);
    if (idprop) {
      if (experimental_features) {
        use_adaptive_subdivision = version_cycles_property_boolean(
            idprop, "use_adaptive_subdivision", false);
      }
      dicing_rate = version_cycles_property_float(idprop, "dicing_rate", 1.0f);
    }

    LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
      if (md->type == eModifierType_Subsurf) {
        SubsurfModifierData *smd = (SubsurfModifierData *)md;
        smd->adaptive_space = SUBSURF_ADAPTIVE_SPACE_PIXEL;
        smd->adaptive_pixel_size = dicing_rate;
        smd->adaptive_object_edge_length = 0.01f;

        if (use_adaptive_subdivision) {
          smd->flags |= eSubsurfModifierFlag_UseAdaptiveSubdivision;
        }
      }
    }
  }
}

void blo_do_versions_500(FileData *fd, Library * /*lib*/, Main *bmain)
{
  using namespace blender;
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 1)) {
    LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
      bke::mesh_sculpt_mask_to_generic(*mesh);
      bke::mesh_custom_normals_to_generic(*mesh);
      rename_mesh_uv_seam_attribute(*mesh);
    }

    update_brush_sizes(*bmain);
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
        ts->uv_selectmode = UV_SELECT_VERT;
        ts->uv_flag |= UV_FLAG_SELECT_ISLAND;
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
          if (!ELEM(sl->spacetype, SPACE_ACTION, SPACE_GRAPH, SPACE_NLA, SPACE_SEQ)) {
            continue;
          }
          ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                 &sl->regionbase;
          ARegion *new_footer = do_versions_add_region_if_not_found(
              regionbase, RGN_TYPE_FOOTER, "footer for animation editors", RGN_TYPE_HEADER);
          if (new_footer == nullptr) {
            continue;
          }

          new_footer->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_TOP :
                                                                    RGN_ALIGN_BOTTOM;
          if (ELEM(sl->spacetype, SPACE_GRAPH, SPACE_NLA)) {
            new_footer->flag |= RGN_FLAG_HIDDEN;
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 25)) {
    version_seq_text_from_legacy(bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 26)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      for_each_mode_paint_settings(*scene, copy_unified_paint_settings);
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
        seq::foreach_strip(&ed->seqbase, [](Strip *strip) -> bool {
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

  /* ImageFormatData gained a new media type which we need to be set according to the
   * existing imtype. */
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

        NodeCompositorFileOutput *storage = static_cast<NodeCompositorFileOutput *>(node->storage);
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 45)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_GEOMETRY) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == GEO_NODE_FILL_CURVE) {
            do_version_fill_curve_options_to_inputs(*node_tree, *node);
          }
          else if (node->type_legacy == GEO_NODE_FILLET_CURVE) {
            do_version_fillet_curve_options_to_inputs(*node_tree, *node);
          }
          else if (node->type_legacy == GEO_NODE_RESAMPLE_CURVE) {
            do_version_resample_curve_options_to_inputs(*node_tree, *node);
          }
          else if (node->type_legacy == GEO_NODE_DISTRIBUTE_POINTS_IN_VOLUME) {
            do_version_distribute_points_in_volume_options_to_inputs(*node_tree, *node);
          }
          else if (node->type_legacy == GEO_NODE_MERGE_BY_DISTANCE) {
            do_version_merge_by_distance_options_to_inputs(*node_tree, *node);
          }
          else if (node->type_legacy == GEO_NODE_MESH_TO_VOLUME) {
            do_version_mesh_to_volume_options_to_inputs(*node_tree, *node);
          }
          else if (node->type_legacy == GEO_NODE_RAYCAST) {
            do_version_raycast_options_to_inputs(*node_tree, *node);
          }
          else if (node->type_legacy == GEO_NODE_REMOVE_ATTRIBUTE) {
            do_version_remove_attribute_options_to_inputs(*node_tree, *node);
          }
          else if (node->type_legacy == GEO_NODE_SAMPLE_GRID) {
            do_version_sample_grid_options_to_inputs(*node_tree, *node);
          }
          else if (node->type_legacy == GEO_NODE_SCALE_ELEMENTS) {
            do_version_scale_elements_options_to_inputs(*node_tree, *node);
          }
          else if (node->type_legacy == GEO_NODE_SET_CURVE_NORMAL) {
            do_version_set_curve_normal_options_to_inputs(*node_tree, *node);
          }
          else if (node->type_legacy == GEO_NODE_SUBDIVISION_SURFACE) {
            do_version_subdivision_surface_options_to_inputs(*node_tree, *node);
          }
          else if (node->type_legacy == GEO_NODE_UV_PACK_ISLANDS) {
            do_version_uv_pack_islands_options_to_inputs(*node_tree, *node);
          }
          else if (node->type_legacy == GEO_NODE_UV_UNWRAP) {
            do_version_uv_unwrap_options_to_inputs(*node_tree, *node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 46)) {
    /* Versioning from 0a0dd4ca37 was wrong, it only created asset shelf regions for Node Editors
     * that are Compositors. If you change a non-Node Editor (e.g. an Image Editor) to a Compositor
     * Editor, all is fine (SpaceLink *node_create gets called, the regions set up correctly), but
     * changing an existing Node Editor (e.g. Shader or Geometry Nodes) to a Compositor, no new
     * Space gets set up (rightfully so) and we are then missing the regions. Now corrected below
     * (version bump in 5.1 since that is also affected). */
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 48)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_COMPOSIT) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy != CMP_NODE_ROTATE) {
          continue;
        }
        if (node->storage != nullptr) {
          continue;
        }
        NodeRotateData *data = MEM_callocN<NodeRotateData>(__func__);
        data->interpolation = node->custom1;
        data->extension_x = CMP_NODE_EXTENSION_MODE_CLIP;
        data->extension_y = CMP_NODE_EXTENSION_MODE_CLIP;
        node->storage = data;
      }
      FOREACH_NODETREE_END;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 49)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_COMPOSIT) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy != CMP_NODE_DISPLACE) {
          continue;
        }
        if (node->storage == nullptr) {
          continue;
        }
        NodeDisplaceData *data = static_cast<NodeDisplaceData *>(node->storage);
        data->extension_x = CMP_NODE_EXTENSION_MODE_CLIP;
        data->extension_y = CMP_NODE_EXTENSION_MODE_CLIP;
      }
      FOREACH_NODETREE_END;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 50)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_COMPOSIT) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy != CMP_NODE_MAP_UV) {
          continue;
        }
        if (node->storage != nullptr) {
          continue;
        }
        NodeMapUVData *data = MEM_callocN<NodeMapUVData>(__func__);
        data->interpolation = node->custom2;
        data->extension_x = CMP_NODE_EXTENSION_MODE_CLIP;
        data->extension_y = CMP_NODE_EXTENSION_MODE_CLIP;
        node->storage = data;
      }
      FOREACH_NODETREE_END;
    }
  }
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 51)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_COMPOSIT) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy != CMP_NODE_CORNERPIN) {
          continue;
        }
        if (node->storage != nullptr) {
          continue;
        }
        NodeCornerPinData *data = MEM_callocN<NodeCornerPinData>(__func__);
        data->interpolation = node->custom1;
        data->extension_x = CMP_NODE_EXTENSION_MODE_CLIP;
        data->extension_y = CMP_NODE_EXTENSION_MODE_CLIP;
        node->storage = data;
      }
      FOREACH_NODETREE_END;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 54)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type != NTREE_COMPOSIT) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
        if (node->type_legacy == CMP_NODE_OUTPUT_FILE) {
          do_version_file_output_node(*node);
        }
      }
      FOREACH_NODETREE_END;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 58)) {
    LISTBASE_FOREACH (Object *, object, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, modifier, &object->modifiers) {
        if (modifier->type != eModifierType_GreasePencilLineart) {
          continue;
        }
        GreasePencilLineartModifierData *lmd = reinterpret_cast<GreasePencilLineartModifierData *>(
            modifier);
        if (lmd->radius != 0.0f) {
          continue;
        }
        lmd->radius = float(lmd->thickness_legacy) *
                      bke::greasepencil::LEGACY_RADIUS_CONVERSION_FACTOR;
      }
    }
  }
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 61)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_COMPOSIT) {
        continue;
      }
      version_node_input_socket_name(ntree, CMP_NODE_GAMMA_DEPRECATED, "Image", "Color");
      version_node_output_socket_name(ntree, CMP_NODE_GAMMA_DEPRECATED, "Image", "Color");

      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy == CMP_NODE_GAMMA_DEPRECATED) {
          node->type_legacy = SH_NODE_GAMMA;
          STRNCPY_UTF8(node->idname, "ShaderNodeGamma");
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 63)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->r.bake_flag & R_BAKE_MULTIRES) {
        scene->r.bake.type = scene->r.bake_mode;
        scene->r.bake.flag |= (scene->r.bake_flag & (R_BAKE_MULTIRES | R_BAKE_LORES_MESH));
        scene->r.bake.margin_type = scene->r.bake_margin_type;
        scene->r.bake.margin = scene->r.bake_margin;
      }
      else {
        scene->r.bake.type = R_BAKE_NORMALS;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 62)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->r.bake.displacement_space = R_BAKE_SPACE_OBJECT;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 64)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      remove_in_and_out_node_interface(*node_tree);
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 65)) {
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      srgb_to_linearrgb_v3_v3(brush->color, brush->rgb);
      srgb_to_linearrgb_v3_v3(brush->secondary_color, brush->secondary_rgb);
    }
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      UnifiedPaintSettings &ups = scene->toolsettings->unified_paint_settings;
      srgb_to_linearrgb_v3_v3(ups.color, ups.rgb);
      srgb_to_linearrgb_v3_v3(ups.secondary_color, ups.secondary_rgb);

      for_each_mode_paint_settings(*scene, [](Scene & /*scene*/, Paint *paint) {
        if (paint != nullptr) {
          UnifiedPaintSettings &ups = paint->unified_paint_settings;
          srgb_to_linearrgb_v3_v3(ups.color, ups.rgb);
          srgb_to_linearrgb_v3_v3(ups.secondary_color, ups.secondary_rgb);
        }
      });
    }
    LISTBASE_FOREACH (Palette *, palette, &bmain->palettes) {
      LISTBASE_FOREACH (PaletteColor *, color, &palette->colors) {
        srgb_to_linearrgb_v3_v3(color->color, color->rgb);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 66)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type != NTREE_COMPOSIT) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
        if (node->type_legacy == CMP_NODE_BLUR) {
          do_version_blur_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_FILTER) {
          do_version_filter_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_VIEW_LEVELS) {
          do_version_levels_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_DILATEERODE) {
          do_version_dilate_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_TONEMAP) {
          do_version_tone_map_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_LENSDIST) {
          do_version_lens_distortion_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_KUWAHARA) {
          do_version_kuwahara_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_DENOISE) {
          do_version_denoise_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_TRANSLATE) {
          do_version_translate_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_TRANSFORM) {
          do_version_transform_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_CORNERPIN) {
          do_version_corner_pin_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_MAP_UV) {
          do_version_map_uv_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_SCALE) {
          do_version_scale_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_ROTATE) {
          do_version_rotate_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_DISPLACE) {
          do_version_displace_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_STABILIZE2D) {
          do_version_stabilize_2d_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_MASK_BOX) {
          do_version_box_mask_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_MASK_ELLIPSE) {
          do_version_ellipse_mask_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_TRACKPOS) {
          do_version_track_position_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_KEYING) {
          do_version_keying_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_MASK) {
          do_version_mask_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_MOVIEDISTORTION) {
          do_version_movie_distortion_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_GLARE) {
          do_version_glare_menus_to_inputs(*node_tree, *node);
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 67)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      initialize_missing_closure_and_bundle_node_storage(*ntree);
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 68)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      sequencer_remove_listbase_pointers(*scene);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 71)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->toolsettings->uvsculpt.size *= 2;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 72)) {
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if (brush->curve_size == nullptr) {
        brush->curve_size = BKE_paint_default_curve();
      }
      if (brush->curve_strength == nullptr) {
        brush->curve_strength = BKE_paint_default_curve();
      }
      if (brush->curve_jitter == nullptr) {
        brush->curve_jitter = BKE_paint_default_curve();
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 73)) {
    /* Old files created on WIN32 use `\r`. */
    LISTBASE_FOREACH (Curve *, cu, &bmain->curves) {
      if (cu->str) {
        BLI_string_replace_char(cu->str, '\r', '\n');
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 74)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed != nullptr) {
        /* Set the first strip modifier as the active one and uncollapse the root panel. */
        blender::seq::foreach_strip(&scene->ed->seqbase, [&](Strip *strip) -> bool {
          seq::modifier_set_active(strip,
                                   static_cast<StripModifierData *>(strip->modifiers.first));
          LISTBASE_FOREACH (StripModifierData *, smd, &strip->modifiers) {
            smd->layout_panel_open_flag |= UI_PANEL_DATA_EXPAND_ROOT;
          }
          return true;
        });
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 75)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        version_node_socket_name(ntree, CMP_NODE_RGB, "RGBA", "Color");
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 81)) {
    LISTBASE_FOREACH (Material *, material, &bmain->materials) {
      do_version_material_remove_use_nodes(bmain, material);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 82)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type != NTREE_COMPOSIT) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
        if (node->type_legacy == CMP_NODE_SETALPHA) {
          do_version_set_alpha_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_CHANNEL_MATTE) {
          do_version_channel_matte_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_COLORBALANCE) {
          do_version_color_balance_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_PREMULKEY) {
          do_version_convert_alpha_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_DIST_MATTE) {
          do_version_distance_matte_menus_to_inputs(*node_tree, *node);
        }
        else if (node->type_legacy == CMP_NODE_COLOR_SPILL) {
          do_version_color_spill_menus_to_inputs(*node_tree, *node);
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 83)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type != NTREE_COMPOSIT) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
        if (node->type_legacy == CMP_NODE_DOUBLEEDGEMASK) {
          do_version_double_edge_mask_options_to_inputs(*node_tree, *node);
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 84)) {
    /* Add sidebar to the preferences editor. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_USERPREF) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;
            ARegion *new_sidebar = do_versions_add_region_if_not_found(
                regionbase, RGN_TYPE_UI, "sidebar for preferences", RGN_TYPE_HEADER);
            if (new_sidebar != nullptr) {
              new_sidebar->alignment = RGN_ALIGN_LEFT;
              new_sidebar->flag &= ~RGN_FLAG_HIDDEN;
            }
          }
        }
      }
    }
  }

  if (MAIN_VERSION_FILE_ATLEAST(bmain, 500, 23) && !MAIN_VERSION_FILE_ATLEAST(bmain, 500, 85)) {
    /* Old sky textures were temporarily removed and restored. */
    /* Change default Sky Texture to Nishita (after removal of old sky models) */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type_legacy == SH_NODE_TEX_SKY && node->storage) {
            NodeTexSky *tex = (NodeTexSky *)node->storage;
            if (tex->sky_model == 0) {
              tex->sky_model = SHD_SKY_SINGLE_SCATTERING;
            }
            if (tex->sky_model == 1) {
              tex->sky_model = SHD_SKY_MULTIPLE_SCATTERING;
            }
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 86)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }
      version_dynamic_viewer_node_items(*ntree);
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 87)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type != NTREE_COMPOSIT) {
        continue;
      }
      version_node_input_socket_name(node_tree, CMP_NODE_ALPHAOVER, "Image_001", "Foreground");
      version_node_input_socket_name(node_tree, CMP_NODE_ALPHAOVER, "Image", "Background");
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 88)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type != NTREE_COMPOSIT) {
        continue;
      }
      version_node_input_socket_name(node_tree, CMP_NODE_DISPLACE, "Vector", "Displacement");
      LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
        if (node->type_legacy == CMP_NODE_DISPLACE) {
          do_version_displace_node_remove_xy_scale(*node_tree, *node);
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 89)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type != NTREE_COMPOSIT) {
        continue;
      }
      version_node_input_socket_name(node_tree, CMP_NODE_BOKEHBLUR, "Bounding box", "Mask");
      LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
        if (node->type_legacy == CMP_NODE_BOKEHBLUR) {
          do_version_bokeh_blur_pixel_size(*node_tree, *node);
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 89)) {
    /* Node Editor: toggle overlays on. */
    if (!DNA_struct_exists(fd->filesdna, "SpaceClipOverlay")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
            if (space->spacetype == SPACE_CLIP) {
              SpaceClip *sclip = (SpaceClip *)space;
              sclip->overlay.flag |= SC_SHOW_OVERLAYS;
              sclip->overlay.flag |= SC_SHOW_CURSOR;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 92)) {
    do_version_adaptive_subdivision(bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 95)) {
    LISTBASE_FOREACH (Camera *, camera, &bmain->cameras) {
      float default_col[4] = {0.5f, 0.5f, 0.5f, 1.0f};
      copy_v4_v4(camera->composition_guide_color, default_col);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 97)) {
    /* Enable new "Optional Label" setting for all menu sockets. This was implicit before. */
    FOREACH_NODETREE_BEGIN (bmain, tree, id) {
      tree->tree_interface.foreach_item([&](bNodeTreeInterfaceItem &item) {
        if (item.item_type != NODE_INTERFACE_SOCKET) {
          return true;
        }
        auto &socket = reinterpret_cast<bNodeTreeInterfaceSocket &>(item);
        if (!STREQ(socket.socket_type, "NodeSocketMenu")) {
          return true;
        }
        socket.flag |= NODE_INTERFACE_SOCKET_OPTIONAL_LABEL;
        return true;
      });
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 98)) {
    /* For a brief period of time, these values were not properly versioned, so it is possible for
     * files to be in an odd state. This versioning was formerly run in 4.2 subversion 23. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      UvSculpt &uvsculpt = scene->toolsettings->uvsculpt;
      if (uvsculpt.size == 0 || uvsculpt.curve_distance_falloff == nullptr) {
        uvsculpt.size = 100;
        uvsculpt.strength = 1.0f;
        uvsculpt.curve_distance_falloff_preset = BRUSH_CURVE_SMOOTH;
        if (uvsculpt.curve_distance_falloff == nullptr) {
          uvsculpt.curve_distance_falloff = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 99)) {
    LISTBASE_FOREACH (wmWindowManager *, wm, &bmain->wm) {
      wm->xr.session_settings.fly_speed = 3.0f;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 102)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->r.time_jump_delta = 1.0f;
      scene->r.time_jump_unit = 1;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 405, 103)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
          if (node->type_legacy == CMP_NODE_COLORBALANCE) {
            do_version_lift_gamma_gain_srgb_to_linear(*node_tree, *node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 104)) {
    /* Dope Sheet Editor: toggle overlays on. */
    if (!DNA_struct_exists(fd->filesdna, "SpaceActionOverlays")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
            if (space->spacetype == SPACE_ACTION) {
              SpaceAction *space_action = (SpaceAction *)space;
              space_action->overlays.flag |= ADS_OVERLAY_SHOW_OVERLAYS;
              space_action->overlays.flag |= ADS_SHOW_SCENE_STRIP_FRAME_RANGE;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 105)) {
    LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
      bke::mesh_uv_select_to_single_attribute(*mesh);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 106)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type != NTREE_COMPOSIT) {
        continue;
      }
      version_node_input_socket_name(
          node_tree, CMP_NODE_COLORCORRECTION, "Master Lift", "Master Offset");
      version_node_input_socket_name(
          node_tree, CMP_NODE_COLORCORRECTION, "Highlights Lift", "Highlights Offset");
      version_node_input_socket_name(
          node_tree, CMP_NODE_COLORCORRECTION, "Midtones Lift", "Midtones Offset");
      version_node_input_socket_name(
          node_tree, CMP_NODE_COLORCORRECTION, "Shadows Lift", "Shadows Offset");
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 107)) {
    LISTBASE_FOREACH (Material *, material, &bmain->materials) {
      /* The flag was actually interpreted as reversed. */
      material->blend_flag ^= MA_BL_LIGHTPROBE_VOLUME_DOUBLE_SIDED;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 108)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_IMAGE) {
            SpaceImage *sima = reinterpret_cast<SpaceImage *>(sl);
            sima->iuser.flag &= ~IMA_SHOW_SEQUENCER_SCENE;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 109)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      repair_node_link_node_pointers(*fd, *ntree);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 112)) {
    /* The ownership of these pointers was moved to #CustomData in #customdata_version_242 and they
     * became deprecated in 05952aa94d33ee when we started using implicit-sharing. However, they
     * were never cleared and became dangling pointers. */
    LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
      mesh->mpoly = nullptr;
      mesh->mloop = nullptr;
      mesh->mvert = nullptr;
      mesh->medge = nullptr;
      mesh->dvert = nullptr;
      mesh->mtface = nullptr;
      mesh->tface = nullptr;
      mesh->mcol = nullptr;
      mesh->mface = nullptr;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 2)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype != SPACE_NODE) {
            continue;
          }

          ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                 &sl->regionbase;

          if (ARegion *new_shelf_region = do_versions_add_region_if_not_found(
                  regionbase,
                  RGN_TYPE_ASSET_SHELF,
                  "Asset shelf for compositing (versioning)",
                  RGN_TYPE_HEADER))
          {
            new_shelf_region->alignment = RGN_ALIGN_BOTTOM;
          }
          if (ARegion *new_shelf_header = do_versions_add_region_if_not_found(
                  regionbase,
                  RGN_TYPE_ASSET_SHELF_HEADER,
                  "Asset shelf header for compositing (versioning)",
                  RGN_TYPE_ASSET_SHELF))
          {
            new_shelf_header->alignment = RGN_ALIGN_BOTTOM | RGN_ALIGN_HIDE_WITH_PREV;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 501, 4)) {
    /* Clear mute flag on node types that set ntype->no_muting = true. */
    static const Set<std::string> no_muting_nodes = {"CompositorNodeViewer",
                                                     "NodeClosureInput",
                                                     "NodeClosureOutput",
                                                     "GeometryNodeForeachGeometryElementInput",
                                                     "GeometryNodeForeachGeometryElementOutput",
                                                     "GeometryNodeRepeatInput",
                                                     "GeometryNodeRepeatOutput",
                                                     "GeometryNodeSimulationInput",
                                                     "GeometryNodeSimulationOutput",
                                                     "GeometryNodeViewer",
                                                     "NodeGroupInput",
                                                     "NodeGroupOutput",
                                                     "ShaderNodeOutputAOV",
                                                     "ShaderNodeOutputLight",
                                                     "ShaderNodeOutputLineStyle",
                                                     "ShaderNodeOutputMaterial",
                                                     "ShaderNodeOutputWorld",
                                                     "TextureNodeOutput",
                                                     "TextureNodeViewer"};
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (no_muting_nodes.contains(node->idname)) {
          node->flag &= ~NODE_MUTED;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 500, 114)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (!scene->ed) {
        continue;
      }
      blender::seq::foreach_strip(&scene->ed->seqbase, [&](Strip *strip) {
        LISTBASE_FOREACH (StripModifierData *, md, &strip->modifiers) {
          md->ui_expand_flag = md->layout_panel_open_flag & UI_PANEL_DATA_EXPAND_ROOT;
        }
        return true;
      });
    }
  }

  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */

  /* Keep this versioning always enabled at the bottom of the function; it can only be moved
   * behind a subversion bump when the file format is changed. */
  LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
    bke::mesh_freestyle_marks_to_generic(*mesh);
  }

  /* TODO: Can be moved to subversion bump. */
  AS_asset_library_import_method_ensure_valid(*bmain);
}
