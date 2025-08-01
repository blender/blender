/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_collection_types.h"
#include "DNA_curves_types.h"
#include "DNA_defaults.h"
#include "DNA_modifier_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string_utf8.h"

#include "BKE_collection.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_file_handler.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_image_format.hh"
#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_paint.hh"
#include "BKE_screen.hh"

#include "SEQ_sequencer.hh"

#include "BLT_translation.hh"

#include "readfile.hh"

#include "versioning_common.hh"

void do_versions_after_linking_430(FileData * /*fd*/, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 6)) {
    /* Shift animation data to accommodate the new Diffuse Roughness input. */
    version_node_socket_index_animdata(bmain, NTREE_SHADER, SH_NODE_BSDF_PRINCIPLED, 7, 1, 30);
  }
}

static void update_paint_modes_for_brush_assets(Main &bmain)
{
  /* Replace paint brushes with a reference to the default brush asset for that mode. */
  LISTBASE_FOREACH (Scene *, scene, &bmain.scenes) {
    BKE_paint_brushes_set_default_references(scene->toolsettings);
  }

  /* Replace persistent tool references with the new single builtin brush tool. */
  LISTBASE_FOREACH (WorkSpace *, workspace, &bmain.workspaces) {
    LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
      if (tref->space_type == SPACE_IMAGE && tref->mode == SI_MODE_PAINT) {
        STRNCPY_UTF8(tref->idname, "builtin.brush");
        continue;
      }
      if (tref->space_type != SPACE_VIEW3D) {
        continue;
      }
      if (!ELEM(tref->mode,
                CTX_MODE_SCULPT,
                CTX_MODE_PAINT_VERTEX,
                CTX_MODE_PAINT_WEIGHT,
                CTX_MODE_PAINT_TEXTURE,
                CTX_MODE_PAINT_GPENCIL_LEGACY,
                CTX_MODE_PAINT_GREASE_PENCIL,
                CTX_MODE_SCULPT_GPENCIL_LEGACY,
                CTX_MODE_SCULPT_GREASE_PENCIL,
                CTX_MODE_WEIGHT_GPENCIL_LEGACY,
                CTX_MODE_WEIGHT_GREASE_PENCIL,
                CTX_MODE_VERTEX_GREASE_PENCIL,
                CTX_MODE_VERTEX_GPENCIL_LEGACY,
                CTX_MODE_SCULPT_CURVES))
      {
        continue;
      }
      STRNCPY_UTF8(tref->idname, "builtin.brush");
    }
  }
}

/**
 * It was possible that curve attributes were initialized to 0 even if that is not allowed for some
 * attributes.
 */
static void fix_built_in_curve_attribute_defaults(Main *bmain)
{
  LISTBASE_FOREACH (Curves *, curves, &bmain->hair_curves) {
    const int curves_num = curves->geometry.curve_num;
    if (int *resolutions = static_cast<int *>(CustomData_get_layer_named_for_write(
            &curves->geometry.curve_data_legacy, CD_PROP_INT32, "resolution", curves_num)))
    {
      for (int &resolution : blender::MutableSpan{resolutions, curves_num}) {
        resolution = std::max(resolution, 1);
      }
    }
    if (int8_t *nurb_orders = static_cast<int8_t *>(CustomData_get_layer_named_for_write(
            &curves->geometry.curve_data_legacy, CD_PROP_INT8, "nurbs_order", curves_num)))
    {
      for (int8_t &nurbs_order : blender::MutableSpan{nurb_orders, curves_num}) {
        nurbs_order = std::max<int8_t>(nurbs_order, 1);
      }
    }
  }
}

static void node_reroute_add_storage(bNodeTree &tree)
{
  for (bNode *node : tree.all_nodes()) {
    if (node->is_reroute()) {
      if (node->storage != nullptr) {
        continue;
      }

      bNodeSocket &input = *static_cast<bNodeSocket *>(node->inputs.first);
      bNodeSocket &output = *static_cast<bNodeSocket *>(node->outputs.first);

      /* Use uniform identifier for sockets. In old Blender versions (<=2021, up to af0b7925), the
       * identifiers were sometimes all lower case. Fixing those wrong socket identifiers is
       * important because otherwise they loose links now that the reroute node also uses node
       * declarations. */
      STRNCPY_UTF8(input.identifier, "Input");
      STRNCPY_UTF8(output.identifier, "Output");

      NodeReroute *data = MEM_callocN<NodeReroute>(__func__);
      STRNCPY_UTF8(data->type_idname, input.idname);
      node->storage = data;
    }
  }
}

static void add_bevel_modifier_attribute_name_defaults(Main &bmain)
{
  LISTBASE_FOREACH (Object *, ob, &bmain.objects) {
    if (ob->type != OB_MESH) {
      continue;
    }
    LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
      if (md->type == eModifierType_Bevel) {
        BevelModifierData *bmd = reinterpret_cast<BevelModifierData *>(md);
        if (bmd->vertex_weight_name[0] == '\0') {
          STRNCPY(bmd->vertex_weight_name, "bevel_weight_vert");
        }
        if (bmd->edge_weight_name[0] == '\0') {
          STRNCPY(bmd->edge_weight_name, "bevel_weight_edge");
        }
      }
    }
  }
}

static void hide_simulation_node_skip_socket_value(Main &bmain)
{
  LISTBASE_FOREACH (bNodeTree *, tree, &bmain.nodetrees) {
    LISTBASE_FOREACH (bNode *, node, &tree->nodes) {
      if (node->type_legacy != GEO_NODE_SIMULATION_OUTPUT) {
        continue;
      }
      bNodeSocket *skip_input = static_cast<bNodeSocket *>(node->inputs.first);
      if (!skip_input || !STREQ(skip_input->identifier, "Skip")) {
        continue;
      }
      auto *default_value = static_cast<bNodeSocketValueBoolean *>(skip_input->default_value);
      if (!default_value->value) {
        continue;
      }
      bool is_linked = false;
      LISTBASE_FOREACH (bNodeLink *, link, &tree->links) {
        if (link->tosock == skip_input) {
          is_linked = true;
        }
      }
      if (is_linked) {
        continue;
      }

      bNode &input_node = version_node_add_empty(*tree, "FunctionNodeInputBool");
      input_node.parent = node->parent;
      input_node.locx_legacy = node->locx_legacy - 25;
      input_node.locy_legacy = node->locy_legacy;

      NodeInputBool *input_node_storage = MEM_callocN<NodeInputBool>(__func__);
      input_node.storage = input_node_storage;
      input_node_storage->boolean = true;

      bNodeSocket &input_node_socket = version_node_add_socket(
          *tree, input_node, SOCK_OUT, "NodeSocketBool", "Boolean");

      version_node_add_link(*tree, input_node, input_node_socket, *node, *skip_input);

      /* Change the old socket value so that the versioning code is not run again. */
      default_value->value = false;
    }
  }
}

void blo_do_versions_430(FileData * /*fd*/, Library * /*lib*/, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 2)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, space_link, &area->spacedata) {
          if (space_link->spacetype == SPACE_NODE) {
            SpaceNode *space_node = reinterpret_cast<SpaceNode *>(space_link);
            space_node->flag &= ~SNODE_FLAG_UNUSED_5;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 3)) {
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if (BrushGpencilSettings *settings = brush->gpencil_settings) {
        /* Copy the `draw_strength` value to the `alpha` value. */
        brush->alpha = settings->draw_strength;

        /* We approximate the simplify pixel threshold by taking the previous threshold (world
         * space) and dividing by the legacy radius conversion factor. This should generally give
         * reasonable "pixel" threshold values, at least for previous GPv2 defaults. */
        settings->simplify_px = settings->simplify_f /
                                blender::bke::greasepencil::LEGACY_RADIUS_CONVERSION_FACTOR * 0.1f;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 4)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->view_settings.temperature = 6500.0f;
      scene->view_settings.tint = 10.0f;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 7)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      SequencerToolSettings *sequencer_tool_settings = blender::seq::tool_settings_ensure(scene);
      sequencer_tool_settings->snap_mode |= SEQ_SNAP_TO_PREVIEW_BORDERS |
                                            SEQ_SNAP_TO_PREVIEW_CENTER |
                                            SEQ_SNAP_TO_STRIPS_PREVIEW;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 8)) {
    update_paint_modes_for_brush_assets(*bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 9)) {
    fix_built_in_curve_attribute_defaults(bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 10)) {
    /* Initialize Color Balance node white point settings. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_CUSTOM) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type_legacy == CMP_NODE_COLORBALANCE) {
            NodeColorBalance *n = static_cast<NodeColorBalance *>(node->storage);
            n->input_temperature = n->output_temperature = 6500.0f;
            n->input_tint = n->output_tint = 10.0f;
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 11)) {
    LISTBASE_FOREACH (Curves *, curves, &bmain->hair_curves) {
      curves->geometry.attributes_active_index = curves->attributes_active_index_legacy;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 13)) {
    Camera default_cam = *DNA_struct_default_get(Camera);
    LISTBASE_FOREACH (Camera *, camera, &bmain->cameras) {
      camera->central_cylindrical_range_u_min = default_cam.central_cylindrical_range_u_min;
      camera->central_cylindrical_range_u_max = default_cam.central_cylindrical_range_u_max;
      camera->central_cylindrical_range_v_min = default_cam.central_cylindrical_range_v_min;
      camera->central_cylindrical_range_v_max = default_cam.central_cylindrical_range_v_max;
      camera->central_cylindrical_radius = default_cam.central_cylindrical_radius;
    }
  }

  /* The File Output node now uses the linear color space setting of its stored image formats. So
   * we need to ensure the color space value is initialized to some sane default based on the image
   * type. Furthermore, the node now gained a new Save As Render option that is global to the node,
   * which will be used if Use Node Format is enabled for each input, so we potentially need to
   * disable Use Node Format in case inputs had different Save As render options. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 14)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_COMPOSIT) {
        continue;
      }

      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type_legacy != CMP_NODE_OUTPUT_FILE) {
          continue;
        }

        /* Initialize node format color space if it is not set. */
        NodeImageMultiFile *storage = static_cast<NodeImageMultiFile *>(node->storage);
        if (storage->format.linear_colorspace_settings.name[0] == '\0') {
          BKE_image_format_update_color_space_for_type(&storage->format);
        }

        if (BLI_listbase_is_empty(&node->inputs)) {
          continue;
        }

        /* Initialize input formats color space if it is not set. */
        LISTBASE_FOREACH (const bNodeSocket *, input, &node->inputs) {
          NodeImageMultiFileSocket *input_storage = static_cast<NodeImageMultiFileSocket *>(
              input->storage);
          if (input_storage->format.linear_colorspace_settings.name[0] == '\0') {
            BKE_image_format_update_color_space_for_type(&input_storage->format);
          }
        }

        /* EXR images don't use Save As Render. */
        if (ELEM(storage->format.imtype, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER)) {
          continue;
        }

        /* Find out if all inputs have the same Save As Render option. */
        const bNodeSocket *first_input = static_cast<bNodeSocket *>(node->inputs.first);
        const NodeImageMultiFileSocket *first_input_storage =
            static_cast<NodeImageMultiFileSocket *>(first_input->storage);
        const bool first_save_as_render = first_input_storage->save_as_render;
        bool all_inputs_have_same_save_as_render = true;
        LISTBASE_FOREACH (const bNodeSocket *, input, &node->inputs) {
          const NodeImageMultiFileSocket *input_storage = static_cast<NodeImageMultiFileSocket *>(
              input->storage);
          if (bool(input_storage->save_as_render) != first_save_as_render) {
            all_inputs_have_same_save_as_render = false;
            break;
          }
        }

        /* All inputs have the same save as render option, so we set the node Save As Render option
         * to that value, and we leave inputs as is. */
        if (all_inputs_have_same_save_as_render) {
          storage->save_as_render = first_save_as_render;
          continue;
        }

        /* For inputs that have Use Node Format enabled, we need to disabled it because otherwise
         * they will use the node's Save As Render option. It follows that we need to copy the
         * node's format to the input format. */
        LISTBASE_FOREACH (const bNodeSocket *, input, &node->inputs) {
          NodeImageMultiFileSocket *input_storage = static_cast<NodeImageMultiFileSocket *>(
              input->storage);

          if (!input_storage->use_node_format) {
            continue;
          }

          input_storage->use_node_format = false;
          input_storage->format = storage->format;
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 15)) {
    using namespace blender;

    LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
      const ListBase *exporters = &collection->exporters;
      LISTBASE_FOREACH (CollectionExport *, data, exporters) {
        /* The name field should be empty at this point. */
        BLI_assert(data->name[0] == '\0');

        bke::FileHandlerType *fh = bke::file_handler_find(data->fh_idname);
        BKE_collection_exporter_name_set(exporters, data, fh ? fh->label : DATA_("Undefined"));
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 16)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->eevee.flag |= SCE_EEVEE_FAST_GI_ENABLED;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 17)) {
    FOREACH_NODETREE_BEGIN (bmain, tree, id) {
      if (tree->default_group_node_width == 0) {
        tree->default_group_node_width = GROUP_NODE_DEFAULT_WIDTH;
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 20)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SEQ) {
            ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_TOOLS);
            if (region != nullptr) {
              region->flag &= ~RGN_FLAG_HIDDEN;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 21)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_CLIP) {
            ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
            if (region != nullptr) {
              View2D *v2d = &region->v2d;
              v2d->flag &= ~V2D_VIEWSYNC_SCREEN_TIME;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 22)) {
    add_bevel_modifier_attribute_name_defaults(*bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 23)) {
    LISTBASE_FOREACH (Object *, object, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
        if (md->type != eModifierType_Nodes) {
          continue;
        }
        NodesModifierData &nmd = *reinterpret_cast<NodesModifierData *>(md);
        if (nmd.bake_target == NODES_MODIFIER_BAKE_TARGET_INHERIT) {
          /* Use disk target for existing modifiers to avoid changing behavior. */
          nmd.bake_target = NODES_MODIFIER_BAKE_TARGET_DISK;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 24)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      node_reroute_add_storage(*ntree);
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 26)) {
    hide_simulation_node_skip_socket_value(*bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 28)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = reinterpret_cast<View3D *>(sl);
            copy_v3_fl(v3d->overlay.gpencil_grid_color, 0.5f);
            copy_v2_fl(v3d->overlay.gpencil_grid_scale, 1.0f);
            copy_v2_fl(v3d->overlay.gpencil_grid_offset, 0.0f);
            v3d->overlay.gpencil_grid_subdivisions = 4;
          }
        }
      }
    }

    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_COMPOSIT) {
        continue;
      }
      LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree->nodes) {
        if (ELEM(node->type_legacy, CMP_NODE_VIEWER, CMP_NODE_COMPOSITE_DEPRECATED)) {
          node->flag &= ~NODE_PREVIEW;
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 29)) {
    /* Open warnings panel by default. */
    LISTBASE_FOREACH (Object *, object, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
        if (md->type == eModifierType_Nodes) {
          md->layout_panel_open_flag |= 1 << NODES_MODIFIER_PANEL_WARNINGS;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 403, 31)) {
    LISTBASE_FOREACH (WorkSpace *, workspace, &bmain->workspaces) {
      LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
        if (tref->space_type != SPACE_SEQ) {
          continue;
        }
        STRNCPY_UTF8(tref->idname, "builtin.select_box");
      }
    }
  }
}
