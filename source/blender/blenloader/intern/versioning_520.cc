/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include "NOD_geometry_nodes_srna.hh"

#include "DNA_ID.h"
#include "DNA_brush_types.h"
#include "DNA_curve_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_tree_interface_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BLI_listbase_iterator.hh"
#include "BLI_string.h"
#include "BLI_string_utils.hh"
#include "BLI_sys_types.h"

#include "BKE_animsys.h"
#include "BKE_colortools.hh"
#include "BKE_curves.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_report.hh"

#include "SEQ_iterator.hh"
#include "SEQ_sequencer.hh"

#include "BLO_read_write.hh"
#include "readfile.hh"

#include "versioning_common.hh"

// #include "CLG_log.h"

namespace blender {

// static CLG_LogRef LOG = {"blend.doversion"};

static void version_geometry_nodes_properties(FileData &fd,
                                              Main &bmain,
                                              Object &object,
                                              NodesModifierData &nmd)
{
  const IDProperty *old_props = nmd.settings_legacy.properties;
  if (!old_props) {
    /* Versioning has already been done, this check makes the function idempotent. */
    return;
  }
  if (!nmd.node_group) {
    IDP_FreeProperty(nmd.settings_legacy.properties);
    nmd.settings_legacy.properties = nullptr;
    BLO_reportf_wrap(fd.reports,
                     RPT_WARNING,
                     "Modifier '%s' from Object '%s' is missing its Geometry Node Group, its "
                     "settings will be lost (reset to default).",
                     nmd.modifier.name,
                     BKE_id_name(object.id));
    return;
  }
  if (ID_MISSING(&nmd.node_group->id)) {
    /* Keeping the old idproperties is not an option, and not really useful, since if the
     * blend-file is saved in this current state, it won't be re-versioned here later anyway.
     *
     * Furthermore, the whole remaining part of the code expects this to be nullptr, and keeping it
     * at runtime actually causes weird issues in depsgraph nodes building phase.
     *
     * So all in all, it's simpler and safer to also just lose these values here - if file is not
     * saved in this state, next loading will do the versioning if the node-group is available
     * again, otherwise that data is lost.
     */
    IDP_FreeProperty(nmd.settings_legacy.properties);
    nmd.settings_legacy.properties = nullptr;
    BLO_reportf_wrap(
        fd.reports,
        RPT_WARNING,
        "Modifier '%s' from Object '%s' is using a missing linked Geometry Node Group, its "
        "settings will be lost (reset to default) if the file is saved in this state.",
        nmd.modifier.name,
        BKE_id_name(object.id));
    return;
  }
  const bNodeTree &ntree = *nmd.node_group;
  ntree.ensure_interface_cache();

  IDProperty *system_props = bke::idprop::create_group("NodesModifierProperties").release();

  IDProperty *inputs = bke::idprop::create_group("inputs").release();
  IDP_AddToGroup(system_props, inputs);

  const std::string inputs_path_prefix = fmt::format("modifiers[\"{}\"]", nmd.modifier.name);
  for (const bNodeTreeInterfaceSocket *input : ntree.interface_inputs()) {
    const StringRefNull identifier = input->identifier;
    IDProperty *old_value_prop = IDP_GetPropertyFromGroup(old_props, identifier);
    if (!old_value_prop) {
      continue;
    }

    IDProperty *group = bke::idprop::create_group(identifier).release();
    IDP_AddToGroup(inputs, group);

    if (input->flag & NODE_INTERFACE_SOCKET_LAYER_SELECTION) {
      IDP_AddToGroup(
          group, bke::idprop::create("type", int(nodes::GeometryNodesInputType::Layer)).release());
      const StringRefNull layer_name = [&]() {
        const IDProperty *layer_name = IDP_GetPropertyFromGroup(old_props, identifier);
        if (layer_name) {
          return StringRefNull(IDP_string_get(layer_name));
        }
        return StringRefNull();
      }();
      IDP_AddToGroup(group, bke::idprop::create("layer_name", layer_name).release());
      continue;
    }

    IDProperty *new_value_prop = IDP_CopyProperty(old_value_prop);
    STRNCPY(new_value_prop->name, "value");
    IDP_AddToGroup(group, new_value_prop);

    const std::string old_value_path = fmt::format("[\"{}\"]", identifier);
    const std::string new_value_path = fmt::format(".properties.inputs.{}.value", identifier);
    BKE_animdata_fix_paths_rename_all_ex(&bmain,
                                         &object.id,
                                         inputs_path_prefix.c_str(),
                                         old_value_path.c_str(),
                                         new_value_path.c_str(),
                                         0,
                                         0,
                                         false,
                                         false);

    if (IDOverrideLibrary *override_library = object.id.override_library) {
      for (IDOverrideLibraryProperty &prop : override_library->properties) {
        const StringRef path = prop.rna_path;
        const int64_t i = path.find(inputs_path_prefix);
        if (i == StringRef::not_found) {
          continue;
        }
        if (path.drop_known_prefix(inputs_path_prefix) != old_value_path) {
          continue;
        }
        MEM_delete(prop.rna_path);
        prop.rna_path = BLI_sprintfN("%s%s", inputs_path_prefix.c_str(), new_value_path.c_str());
      }
    }

    bool use_attribute = false;
    if (const IDProperty *use_attribute_prop = IDP_GetPropertyFromGroup(
            old_props, identifier + "_use_attribute"))
    {
      /* This property changed to an enum property and animation is not versioned. */
      if (use_attribute_prop->type == IDP_INT) {
        use_attribute = bool(IDP_int_get(use_attribute_prop));
      }
      else {
        use_attribute = bool(IDP_bool_get(use_attribute_prop));
      }
    }

    const auto input_type = use_attribute ? nodes::GeometryNodesInputType::Attribute :
                                            nodes::GeometryNodesInputType::Value;
    IDP_AddToGroup(group, bke::idprop::create("type", int(input_type)).release());
    const StringRefNull attribute_name = [&]() {
      const IDProperty *attribute_name = IDP_GetPropertyFromGroup(old_props,
                                                                  identifier + "_attribute_name");
      if (attribute_name) {
        return StringRefNull(IDP_string_get(attribute_name));
      }
      return StringRefNull();
    }();
    IDP_AddToGroup(group, bke::idprop::create("attribute_name", attribute_name).release());
  }

  IDProperty *outputs = bke::idprop::create_group("outputs").release();
  IDP_AddToGroup(system_props, outputs);
  for (const bNodeTreeInterfaceSocket *output : ntree.interface_outputs()) {
    const StringRef identifier = output->identifier;
    IDProperty *old_name_prop = IDP_GetPropertyFromGroup(old_props,
                                                         identifier + "_attribute_name");
    if (!old_name_prop) {
      continue;
    }
    IDProperty *group = bke::idprop::create_group(identifier).release();
    IDP_AddToGroup(outputs, group);

    IDProperty *new_value_prop = IDP_CopyProperty(old_name_prop);
    STRNCPY(new_value_prop->name, "attribute_name");
    IDP_AddToGroup(group, new_value_prop);
  }

  if (nmd.modifier.system_properties) {
    IDP_FreeProperty(nmd.modifier.system_properties);
  }
  nmd.modifier.system_properties = system_props;
  IDP_FreeProperty(nmd.settings_legacy.properties);
  nmd.settings_legacy.properties = nullptr;
}

static void sanitize_node_tree_interface_socket_identifiers(bNodeTree &node_tree)
{
  node_tree.ensure_interface_cache();
  Set<StringRef> all_identifiers;
  for (bNodeTreeInterfaceItem *item : node_tree.interface_items()) {
    if (item->item_type == NODE_INTERFACE_PANEL) {
      continue;
    }
    auto &socket = *bke::node_interface::get_item_as<bNodeTreeInterfaceSocket>(item);
    /* Socket identifiers are required to be valid RNA identifiers and unique. */
    if (!RNA_validate_identifier(socket.identifier, true)) {
      RNA_identifier_sanitize(socket.identifier, true);
      if (all_identifiers.contains(socket.identifier)) {
        std::string new_identifier = BLI_uniquename_cb(
            [&](StringRef name) { return all_identifiers.contains(name); },
            '_',
            socket.identifier);
        MEM_SAFE_DELETE(socket.identifier);
        socket.identifier = BLI_strdup(new_identifier.c_str());
      }
    }
    all_identifiers.add(socket.identifier);
  }
}

/* Saving file extension is now a property of the File Output node. So inherit this
 * setting from the active scene to restore the old behavior.
 * Note: One limitation is that node groups containing file outputs that are not part of any
 * scene are not affected by versioning. */
static void do_version_file_output_use_file_extension_recursive(bNodeTree &node_tree,
                                                                const Scene &scene)
{
  for (bNode &node : node_tree.nodes) {
    if (node.type_legacy == CMP_NODE_OUTPUT_FILE) {
      NodeCompositorFileOutput *data = static_cast<NodeCompositorFileOutput *>(node.storage);
      data->use_file_extension = (scene.r.scemode & R_EXTENSION) != 0;
    }
    else if (node.type_legacy == NODE_GROUP) {
      bNodeTree *ngroup = id_cast<bNodeTree *>(node.id);
      if (ngroup) {
        do_version_file_output_use_file_extension_recursive(*ngroup, scene);
      }
    }
  }
}

static void version_clear_strip_linear_modifier_flag(Main &bmain)
{
  for (Scene &scene : bmain.scenes) {
    Editing *ed = seq::editing_get(&scene);
    if (ed != nullptr) {
      seq::foreach_strip(&ed->seqbase, [&](Strip *strip) {
        constexpr eStripFlag flag_linear_modifiers = eStripFlag(1 << 23);
        strip->flag &= ~flag_linear_modifiers;
        return true;
      });
    }
  }
}

static void fix_single_point_curves_custom_knots(Main *bmain)
{
  /* Fix corrupted flagu/flagv values created by older versions of the Curve Pen tool.
   * The tool could create loose vertices with invalid flag values (e.g. -2), where
   * CU_NURB_CUSTOM was set alongside other flags and knotsu/knotsv was left null,
   * causing a crash when opening these files in newer versions. */
  for (Curve &cu : bmain->curves) {
    for (Nurb *nu = static_cast<Nurb *>(cu.nurb.first); nu != nullptr; nu = nu->next) {
      if (nu->knotsu == nullptr && (nu->flagu & CU_NURB_CUSTOM)) {
        nu->flagu &= (CU_NURB_CYCLIC | CU_NURB_BEZIER | CU_NURB_ENDPOINT);
      }
      if (nu->knotsv == nullptr && (nu->flagv & CU_NURB_CUSTOM)) {
        nu->flagv &= (CU_NURB_CYCLIC | CU_NURB_BEZIER | CU_NURB_ENDPOINT);
      }
    }
  }
}

static void version_strip_modifier_show_preview_flag(Main &bmain)
{
  for (Scene &scene : bmain.scenes) {
    Editing *ed = seq::editing_get(&scene);
    if (ed == nullptr) {
      continue;
    }
    seq::foreach_strip(&ed->seqbase, [&](Strip *strip) {
      for (StripModifierData &smd : strip->modifiers) {
        if ((smd.flag & STRIP_MODIFIER_FLAG_MUTE) == 0) {
          smd.flag |= STRIP_MODIFIER_FLAG_SHOW_PREVIEW;
        }
      }
      return true;
    });
  }
}

void do_versions_after_linking_520(FileData *fd, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 2)) {
    for (Scene &scene : bmain->scenes) {
      bNodeTree *node_tree = version_get_scene_compositor_node_tree(bmain, &scene);
      if (node_tree == nullptr) {
        continue;
      }
      do_version_file_output_use_file_extension_recursive(*node_tree, scene);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 16)) {
    for (Object &object : bmain->objects) {
      for (ModifierData &md : object.modifiers) {
        if (md.type == eModifierType_Nodes) {
          version_geometry_nodes_properties(
              *fd, *bmain, object, reinterpret_cast<NodesModifierData &>(md));
        }
      }
    }
  }

  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */
}

void blo_do_versions_520(FileData * /*fd*/, Library * /*lib*/, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 1)) {
    for (Scene &scene : bmain->scenes) {
      scene.r.mode |= R_SAVE_OUTPUT;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 4)) {
    for (Brush &brush : bmain->brushes) {
      if (brush.gpencil_settings != nullptr) {
        brush.blend = 0;
      }
    }
  }
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 5)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id_owner) {
      for (bNode &node : node_tree->nodes) {
        if (node.type_legacy == FN_NODE_INPUT_VECTOR) {
          auto &data = *static_cast<NodeInputVector *>(node.storage);
          data.vector[3] = 0.0f;
          data.dimensions = 3;
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 6)) {
    for (Scene &scene : bmain->scenes) {
      SequencerToolSettings *sequencer_tool_settings = seq::tool_settings_ensure(&scene);
      sequencer_tool_settings->snap_flag |= SEQ_SNAP_TO_ALL_CHANNEL_STRIPS;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 7)) {
    for (Scene &scene : bmain->scenes) {
      scene.r.anisotropic_filter = 2;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 9)) {
    for (Mesh &mesh : bmain->meshes) {
      bke::mesh_freestyle_marks_to_generic(mesh);
    }
  }

  /* Convert H.264 codec value for older files (2.79), see #155775. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 10)) {
    for (Scene &scene : bmain->scenes) {
      if (scene.r.ffcodecdata.codec == 28) {
        scene.r.ffcodecdata.codec = 27;
      }
    }
  }

  /* Disable "unified" flags for Grease Pencil Draw mode. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 11)) {
    for (Scene &scene : bmain->scenes) {
      if (scene.toolsettings->gp_paint) {
        UnifiedPaintSettings &settings =
            scene.toolsettings->gp_paint->paint.unified_paint_settings;
        settings.flag &= ~(UNIFIED_PAINT_SIZE | UNIFIED_PAINT_ALPHA | UNIFIED_PAINT_COLOR);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 12)) {
    for (bScreen &screen : bmain->screens) {
      for (ScrArea &area : screen.areabase) {
        for (SpaceLink &space : area.spacedata) {
          if (space.spacetype == SPACE_NODE) {
            SpaceNode *space_node = reinterpret_cast<SpaceNode *>(&space);
            space_node->overlay.flag |= SN_OVERLAY_SHOW_RENDER_REGION;
            space_node->overlay.passepartout_alpha = 0.5f;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 13)) {
    version_clear_strip_linear_modifier_flag(*bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 14)) {
    fix_single_point_curves_custom_knots(bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 15)) {
    for (Scene &scene : bmain->scenes) {
      scene.r.scemode |= R_USE_TEXTURE_CACHE;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 16)) {
    for (Brush &brush : bmain->brushes) {
      if (brush.gpencil_settings != nullptr) {
        brush.gpencil_settings->curve_type = CURVE_TYPE_POLY;
        brush.gpencil_settings->conversion_threshold = 0.001f;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 17)) {
    for (Material &materials : bmain->materials) {
      if (materials.gp_style != nullptr) {
        materials.gp_style->placement_mode = GP_MATERIAL_PLACEMENT_COUNT;
        materials.gp_style->placement_count = 1;
        materials.gp_style->placement_density = 10.0f;
        materials.gp_style->placement_radius_spacing = 100.0f;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 18)) {
    for (Scene &scene : bmain->scenes) {
      if (scene.toolsettings->sculpt) {
        Sculpt &sculpt = *scene.toolsettings->sculpt;
        MeshAutomaskingSettings *settings = MEM_new<MeshAutomaskingSettings>(__func__);
        settings->flags = sculpt.automasking_flags;
        settings->boundary_edges_propagation_steps =
            sculpt.automasking_boundary_edges_propagation_steps;
        settings->cavity_blur_steps = sculpt.automasking_cavity_blur_steps;
        settings->cavity_factor = sculpt.automasking_cavity_factor;
        settings->start_normal_limit = sculpt.automasking_start_normal_limit;
        settings->start_normal_falloff = sculpt.automasking_start_normal_falloff;
        settings->view_normal_limit = sculpt.automasking_view_normal_limit;
        settings->view_normal_falloff = sculpt.automasking_view_normal_falloff;
        settings->cavity_curve = BKE_curvemapping_copy(sculpt.automasking_cavity_curve);
        settings->cavity_curve_op = BKE_curvemapping_copy(sculpt.automasking_cavity_curve_op);

        scene.toolsettings->sculpt->paint.mesh_automasking_settings = settings;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 19)) {
    for (bNodeTree &tree : bmain->nodetrees) {
      sanitize_node_tree_interface_socket_identifiers(tree);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 20)) {
    for (Brush &brush : bmain->brushes) {
      if (brush.ob_mode != OB_MODE_SCULPT) {
        continue;
      }

      brush.mesh_automasking_settings = MEM_new<MeshAutomaskingSettings>(__func__);
      brush.mesh_automasking_settings->flags = brush.automasking_flags;
      brush.mesh_automasking_settings->boundary_edges_propagation_steps =
          brush.automasking_boundary_edges_propagation_steps;
      brush.mesh_automasking_settings->cavity_blur_steps = brush.automasking_cavity_blur_steps;
      brush.mesh_automasking_settings->cavity_factor = brush.automasking_cavity_factor;
      brush.mesh_automasking_settings->start_normal_falloff =
          brush.automasking_start_normal_falloff;
      brush.mesh_automasking_settings->start_normal_limit = brush.automasking_start_normal_limit;
      brush.mesh_automasking_settings->view_normal_falloff = brush.automasking_view_normal_falloff;
      brush.mesh_automasking_settings->view_normal_limit = brush.automasking_view_normal_limit;
      brush.mesh_automasking_settings->cavity_curve = BKE_curvemapping_copy(
          brush.automasking_cavity_curve);
      brush.mesh_automasking_settings->cavity_curve_op = nullptr;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 21)) {
    for (Material &materials : bmain->materials) {
      if (materials.gp_style != nullptr) {
        materials.gp_style->random_size_factor = 0.0f;
        materials.gp_style->random_strength_factor = 0.0f;
        materials.gp_style->random_rotation_factor = 0.0f;
        materials.gp_style->random_hue_factor = 0.0f;
        materials.gp_style->random_saturation_factor = 0.0f;
        materials.gp_style->random_value_factor = 0.0f;
        materials.gp_style->random_noise_scale = 1.0f;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 22)) {
    version_strip_modifier_show_preview_flag(*bmain);
  }

  /* The ID member of the Viewer node is no longer initialized to the Viewer Image, so clear that
   * member. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 23)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        for (bNode &node : node_tree->nodes) {
          if (node.type_legacy == CMP_NODE_VIEWER) {
            node.id = nullptr;
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 24)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_SHADER) {
        for (bNode &node : node_tree->nodes) {
          if (node.type_legacy == SH_NODE_RAYCAST && node.storage == nullptr) {
            node.storage = MEM_new<NodeShaderRaycast>(__func__);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 25)) {
    for (bScreen &screen : bmain->screens) {
      for (ScrArea &area : screen.areabase) {
        for (SpaceLink &space : area.spacedata) {
          if (space.spacetype == SPACE_OUTLINER) {
            SpaceOutliner *space_outliner = reinterpret_cast<SpaceOutliner *>(&space);
            space_outliner->flag |= SO_SCROLL_TO_ACTIVE;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 502, 26)) {
    FOREACH_NODETREE_BEGIN (bmain, tree, id) {
      if (tree->type != NTREE_GEOMETRY) {
        continue;
      }
      for (bNode &node : tree->nodes) {
        switch (node.type_legacy) {
          case FN_NODE_COMPARE:
          case FN_NODE_RANDOM_VALUE: {
            version_socket_identifier_suffixes_for_dynamic_types(node.inputs, "_");
            version_socket_identifier_suffixes_for_dynamic_types(node.outputs, "_");
            break;
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

}  // namespace blender
