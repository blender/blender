/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 *
 * This file handles updating the `startup.blend`, this is used when reading old files.
 *
 * Unlike regular versioning this makes changes that ensure the startup file
 * has brushes and other presets setup to take advantage of newer features.
 *
 * To update preference defaults see `userdef_default.c`.
 */

#define DNA_DEPRECATED_ALLOW

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_mempool.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "DNA_camera_types.h"
#include "DNA_curveprofile_types.h"
#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_light_types.h"
#include "DNA_mask_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"
#include "DNA_world_types.h"

#include "BKE_appdir.hh"
#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_curveprofile.h"
#include "BKE_customdata.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_idprop.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_main_namemap.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"
#include "BKE_screen.hh"
#include "BKE_workspace.hh"

#include "BLO_readfile.hh"

#include "BLT_translation.hh"

#include "versioning_common.hh"

/* Make preferences read-only, use `versioning_userdef.cc`. */
#define U (*((const UserDef *)&U))

static bool blo_is_builtin_template(const char *app_template)
{
  /* For all builtin templates shipped with Blender. */
  return (!app_template || STR_ELEM(app_template,
                                    N_("2D_Animation"),
                                    N_("Storyboarding"),
                                    N_("Sculpting"),
                                    N_("VFX"),
                                    N_("Video_Editing")));
}

static void blo_update_defaults_screen(bScreen *screen,
                                       const char *app_template,
                                       const char *workspace_name)
{
  /* For all app templates. */
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      /* Some toolbars have been saved as initialized,
       * we don't want them to have odd zoom-level or scrolling set, see: #47047 */
      if (ELEM(region->regiontype, RGN_TYPE_UI, RGN_TYPE_TOOLS, RGN_TYPE_TOOL_PROPS)) {
        region->v2d.flag &= ~V2D_IS_INIT;
      }
    }

    /* Set default folder. */
    LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
      if (sl->spacetype == SPACE_FILE) {
        SpaceFile *sfile = (SpaceFile *)sl;
        if (sfile->params) {
          const char *dir_default = BKE_appdir_folder_default();
          if (dir_default) {
            STRNCPY(sfile->params->dir, dir_default);
            sfile->params->file[0] = '\0';
          }
        }
      }
    }
  }

  /* For builtin templates only. */
  if (!blo_is_builtin_template(app_template)) {
    return;
  }

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      /* Remove all stored panels, we want to use defaults
       * (order, open/closed) as defined by UI code here! */
      BKE_area_region_panels_free(&region->panels);
      BLI_freelistN(&region->panels_category_active);

      /* Reset size so it uses consistent defaults from the region types. */
      region->sizex = 0;
      region->sizey = 0;
    }

    if (area->spacetype == SPACE_IMAGE) {
      if (STREQ(workspace_name, "UV Editing")) {
        SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
        if (sima->mode == SI_MODE_VIEW) {
          sima->mode = SI_MODE_UV;
        }
        sima->uv_face_opacity = 1.0f;
      }
      else if (STR_ELEM(workspace_name, "Texture Paint", "Shading")) {
        SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
        sima->uv_face_opacity = 0.0f;
      }
    }
    else if (area->spacetype == SPACE_ACTION) {
      /* Show markers region, hide channels and collapse summary in timelines. */
      SpaceAction *saction = static_cast<SpaceAction *>(area->spacedata.first);
      saction->flag |= SACTION_SHOW_MARKERS;
      if (saction->mode == SACTCONT_TIMELINE) {
        saction->ads.flag |= ADS_FLAG_SUMMARY_COLLAPSED;

        LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
          if (region->regiontype == RGN_TYPE_CHANNELS) {
            region->flag |= RGN_FLAG_HIDDEN;
          }
        }
      }
      else {
        /* Open properties panel by default. */
        LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
          if (region->regiontype == RGN_TYPE_UI) {
            region->flag &= ~RGN_FLAG_HIDDEN;
          }
        }
      }
    }
    else if (area->spacetype == SPACE_GRAPH) {
      SpaceGraph *sipo = static_cast<SpaceGraph *>(area->spacedata.first);
      sipo->flag |= SIPO_SHOW_MARKERS;
    }
    else if (area->spacetype == SPACE_NLA) {
      SpaceNla *snla = static_cast<SpaceNla *>(area->spacedata.first);
      snla->flag |= SNLA_SHOW_MARKERS;
    }
    else if (area->spacetype == SPACE_SEQ) {
      SpaceSeq *seq = static_cast<SpaceSeq *>(area->spacedata.first);
      seq->flag |= SEQ_SHOW_MARKERS | SEQ_ZOOM_TO_FIT | SEQ_USE_PROXIES | SEQ_SHOW_OVERLAY;
      seq->render_size = SEQ_RENDER_SIZE_PROXY_100;
      seq->timeline_overlay.flag |= SEQ_TIMELINE_SHOW_STRIP_SOURCE | SEQ_TIMELINE_SHOW_STRIP_NAME |
                                    SEQ_TIMELINE_SHOW_STRIP_DURATION | SEQ_TIMELINE_SHOW_GRID |
                                    SEQ_TIMELINE_SHOW_STRIP_COLOR_TAG |
                                    SEQ_TIMELINE_SHOW_STRIP_RETIMING |
                                    SEQ_TIMELINE_WAVEFORMS_HALF | SEQ_TIMELINE_SHOW_THUMBNAILS;
      seq->preview_overlay.flag |= SEQ_PREVIEW_SHOW_OUTLINE_SELECTED;
      seq->cache_overlay.flag = SEQ_CACHE_SHOW | SEQ_CACHE_SHOW_FINAL_OUT;
      seq->draw_flag |= SEQ_DRAW_TRANSFORM_PREVIEW;
    }
    else if (area->spacetype == SPACE_TEXT) {
      /* Show syntax and line numbers in Script workspace text editor. */
      SpaceText *stext = static_cast<SpaceText *>(area->spacedata.first);
      stext->showsyntax = true;
      stext->showlinenrs = true;
      stext->flags |= ST_FIND_WRAP;
    }
    else if (area->spacetype == SPACE_VIEW3D) {
      View3D *v3d = static_cast<View3D *>(area->spacedata.first);
      /* Screen space cavity by default for faster performance. */
      v3d->shading.cavity_type = V3D_SHADING_CAVITY_CURVATURE;
      v3d->shading.flag |= V3D_SHADING_SPECULAR_HIGHLIGHT;
      v3d->overlay.texture_paint_mode_opacity = 1.0f;
      v3d->overlay.weight_paint_mode_opacity = 1.0f;
      v3d->overlay.vertex_paint_mode_opacity = 1.0f;
      /* Update default Z bias for retopology overlay. */
      v3d->overlay.retopology_offset = 0.01f;
      /* Clear this deprecated bit for later reuse. */
      v3d->overlay.edit_flag &= ~V3D_OVERLAY_EDIT_EDGES_DEPRECATED;
      /* grease pencil settings */
      v3d->vertex_opacity = 1.0f;
      v3d->gp_flag |= V3D_GP_SHOW_EDIT_LINES;
      /* Remove dither pattern in wireframe mode. */
      v3d->shading.xray_alpha_wire = 0.0f;
      v3d->clip_start = 0.01f;
      /* Skip startups that use the viewport color by default. */
      if (v3d->shading.background_type != V3D_SHADING_BACKGROUND_VIEWPORT) {
        copy_v3_fl(v3d->shading.background_color, 0.05f);
      }
      /* Disable Curve Normals. */
      v3d->overlay.edit_flag &= ~V3D_OVERLAY_EDIT_CU_NORMALS;
      v3d->overlay.normals_constant_screen_size = 7.0f;
      /* Always enable Grease Pencil vertex color overlay by default. */
      v3d->overlay.gpencil_vertex_paint_opacity = 1.0f;
      /* Always use theme color for wireframe by default. */
      v3d->shading.wire_color_type = V3D_SHADING_SINGLE_COLOR;

      /* Level out the 3D Viewport camera rotation, see: #113751. */
      constexpr float viewports_to_level[][4] = {
          /* Animation, Modeling, Scripting, Texture Paint, UV Editing. */
          {0x1.6e7cb8p-1, -0x1.c1747p-2, -0x1.2997dap-2, -0x1.d5d806p-2},
          /* Layout. */
          {0x1.6e7cb8p-1, -0x1.c17478p-2, -0x1.2997dcp-2, -0x1.d5d80cp-2},
          /* Geometry Nodes. */
          {0x1.6e7cb6p-1, -0x1.c17476p-2, -0x1.2997dep-2, -0x1.d5d80cp-2},
      };

      constexpr float viewports_to_clear_ofs[][4] = {
          /* Geometry Nodes. */
          {0x1.6e7cb6p-1, -0x1.c17476p-2, -0x1.2997dep-2, -0x1.d5d80cp-2},
          /* Sculpting. */
          {0x1.885b28p-1, -0x1.2d10cp-1, -0x1.42ae54p-3, -0x1.a486a2p-3},
      };

      constexpr float unified_viewquat[4] = {
          0x1.6cbc88p-1, -0x1.c3a5c8p-2, -0x1.26413ep-2, -0x1.db430ap-2};

      LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
        if (region->regiontype == RGN_TYPE_WINDOW) {
          RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

          for (int i = 0; i < ARRAY_SIZE(viewports_to_clear_ofs); i++) {
            if (equals_v4v4(rv3d->viewquat, viewports_to_clear_ofs[i])) {
              zero_v3(rv3d->ofs);
            }
          }

          for (int i = 0; i < ARRAY_SIZE(viewports_to_level); i++) {
            if (equals_v4v4(rv3d->viewquat, viewports_to_level[i])) {
              copy_qt_qt(rv3d->viewquat, unified_viewquat);
            }
          }
        }
      }
    }
    else if (area->spacetype == SPACE_CLIP) {
      SpaceClip *sclip = static_cast<SpaceClip *>(area->spacedata.first);
      sclip->around = V3D_AROUND_CENTER_MEDIAN;
      sclip->mask_info.blend_factor = 0.7f;
      sclip->mask_info.draw_flag = MASK_DRAWFLAG_SPLINE;
    }
  }

  /* Show tool-header by default (for most cases at least, hide for others). */
  const bool hide_image_tool_header = STREQ(workspace_name, "Rendering");
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
      ListBase *regionbase = (sl == static_cast<SpaceLink *>(area->spacedata.first)) ?
                                 &area->regionbase :
                                 &sl->regionbase;

      LISTBASE_FOREACH (ARegion *, region, regionbase) {
        if (region->regiontype == RGN_TYPE_TOOL_HEADER) {
          if (((sl->spacetype == SPACE_IMAGE) && hide_image_tool_header) ||
              sl->spacetype == SPACE_SEQ)
          {
            region->flag |= RGN_FLAG_HIDDEN;
          }
          else {
            region->flag &= ~(RGN_FLAG_HIDDEN | RGN_FLAG_HIDDEN_BY_USER);
          }
        }
      }
    }
  }

  /* 2D animation template. */
  if (app_template && STREQ(app_template, "2D_Animation")) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (area->spacetype == SPACE_ACTION) {
        SpaceAction *saction = static_cast<SpaceAction *>(area->spacedata.first);
        /* Enable Sliders. */
        saction->flag |= SACTION_SLIDERS;
      }
      else if (area->spacetype == SPACE_VIEW3D) {
        View3D *v3d = static_cast<View3D *>(area->spacedata.first);
        /* Set Material Color by default. */
        v3d->shading.color_type = V3D_SHADING_MATERIAL_COLOR;
        /* Enable Annotations. */
        v3d->flag2 |= V3D_SHOW_ANNOTATION;
      }
    }
  }
}

void BLO_update_defaults_workspace(WorkSpace *workspace, const char *app_template)
{
  LISTBASE_FOREACH (WorkSpaceLayout *, layout, &workspace->layouts) {
    if (layout->screen) {
      blo_update_defaults_screen(layout->screen, app_template, workspace->id.name + 2);
    }
  }

  if (blo_is_builtin_template(app_template)) {
    /* Clear all tools to use default options instead, ignore the tool saved in the file. */
    while (!BLI_listbase_is_empty(&workspace->tools)) {
      BKE_workspace_tool_remove(workspace, static_cast<bToolRef *>(workspace->tools.first));
    }

    /* For 2D animation template. */
    if (STREQ(workspace->id.name + 2, "Drawing")) {
      workspace->object_mode = OB_MODE_PAINT_GREASE_PENCIL;
    }

    /* For Sculpting template. */
    if (STREQ(workspace->id.name + 2, "Sculpting")) {
      LISTBASE_FOREACH (WorkSpaceLayout *, layout, &workspace->layouts) {
        bScreen *screen = layout->screen;
        if (screen) {
          LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
            LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
              if (area->spacetype == SPACE_VIEW3D) {
                View3D *v3d = static_cast<View3D *>(area->spacedata.first);
                v3d->shading.flag &= ~V3D_SHADING_CAVITY;
                copy_v3_fl(v3d->shading.single_color, 1.0f);
                STRNCPY(v3d->shading.matcap, "basic_1");
              }
            }
          }
        }
      }
    }
  }
  /* For Video Editing template. */
  if (STRPREFIX(workspace->id.name + 2, "Video Editing")) {
    LISTBASE_FOREACH (WorkSpaceLayout *, layout, &workspace->layouts) {
      bScreen *screen = layout->screen;
      if (screen) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_SEQ) {
              if (((SpaceSeq *)sl)->view == SEQ_VIEW_PREVIEW) {
                continue;
              }
              ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                     &sl->regionbase;
              ARegion *sidebar = BKE_region_find_in_listbase_by_type(regionbase, RGN_TYPE_UI);
              sidebar->flag |= RGN_FLAG_HIDDEN;
            }
            if (sl->spacetype == SPACE_PROPERTIES) {
              SpaceProperties *properties = reinterpret_cast<SpaceProperties *>(sl);
              properties->mainb = properties->mainbo = properties->mainbuser = BCONTEXT_STRIP;
            }
          }
        }
      }
    }
  }
}

static void blo_update_defaults_paint(Paint *paint)
{
  if (!paint) {
    return;
  }

  /* Ensure input_samples has a correct default value of 1. */
  if (paint->unified_paint_settings.input_samples == 0) {
    paint->unified_paint_settings.input_samples = 1;
  }

  const UnifiedPaintSettings &default_ups = *DNA_struct_default_get(UnifiedPaintSettings);
  paint->unified_paint_settings.size = default_ups.size;
  paint->unified_paint_settings.input_samples = default_ups.input_samples;
  paint->unified_paint_settings.unprojected_size = default_ups.unprojected_size;
  paint->unified_paint_settings.alpha = default_ups.alpha;
  paint->unified_paint_settings.weight = default_ups.weight;
  paint->unified_paint_settings.flag = default_ups.flag;
  copy_v3_v3(paint->unified_paint_settings.color, default_ups.color);
  copy_v3_v3(paint->unified_paint_settings.secondary_color, default_ups.secondary_color);

  if (paint->unified_paint_settings.curve_rand_hue == nullptr) {
    paint->unified_paint_settings.curve_rand_hue = BKE_paint_default_curve();
  }
  if (paint->unified_paint_settings.curve_rand_saturation == nullptr) {
    paint->unified_paint_settings.curve_rand_saturation = BKE_paint_default_curve();
  }
  if (paint->unified_paint_settings.curve_rand_value == nullptr) {
    paint->unified_paint_settings.curve_rand_value = BKE_paint_default_curve();
  }
}

static void blo_update_defaults_windowmanager(wmWindowManager *wm)
{
  wm->xr.session_settings.fly_speed = 3.0f;
}

static void blo_update_defaults_scene(Main *bmain, Scene *scene)
{
  ToolSettings *ts = scene->toolsettings;

  STRNCPY_UTF8(scene->r.engine, RE_engine_id_BLENDER_EEVEE);

  scene->r.cfra = 1.0f;
  scene->r.im_format.exr_flag |= R_IMF_EXR_FLAG_MULTIPART;
  scene->r.bake.im_format.exr_flag |= R_IMF_EXR_FLAG_MULTIPART;

  /* Don't enable compositing nodes. */
  if (scene->nodetree) {
    blender::bke::node_tree_free_embedded_tree(scene->nodetree);
    MEM_freeN(scene->nodetree);
    scene->nodetree = nullptr;
    scene->use_nodes = false;
  }

  /* Rename render layers. */
  BKE_view_layer_rename(
      bmain, scene, static_cast<ViewLayer *>(scene->view_layers.first), "ViewLayer");

  /* Disable Z pass by default. */
  LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
    view_layer->passflag &= ~SCE_PASS_DEPTH;
    view_layer->eevee.ambient_occlusion_distance = 10.0f;
  }

  if (scene->ed) {
    /* Display missing media by default. */
    scene->ed->show_missing_media_flag |= SEQ_EDIT_SHOW_MISSING_MEDIA;
    /* Turn on frame pre-fetching per default. */
    scene->ed->cache_flag |= SEQ_CACHE_PREFETCH_ENABLE;
  }

  /* New EEVEE defaults. */
  scene->eevee.motion_blur_shutter_deprecated = 0.5f;
  scene->eevee.flag &= ~SCE_EEVEE_VOLUME_CUSTOM_RANGE;

  copy_v3_v3(scene->display.light_direction, blender::float3(M_SQRT1_3));
  copy_v2_fl2(scene->safe_areas.title, 0.1f, 0.05f);
  copy_v2_fl2(scene->safe_areas.action, 0.035f, 0.035f);

  ts->uv_flag |= UV_FLAG_SELECT_SYNC;

  /* Default Rotate Increment. */
  const float default_snap_angle_increment = DEG2RADF(5.0f);
  ts->snap_angle_increment_2d = default_snap_angle_increment;
  ts->snap_angle_increment_3d = default_snap_angle_increment;
  const float default_snap_angle_increment_precision = DEG2RADF(1.0f);
  ts->snap_angle_increment_2d_precision = default_snap_angle_increment_precision;
  ts->snap_angle_increment_3d_precision = default_snap_angle_increment_precision;

  /* Be sure `curfalloff` and primitive are initialized. */
  if (ts->gp_sculpt.cur_falloff == nullptr) {
    ts->gp_sculpt.cur_falloff = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
    CurveMapping *gp_falloff_curve = ts->gp_sculpt.cur_falloff;
    BKE_curvemapping_init(gp_falloff_curve);
    BKE_curvemap_reset(gp_falloff_curve->cm,
                       &gp_falloff_curve->clipr,
                       CURVE_PRESET_GAUSS,
                       CurveMapSlopeType::Positive);
  }
  if (ts->gp_sculpt.cur_primitive == nullptr) {
    ts->gp_sculpt.cur_primitive = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
    CurveMapping *gp_primitive_curve = ts->gp_sculpt.cur_primitive;
    BKE_curvemapping_init(gp_primitive_curve);
    BKE_curvemap_reset(gp_primitive_curve->cm,
                       &gp_primitive_curve->clipr,
                       CURVE_PRESET_BELL,
                       CurveMapSlopeType::Positive);
  }

  if (ts->sculpt) {
    ts->sculpt->flags = DNA_struct_default_get(Sculpt)->flags;
  }

  /* Correct default startup UVs. */
  Mesh *mesh = static_cast<Mesh *>(BLI_findstring(&bmain->meshes, "Cube", offsetof(ID, name) + 2));
  if (mesh && (mesh->corners_num == 24) &&
      CustomData_has_layer(&mesh->corner_data, CD_PROP_FLOAT2))
  {
    const float uv_values[24][2] = {
        {0.625, 0.50}, {0.875, 0.50}, {0.875, 0.75}, {0.625, 0.75}, {0.375, 0.75}, {0.625, 0.75},
        {0.625, 1.00}, {0.375, 1.00}, {0.375, 0.00}, {0.625, 0.00}, {0.625, 0.25}, {0.375, 0.25},
        {0.125, 0.50}, {0.375, 0.50}, {0.375, 0.75}, {0.125, 0.75}, {0.375, 0.50}, {0.625, 0.50},
        {0.625, 0.75}, {0.375, 0.75}, {0.375, 0.25}, {0.625, 0.25}, {0.625, 0.50}, {0.375, 0.50},
    };
    float (*uv_map)[2] = static_cast<float (*)[2]>(
        CustomData_get_layer_for_write(&mesh->corner_data, CD_PROP_FLOAT2, mesh->corners_num));
    memcpy(uv_map, uv_values, sizeof(float[2]) * mesh->corners_num);
  }

  /* Make sure that the curve profile is initialized */
  if (ts->custom_bevel_profile_preset == nullptr) {
    ts->custom_bevel_profile_preset = BKE_curveprofile_add(PROF_PRESET_LINE);
  }

  /* Clear ID properties so Cycles gets defaults. */
  IDProperty *idprop = IDP_GetProperties(&scene->id);
  if (idprop) {
    IDP_ClearProperty(idprop);
  }

  if (ts->sculpt) {
    ts->sculpt->automasking_boundary_edges_propagation_steps = 1;
  }

  /* Ensure input_samples has a correct default value of 1. */
  if (ts->unified_paint_settings.input_samples == 0) {
    ts->unified_paint_settings.input_samples = 1;
  }

  const UnifiedPaintSettings &default_ups = *DNA_struct_default_get(UnifiedPaintSettings);
  ts->unified_paint_settings.flag = default_ups.flag;
  copy_v3_v3(ts->unified_paint_settings.color, default_ups.color);
  copy_v3_v3(ts->unified_paint_settings.secondary_color, default_ups.secondary_color);

  if (ts->unified_paint_settings.curve_rand_hue == nullptr) {
    ts->unified_paint_settings.curve_rand_hue = BKE_paint_default_curve();
  }
  if (ts->unified_paint_settings.curve_rand_saturation == nullptr) {
    ts->unified_paint_settings.curve_rand_saturation = BKE_paint_default_curve();
  }
  if (ts->unified_paint_settings.curve_rand_value == nullptr) {
    ts->unified_paint_settings.curve_rand_value = BKE_paint_default_curve();
  }

  blo_update_defaults_paint(reinterpret_cast<Paint *>(ts->vpaint));
  blo_update_defaults_paint(reinterpret_cast<Paint *>(ts->wpaint));
  blo_update_defaults_paint(reinterpret_cast<Paint *>(ts->sculpt));
  blo_update_defaults_paint(reinterpret_cast<Paint *>(ts->gp_paint));
  blo_update_defaults_paint(reinterpret_cast<Paint *>(ts->gp_vertexpaint));
  blo_update_defaults_paint(reinterpret_cast<Paint *>(ts->gp_sculptpaint));
  blo_update_defaults_paint(reinterpret_cast<Paint *>(ts->curves_sculpt));
  blo_update_defaults_paint(reinterpret_cast<Paint *>(&ts->imapaint));

  /* Weight Paint settings */
  ts->weightuser = OB_DRAW_GROUPUSER_ACTIVE;
}

void BLO_update_defaults_startup_blend(Main *bmain, const char *app_template)
{
  /* For all app templates. */
  LISTBASE_FOREACH (WorkSpace *, workspace, &bmain->workspaces) {
    BLO_update_defaults_workspace(workspace, app_template);
  }

  /* Grease pencil materials and paint modes setup. */
  {
    /* Rename and fix materials and enable default object lights on. */
    if (app_template && STREQ(app_template, "2D_Animation")) {
      Material *ma = nullptr;
      do_versions_rename_id(bmain, ID_MA, "Black", "Solid Stroke");
      do_versions_rename_id(bmain, ID_MA, "Red", "Squares Stroke");
      do_versions_rename_id(bmain, ID_MA, "Grey", "Solid Fill");
      do_versions_rename_id(bmain, ID_MA, "Black Dots", "Dots Stroke");

      /* Dots Stroke. */
      ma = static_cast<Material *>(
          BLI_findstring(&bmain->materials, "Dots Stroke", offsetof(ID, name) + 2));
      if (ma == nullptr) {
        ma = BKE_gpencil_material_add(bmain, "Dots Stroke");
      }
      ma->gp_style->mode = GP_MATERIAL_MODE_DOT;

      /* Squares Stroke. */
      ma = static_cast<Material *>(
          BLI_findstring(&bmain->materials, "Squares Stroke", offsetof(ID, name) + 2));
      if (ma == nullptr) {
        ma = BKE_gpencil_material_add(bmain, "Squares Stroke");
      }
      ma->gp_style->mode = GP_MATERIAL_MODE_SQUARE;

      /* Change Solid Stroke settings. */
      ma = static_cast<Material *>(
          BLI_findstring(&bmain->materials, "Solid Stroke", offsetof(ID, name) + 2));
      if (ma != nullptr) {
        ma->gp_style->mix_rgba[3] = 1.0f;
        ma->gp_style->texture_offset[0] = -0.5f;
        ma->gp_style->mix_factor = 0.5f;
      }

      /* Change Solid Fill settings. */
      ma = static_cast<Material *>(
          BLI_findstring(&bmain->materials, "Solid Fill", offsetof(ID, name) + 2));
      if (ma != nullptr) {
        ma->gp_style->flag &= ~GP_MATERIAL_STROKE_SHOW;
        ma->gp_style->mix_rgba[3] = 1.0f;
        ma->gp_style->texture_offset[0] = -0.5f;
        ma->gp_style->mix_factor = 0.5f;
      }

      Object *ob = static_cast<Object *>(
          BLI_findstring(&bmain->objects, "Stroke", offsetof(ID, name) + 2));
      if (ob && ob->type == OB_GPENCIL_LEGACY) {
        ob->dtx |= OB_USE_GPENCIL_LIGHTS;
      }
    }

    /* Add library weak references to avoid duplicating materials from essentials. */
    const std::optional<std::string> assets_path = BKE_appdir_folder_id(BLENDER_SYSTEM_DATAFILES,
                                                                        "assets/brushes");
    if (assets_path.has_value()) {
      const std::string assets_blend_path = *assets_path + "/essentials_brushes-gp_draw.blend";
      LISTBASE_FOREACH (Material *, material, &bmain->materials) {
        BKE_main_library_weak_reference_add(
            &material->id, assets_blend_path.c_str(), material->id.name);
      }
    }

    /* Reset grease pencil paint modes. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *ts = scene->toolsettings;

      /* Ensure new Paint modes. */
      BKE_paint_ensure_from_paintmode(scene, PaintMode::VertexGPencil);
      BKE_paint_ensure_from_paintmode(scene, PaintMode::SculptGPencil);
      BKE_paint_ensure_from_paintmode(scene, PaintMode::WeightGPencil);

      /* Enable cursor. */
      if (ts->gp_paint) {
        ts->gp_paint->paint.flags |= PAINT_SHOW_BRUSH;
      }

      /* Ensure Palette by default. */
      if (ts->gp_paint) {
        BKE_gpencil_palette_ensure(bmain, scene);
      }
    }
  }

  /* For builtin templates only. */
  if (!blo_is_builtin_template(app_template)) {
    return;
  }

  /* Work-spaces. */
  LISTBASE_FOREACH (wmWindowManager *, wm, &bmain->wm) {
    blo_update_defaults_windowmanager(wm);

    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      LISTBASE_FOREACH (WorkSpace *, workspace, &bmain->workspaces) {
        WorkSpaceLayout *layout = BKE_workspace_active_layout_for_workspace_get(
            win->workspace_hook, workspace);
        /* Name all screens by their workspaces (avoids 'Default.###' names). */
        /* Default only has one window. */
        if (layout->screen) {
          bScreen *screen = layout->screen;
          if (!STREQ(screen->id.name + 2, workspace->id.name + 2)) {
            BKE_libblock_rename(*bmain, screen->id, workspace->id.name + 2);
          }
        }

        /* For some reason we have unused screens, needed until re-saving.
         * Clear unused layouts because they're visible in the outliner & Python API. */
        LISTBASE_FOREACH_MUTABLE (WorkSpaceLayout *, layout_iter, &workspace->layouts) {
          if (layout != layout_iter) {
            BKE_workspace_layout_remove(bmain, workspace, layout_iter);
          }
        }
      }
    }
  }

  /* Scenes */
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    blo_update_defaults_scene(bmain, scene);

    if (app_template && STR_ELEM(app_template, "Video_Editing", "2D_Animation")) {
      /* Filmic is too slow, use standard until it is optimized. */
      STRNCPY_UTF8(scene->view_settings.view_transform, "Standard");
      STRNCPY_UTF8(scene->view_settings.look, "None");
    }
    else {
      /* Default to AgX view transform. */
      STRNCPY_UTF8(scene->view_settings.view_transform, "AgX");
    }

    if (app_template && STREQ(app_template, "Video_Editing")) {
      /* Pass: no extra tweaks needed. Keep the view settings configured above, and rely on the
       * default state of enabled AV sync. */
    }
    else {
      /* AV Sync break physics sim caching, disable until that is fixed. */
      scene->audio.flag &= ~AUDIO_SYNC;
      scene->flag &= ~SCE_FRAME_DROP;
    }

    /* Change default selection mode for Grease Pencil. */
    if (app_template && STREQ(app_template, "2D_Animation")) {
      ToolSettings *ts = scene->toolsettings;
      ts->gpencil_selectmode_edit = GP_SELECTMODE_STROKE;
    }
  }

  /* Objects */
  do_versions_rename_id(bmain, ID_OB, "Lamp", "Light");
  do_versions_rename_id(bmain, ID_LA, "Lamp", "Light");

  if (app_template && STREQ(app_template, "2D_Animation")) {
    LISTBASE_FOREACH (Object *, object, &bmain->objects) {
      if (object->type == OB_GPENCIL_LEGACY) {
        /* Set grease pencil object in drawing mode */
        bGPdata *gpd = (bGPdata *)object->data;
        object->mode = OB_MODE_PAINT_GREASE_PENCIL;
        gpd->flag |= GP_DATA_STROKE_PAINTMODE;
        break;
      }
    }
  }

  LISTBASE_FOREACH (Object *, object, &bmain->objects) {
    const Object *dob = DNA_struct_default_get(Object);
    /* Set default for shadow terminator bias. */
    object->shadow_terminator_normal_offset = dob->shadow_terminator_normal_offset;
    object->shadow_terminator_geometry_offset = dob->shadow_terminator_geometry_offset;
    object->shadow_terminator_shading_offset = dob->shadow_terminator_shading_offset;
  }

  LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
    /* Match default for new meshes. */
    mesh->smoothresh_legacy = DEG2RADF(30);
    /* Match voxel remesher options for all existing meshes in templates. */
    mesh->flag |= ME_REMESH_REPROJECT_VOLUME | ME_REMESH_REPROJECT_ATTRIBUTES;

    /* For Sculpting template. */
    if (app_template && STREQ(app_template, "Sculpting")) {
      mesh->remesh_voxel_size = 0.035f;
      blender::bke::mesh_smooth_set(*mesh, false);
    }
    else {
      /* Remove sculpt-mask data in default mesh objects for all non-sculpt templates. */
      CustomData_free_layers(&mesh->vert_data, CD_PAINT_MASK);
      CustomData_free_layers(&mesh->corner_data, CD_GRID_PAINT_MASK);
    }
    mesh->attributes_for_write().remove(".sculpt_face_set");
  }

  LISTBASE_FOREACH (Camera *, camera, &bmain->cameras) {
    /* Initialize to a useful value. */
    camera->dof.focus_distance = 10.0f;
    camera->dof.aperture_fstop = 2.8f;
  }

  LISTBASE_FOREACH (Light *, light, &bmain->lights) {
    /* Fix lights defaults. */
    light->clipsta = 0.05f;
    light->att_dist = 40.0f;
  }

  /* Materials */
  LISTBASE_FOREACH (Material *, ma, &bmain->materials) {
    /* Update default material to be a bit more rough. */
    ma->roughness = 0.5f;
    /* Enable transparent shadows. */
    ma->blend_flag |= MA_BL_TRANSPARENT_SHADOW;

    if (ma->nodetree) {
      for (bNode *node : ma->nodetree->all_nodes()) {
        if (node->type_legacy == SH_NODE_BSDF_PRINCIPLED) {
          bNodeSocket *roughness_socket = blender::bke::node_find_socket(
              *node, SOCK_IN, "Roughness");
          *version_cycles_node_socket_float_value(roughness_socket) = 0.5f;
          bNodeSocket *emission = blender::bke::node_find_socket(*node, SOCK_IN, "Emission Color");
          copy_v4_fl(version_cycles_node_socket_rgba_value(emission), 1.0f);
          bNodeSocket *emission_strength = blender::bke::node_find_socket(
              *node, SOCK_IN, "Emission Strength");
          *version_cycles_node_socket_float_value(emission_strength) = 0.0f;
          bNodeSocket *ior = blender::bke::node_find_socket(*node, SOCK_IN, "IOR");
          *version_cycles_node_socket_float_value(ior) = 1.5f;

          node->custom1 = SHD_GLOSSY_MULTI_GGX;
          node->custom2 = SHD_SUBSURFACE_RANDOM_WALK;

          node->location[0] = -200.0f;
          node->location[1] = 100.0f;
          BKE_ntree_update_tag_node_property(ma->nodetree, node);
        }
        else if (node->type_legacy == SH_NODE_SUBSURFACE_SCATTERING) {
          node->custom1 = SHD_SUBSURFACE_RANDOM_WALK;
          BKE_ntree_update_tag_node_property(ma->nodetree, node);
        }
        else if (node->type_legacy == SH_NODE_OUTPUT_MATERIAL) {
          node->location[0] = 200.0f;
          node->location[1] = 100.0f;
        }
      }
    }
  }

  /* Brushes */
  {
    /* Remove default brushes replaced by assets. Also remove outliner `treestore` that may point
     * to brushes. Normally the treestore is updated properly but it doesn't seem to update during
     * versioning code. It's not helpful anyway. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, space_link, &area->spacedata) {
          if (space_link->spacetype == SPACE_OUTLINER) {
            SpaceOutliner *space_outliner = reinterpret_cast<SpaceOutliner *>(space_link);
            if (space_outliner->treestore) {
              BLI_mempool_destroy(space_outliner->treestore);
              space_outliner->treestore = nullptr;
            }
          }
        }
      }
    }
    LISTBASE_FOREACH_MUTABLE (Brush *, brush, &bmain->brushes) {
      BKE_id_delete(bmain, brush);
    }
  }

  {
    LISTBASE_FOREACH (Light *, light, &bmain->lights) {
      light->shadow_maximum_resolution = 0.001f;
      light->transmission_fac = 1.0f;
      SET_FLAG_FROM_TEST(light->mode, false, LA_SHAD_RES_ABSOLUTE);
    }
  }

  {
    LISTBASE_FOREACH (World *, world, &bmain->worlds) {
      SET_FLAG_FROM_TEST(world->flag, true, WO_USE_SUN_SHADOW);
      if (world->nodetree) {
        for (bNode *node : world->nodetree->all_nodes()) {
          if (node->type_legacy == SH_NODE_OUTPUT_WORLD) {
            node->location[0] = 200.0f;
            node->location[1] = 100.0f;
          }
          else if (node->type_legacy == SH_NODE_BACKGROUND) {
            node->location[0] = -200.0f;
            node->location[1] = 100.0f;
          }
        }
      }
    }
  }

  /* Grease Pencil Anti-Aliasing. */
  {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->grease_pencil_settings.smaa_threshold = 1.0f;
      scene->grease_pencil_settings.smaa_threshold_render = 0.25f;
      scene->grease_pencil_settings.aa_samples = 8;
      scene->grease_pencil_settings.motion_blur_steps = 8;
    }
  }
}
