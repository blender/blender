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

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_system.h"

#include "DNA_gpencil_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "BKE_appdir.h"
#include "BKE_brush.h"
#include "BKE_colorband.h"
#include "BKE_colortools.h"
#include "BKE_idprop.h"
#include "BKE_keyconfig.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_paint.h"
#include "BKE_screen.h"
#include "BKE_workspace.h"

#include "BLO_readfile.h"

/**
 * Override values in in-memory startup.blend, avoids resaving for small changes.
 */
void BLO_update_defaults_userpref_blend(void)
{
  /* default so DPI is detected automatically */
  U.dpi = 0;
  U.ui_scale = 1.0f;

#ifdef WITH_PYTHON_SECURITY
  /* use alternative setting for security nuts
   * otherwise we'd need to patch the binary blob - startup.blend.c */
  U.flag |= USER_SCRIPT_AUTOEXEC_DISABLE;
#else
  U.flag &= ~USER_SCRIPT_AUTOEXEC_DISABLE;
#endif

  /* Transform tweak with single click and drag. */
  U.flag |= USER_RELEASECONFIRM;

  U.flag &= ~(USER_DEVELOPER_UI | USER_TOOLTIPS_PYTHON);

  /* Clear addon preferences. */
  for (bAddon *addon = U.addons.first, *addon_next; addon != NULL; addon = addon_next) {
    addon_next = addon->next;

    if (addon->prop) {
      IDP_FreeProperty(addon->prop);
      addon->prop = NULL;
    }
  }

  /* Ignore the theme saved in the blend file,
   * instead use the theme from 'userdef_default_theme.c' */
  {
    bTheme *theme = U.themes.first;
    memcpy(theme, &U_theme_default, sizeof(bTheme));
  }

  /* Leave temp directory empty, will then get appropriate value per OS. */
  U.tempdir[0] = '\0';

  /* System-specific fonts directory. */
  BKE_appdir_font_folder_default(U.fontdir);

  /* Only enable tooltips translation by default,
   * without actually enabling translation itself, for now. */
  U.transopts = USER_TR_TOOLTIPS;
  U.memcachelimit = min_ii(BLI_system_memory_max_in_megabytes_int() / 2, 4096);

  /* Auto perspective. */
  U.uiflag |= USER_AUTOPERSP;

  /* Init weight paint range. */
  BKE_colorband_init(&U.coba_weight, true);

  /* Default visible section. */
  U.userpref = USER_SECTION_INTERFACE;

  /* Default to left click select. */
  BKE_keyconfig_pref_set_select_mouse(&U, 0, true);
}

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
  for (ID *idtest = lb->first; idtest; idtest = idtest->next) {
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

/**
 * Update defaults in startup.blend, without having to save and embed the file.
 * This function can be emptied each time the startup.blend is updated. */
void BLO_update_defaults_startup_blend(Main *bmain, const char *app_template)
{
  /* For all builtin templates shipped with Blender. */
  const bool builtin_template =
      (!app_template ||
       STR_ELEM(app_template, "2D_Animation", "Sculpting", "VFX", "Video_Editing"));

  /* For all startup.blend files. */
  for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
    for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
      for (ARegion *ar = sa->regionbase.first; ar; ar = ar->next) {
        /* Remove all stored panels, we want to use defaults
         * (order, open/closed) as defined by UI code here! */
        BKE_area_region_panels_free(&ar->panels);

        /* Reset size so it uses consistent defaults from the region types. */
        ar->sizex = 0;
        ar->sizey = 0;

        /* some toolbars have been saved as initialized,
         * we don't want them to have odd zoom-level or scrolling set, see: T47047 */
        if (ELEM(ar->regiontype, RGN_TYPE_UI, RGN_TYPE_TOOLS, RGN_TYPE_TOOL_PROPS)) {
          ar->v2d.flag &= ~V2D_IS_INITIALISED;
        }
      }

      for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
        switch (sl->spacetype) {
          case SPACE_VIEW3D: {
            View3D *v3d = (View3D *)sl;
            v3d->overlay.texture_paint_mode_opacity = 1.0f;
            v3d->overlay.weight_paint_mode_opacity = 1.0f;
            v3d->overlay.vertex_paint_mode_opacity = 1.0f;
            /* grease pencil settings */
            v3d->vertex_opacity = 1.0f;
            v3d->gp_flag |= V3D_GP_SHOW_EDIT_LINES;
            /* Skip startups that use the viewport color by default. */
            if (v3d->shading.background_type != V3D_SHADING_BACKGROUND_VIEWPORT) {
              copy_v3_fl(v3d->shading.background_color, 0.05f);
            }
            break;
          }
          case SPACE_FILE: {
            SpaceFile *sfile = (SpaceFile *)sl;
            if (sfile->params) {
              const char *dir_default = BKE_appdir_folder_default();
              if (dir_default) {
                STRNCPY(sfile->params->dir, dir_default);
                sfile->params->file[0] = '\0';
              }
            }
            break;
          }
        }
      }
    }
  }

  if (builtin_template) {
    /* Name all screens by their workspaces (avoids 'Default.###' names). */
    /* Default only has one window. */
    wmWindow *win = ((wmWindowManager *)bmain->wm.first)->windows.first;
    for (WorkSpace *workspace = bmain->workspaces.first; workspace;
         workspace = workspace->id.next) {
      WorkSpaceLayout *layout = BKE_workspace_hook_layout_for_workspace_get(win->workspace_hook,
                                                                            workspace);
      bScreen *screen = layout->screen;
      BLI_strncpy(screen->id.name + 2, workspace->id.name + 2, sizeof(screen->id.name) - 2);
      BLI_libblock_ensure_unique_name(bmain, screen->id.name);
    }
  }

  if (app_template == NULL) {
    /* 'UV Editing' should use UV mode. */
    bScreen *screen = BLI_findstring(&bmain->screens, "UV Editing", offsetof(ID, name) + 2);
    for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
      for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
        if (sl->spacetype == SPACE_IMAGE) {
          SpaceImage *sima = (SpaceImage *)sl;
          if (sima->mode == SI_MODE_VIEW) {
            sima->mode = SI_MODE_UV;
          }
        }
      }
    }
  }

  /* For 2D animation template. */
  if (app_template && STREQ(app_template, "2D_Animation")) {
    for (WorkSpace *workspace = bmain->workspaces.first; workspace;
         workspace = workspace->id.next) {
      const char *name = workspace->id.name + 2;

      if (STREQ(name, "Drawing")) {
        workspace->object_mode = OB_MODE_PAINT_GPENCIL;
      }
    }
    /* set object in drawing mode */
    for (Object *object = bmain->objects.first; object; object = object->id.next) {
      if (object->type == OB_GPENCIL) {
        bGPdata *gpd = (bGPdata *)object->data;
        object->mode = OB_MODE_PAINT_GPENCIL;
        gpd->flag |= GP_DATA_STROKE_PAINTMODE;
        break;
      }
    }

    /* Be sure curfalloff and primitive are initializated */
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      ToolSettings *ts = scene->toolsettings;
      if (ts->gp_sculpt.cur_falloff == NULL) {
        ts->gp_sculpt.cur_falloff = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
        CurveMapping *gp_falloff_curve = ts->gp_sculpt.cur_falloff;
        curvemapping_initialize(gp_falloff_curve);
        curvemap_reset(gp_falloff_curve->cm,
                       &gp_falloff_curve->clipr,
                       CURVE_PRESET_GAUSS,
                       CURVEMAP_SLOPE_POSITIVE);
      }
      if (ts->gp_sculpt.cur_primitive == NULL) {
        ts->gp_sculpt.cur_primitive = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
        CurveMapping *gp_primitive_curve = ts->gp_sculpt.cur_primitive;
        curvemapping_initialize(gp_primitive_curve);
        curvemap_reset(gp_primitive_curve->cm,
                       &gp_primitive_curve->clipr,
                       CURVE_PRESET_BELL,
                       CURVEMAP_SLOPE_POSITIVE);
      }
    }
  }

  if (builtin_template) {
    /* Clear all tools to use default options instead, ignore the tool saved in the file. */
    for (WorkSpace *workspace = bmain->workspaces.first; workspace;
         workspace = workspace->id.next) {
      while (!BLI_listbase_is_empty(&workspace->tools)) {
        BKE_workspace_tool_remove(workspace, workspace->tools.first);
      }
    }

    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
        if (sa->spacetype == SPACE_ACTION) {
          /* Show marker lines, hide channels and collapse summary in timelines. */
          SpaceAction *saction = sa->spacedata.first;
          saction->flag |= SACTION_SHOW_MARKER_LINES;

          if (saction->mode == SACTCONT_TIMELINE) {
            saction->ads.flag |= ADS_FLAG_SUMMARY_COLLAPSED;

            for (ARegion *ar = sa->regionbase.first; ar; ar = ar->next) {
              if (ar->regiontype == RGN_TYPE_CHANNELS) {
                ar->flag |= RGN_FLAG_HIDDEN;
              }
            }
          }
        }
        else if (sa->spacetype == SPACE_GRAPH) {
          SpaceGraph *sipo = sa->spacedata.first;
          sipo->flag |= SIPO_MARKER_LINES;
        }
        else if (sa->spacetype == SPACE_NLA) {
          SpaceNla *snla = sa->spacedata.first;
          snla->flag |= SNLA_SHOW_MARKER_LINES;
        }
        else if (sa->spacetype == SPACE_TEXT) {
          /* Show syntax and line numbers in Script workspace text editor. */
          SpaceText *stext = sa->spacedata.first;
          stext->showsyntax = true;
          stext->showlinenrs = true;
        }
        else if (sa->spacetype == SPACE_VIEW3D) {
          /* Screen space cavity by default for faster performance. */
          View3D *v3d = sa->spacedata.first;
          v3d->shading.cavity_type = V3D_SHADING_CAVITY_CURVATURE;
        }
        else if (sa->spacetype == SPACE_CLIP) {
          SpaceClip *sclip = sa->spacedata.first;
          sclip->around = V3D_AROUND_CENTER_MEDIAN;
        }
      }
    }

    /* Show toopbar for sculpt/paint modes. */
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      bool show_tool_header = false;
      if (app_template == NULL) {
        if (STR_ELEM(screen->id.name + 2, "Sculpting", "Texture Paint")) {
          show_tool_header = true;
        }
      }
      else if (STREQ(app_template, "2D_Animation")) {
        if (STR_ELEM(screen->id.name + 2, "2D Animation", "2D Full Canvas")) {
          show_tool_header = true;
        }
      }
      else if (STREQ(app_template, "Sculpting")) {
        if (STR_ELEM(screen->id.name + 2, "Sculpting")) {
          show_tool_header = true;
        }
      }

      if (show_tool_header) {
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
            ListBase *regionbase = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;
            for (ARegion *ar = regionbase->first; ar; ar = ar->next) {
              if (ar->regiontype == RGN_TYPE_TOOL_HEADER) {
                ar->flag &= ~(RGN_FLAG_HIDDEN | RGN_FLAG_HIDDEN_BY_USER);
              }
            }
          }
        }
      }
    }

    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      BLI_strncpy(scene->r.engine, RE_engine_id_BLENDER_EEVEE, sizeof(scene->r.engine));

      scene->r.cfra = 1.0f;
      scene->r.displaymode = R_OUTPUT_WINDOW;

      if (app_template && STREQ(app_template, "Video_Editing")) {
        /* Filmic is too slow, use default until it is optimized. */
        STRNCPY(scene->view_settings.view_transform, "Default");
        STRNCPY(scene->view_settings.look, "None");
      }
      else {
        /* AV Sync break physics sim caching, disable until that is fixed. */
        scene->audio.flag &= ~AUDIO_SYNC;
        scene->flag &= ~SCE_FRAME_DROP;
      }

      /* Don't enable compositing nodes. */
      if (scene->nodetree) {
        ntreeFreeNestedTree(scene->nodetree);
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
    }

    /* Rename light objects. */
    rename_id_for_versioning(bmain, ID_OB, "Lamp", "Light");
    rename_id_for_versioning(bmain, ID_LA, "Lamp", "Light");

    for (Mesh *mesh = bmain->meshes.first; mesh; mesh = mesh->id.next) {
      /* Match default for new meshes. */
      mesh->smoothresh = DEG2RADF(30);
    }
  }

  for (bScreen *sc = bmain->screens.first; sc; sc = sc->id.next) {
    for (ScrArea *sa = sc->areabase.first; sa; sa = sa->next) {
      for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
        if (sl->spacetype == SPACE_VIEW3D) {
          View3D *v3d = (View3D *)sl;
          v3d->shading.flag |= V3D_SHADING_SPECULAR_HIGHLIGHT;
        }
      }
    }
  }

  for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
    copy_v3_v3(scene->display.light_direction, (float[3]){M_SQRT1_3, M_SQRT1_3, M_SQRT1_3});
    copy_v2_fl2(scene->safe_areas.title, 0.1f, 0.05f);
    copy_v2_fl2(scene->safe_areas.action, 0.035f, 0.035f);
  }

  if (app_template == NULL) {
    /* Enable for UV sculpt (other brush types will be created as needed),
     * without this the grab brush will be active but not selectable from the list. */
    Brush *brush = BLI_findstring(&bmain->brushes, "Grab", offsetof(ID, name) + 2);
    brush->ob_mode |= OB_MODE_EDIT;
  }

  for (Brush *brush = bmain->brushes.first; brush; brush = brush->id.next) {
    brush->blur_kernel_radius = 2;
  }
}
