/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#define DNA_DEPRECATED_ALLOW

#include "DNA_ID.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_curves_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_listbase_iterator.hh"
#include "BLI_math_base.hh"
#include "BLI_string_ref.hh"
#include "BLI_sys_types.hh"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_camera.h"
#include "BKE_compositor.hh"
#include "BKE_main.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"
#include "BKE_screen.hh"

#include "SEQ_iterator.hh"
#include "SEQ_sequencer.hh"

#include "readfile.hh"

#include "versioning_common.hh"

// #include "CLG_log.h"

namespace blender {

// static CLG_LogRef LOG = {"blend.doversion"};

static void do_version_set_grease_pencil_colors_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Mode"_ustr)) {
    return;
  }
  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Mode");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;
}

static void do_version_set_grease_pencil_depth_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (blender::bke::node_find_socket(node, SOCK_IN, "Depth Order"_ustr)) {
    return;
  }
  bNodeSocket &socket = version_node_add_socket(
      ntree, node, SOCK_IN, "NodeSocketMenu", "Depth Order");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = node.custom1;
}

static void do_version_merge_layers_options_to_inputs(bNodeTree &ntree, bNode &node)
{
  if (!version_node_ensure_storage_or_invalidate(node)) {
    return;
  }

  auto &storage = *reinterpret_cast<NodeGeometryMergeLayers *>(node.storage);

  if (blender::bke::node_find_socket(node, SOCK_IN, "Mode"_ustr)) {
    return;
  }
  bNodeSocket &socket = version_node_add_socket(ntree, node, SOCK_IN, "NodeSocketMenu", "Mode");
  socket.default_value_typed<bNodeSocketValueMenu>()->value = storage.mode;
}

static void compositing_node_group_to_effect(Main &main, Scene &scene)
{
  bNodeTree *node_group = version_get_scene_compositor_node_tree(&main, &scene);
  if (!node_group) {
    return;
  }

  SceneCompositorEffect &effect = bke::compositor::new_effect(scene, "Effect");
  effect.node_group = node_group;
  if (!node_group->compositor_node_asset_traits) {
    node_group->compositor_node_asset_traits = MEM_new<CompositorNodeAssetTraits>(__func__);
  }
  node_group->compositor_node_asset_traits->flag |= COMPOSIT_NODE_ASSET_SCENE_EFFECT;
  bke::node_update_asset_metadata(*node_group);
  scene.compositing_node_group = nullptr;
}

static float version_503_camera_view_viewfac(const int sensor_fit,
                                             const int winx,
                                             const int winy,
                                             const float frame_aspect)
{
  const float2 frame_size = BKE_camera_frame_size(winx, winy, frame_aspect);

  return (BKE_camera_sensor_fit(sensor_fit, 1.0f, frame_aspect) == CAMERA_SENSOR_FIT_HOR) ?
             frame_size.x :
             frame_size.y;
}

static void version_503_camera_view_fit(RegionView3D *rv3d,
                                        const int sensor_fit,
                                        const int winx,
                                        const int winy,
                                        const float render_aspect)
{
  /* The camera frame is now automatically fitted to the viewport, taking into count
   * the render aspect ratio. Here we version existing camera views to match. */
  if ((rv3d->persp != RV3D_CAMOB) && (rv3d->camzoom == 0.0f) && (rv3d->camdx == 0.0f) &&
      (rv3d->camdy == 0.0f))
  {
    return;
  }

  const float viewfac_old = version_503_camera_view_viewfac(
      sensor_fit, winx, winy, float(winy) / float(winx));
  const float viewfac_new = version_503_camera_view_viewfac(sensor_fit, winx, winy, render_aspect);

  /* Adjust zoom factor. */
  const float zoomfac_old = BKE_screen_view3d_zoom_to_fac(rv3d->camzoom);
  rv3d->camzoom = math::clamp(
      BKE_screen_view3d_zoom_from_fac(zoomfac_old * viewfac_old / viewfac_new),
      float(RV3D_CAMZOOM_MIN),
      float(RV3D_CAMZOOM_MAX));

  /* Adjust panning. */
  const float zoomfac_new = BKE_screen_view3d_zoom_to_fac(rv3d->camzoom);
  rv3d->camdx = math::clamp(rv3d->camdx * zoomfac_old / zoomfac_new, -1.0f, 1.0f);
  rv3d->camdy = math::clamp(rv3d->camdy * zoomfac_old / zoomfac_new, -1.0f, 1.0f);
}

static void do_versioning_camera_view_zoom(Main *bmain)
{
  for (bScreen &screen : bmain->screens) {
    Scene *scene = bmain->scenes.first_as<Scene>();
    for (wmWindowManager &wm : bmain->wm) {
      for (wmWindow &win : wm.windows) {
        if (win.winid == screen.winid) {
          scene = win.scene;
        }
      }
    }
    if (scene == nullptr) {
      continue;
    }
    const float render_aspect = (float(scene->r.ysch) * scene->r.yasp) /
                                (float(scene->r.xsch) * scene->r.xasp);

    for (ScrArea &area : screen.areabase) {
      for (SpaceLink &space : area.spacedata) {
        if (space.spacetype != SPACE_VIEW3D) {
          continue;
        }
        View3D *v3d = reinterpret_cast<View3D *>(&space);

        const Object *camera = (v3d->camera != nullptr) ? v3d->camera : scene->camera;
        const int sensor_fit = (camera != nullptr && camera->type == OB_CAMERA &&
                                camera->data != nullptr) ?
                                   id_cast<const Camera *>(camera->data)->sensor_fit :
                                   CAMERA_SENSOR_FIT_AUTO;

        ListBaseT<ARegion> *regions = (area.spacedata.first() == &space) ? &area.regionbase :
                                                                           &space.regionbase;
        for (ARegion &region : *regions) {
          if (region.regiontype != RGN_TYPE_WINDOW) {
            continue;
          }
          RegionView3D *rv3d = static_cast<RegionView3D *>(region.regiondata);
          if (rv3d == nullptr) {
            continue;
          }
          if ((region.winx <= 1) || (region.winy <= 1)) {
            continue;
          }

          version_503_camera_view_fit(rv3d, sensor_fit, region.winx, region.winy, render_aspect);
          if (rv3d->localvd != nullptr) {
            version_503_camera_view_fit(
                rv3d->localvd, sensor_fit, region.winx, region.winy, render_aspect);
          }
        }
      }
    }
  }
}

void do_versions_after_linking_503(FileData * /*fd*/, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 503, 8)) {
    version_node_socket_index_animdata(
        bmain, NTREE_GEOMETRY, "GeometryNodeSetGreasePencilColor", 5, 1, 6);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 503, 15)) {
    for (Scene &scene : bmain->scenes) {
      compositing_node_group_to_effect(*bmain, scene);
    }
  }
  else {
    /* The now deprecated compositing_node_group is always written on file writes for forward
     * compatibility, so it has to be reset to nullptr if no versioning was needed.
     *
     * Todo(#140111): Forward compatibility support will be removed in 6.0, and this loop can then
     * be placed behind a `MAIN_VERSION_FILE_OLDER(bmain, 600, xxx)` check . */
    for (Scene &scene : bmain->scenes) {
      scene.compositing_node_group = nullptr;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 503, 16)) {
    /* Shift animation data to accommodate the new dispersion inputs. */
    version_node_socket_index_animdata(bmain, NTREE_SHADER, "ShaderNodeBsdfPrincipled", 20, 2, 33);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 503, 19)) {
    /* Runs here instead of `blo_do_versions_503`, since screen datablocks are not fully linked
     * (e.g. `#View3D.camera`) until after linking. */
    do_versioning_camera_view_zoom(bmain);
  }

  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */
}

void blo_do_versions_503(FileData * /*fd*/, Library * /*lib*/, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 503, 1)) {
    for (Scene &scene : bmain->scenes) {
      VPaint *wpaint = scene.toolsettings->wpaint;
      if (wpaint && wpaint->paint.brush_asset_reference) {
        const StringRefNull old_asset_id =
            wpaint->paint.brush_asset_reference->relative_asset_identifier;
        if (wpaint->paint.brush == nullptr && old_asset_id.endswith("Paint")) {
          /* The "Paint" brush asset was renamed to "Add Weight", find it via the default instead
           * of hard-coding the new name. */
          if (std::optional<AssetWeakReference> paint_brush_asset_reference =
                  BKE_paint_brush_type_default_reference(bke::paint::AssetCategory::MeshWeight,
                                                         WPAINT_BRUSH_TYPE_DRAW))
          {
            BKE_paint_brush_set(bmain, &wpaint->paint, *paint_brush_asset_reference);
          }
        }
      }
    }
  }

  /* The compositor previously did not support default inputs for group nodes, but some built-in
   * nodes had the position field default type for some inputs, so node groups would gain it as a
   * default type through some operators. Later, the default inputs were supported for group nodes,
   * though position field were not supported in the compositor, so it would assert. To fix this,
   * we reset any position field default input to the default value. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 503, 3)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
      if (node_tree->type == NTREE_COMPOSIT) {
        node_tree->ensure_interface_cache();
        for (bNodeTreeInterfaceSocket *input : node_tree->interface_inputs()) {
          if (input->default_input == NODE_DEFAULT_INPUT_POSITION_FIELD) {
            input->default_input = NODE_DEFAULT_INPUT_VALUE;
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 503, 4)) {
    for (bScreen &screen : bmain->screens) {
      for (ScrArea &area : screen.areabase) {
        for (SpaceLink &sl : area.spacedata) {
          if (sl.spacetype == SPACE_ACTION) {
            SpaceAction *saction = reinterpret_cast<SpaceAction *>(&sl);
            saction->cache_display |= TIME_CACHE_COMPOSITOR;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 503, 6)) {
    for (Brush &brush : bmain->brushes) {
      if (ELEM(brush.ob_mode, OB_MODE_WEIGHT_PAINT, OB_MODE_VERTEX_PAINT)) {
        brush.mesh_automasking_settings = MEM_new<MeshAutomaskingSettings>(__func__);
        brush.mesh_automasking_settings->cavity_curve = BKE_sculpt_default_cavity_curve();
      }
    }

    auto apply_to_paint = [&](Paint *paint) {
      if (paint == nullptr) {
        return;
      }

      paint->mesh_automasking_settings = MEM_new<MeshAutomaskingSettings>("blo_do_versions_520");
      paint->mesh_automasking_settings->cavity_curve = BKE_sculpt_default_cavity_curve();
      paint->mesh_automasking_settings->cavity_curve_op = BKE_sculpt_default_cavity_curve();
    };

    for (Scene &scene : bmain->scenes) {
      apply_to_paint(reinterpret_cast<Paint *>(scene.toolsettings->vpaint));
      apply_to_paint(reinterpret_cast<Paint *>(scene.toolsettings->wpaint));
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 503, 7)) {
    for (Scene &scene : bmain->scenes) {
      for (ViewLayer &view_layer : scene.view_layers) {
        view_layer.eevee.denoising_pass_flags =
            EEVEE_DENOISING_PASS_USE_ALBEDO_ROUGHNESS_WEIGHTING;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 503, 8)) {
    FOREACH_NODETREE_BEGIN (bmain, node_tree, id_owner) {
      for (bNode &node : node_tree->nodes) {
        if (STREQ(node.idname, "GeometryNodeSetGreasePencilColor")) {
          do_version_set_grease_pencil_colors_options_to_inputs(*node_tree, node);
        }
        if (STREQ(node.idname, "GeometryNodeSetGreasePencilDepth")) {
          do_version_set_grease_pencil_depth_options_to_inputs(*node_tree, node);
        }
        if (STREQ(node.idname, "GeometryNodeMergeLayers")) {
          do_version_merge_layers_options_to_inputs(*node_tree, node);
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 503, 9)) {
    for (Brush &brush : bmain->brushes) {
      if (brush.curve_hardness == nullptr) {
        brush.curve_hardness = brush.paint_flags & BRUSH_PAINT_HARDNESS_PRESSURE_INVERT ?
                                   BKE_paint_default_curve_inverted() :
                                   BKE_paint_default_curve();
      }
      if (brush.curve_auto_smooth == nullptr) {
        brush.curve_auto_smooth = BKE_paint_default_curve_inverted();
      }
      if (brush.curve_spacing == nullptr) {
        brush.curve_spacing = BKE_paint_default_curve();
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 503, 11)) {
    /* This code formerly ran whenever entering Sculpt Mode, it is unlikely that there are any
     * files that have these settings with invalid values, but since these values have never been
     * set in versioning, perform this
     */
    for (Scene &scene : bmain->scenes) {
      Sculpt *sd = scene.toolsettings->sculpt;
      if (sd == nullptr) {
        continue;
      }

      const Sculpt defaults = {};
      if (sd->detail_percent == 0.0f) {
        sd->detail_percent = defaults.detail_percent;
      }
      if (sd->constant_detail == 0.0f) {
        sd->constant_detail = defaults.constant_detail;
      }
      if (sd->detail_size == 0.0f) {
        sd->detail_size = defaults.detail_size;
      }

      if (!sd->paint.tile_offset[0]) {
        sd->paint.tile_offset[0] = 1.0f;
      }
      if (!sd->paint.tile_offset[1]) {
        sd->paint.tile_offset[1] = 1.0f;
      }
      if (!sd->paint.tile_offset[2]) {
        sd->paint.tile_offset[2] = 1.0f;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 503, 12)) {
    for (Brush &brush : bmain->brushes) {
      if (brush.ob_mode & OB_MODE_WEIGHT_PAINT || brush.ob_mode & OB_MODE_VERTEX_PAINT) {
        if (brush.flag & BRUSH_FRONTFACE_FALLOFF_DEPRECATED && brush.falloff_angle_legacy != 0.0f)
        {
          switch (brush.falloff_shape) {
            case PAINT_FALLOFF_SHAPE_SPHERE:
              brush.mesh_automasking_settings->flags |= BRUSH_AUTOMASKING_BRUSH_NORMAL;
              brush.mesh_automasking_settings->start_normal_falloff = 0.5f;
              brush.mesh_automasking_settings->start_normal_limit = brush.falloff_angle_legacy;
              break;
            case PAINT_FALLOFF_SHAPE_TUBE:
              brush.mesh_automasking_settings->flags |= BRUSH_AUTOMASKING_VIEW_NORMAL;
              brush.mesh_automasking_settings->view_normal_falloff = 0.5f;
              brush.mesh_automasking_settings->view_normal_limit = brush.falloff_angle_legacy;
              break;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 503, 13)) {
    auto validate_active_index_fn = [](const AttributeOwner &owner, int &active_index) -> void {
      const bke::AttributeStorage *storage = owner.get_storage();
      if ((active_index < 0) || (active_index >= storage->count()) ||
          !bke::allow_procedural_attribute_access(storage->at_index(active_index).name()))
      {
        active_index = -1;
      }
    };

    for (Mesh &mesh : bmain->meshes) {
      validate_active_index_fn(AttributeOwner::from_id(&mesh.id), mesh.attributes_active_index);
    }
    for (Curves &curves : bmain->hair_curves) {
      validate_active_index_fn(AttributeOwner::from_id(&curves.id),
                               curves.geometry.attributes_active_index);
    }
    for (GreasePencil &grease_pencil : bmain->grease_pencils) {
      validate_active_index_fn(AttributeOwner::from_id(&grease_pencil.id),
                               grease_pencil.attributes_active_index);
      /* Also check the attributes_active_index in the individual drawings */
      for (GreasePencilDrawingBase *drawing_base : grease_pencil.drawings()) {
        if (drawing_base->type == GP_DRAWING) {
          GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base);
          validate_active_index_fn(
              AttributeOwner(AttributeOwnerType::GreasePencilDrawing, drawing),
              drawing->geometry.attributes_active_index);
        }
      }
    }
    for (PointCloud &pointcloud : bmain->pointclouds) {
      validate_active_index_fn(AttributeOwner::from_id(&pointcloud.id),
                               pointcloud.attributes_active_index);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 503, 14)) {
    for (bScreen &screen : bmain->screens) {
      for (ScrArea &area : screen.areabase) {
        for (SpaceLink &space : area.spacedata) {
          if (space.spacetype == SPACE_OUTLINER) {
            SpaceOutliner *space_outliner = reinterpret_cast<SpaceOutliner *>(&space);
            space_outliner->flag |= SO_EXPAND_ON_FOCUS;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 503, 16)) {
    for (Brush &brush : bmain->brushes) {
      if (ELEM(brush.ob_mode,
               OB_MODE_SCULPT,
               OB_MODE_VERTEX_PAINT,
               OB_MODE_TEXTURE_PAINT,
               OB_MODE_SCULPT_GREASE_PENCIL,
               OB_MODE_VERTEX_GREASE_PENCIL))
      {
        brush.unified_paint_flags |= BRUSH_USE_UNIFIED_PAINT_SIZE | BRUSH_USE_UNIFIED_PAINT_COLOR;
      }
      if (ELEM(brush.ob_mode,
               OB_MODE_SCULPT_CURVES,
               OB_MODE_WEIGHT_PAINT,
               OB_MODE_WEIGHT_GREASE_PENCIL))
      {
        brush.unified_paint_flags |= BRUSH_USE_UNIFIED_PAINT_SIZE;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 503, 17)) {
    for (Scene &scene : bmain->scenes) {
      if (Editing *ed = seq::editing_get(&scene)) {
        seq::foreach_strip(&ed->seqbase, [&](Strip *strip) {
          switch (strip->type) {
            case STRIP_TYPE_IMAGE:
              /* Every image in the sequence has its own #StripElem. Content trimming
               * (#anim_startofs, #anim_endofs) hides elements at both ends without shrinking the
               * array, so the full array is always this many elements long. */
              strip->data->stripdata_num = strip->anim_startofs + strip->len + strip->anim_endofs;
              break;
            case STRIP_TYPE_MOVIE:
            case STRIP_TYPE_SOUND:
              /* Single #StripElem storing the file path. */
              strip->data->stripdata_num = strip->data->stripdata ? 1 : 0;
              break;
            default:
              strip->data->stripdata_num = 0;
              break;
          }
          return true;
        });
      }
    }
  }

  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */

  /* Keep this versioning always enabled at the bottom of the function; it can only be moved
   * behind a subversion bump when the file format is changed (#mesh_skin_to_legacy is removed from
   * #mesh_blend_write). Since that function keeps writing the old-format #CD_MVERT_SKIN layer for
   * forward compatibility, files saved by *this* version also need
   * this conversion to run unconditionally on read, not just for files older than this subversion.
   */
  for (Mesh &mesh : bmain->meshes) {
    bke::mesh_skin_to_generic(mesh);
  }
}

}  // namespace blender
