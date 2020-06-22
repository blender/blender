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
 */

/** \file
 * \ingroup blenloader
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_system.h"
#include "BLI_utildefines.h"

#include "DNA_camera_types.h"
#include "DNA_curveprofile_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_light_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "BKE_appdir.h"
#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_curveprofile.h"
#include "BKE_gpencil.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_paint.h"
#include "BKE_screen.h"
#include "BKE_workspace.h"

#include "BLO_readfile.h"

/* Make preferences read-only, use versioning_userdef.c. */
#define U (*((const UserDef *)&U))

/**
 * Rename if the ID doesn't exist.
 */
static ID *rename_id_for_versioning(Main *bmain,
                                    const short id_type,
                                    const char *name_src,
                                    const char *name_dst)
{
  /* We can ignore libraries */
  ListBase *lb = which_libbase(bmain, id_type);
  ID *id = NULL;
  LISTBASE_FOREACH (ID *, idtest, lb) {
    if (idtest->lib == NULL) {
      if (STREQ(idtest->name + 2, name_src)) {
        id = idtest;
      }
      if (STREQ(idtest->name + 2, name_dst)) {
        return NULL;
      }
    }
  }
  if (id != NULL) {
    BLI_strncpy(id->name + 2, name_dst, sizeof(id->name) - 2);
    /* We know it's unique, this just sorts. */
    BLI_libblock_ensure_unique_name(bmain, id->name);
  }
  return id;
}

static bool blo_is_builtin_template(const char *app_template)
{
  /* For all builtin templates shipped with Blender. */
  return (!app_template ||
          STR_ELEM(app_template, "2D_Animation", "Sculpting", "VFX", "Video_Editing"));
}

static void blo_update_defaults_screen(bScreen *screen,
                                       const char *app_template,
                                       const char *workspace_name)
{
  /* For all app templates. */
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      /* Some toolbars have been saved as initialized,
       * we don't want them to have odd zoom-level or scrolling set, see: T47047 */
      if (ELEM(region->regiontype, RGN_TYPE_UI, RGN_TYPE_TOOLS, RGN_TYPE_TOOL_PROPS)) {
        region->v2d.flag &= ~V2D_IS_INITIALISED;
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
        SpaceImage *sima = area->spacedata.first;
        if (sima->mode == SI_MODE_VIEW) {
          sima->mode = SI_MODE_UV;
        }
      }
    }
    else if (area->spacetype == SPACE_ACTION) {
      /* Show markers region, hide channels and collapse summary in timelines. */
      SpaceAction *saction = area->spacedata.first;
      saction->flag |= SACTION_SHOW_MARKERS;
      if (saction->mode == SACTCONT_TIMELINE) {
        saction->ads.flag |= ADS_FLAG_SUMMARY_COLLAPSED;

        LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
          if (region->regiontype == RGN_TYPE_CHANNELS) {
            region->flag |= RGN_FLAG_HIDDEN;
          }
        }
      }
    }
    else if (area->spacetype == SPACE_GRAPH) {
      SpaceGraph *sipo = area->spacedata.first;
      sipo->flag |= SIPO_SHOW_MARKERS;
    }
    else if (area->spacetype == SPACE_NLA) {
      SpaceNla *snla = area->spacedata.first;
      snla->flag |= SNLA_SHOW_MARKERS;
    }
    else if (area->spacetype == SPACE_SEQ) {
      SpaceSeq *seq = area->spacedata.first;
      seq->flag |= SEQ_SHOW_MARKERS | SEQ_SHOW_FCURVES;
    }
    else if (area->spacetype == SPACE_TEXT) {
      /* Show syntax and line numbers in Script workspace text editor. */
      SpaceText *stext = area->spacedata.first;
      stext->showsyntax = true;
      stext->showlinenrs = true;
    }
    else if (area->spacetype == SPACE_VIEW3D) {
      View3D *v3d = area->spacedata.first;
      /* Screen space cavity by default for faster performance. */
      v3d->shading.cavity_type = V3D_SHADING_CAVITY_CURVATURE;
      v3d->shading.flag |= V3D_SHADING_SPECULAR_HIGHLIGHT;
      v3d->overlay.texture_paint_mode_opacity = 1.0f;
      v3d->overlay.weight_paint_mode_opacity = 1.0f;
      v3d->overlay.vertex_paint_mode_opacity = 1.0f;
      /* Use dimmed selected edges. */
      v3d->overlay.edit_flag &= ~V3D_OVERLAY_EDIT_EDGES;
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
    }
    else if (area->spacetype == SPACE_CLIP) {
      SpaceClip *sclip = area->spacedata.first;
      sclip->around = V3D_AROUND_CENTER_MEDIAN;
    }
  }

  /* Show tool-header by default (for most cases at least, hide for others). */
  const bool hide_image_tool_header = STREQ(workspace_name, "Rendering");
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
      ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase : &sl->regionbase;

      LISTBASE_FOREACH (ARegion *, region, regionbase) {
        if (region->regiontype == RGN_TYPE_TOOL_HEADER) {
          if ((sl->spacetype == SPACE_IMAGE) && hide_image_tool_header) {
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
        SpaceAction *saction = area->spacedata.first;
        /* Enable Sliders. */
        saction->flag |= SACTION_SLIDERS;
      }
      else if (area->spacetype == SPACE_VIEW3D) {
        View3D *v3d = area->spacedata.first;
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
      BKE_workspace_tool_remove(workspace, workspace->tools.first);
    }

    /* For 2D animation template. */
    if (STREQ(workspace->id.name + 2, "Drawing")) {
      workspace->object_mode = OB_MODE_PAINT_GPENCIL;
    }

    /* For Sculpting template. */
    if (STREQ(workspace->id.name + 2, "Sculpting")) {
      LISTBASE_FOREACH (WorkSpaceLayout *, layout, &workspace->layouts) {
        bScreen *screen = layout->screen;
        if (screen) {
          LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
            LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
              if (area->spacetype == SPACE_VIEW3D) {
                View3D *v3d = area->spacedata.first;
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
}

static void blo_update_defaults_scene(Main *bmain, Scene *scene)
{
  BLI_strncpy(scene->r.engine, RE_engine_id_BLENDER_EEVEE, sizeof(scene->r.engine));

  scene->r.cfra = 1.0f;

  /* Don't enable compositing nodes. */
  if (scene->nodetree) {
    ntreeFreeEmbeddedTree(scene->nodetree);
    MEM_freeN(scene->nodetree);
    scene->nodetree = NULL;
    scene->use_nodes = false;
  }

  /* Rename render layers. */
  BKE_view_layer_rename(bmain, scene, scene->view_layers.first, "View Layer");

  /* New EEVEE defaults. */
  scene->eevee.bloom_intensity = 0.05f;
  scene->eevee.bloom_clamp = 0.0f;
  scene->eevee.motion_blur_shutter = 0.5f;

  copy_v3_v3(scene->display.light_direction, (float[3]){M_SQRT1_3, M_SQRT1_3, M_SQRT1_3});
  copy_v2_fl2(scene->safe_areas.title, 0.1f, 0.05f);
  copy_v2_fl2(scene->safe_areas.action, 0.035f, 0.035f);

  /* Change default cubemap quality. */
  scene->eevee.gi_filter_quality = 3.0f;

  /* Enable Soft Shadows by default. */
  scene->eevee.flag |= SCE_EEVEE_SHADOW_SOFT;

  /* Be sure curfalloff and primitive are initializated */
  ToolSettings *ts = scene->toolsettings;
  if (ts->gp_sculpt.cur_falloff == NULL) {
    ts->gp_sculpt.cur_falloff = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
    CurveMapping *gp_falloff_curve = ts->gp_sculpt.cur_falloff;
    BKE_curvemapping_initialize(gp_falloff_curve);
    BKE_curvemap_reset(gp_falloff_curve->cm,
                       &gp_falloff_curve->clipr,
                       CURVE_PRESET_GAUSS,
                       CURVEMAP_SLOPE_POSITIVE);
  }
  if (ts->gp_sculpt.cur_primitive == NULL) {
    ts->gp_sculpt.cur_primitive = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
    CurveMapping *gp_primitive_curve = ts->gp_sculpt.cur_primitive;
    BKE_curvemapping_initialize(gp_primitive_curve);
    BKE_curvemap_reset(gp_primitive_curve->cm,
                       &gp_primitive_curve->clipr,
                       CURVE_PRESET_BELL,
                       CURVEMAP_SLOPE_POSITIVE);
  }

  if (ts->sculpt) {
    ts->sculpt->paint.symmetry_flags |= PAINT_SYMMETRY_FEATHER;
  }

  /* Correct default startup UV's. */
  Mesh *me = BLI_findstring(&bmain->meshes, "Cube", offsetof(ID, name) + 2);
  if (me && (me->totloop == 24) && (me->mloopuv != NULL)) {
    const float uv_values[24][2] = {
        {0.625, 0.50}, {0.875, 0.50}, {0.875, 0.75}, {0.625, 0.75}, {0.375, 0.75}, {0.625, 0.75},
        {0.625, 1.00}, {0.375, 1.00}, {0.375, 0.00}, {0.625, 0.00}, {0.625, 0.25}, {0.375, 0.25},
        {0.125, 0.50}, {0.375, 0.50}, {0.375, 0.75}, {0.125, 0.75}, {0.375, 0.50}, {0.625, 0.50},
        {0.625, 0.75}, {0.375, 0.75}, {0.375, 0.25}, {0.625, 0.25}, {0.625, 0.50}, {0.375, 0.50},
    };
    for (int i = 0; i < ARRAY_SIZE(uv_values); i++) {
      copy_v2_v2(me->mloopuv[i].uv, uv_values[i]);
    }
  }

  /* Make sure that the curve profile is initialized */
  if (ts->custom_bevel_profile_preset == NULL) {
    ts->custom_bevel_profile_preset = BKE_curveprofile_add(PROF_PRESET_LINE);
  }
}

/**
 * Update defaults in startup.blend, without having to save and embed the file.
 * This function can be emptied each time the startup.blend is updated.
 *
 * \note Screen data may be cleared at this point, this will happen in the case
 * an app-template's data needs to be versioned when read-file is called with "Load UI" disabled.
 * Versioning the screen data can be safely skipped without "Load UI" since the screen data
 * will have been versioned when it was first loaded.
 */
void BLO_update_defaults_startup_blend(Main *bmain, const char *app_template)
{
  /* For all app templates. */
  for (WorkSpace *workspace = bmain->workspaces.first; workspace; workspace = workspace->id.next) {
    BLO_update_defaults_workspace(workspace, app_template);
  }

  /* New grease pencil brushes and vertex paint setup. */
  {
    /* Update Grease Pencil brushes. */
    Brush *brush;

    /* Pencil brush. */
    rename_id_for_versioning(bmain, ID_BR, "Draw Pencil", "Pencil");

    /* Pen brush. */
    rename_id_for_versioning(bmain, ID_BR, "Draw Pen", "Pen");

    /* Pen Soft brush. */
    brush = (Brush *)rename_id_for_versioning(bmain, ID_BR, "Draw Soft", "Pencil Soft");
    if (brush) {
      brush->gpencil_settings->icon_id = GP_BRUSH_ICON_PEN;
    }

    /* Ink Pen brush. */
    rename_id_for_versioning(bmain, ID_BR, "Draw Ink", "Ink Pen");

    /* Ink Pen Rough brush. */
    rename_id_for_versioning(bmain, ID_BR, "Draw Noise", "Ink Pen Rough");

    /* Marker Bold brush. */
    rename_id_for_versioning(bmain, ID_BR, "Draw Marker", "Marker Bold");

    /* Marker Chisel brush. */
    rename_id_for_versioning(bmain, ID_BR, "Draw Block", "Marker Chisel");

    /* Remove useless Fill Area.001 brush. */
    brush = BLI_findstring(&bmain->brushes, "Fill Area.001", offsetof(ID, name) + 2);
    if (brush) {
      BKE_id_delete(bmain, brush);
    }

    /* Rename and fix materials and enable default object lights on. */
    if (app_template && STREQ(app_template, "2D_Animation")) {
      Material *ma = NULL;
      rename_id_for_versioning(bmain, ID_MA, "Black", "Solid Stroke");
      rename_id_for_versioning(bmain, ID_MA, "Red", "Squares Stroke");
      rename_id_for_versioning(bmain, ID_MA, "Grey", "Solid Fill");
      rename_id_for_versioning(bmain, ID_MA, "Black Dots", "Dots Stroke");

      /* Dots Stroke. */
      ma = BLI_findstring(&bmain->materials, "Dots Stroke", offsetof(ID, name) + 2);
      if (ma == NULL) {
        ma = BKE_gpencil_material_add(bmain, "Dots Stroke");
      }
      ma->gp_style->mode = GP_MATERIAL_MODE_DOT;

      /* Squares Stroke. */
      ma = BLI_findstring(&bmain->materials, "Squares Stroke", offsetof(ID, name) + 2);
      if (ma == NULL) {
        ma = BKE_gpencil_material_add(bmain, "Squares Stroke");
      }
      ma->gp_style->mode = GP_MATERIAL_MODE_SQUARE;

      /* Change Solid Stroke settings. */
      ma = BLI_findstring(&bmain->materials, "Solid Stroke", offsetof(ID, name) + 2);
      if (ma != NULL) {
        ma->gp_style->mix_rgba[3] = 1.0f;
        ma->gp_style->texture_offset[0] = -0.5f;
        ma->gp_style->mix_factor = 0.5f;
      }

      /* Change Solid Fill settings. */
      ma = BLI_findstring(&bmain->materials, "Solid Fill", offsetof(ID, name) + 2);
      if (ma != NULL) {
        ma->gp_style->flag &= ~GP_MATERIAL_STROKE_SHOW;
        ma->gp_style->mix_rgba[3] = 1.0f;
        ma->gp_style->texture_offset[0] = -0.5f;
        ma->gp_style->mix_factor = 0.5f;
      }

      Object *ob = BLI_findstring(&bmain->objects, "Stroke", offsetof(ID, name) + 2);
      if (ob && ob->type == OB_GPENCIL) {
        ob->dtx |= OB_USE_GPENCIL_LIGHTS;
      }
    }

    /* Reset all grease pencil brushes. */
    Scene *scene = bmain->scenes.first;
    BKE_brush_gpencil_paint_presets(bmain, scene->toolsettings, true);
    BKE_brush_gpencil_sculpt_presets(bmain, scene->toolsettings, true);
    BKE_brush_gpencil_vertex_presets(bmain, scene->toolsettings, true);
    BKE_brush_gpencil_weight_presets(bmain, scene->toolsettings, true);

    /* Ensure new Paint modes. */
    BKE_paint_ensure_from_paintmode(scene, PAINT_MODE_VERTEX_GPENCIL);
    BKE_paint_ensure_from_paintmode(scene, PAINT_MODE_SCULPT_GPENCIL);
    BKE_paint_ensure_from_paintmode(scene, PAINT_MODE_WEIGHT_GPENCIL);

    /* Enable cursor. */
    GpPaint *gp_paint = scene->toolsettings->gp_paint;
    gp_paint->paint.flags |= PAINT_SHOW_BRUSH;

    /* Ensure Palette by default. */
    BKE_gpencil_palette_ensure(bmain, scene);
  }

  /* For builtin templates only. */
  if (!blo_is_builtin_template(app_template)) {
    return;
  }

  /* Workspaces. */
  LISTBASE_FOREACH (wmWindowManager *, wm, &bmain->wm) {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      LISTBASE_FOREACH (WorkSpace *, workspace, &bmain->workspaces) {
        WorkSpaceLayout *layout = BKE_workspace_active_layout_for_workspace_get(
            win->workspace_hook, workspace);
        /* Name all screens by their workspaces (avoids 'Default.###' names). */
        /* Default only has one window. */
        if (layout->screen) {
          bScreen *screen = layout->screen;
          BLI_strncpy(screen->id.name + 2, workspace->id.name + 2, sizeof(screen->id.name) - 2);
          BLI_libblock_ensure_unique_name(bmain, screen->id.name);
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
  for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
    blo_update_defaults_scene(bmain, scene);

    if (app_template && STREQ(app_template, "Video_Editing")) {
      /* Filmic is too slow, use standard until it is optimized. */
      STRNCPY(scene->view_settings.view_transform, "Standard");
      STRNCPY(scene->view_settings.look, "None");
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
  rename_id_for_versioning(bmain, ID_OB, "Lamp", "Light");
  rename_id_for_versioning(bmain, ID_LA, "Lamp", "Light");

  if (app_template && STREQ(app_template, "2D_Animation")) {
    for (Object *object = bmain->objects.first; object; object = object->id.next) {
      if (object->type == OB_GPENCIL) {
        /* Set grease pencil object in drawing mode */
        bGPdata *gpd = (bGPdata *)object->data;
        object->mode = OB_MODE_PAINT_GPENCIL;
        gpd->flag |= GP_DATA_STROKE_PAINTMODE;
        break;
      }
    }
  }

  for (Mesh *mesh = bmain->meshes.first; mesh; mesh = mesh->id.next) {
    /* Match default for new meshes. */
    mesh->smoothresh = DEG2RADF(30);

    /* For Sculpting template. */
    if (app_template && STREQ(app_template, "Sculpting")) {
      mesh->remesh_voxel_size = 0.035f;
      mesh->flag |= ME_REMESH_FIX_POLES | ME_REMESH_REPROJECT_VOLUME;
      BKE_mesh_smooth_flag_set(mesh, false);
    }
  }

  for (Camera *camera = bmain->cameras.first; camera; camera = camera->id.next) {
    /* Initialize to a useful value. */
    camera->dof.focus_distance = 10.0f;
    camera->dof.aperture_fstop = 2.8f;
  }

  for (Light *light = bmain->lights.first; light; light = light->id.next) {
    /* Fix lights defaults. */
    light->clipsta = 0.05f;
    light->att_dist = 40.0f;
  }

  /* Materials */
  for (Material *ma = bmain->materials.first; ma; ma = ma->id.next) {
    /* Update default material to be a bit more rough. */
    ma->roughness = 0.4f;

    if (ma->nodetree) {
      LISTBASE_FOREACH (bNode *, node, &ma->nodetree->nodes) {
        if (node->type == SH_NODE_BSDF_PRINCIPLED) {
          bNodeSocket *roughness_socket = nodeFindSocket(node, SOCK_IN, "Roughness");
          bNodeSocketValueFloat *roughness_data = roughness_socket->default_value;
          roughness_data->value = 0.4f;
        }
      }
    }
  }

  /* Brushes */
  {
    /* Enable for UV sculpt (other brush types will be created as needed),
     * without this the grab brush will be active but not selectable from the list. */
    const char *brush_name = "Grab";
    Brush *brush = BLI_findstring(&bmain->brushes, brush_name, offsetof(ID, name) + 2);
    if (brush) {
      brush->ob_mode |= OB_MODE_EDIT;
    }
  }

  for (Brush *brush = bmain->brushes.first; brush; brush = brush->id.next) {
    brush->blur_kernel_radius = 2;

    /* Use full strength for all non-sculpt brushes,
     * when painting we want to use full color/weight always.
     *
     * Note that sculpt is an exception,
     * it's values are overwritten by #BKE_brush_sculpt_reset below. */
    brush->alpha = 1.0;

    /* Enable antialiasing by default */
    brush->sampling_flag |= BRUSH_PAINT_ANTIALIASING;
  }

  {
    /* Change the spacing of the Smear brush to 3.0% */
    const char *brush_name;
    Brush *brush;

    brush_name = "Smear";
    brush = BLI_findstring(&bmain->brushes, brush_name, offsetof(ID, name) + 2);
    if (brush) {
      brush->spacing = 3.0;
    }

    brush_name = "Draw Sharp";
    brush = BLI_findstring(&bmain->brushes, brush_name, offsetof(ID, name) + 2);
    if (!brush) {
      brush = BKE_brush_add(bmain, brush_name, OB_MODE_SCULPT);
      id_us_min(&brush->id);
      brush->sculpt_tool = SCULPT_TOOL_DRAW_SHARP;
    }

    brush_name = "Elastic Deform";
    brush = BLI_findstring(&bmain->brushes, brush_name, offsetof(ID, name) + 2);
    if (!brush) {
      brush = BKE_brush_add(bmain, brush_name, OB_MODE_SCULPT);
      id_us_min(&brush->id);
      brush->sculpt_tool = SCULPT_TOOL_ELASTIC_DEFORM;
    }

    brush_name = "Pose";
    brush = BLI_findstring(&bmain->brushes, brush_name, offsetof(ID, name) + 2);
    if (!brush) {
      brush = BKE_brush_add(bmain, brush_name, OB_MODE_SCULPT);
      id_us_min(&brush->id);
      brush->sculpt_tool = SCULPT_TOOL_POSE;
    }

    brush_name = "Multi-plane Scrape";
    brush = BLI_findstring(&bmain->brushes, brush_name, offsetof(ID, name) + 2);
    if (!brush) {
      brush = BKE_brush_add(bmain, brush_name, OB_MODE_SCULPT);
      id_us_min(&brush->id);
      brush->sculpt_tool = SCULPT_TOOL_MULTIPLANE_SCRAPE;
    }

    brush_name = "Clay Thumb";
    brush = BLI_findstring(&bmain->brushes, brush_name, offsetof(ID, name) + 2);
    if (!brush) {
      brush = BKE_brush_add(bmain, brush_name, OB_MODE_SCULPT);
      id_us_min(&brush->id);
      brush->sculpt_tool = SCULPT_TOOL_CLAY_THUMB;
    }

    brush_name = "Cloth";
    brush = BLI_findstring(&bmain->brushes, brush_name, offsetof(ID, name) + 2);
    if (!brush) {
      brush = BKE_brush_add(bmain, brush_name, OB_MODE_SCULPT);
      id_us_min(&brush->id);
      brush->sculpt_tool = SCULPT_TOOL_CLOTH;
    }

    brush_name = "Slide Relax";
    brush = BLI_findstring(&bmain->brushes, brush_name, offsetof(ID, name) + 2);
    if (!brush) {
      brush = BKE_brush_add(bmain, brush_name, OB_MODE_SCULPT);
      id_us_min(&brush->id);
      brush->sculpt_tool = SCULPT_TOOL_SLIDE_RELAX;
    }

    brush_name = "Paint";
    brush = BLI_findstring(&bmain->brushes, brush_name, offsetof(ID, name) + 2);
    if (!brush) {
      brush = BKE_brush_add(bmain, brush_name, OB_MODE_SCULPT);
      id_us_min(&brush->id);
      brush->sculpt_tool = SCULPT_TOOL_PAINT;
    }

    brush_name = "Smear";
    brush = BLI_findstring(&bmain->brushes, brush_name, offsetof(ID, name) + 2);
    if (!brush) {
      brush = BKE_brush_add(bmain, brush_name, OB_MODE_SCULPT);
      id_us_min(&brush->id);
      brush->sculpt_tool = SCULPT_TOOL_SMEAR;
    }

    brush_name = "Simplify";
    brush = BLI_findstring(&bmain->brushes, brush_name, offsetof(ID, name) + 2);
    if (!brush) {
      brush = BKE_brush_add(bmain, brush_name, OB_MODE_SCULPT);
      id_us_min(&brush->id);
      brush->sculpt_tool = SCULPT_TOOL_SIMPLIFY;
    }

    brush_name = "Draw Face Sets";
    brush = BLI_findstring(&bmain->brushes, brush_name, offsetof(ID, name) + 2);
    if (!brush) {
      brush = BKE_brush_add(bmain, brush_name, OB_MODE_SCULPT);
      id_us_min(&brush->id);
      brush->sculpt_tool = SCULPT_TOOL_DRAW_FACE_SETS;
    }

    /* Use the same tool icon color in the brush cursor */
    for (brush = bmain->brushes.first; brush; brush = brush->id.next) {
      if (brush->ob_mode & OB_MODE_SCULPT) {
        BLI_assert(brush->sculpt_tool != 0);
        BKE_brush_sculpt_reset(brush);
      }
    }
  }
}
