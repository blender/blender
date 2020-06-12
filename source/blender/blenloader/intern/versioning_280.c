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

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include <float.h>
#include <string.h>

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_mempool.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_defaults.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_cloth_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_curveprofile_types.h"
#include "DNA_fluid_types.h"
#include "DNA_freestyle_types.h"
#include "DNA_genfile.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpu_types.h"
#include "DNA_key_types.h"
#include "DNA_layer_types.h"
#include "DNA_light_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_text_types.h"
#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"
#include "DNA_workspace_types.h"
#include "DNA_world_types.h"

#include "BKE_animsys.h"
#include "BKE_brush.h"
#include "BKE_cloth.h"
#include "BKE_collection.h"
#include "BKE_colortools.h"
#include "BKE_constraint.h"
#include "BKE_curveprofile.h"
#include "BKE_customdata.h"
#include "BKE_fcurve.h"
#include "BKE_fcurve_driver.h"
#include "BKE_freestyle.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_idprop.h"
#include "BKE_key.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_paint.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"
#include "BKE_rigidbody.h"
#include "BKE_screen.h"
#include "BKE_sequencer.h"
#include "BKE_studiolight.h"
#include "BKE_unit.h"
#include "BKE_workspace.h"

/* Only for IMB_BlendMode */
#include "IMB_imbuf.h"

#include "DEG_depsgraph.h"

#include "BLT_translation.h"

#include "BLO_readfile.h"
#include "readfile.h"

#include "MEM_guardedalloc.h"

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

static bScreen *screen_parent_find(const bScreen *screen)
{
  /* Can avoid lookup if screen state isn't maximized/full
   * (parent and child store the same state). */
  if (ELEM(screen->state, SCREENMAXIMIZED, SCREENFULL)) {
    LISTBASE_FOREACH (const ScrArea *, area, &screen->areabase) {
      if (area->full && area->full != screen) {
        BLI_assert(area->full->state == screen->state);
        return area->full;
      }
    }
  }

  return NULL;
}

static void do_version_workspaces_create_from_screens(Main *bmain)
{
  bmain->is_locked_for_linking = false;

  for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
    const bScreen *screen_parent = screen_parent_find(screen);
    WorkSpace *workspace;
    if (screen->temp) {
      continue;
    }

    if (screen_parent) {
      /* Full-screen with "Back to Previous" option, don't create
       * a new workspace, add layout workspace containing parent. */
      workspace = BLI_findstring(
          &bmain->workspaces, screen_parent->id.name + 2, offsetof(ID, name) + 2);
    }
    else {
      workspace = BKE_workspace_add(bmain, screen->id.name + 2);
    }
    if (workspace == NULL) {
      continue; /* Not much we can do.. */
    }
    BKE_workspace_layout_add(bmain, workspace, screen, screen->id.name + 2);
  }

  bmain->is_locked_for_linking = true;
}

static void do_version_area_change_space_to_space_action(ScrArea *area, const Scene *scene)
{
  SpaceType *stype = BKE_spacetype_from_id(SPACE_ACTION);
  SpaceAction *saction = (SpaceAction *)stype->new (area, scene);
  ARegion *region_channels;

  /* Properly free current regions */
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    BKE_area_region_free(area->type, region);
  }
  BLI_freelistN(&area->regionbase);

  area->type = stype;
  area->spacetype = stype->spaceid;

  BLI_addhead(&area->spacedata, saction);
  area->regionbase = saction->regionbase;
  BLI_listbase_clear(&saction->regionbase);

  /* Different defaults for timeline */
  region_channels = BKE_area_find_region_type(area, RGN_TYPE_CHANNELS);
  region_channels->flag |= RGN_FLAG_HIDDEN;

  saction->mode = SACTCONT_TIMELINE;
  saction->ads.flag |= ADS_FLAG_SUMMARY_COLLAPSED;
  saction->ads.filterflag |= ADS_FILTER_SUMMARY;
}

/**
 * \brief After lib-link versioning for new workspace design.
 *
 * - Adds a workspace for (almost) each screen of the old file
 *   and adds the needed workspace-layout to wrap the screen.
 * - Active screen isn't stored directly in window anymore, but in the active workspace.
 * - Active scene isn't stored in screen anymore, but in window.
 * - Create workspace instance hook for each window.
 *
 * \note Some of the created workspaces might be deleted again
 * in case of reading the default `startup.blend`.
 */
static void do_version_workspaces_after_lib_link(Main *bmain)
{
  BLI_assert(BLI_listbase_is_empty(&bmain->workspaces));

  do_version_workspaces_create_from_screens(bmain);

  for (wmWindowManager *wm = bmain->wm.first; wm; wm = wm->id.next) {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      bScreen *screen_parent = screen_parent_find(win->screen);
      bScreen *screen = screen_parent ? screen_parent : win->screen;

      if (screen->temp) {
        /* We do not generate a new workspace for those screens...
         * still need to set some data in win. */
        win->workspace_hook = BKE_workspace_instance_hook_create(bmain);
        win->scene = screen->scene;
        /* Deprecated from now on! */
        win->screen = NULL;
        continue;
      }

      WorkSpace *workspace = BLI_findstring(
          &bmain->workspaces, screen->id.name + 2, offsetof(ID, name) + 2);
      BLI_assert(workspace != NULL);
      WorkSpaceLayout *layout = BKE_workspace_layout_find(workspace, win->screen);
      BLI_assert(layout != NULL);

      win->workspace_hook = BKE_workspace_instance_hook_create(bmain);

      BKE_workspace_active_set(win->workspace_hook, workspace);
      BKE_workspace_active_layout_set(win->workspace_hook, workspace, layout);

      /* Move scene and view layer to window. */
      Scene *scene = screen->scene;
      ViewLayer *layer = BLI_findlink(&scene->view_layers, scene->r.actlay);
      if (!layer) {
        layer = BKE_view_layer_default_view(scene);
      }

      win->scene = scene;
      STRNCPY(win->view_layer_name, layer->name);

      /* Deprecated from now on! */
      win->screen = NULL;
    }
  }

  for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
    /* Deprecated from now on! */
    BLI_freelistN(&screen->scene->transform_spaces);
    screen->scene = NULL;
  }
}

#ifdef USE_COLLECTION_COMPAT_28
enum {
  COLLECTION_DEPRECATED_VISIBLE = (1 << 0),
  COLLECTION_DEPRECATED_VIEWPORT = (1 << 0),
  COLLECTION_DEPRECATED_SELECTABLE = (1 << 1),
  COLLECTION_DEPRECATED_DISABLED = (1 << 2),
  COLLECTION_DEPRECATED_RENDER = (1 << 3),
};

static void do_version_view_layer_visibility(ViewLayer *view_layer)
{
  /* Convert from deprecated VISIBLE flag to DISABLED */
  LayerCollection *lc;
  for (lc = view_layer->layer_collections.first; lc; lc = lc->next) {
    if (lc->flag & COLLECTION_DEPRECATED_DISABLED) {
      lc->flag &= ~COLLECTION_DEPRECATED_DISABLED;
    }

    if ((lc->flag & COLLECTION_DEPRECATED_VISIBLE) == 0) {
      lc->flag |= COLLECTION_DEPRECATED_DISABLED;
    }

    lc->flag |= COLLECTION_DEPRECATED_VIEWPORT | COLLECTION_DEPRECATED_RENDER;
  }
}

static void do_version_layer_collection_pre(ViewLayer *view_layer,
                                            ListBase *lb,
                                            GSet *enabled_set,
                                            GSet *selectable_set)
{
  /* Convert from deprecated DISABLED to new layer collection and collection flags */
  LISTBASE_FOREACH (LayerCollection *, lc, lb) {
    if (lc->scene_collection) {
      if (!(lc->flag & COLLECTION_DEPRECATED_DISABLED)) {
        BLI_gset_insert(enabled_set, lc->scene_collection);
      }
      if (lc->flag & COLLECTION_DEPRECATED_SELECTABLE) {
        BLI_gset_insert(selectable_set, lc->scene_collection);
      }
    }

    do_version_layer_collection_pre(
        view_layer, &lc->layer_collections, enabled_set, selectable_set);
  }
}

static void do_version_layer_collection_post(ViewLayer *view_layer,
                                             ListBase *lb,
                                             GSet *enabled_set,
                                             GSet *selectable_set,
                                             GHash *collection_map)
{
  /* Apply layer collection exclude flags. */
  LISTBASE_FOREACH (LayerCollection *, lc, lb) {
    if (!(lc->collection->flag & COLLECTION_IS_MASTER)) {
      SceneCollection *sc = BLI_ghash_lookup(collection_map, lc->collection);
      const bool enabled = (sc && BLI_gset_haskey(enabled_set, sc));
      const bool selectable = (sc && BLI_gset_haskey(selectable_set, sc));

      if (!enabled) {
        lc->flag |= LAYER_COLLECTION_EXCLUDE;
      }
      if (enabled && !selectable) {
        lc->collection->flag |= COLLECTION_RESTRICT_SELECT;
      }
    }

    do_version_layer_collection_post(
        view_layer, &lc->layer_collections, enabled_set, selectable_set, collection_map);
  }
}

static void do_version_scene_collection_convert(
    Main *bmain, ID *id, SceneCollection *sc, Collection *collection, GHash *collection_map)
{
  if (collection_map) {
    BLI_ghash_insert(collection_map, collection, sc);
  }

  for (SceneCollection *nsc = sc->scene_collections.first; nsc;) {
    SceneCollection *nsc_next = nsc->next;
    Collection *ncollection = BKE_collection_add(bmain, collection, nsc->name);
    ncollection->id.lib = id->lib;
    do_version_scene_collection_convert(bmain, id, nsc, ncollection, collection_map);
    nsc = nsc_next;
  }

  LISTBASE_FOREACH (LinkData *, link, &sc->objects) {
    Object *ob = link->data;
    if (ob) {
      BKE_collection_object_add(bmain, collection, ob);
      id_us_min(&ob->id);
    }
  }

  BLI_freelistN(&sc->objects);
  MEM_freeN(sc);
}

static void do_version_group_collection_to_collection(Main *bmain, Collection *group)
{
  /* Convert old 2.8 group collections to new unified collections. */
  if (group->collection) {
    do_version_scene_collection_convert(bmain, &group->id, group->collection, group, NULL);
  }

  group->collection = NULL;
  group->view_layer = NULL;
  id_fake_user_set(&group->id);
}

static void do_version_scene_collection_to_collection(Main *bmain, Scene *scene)
{
  /* Convert old 2.8 scene collections to new unified collections. */

  /* Temporarily clear view layers so we don't do any layer collection syncing
   * and destroy old flags that we want to restore. */
  ListBase view_layers = scene->view_layers;
  BLI_listbase_clear(&scene->view_layers);

  if (!scene->master_collection) {
    scene->master_collection = BKE_collection_master_add();
  }

  /* Convert scene collections. */
  GHash *collection_map = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
  if (scene->collection) {
    do_version_scene_collection_convert(
        bmain, &scene->id, scene->collection, scene->master_collection, collection_map);
    scene->collection = NULL;
  }

  scene->view_layers = view_layers;

  /* Convert layer collections. */
  ViewLayer *view_layer;
  for (view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
    GSet *enabled_set = BLI_gset_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
    GSet *selectable_set = BLI_gset_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);

    do_version_layer_collection_pre(
        view_layer, &view_layer->layer_collections, enabled_set, selectable_set);

    BKE_layer_collection_sync(scene, view_layer);

    do_version_layer_collection_post(
        view_layer, &view_layer->layer_collections, enabled_set, selectable_set, collection_map);

    BLI_gset_free(enabled_set, NULL);
    BLI_gset_free(selectable_set, NULL);

    BKE_layer_collection_sync(scene, view_layer);
  }

  BLI_ghash_free(collection_map, NULL, NULL);
}
#endif

static void do_version_layers_to_collections(Main *bmain, Scene *scene)
{
  /* Since we don't have access to FileData we check the (always valid) first
   * render layer instead. */
  if (!scene->master_collection) {
    scene->master_collection = BKE_collection_master_add();
  }

  if (scene->view_layers.first) {
    return;
  }

  /* Create collections from layers. */
  Collection *collection_master = scene->master_collection;
  Collection *collections[20] = {NULL};

  for (int layer = 0; layer < 20; layer++) {
    LISTBASE_FOREACH (Base *, base, &scene->base) {
      if (base->lay & (1 << layer)) {
        /* Create collections when needed only. */
        if (collections[layer] == NULL) {
          char name[MAX_NAME];

          BLI_snprintf(
              name, sizeof(collection_master->id.name), DATA_("Collection %d"), layer + 1);

          Collection *collection = BKE_collection_add(bmain, collection_master, name);
          collection->id.lib = scene->id.lib;
          if (collection->id.lib != NULL) {
            collection->id.tag |= LIB_TAG_INDIRECT;
          }
          collections[layer] = collection;

          if (!(scene->lay & (1 << layer))) {
            collection->flag |= COLLECTION_RESTRICT_VIEWPORT | COLLECTION_RESTRICT_RENDER;
          }
        }

        /* Note usually this would do slow collection syncing for view layers,
         * but since no view layers exists yet at this point it's fast. */
        BKE_collection_object_add(bmain, collections[layer], base->object);
      }

      if (base->flag & SELECT) {
        base->object->flag |= SELECT;
      }
      else {
        base->object->flag &= ~SELECT;
      }
    }
  }

  /* Handle legacy render layers. */
  bool have_override = false;
  const bool need_default_renderlayer = scene->r.layers.first == NULL;

  LISTBASE_FOREACH (SceneRenderLayer *, srl, &scene->r.layers) {
    ViewLayer *view_layer = BKE_view_layer_add(scene, srl->name, NULL, VIEWLAYER_ADD_NEW);

    if (srl->layflag & SCE_LAY_DISABLE) {
      view_layer->flag &= ~VIEW_LAYER_RENDER;
    }

    if ((srl->layflag & SCE_LAY_FRS) == 0) {
      view_layer->flag &= ~VIEW_LAYER_FREESTYLE;
    }

    view_layer->layflag = srl->layflag;
    view_layer->passflag = srl->passflag;
    view_layer->pass_alpha_threshold = srl->pass_alpha_threshold;
    view_layer->samples = srl->samples;
    view_layer->mat_override = srl->mat_override;

    BKE_freestyle_config_free(&view_layer->freestyle_config, true);
    view_layer->freestyle_config = srl->freestyleConfig;
    view_layer->id_properties = srl->prop;

    /* Set exclusion and overrides. */
    for (int layer = 0; layer < 20; layer++) {
      Collection *collection = collections[layer];
      if (collection) {
        LayerCollection *lc = BKE_layer_collection_first_from_scene_collection(view_layer,
                                                                               collection);

        if (srl->lay_exclude & (1 << layer)) {
          /* Disable excluded layer. */
          have_override = true;
          lc->flag |= LAYER_COLLECTION_EXCLUDE;
          LISTBASE_FOREACH (LayerCollection *, nlc, &lc->layer_collections) {
            nlc->flag |= LAYER_COLLECTION_EXCLUDE;
          }
        }
        else {
          if (srl->lay_zmask & (1 << layer)) {
            have_override = true;
            lc->flag |= LAYER_COLLECTION_HOLDOUT;
          }

          if ((srl->lay & (1 << layer)) == 0) {
            have_override = true;
            lc->flag |= LAYER_COLLECTION_INDIRECT_ONLY;
          }
        }
      }
    }

    /* for convenience set the same active object in all the layers */
    if (scene->basact) {
      view_layer->basact = BKE_view_layer_base_find(view_layer, scene->basact->object);
    }

    LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
      if ((base->flag & BASE_SELECTABLE) && (base->object->flag & SELECT)) {
        base->flag |= BASE_SELECTED;
      }
    }
  }

  BLI_freelistN(&scene->r.layers);

  /* If render layers included overrides, or there are no render layers,
   * we also create a vanilla viewport layer. */
  if (have_override || need_default_renderlayer) {
    ViewLayer *view_layer = BKE_view_layer_add(scene, "Viewport", NULL, VIEWLAYER_ADD_NEW);

    /* If we ported all the original render layers,
     * we don't need to make the viewport layer renderable. */
    if (!BLI_listbase_is_single(&scene->view_layers)) {
      view_layer->flag &= ~VIEW_LAYER_RENDER;
    }

    /* convert active base */
    if (scene->basact) {
      view_layer->basact = BKE_view_layer_base_find(view_layer, scene->basact->object);
    }

    /* convert selected bases */
    LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
      if ((base->flag & BASE_SELECTABLE) && (base->object->flag & SELECT)) {
        base->flag |= BASE_SELECTED;
      }

      /* keep lay around for forward compatibility (open those files in 2.79) */
      base->lay = base->object->lay;
    }
  }

  /* remove bases once and for all */
  LISTBASE_FOREACH (Base *, base, &scene->base) {
    id_us_min(&base->object->id);
  }

  BLI_freelistN(&scene->base);
  scene->basact = NULL;
}

static void do_version_collection_propagate_lib_to_children(Collection *collection)
{
  if (collection->id.lib != NULL) {
    for (CollectionChild *collection_child = collection->children.first; collection_child != NULL;
         collection_child = collection_child->next) {
      if (collection_child->collection->id.lib == NULL) {
        collection_child->collection->id.lib = collection->id.lib;
      }
      do_version_collection_propagate_lib_to_children(collection_child->collection);
    }
  }
}

/** convert old annotations colors */
static void do_versions_fix_annotations(bGPdata *gpd)
{
  LISTBASE_FOREACH (const bGPDpalette *, palette, &gpd->palettes) {
    LISTBASE_FOREACH (bGPDpalettecolor *, palcolor, &palette->colors) {
      /* fix layers */
      LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
        /* unlock/unhide layer */
        gpl->flag &= ~GP_LAYER_LOCKED;
        gpl->flag &= ~GP_LAYER_HIDE;
        /* set opacity to 1 */
        gpl->opacity = 1.0f;
        /* disable tint */
        gpl->tintcolor[3] = 0.0f;

        LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
          LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
            if ((gps->colorname[0] != '\0') && (STREQ(gps->colorname, palcolor->info))) {
              /* copy color settings */
              copy_v4_v4(gpl->color, palcolor->color);
            }
          }
        }
      }
    }
  }
}

static void do_versions_remove_region(ListBase *regionbase, ARegion *region)
{
  BLI_freelinkN(regionbase, region);
}

static void do_versions_remove_regions_by_type(ListBase *regionbase, int regiontype)
{
  ARegion *region, *region_next;
  for (region = regionbase->first; region; region = region_next) {
    region_next = region->next;
    if (region->regiontype == regiontype) {
      do_versions_remove_region(regionbase, region);
    }
  }
}

static ARegion *do_versions_find_region_or_null(ListBase *regionbase, int regiontype)
{
  LISTBASE_FOREACH (ARegion *, region, regionbase) {
    if (region->regiontype == regiontype) {
      return region;
    }
  }
  return NULL;
}

static ARegion *do_versions_find_region(ListBase *regionbase, int regiontype)
{
  ARegion *region = do_versions_find_region_or_null(regionbase, regiontype);
  if (region == NULL) {
    BLI_assert(!"Did not find expected region in versioning");
  }
  return region;
}

static ARegion *do_versions_add_region(int regiontype, const char *name)
{
  ARegion *region = MEM_callocN(sizeof(ARegion), name);
  region->regiontype = regiontype;
  return region;
}

static void do_versions_area_ensure_tool_region(Main *bmain,
                                                const short space_type,
                                                const short region_flag)
{
  for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        if (sl->spacetype == space_type) {
          ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                 &sl->regionbase;
          ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_TOOLS);
          if (!region) {
            ARegion *header = BKE_area_find_region_type(area, RGN_TYPE_HEADER);
            region = do_versions_add_region(RGN_TYPE_TOOLS, "tools region");
            BLI_insertlinkafter(regionbase, header, region);
            region->alignment = RGN_ALIGN_LEFT;
            region->flag = region_flag;
          }
        }
      }
    }
  }
}

static void do_version_bones_split_bbone_scale(ListBase *lb)
{
  LISTBASE_FOREACH (Bone *, bone, lb) {
    bone->scale_in_y = bone->scale_in_x;
    bone->scale_out_y = bone->scale_out_x;

    do_version_bones_split_bbone_scale(&bone->childbase);
  }
}

static void do_version_bones_inherit_scale(ListBase *lb)
{
  LISTBASE_FOREACH (Bone *, bone, lb) {
    if (bone->flag & BONE_NO_SCALE) {
      bone->inherit_scale_mode = BONE_INHERIT_SCALE_NONE_LEGACY;
      bone->flag &= ~BONE_NO_SCALE;
    }

    do_version_bones_inherit_scale(&bone->childbase);
  }
}

static bool replace_bbone_scale_rnapath(char **p_old_path)
{
  char *old_path = *p_old_path;

  if (old_path == NULL) {
    return false;
  }

  if (BLI_str_endswith(old_path, "bbone_scalein") ||
      BLI_str_endswith(old_path, "bbone_scaleout")) {
    *p_old_path = BLI_strdupcat(old_path, "x");

    MEM_freeN(old_path);
    return true;
  }

  return false;
}

static void do_version_bbone_scale_fcurve_fix(ListBase *curves, FCurve *fcu)
{
  /* Update driver variable paths. */
  if (fcu->driver) {
    LISTBASE_FOREACH (DriverVar *, dvar, &fcu->driver->variables) {
      DRIVER_TARGETS_LOOPER_BEGIN (dvar) {
        replace_bbone_scale_rnapath(&dtar->rna_path);
      }
      DRIVER_TARGETS_LOOPER_END;
    }
  }

  /* Update F-Curve's path. */
  if (replace_bbone_scale_rnapath(&fcu->rna_path)) {
    /* If matched, duplicate the curve and tweak name. */
    FCurve *second = BKE_fcurve_copy(fcu);

    second->rna_path[strlen(second->rna_path) - 1] = 'y';

    BLI_insertlinkafter(curves, fcu, second);

    /* Add to the curve group. */
    second->grp = fcu->grp;

    if (fcu->grp != NULL && fcu->grp->channels.last == fcu) {
      fcu->grp->channels.last = second;
    }
  }
}

static void do_version_bbone_scale_animdata_cb(ID *UNUSED(id),
                                               AnimData *adt,
                                               void *UNUSED(wrapper_data))
{
  LISTBASE_FOREACH_MUTABLE (FCurve *, fcu, &adt->drivers) {
    do_version_bbone_scale_fcurve_fix(&adt->drivers, fcu);
  }
}

static void do_version_constraints_maintain_volume_mode_uniform(ListBase *lb)
{
  LISTBASE_FOREACH (bConstraint *, con, lb) {
    if (con->type == CONSTRAINT_TYPE_SAMEVOL) {
      bSameVolumeConstraint *data = (bSameVolumeConstraint *)con->data;
      data->mode = SAMEVOL_UNIFORM;
    }
  }
}

static void do_version_constraints_copy_scale_power(ListBase *lb)
{
  LISTBASE_FOREACH (bConstraint *, con, lb) {
    if (con->type == CONSTRAINT_TYPE_SIZELIKE) {
      bSizeLikeConstraint *data = (bSizeLikeConstraint *)con->data;
      data->power = 1.0f;
    }
  }
}

static void do_version_constraints_copy_rotation_mix_mode(ListBase *lb)
{
  LISTBASE_FOREACH (bConstraint *, con, lb) {
    if (con->type == CONSTRAINT_TYPE_ROTLIKE) {
      bRotateLikeConstraint *data = (bRotateLikeConstraint *)con->data;
      data->mix_mode = (data->flag & ROTLIKE_OFFSET) ? ROTLIKE_MIX_OFFSET : ROTLIKE_MIX_REPLACE;
      data->flag &= ~ROTLIKE_OFFSET;
    }
  }
}

static void do_versions_seq_alloc_transform_and_crop(ListBase *seqbase)
{
  for (Sequence *seq = seqbase->first; seq != NULL; seq = seq->next) {
    if (ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SOUND_HD) == 0) {
      if (seq->strip->transform == NULL) {
        seq->strip->transform = MEM_callocN(sizeof(struct StripTransform), "StripTransform");
      }

      if (seq->strip->crop == NULL) {
        seq->strip->crop = MEM_callocN(sizeof(struct StripCrop), "StripCrop");
      }

      if (seq->seqbase.first != NULL) {
        do_versions_seq_alloc_transform_and_crop(&seq->seqbase);
      }
    }
  }
}

/* Return true if there is something to convert. */
static void do_versions_material_convert_legacy_blend_mode(bNodeTree *ntree, char blend_method)
{
  bool need_update = false;

  /* Iterate backwards from end so we don't encounter newly added links. */
  bNodeLink *prevlink;
  for (bNodeLink *link = ntree->links.last; link; link = prevlink) {
    prevlink = link->prev;

    /* Detect link to replace. */
    bNode *fromnode = link->fromnode;
    bNodeSocket *fromsock = link->fromsock;
    bNode *tonode = link->tonode;
    bNodeSocket *tosock = link->tosock;

    if (!(tonode->type == SH_NODE_OUTPUT_MATERIAL && STREQ(tosock->identifier, "Surface"))) {
      continue;
    }

    /* Only do outputs that are enabled for EEVEE */
    if (!ELEM(tonode->custom1, SHD_OUTPUT_ALL, SHD_OUTPUT_EEVEE)) {
      continue;
    }

    if (blend_method == 1 /* MA_BM_ADD */) {
      nodeRemLink(ntree, link);

      bNode *add_node = nodeAddStaticNode(NULL, ntree, SH_NODE_ADD_SHADER);
      add_node->locx = 0.5f * (fromnode->locx + tonode->locx);
      add_node->locy = 0.5f * (fromnode->locy + tonode->locy);

      bNodeSocket *shader1_socket = add_node->inputs.first;
      bNodeSocket *shader2_socket = add_node->inputs.last;
      bNodeSocket *add_socket = nodeFindSocket(add_node, SOCK_OUT, "Shader");

      bNode *transp_node = nodeAddStaticNode(NULL, ntree, SH_NODE_BSDF_TRANSPARENT);
      transp_node->locx = add_node->locx;
      transp_node->locy = add_node->locy - 110.0f;

      bNodeSocket *transp_socket = nodeFindSocket(transp_node, SOCK_OUT, "BSDF");

      /* Link to input and material output node. */
      nodeAddLink(ntree, fromnode, fromsock, add_node, shader1_socket);
      nodeAddLink(ntree, transp_node, transp_socket, add_node, shader2_socket);
      nodeAddLink(ntree, add_node, add_socket, tonode, tosock);

      need_update = true;
    }
    else if (blend_method == 2 /* MA_BM_MULTIPLY */) {
      nodeRemLink(ntree, link);

      bNode *transp_node = nodeAddStaticNode(NULL, ntree, SH_NODE_BSDF_TRANSPARENT);

      bNodeSocket *color_socket = nodeFindSocket(transp_node, SOCK_IN, "Color");
      bNodeSocket *transp_socket = nodeFindSocket(transp_node, SOCK_OUT, "BSDF");

      /* If incomming link is from a closure socket, we need to convert it. */
      if (fromsock->type == SOCK_SHADER) {
        transp_node->locx = 0.33f * fromnode->locx + 0.66f * tonode->locx;
        transp_node->locy = 0.33f * fromnode->locy + 0.66f * tonode->locy;

        bNode *shtorgb_node = nodeAddStaticNode(NULL, ntree, SH_NODE_SHADERTORGB);
        shtorgb_node->locx = 0.66f * fromnode->locx + 0.33f * tonode->locx;
        shtorgb_node->locy = 0.66f * fromnode->locy + 0.33f * tonode->locy;

        bNodeSocket *shader_socket = nodeFindSocket(shtorgb_node, SOCK_IN, "Shader");
        bNodeSocket *rgba_socket = nodeFindSocket(shtorgb_node, SOCK_OUT, "Color");

        nodeAddLink(ntree, fromnode, fromsock, shtorgb_node, shader_socket);
        nodeAddLink(ntree, shtorgb_node, rgba_socket, transp_node, color_socket);
      }
      else {
        transp_node->locx = 0.5f * (fromnode->locx + tonode->locx);
        transp_node->locy = 0.5f * (fromnode->locy + tonode->locy);

        nodeAddLink(ntree, fromnode, fromsock, transp_node, color_socket);
      }

      /* Link to input and material output node. */
      nodeAddLink(ntree, transp_node, transp_socket, tonode, tosock);

      need_update = true;
    }
  }

  if (need_update) {
    ntreeUpdateTree(NULL, ntree);
  }
}

static void do_versions_local_collection_bits_set(LayerCollection *layer_collection)
{
  layer_collection->local_collections_bits = ~(0);
  LISTBASE_FOREACH (LayerCollection *, child, &layer_collection->layer_collections) {
    do_versions_local_collection_bits_set(child);
  }
}

static void do_version_curvemapping_flag_extend_extrapolate(CurveMapping *cumap)
{
  if (cumap == NULL) {
    return;
  }

#define CUMA_EXTEND_EXTRAPOLATE_OLD 1
  for (int curve_map_index = 0; curve_map_index < 4; curve_map_index++) {
    CurveMap *cuma = &cumap->cm[curve_map_index];
    if (cuma->flag & CUMA_EXTEND_EXTRAPOLATE_OLD) {
      cumap->flag |= CUMA_EXTEND_EXTRAPOLATE;
      return;
    }
  }
#undef CUMA_EXTEND_EXTRAPOLATE_OLD
}

/* Util version to walk over all CurveMappings in the given `bmain` */
static void do_version_curvemapping_walker(Main *bmain, void (*callback)(CurveMapping *cumap))
{
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    callback(&scene->r.mblur_shutter_curve);

    if (scene->view_settings.curve_mapping) {
      callback(scene->view_settings.curve_mapping);
    }

    if (scene->ed != NULL) {
      LISTBASE_FOREACH (Sequence *, seq, &scene->ed->seqbase) {
        LISTBASE_FOREACH (SequenceModifierData *, smd, &seq->modifiers) {
          const SequenceModifierTypeInfo *smti = BKE_sequence_modifier_type_info_get(smd->type);

          if (smti) {
            if (smd->type == seqModifierType_Curves) {
              CurvesModifierData *cmd = (CurvesModifierData *)smd;
              callback(&cmd->curve_mapping);
            }
            else if (smd->type == seqModifierType_HueCorrect) {
              HueCorrectModifierData *hcmd = (HueCorrectModifierData *)smd;
              callback(&hcmd->curve_mapping);
            }
          }
        }
      }
    }

    // toolsettings
    ToolSettings *ts = scene->toolsettings;
    if (ts->vpaint) {
      callback(ts->vpaint->paint.cavity_curve);
    }
    if (ts->wpaint) {
      callback(ts->wpaint->paint.cavity_curve);
    }
    if (ts->sculpt) {
      callback(ts->sculpt->paint.cavity_curve);
    }
    if (ts->uvsculpt) {
      callback(ts->uvsculpt->paint.cavity_curve);
    }
    if (ts->gp_paint) {
      callback(ts->gp_paint->paint.cavity_curve);
    }
    if (ts->gp_interpolate.custom_ipo) {
      callback(ts->gp_interpolate.custom_ipo);
    }
    if (ts->gp_sculpt.cur_falloff) {
      callback(ts->gp_sculpt.cur_falloff);
    }
    if (ts->gp_sculpt.cur_primitive) {
      callback(ts->gp_sculpt.cur_primitive);
    }
    callback(ts->imapaint.paint.cavity_curve);
  }

  FOREACH_NODETREE_BEGIN (bmain, node_tree, id) {
    LISTBASE_FOREACH (bNode *, node, &node_tree->nodes) {
      if (ELEM(node->type,
               SH_NODE_CURVE_VEC,
               SH_NODE_CURVE_RGB,
               CMP_NODE_CURVE_VEC,
               CMP_NODE_CURVE_RGB,
               CMP_NODE_TIME,
               CMP_NODE_HUECORRECT,
               TEX_NODE_CURVE_RGB,
               TEX_NODE_CURVE_TIME)) {
        callback((CurveMapping *)node->storage);
      }
    }
  }
  FOREACH_NODETREE_END;

  LISTBASE_FOREACH (Light *, light, &bmain->lights) {
    if (light->curfalloff) {
      callback(light->curfalloff);
    }
  }

  LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
    if (brush->curve) {
      callback(brush->curve);
    }
    if (brush->gpencil_settings) {
      if (brush->gpencil_settings->curve_sensitivity) {
        callback(brush->gpencil_settings->curve_sensitivity);
      }
      if (brush->gpencil_settings->curve_strength) {
        callback(brush->gpencil_settings->curve_strength);
      }
      if (brush->gpencil_settings->curve_jitter) {
        callback(brush->gpencil_settings->curve_jitter);
      }
    }
  }

  LISTBASE_FOREACH (ParticleSettings *, part, &bmain->particles) {
    if (part->clumpcurve) {
      callback(part->clumpcurve);
    }
    if (part->roughcurve) {
      callback(part->roughcurve);
    }
    if (part->twistcurve) {
      callback(part->twistcurve);
    }
  }

  /* Object */
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    /* Object modifiers */
    LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
      if (md->type == eModifierType_Hook) {
        HookModifierData *hmd = (HookModifierData *)md;

        if (hmd->curfalloff) {
          callback(hmd->curfalloff);
        }
      }
      else if (md->type == eModifierType_Warp) {
        WarpModifierData *tmd = (WarpModifierData *)md;
        if (tmd->curfalloff) {
          callback(tmd->curfalloff);
        }
      }
      else if (md->type == eModifierType_WeightVGEdit) {
        WeightVGEditModifierData *wmd = (WeightVGEditModifierData *)md;

        if (wmd->cmap_curve) {
          callback(wmd->cmap_curve);
        }
      }
    }
    /* Grease pencil modifiers */
    LISTBASE_FOREACH (ModifierData *, md, &ob->greasepencil_modifiers) {
      if (md->type == eGpencilModifierType_Thick) {
        ThickGpencilModifierData *gpmd = (ThickGpencilModifierData *)md;

        if (gpmd->curve_thickness) {
          callback(gpmd->curve_thickness);
        }
      }
      else if (md->type == eGpencilModifierType_Hook) {
        HookGpencilModifierData *gpmd = (HookGpencilModifierData *)md;

        if (gpmd->curfalloff) {
          callback(gpmd->curfalloff);
        }
      }
      else if (md->type == eGpencilModifierType_Noise) {
        NoiseGpencilModifierData *gpmd = (NoiseGpencilModifierData *)md;

        if (gpmd->curve_intensity) {
          callback(gpmd->curve_intensity);
        }
      }
      else if (md->type == eGpencilModifierType_Tint) {
        TintGpencilModifierData *gpmd = (TintGpencilModifierData *)md;

        if (gpmd->curve_intensity) {
          callback(gpmd->curve_intensity);
        }
      }
      else if (md->type == eGpencilModifierType_Smooth) {
        SmoothGpencilModifierData *gpmd = (SmoothGpencilModifierData *)md;

        if (gpmd->curve_intensity) {
          callback(gpmd->curve_intensity);
        }
      }
      else if (md->type == eGpencilModifierType_Color) {
        ColorGpencilModifierData *gpmd = (ColorGpencilModifierData *)md;

        if (gpmd->curve_intensity) {
          callback(gpmd->curve_intensity);
        }
      }
      else if (md->type == eGpencilModifierType_Opacity) {
        OpacityGpencilModifierData *gpmd = (OpacityGpencilModifierData *)md;

        if (gpmd->curve_intensity) {
          callback(gpmd->curve_intensity);
        }
      }
    }
  }

  /* Free Style */
  LISTBASE_FOREACH (struct FreestyleLineStyle *, linestyle, &bmain->linestyles) {
    LISTBASE_FOREACH (LineStyleModifier *, m, &linestyle->alpha_modifiers) {
      switch (m->type) {
        case LS_MODIFIER_ALONG_STROKE:
          callback(((LineStyleAlphaModifier_AlongStroke *)m)->curve);
          break;
        case LS_MODIFIER_DISTANCE_FROM_CAMERA:
          callback(((LineStyleAlphaModifier_DistanceFromCamera *)m)->curve);
          break;
        case LS_MODIFIER_DISTANCE_FROM_OBJECT:
          callback(((LineStyleAlphaModifier_DistanceFromObject *)m)->curve);
          break;
        case LS_MODIFIER_MATERIAL:
          callback(((LineStyleAlphaModifier_Material *)m)->curve);
          break;
        case LS_MODIFIER_TANGENT:
          callback(((LineStyleAlphaModifier_Tangent *)m)->curve);
          break;
        case LS_MODIFIER_NOISE:
          callback(((LineStyleAlphaModifier_Noise *)m)->curve);
          break;
        case LS_MODIFIER_CREASE_ANGLE:
          callback(((LineStyleAlphaModifier_CreaseAngle *)m)->curve);
          break;
        case LS_MODIFIER_CURVATURE_3D:
          callback(((LineStyleAlphaModifier_Curvature_3D *)m)->curve);
          break;
      }
    }

    LISTBASE_FOREACH (LineStyleModifier *, m, &linestyle->thickness_modifiers) {
      switch (m->type) {
        case LS_MODIFIER_ALONG_STROKE:
          callback(((LineStyleThicknessModifier_AlongStroke *)m)->curve);
          break;
        case LS_MODIFIER_DISTANCE_FROM_CAMERA:
          callback(((LineStyleThicknessModifier_DistanceFromCamera *)m)->curve);
          break;
        case LS_MODIFIER_DISTANCE_FROM_OBJECT:
          callback(((LineStyleThicknessModifier_DistanceFromObject *)m)->curve);
          break;
        case LS_MODIFIER_MATERIAL:
          callback(((LineStyleThicknessModifier_Material *)m)->curve);
          break;
        case LS_MODIFIER_TANGENT:
          callback(((LineStyleThicknessModifier_Tangent *)m)->curve);
          break;
        case LS_MODIFIER_CREASE_ANGLE:
          callback(((LineStyleThicknessModifier_CreaseAngle *)m)->curve);
          break;
        case LS_MODIFIER_CURVATURE_3D:
          callback(((LineStyleThicknessModifier_Curvature_3D *)m)->curve);
          break;
      }
    }
  }
}

static void do_version_fcurve_hide_viewport_fix(struct ID *UNUSED(id),
                                                struct FCurve *fcu,
                                                void *UNUSED(user_data))
{
  if (strcmp(fcu->rna_path, "hide")) {
    return;
  }

  MEM_freeN(fcu->rna_path);
  fcu->rna_path = BLI_strdupn("hide_viewport", 13);
}

void do_versions_after_linking_280(Main *bmain, ReportList *UNUSED(reports))
{
  bool use_collection_compat_28 = true;

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 0)) {
    use_collection_compat_28 = false;

    /* Convert group layer visibility flags to hidden nested collection. */
    for (Collection *collection = bmain->collections.first; collection;
         collection = collection->id.next) {
      /* Add fake user for all existing groups. */
      id_fake_user_set(&collection->id);

      if (collection->flag & (COLLECTION_RESTRICT_VIEWPORT | COLLECTION_RESTRICT_RENDER)) {
        continue;
      }

      Collection *hidden_collection_array[20] = {NULL};
      for (CollectionObject *cob = collection->gobject.first, *cob_next = NULL; cob;
           cob = cob_next) {
        cob_next = cob->next;
        Object *ob = cob->ob;

        if (!(ob->lay & collection->layer)) {
          /* Find or create hidden collection matching object's first layer. */
          Collection **collection_hidden = NULL;
          int coll_idx = 0;
          for (; coll_idx < 20; coll_idx++) {
            if (ob->lay & (1 << coll_idx)) {
              collection_hidden = &hidden_collection_array[coll_idx];
              break;
            }
          }
          BLI_assert(collection_hidden != NULL);

          if (*collection_hidden == NULL) {
            char name[MAX_ID_NAME];
            BLI_snprintf(name, sizeof(name), DATA_("Hidden %d"), coll_idx + 1);
            *collection_hidden = BKE_collection_add(bmain, collection, name);
            (*collection_hidden)->flag |= COLLECTION_RESTRICT_VIEWPORT |
                                          COLLECTION_RESTRICT_RENDER;
          }

          BKE_collection_object_add(bmain, *collection_hidden, ob);
          BKE_collection_object_remove(bmain, collection, ob, true);
        }
      }
    }

    /* We need to assign lib pointer to generated hidden collections *after* all have been
     * created, otherwise we'll end up with several data-blocks sharing same name/library,
     * which is FORBIDDEN! Note: we need this to be recursive, since a child collection may be
     * sorted before its parent in bmain. */
    for (Collection *collection = bmain->collections.first; collection != NULL;
         collection = collection->id.next) {
      do_version_collection_propagate_lib_to_children(collection);
    }

    /* Convert layers to collections. */
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      do_version_layers_to_collections(bmain, scene);
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 0)) {
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      /* same render-layer as do_version_workspaces_after_lib_link will activate,
       * so same layer as BKE_view_layer_default_view would return */
      ViewLayer *layer = screen->scene->view_layers.first;

      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
          if (space->spacetype == SPACE_OUTLINER) {
            SpaceOutliner *soutliner = (SpaceOutliner *)space;

            soutliner->outlinevis = SO_VIEW_LAYER;

            if (BLI_listbase_count_at_most(&layer->layer_collections, 2) == 1) {
              if (soutliner->treestore == NULL) {
                soutliner->treestore = BLI_mempool_create(
                    sizeof(TreeStoreElem), 1, 512, BLI_MEMPOOL_ALLOW_ITER);
              }

              /* Create a tree store element for the collection. This is normally
               * done in check_persistent (outliner_tree.c), but we need to access
               * it here :/ (expand element if it's the only one) */
              TreeStoreElem *tselem = BLI_mempool_calloc(soutliner->treestore);
              tselem->type = TSE_LAYER_COLLECTION;
              tselem->id = layer->layer_collections.first;
              tselem->nr = tselem->used = 0;
              tselem->flag &= ~TSE_CLOSED;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 0)) {
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
          if (space->spacetype == SPACE_IMAGE) {
            SpaceImage *sima = (SpaceImage *)space;
            if ((sima) && (sima->gpd)) {
              sima->gpd->flag |= GP_DATA_ANNOTATIONS;
              do_versions_fix_annotations(sima->gpd);
            }
          }
          if (space->spacetype == SPACE_CLIP) {
            SpaceClip *spclip = (SpaceClip *)space;
            MovieClip *clip = spclip->clip;
            if ((clip) && (clip->gpd)) {
              clip->gpd->flag |= GP_DATA_ANNOTATIONS;
              do_versions_fix_annotations(clip->gpd);
            }
          }
        }
      }
    }
  }

  /* New workspace design */
  if (!MAIN_VERSION_ATLEAST(bmain, 280, 1)) {
    do_version_workspaces_after_lib_link(bmain);
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 2)) {
    /* Cleanup any remaining SceneRenderLayer data for files that were created
     * with Blender 2.8 before the SceneRenderLayer > RenderLayer refactor. */
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      LISTBASE_FOREACH (SceneRenderLayer *, srl, &scene->r.layers) {
        if (srl->prop) {
          IDP_FreeProperty(srl->prop);
        }
        BKE_freestyle_config_free(&srl->freestyleConfig, true);
      }
      BLI_freelistN(&scene->r.layers);
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 3)) {
    /* Due to several changes to particle RNA and draw code particles from older files may
     * no longer be visible.
     * Here we correct this by setting a default draw size for those files. */
    for (Object *object = bmain->objects.first; object; object = object->id.next) {
      LISTBASE_FOREACH (ParticleSystem *, psys, &object->particlesystem) {
        if (psys->part->draw_size == 0.0f) {
          psys->part->draw_size = 0.1f;
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 4)) {
    for (Object *object = bmain->objects.first; object; object = object->id.next) {
      if (object->particlesystem.first) {
        object->duplicator_visibility_flag = OB_DUPLI_FLAG_VIEWPORT;
        LISTBASE_FOREACH (ParticleSystem *, psys, &object->particlesystem) {
          if (psys->part->draw & PART_DRAW_EMITTER) {
            object->duplicator_visibility_flag |= OB_DUPLI_FLAG_RENDER;
            break;
          }
        }
      }
      else if (object->transflag & OB_DUPLI) {
        object->duplicator_visibility_flag = OB_DUPLI_FLAG_VIEWPORT;
      }
      else {
        object->duplicator_visibility_flag = OB_DUPLI_FLAG_VIEWPORT | OB_DUPLI_FLAG_RENDER;
      }
    }

    /* Cleanup deprecated flag from particlesettings data-blocks. */
    for (ParticleSettings *part = bmain->particles.first; part; part = part->id.next) {
      part->draw &= ~PART_DRAW_EMITTER;
    }
  }

  /* SpaceTime & SpaceLogic removal/replacing */
  if (!MAIN_VERSION_ATLEAST(bmain, 280, 9)) {
    const wmWindowManager *wm = bmain->wm.first;
    const Scene *scene = bmain->scenes.first;

    if (wm != NULL) {
      /* Action editors need a scene for creation. First, update active
       * screens using the active scene of the window they're displayed in.
       * Next, update remaining screens using first scene in main listbase. */

      LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
        const bScreen *screen = BKE_workspace_active_screen_get(win->workspace_hook);
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          if (ELEM(area->butspacetype, SPACE_TIME, SPACE_LOGIC)) {
            do_version_area_change_space_to_space_action(area, win->scene);

            /* Don't forget to unset! */
            area->butspacetype = SPACE_EMPTY;
          }
        }
      }
    }
    if (scene != NULL) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          if (ELEM(area->butspacetype, SPACE_TIME, SPACE_LOGIC)) {
            /* Areas that were already handled won't be handled again */
            do_version_area_change_space_to_space_action(area, scene);

            /* Don't forget to unset! */
            area->butspacetype = SPACE_EMPTY;
          }
        }
      }
    }
  }

#ifdef USE_COLLECTION_COMPAT_28
  if (use_collection_compat_28 && !MAIN_VERSION_ATLEAST(bmain, 280, 14)) {
    for (Collection *group = bmain->collections.first; group; group = group->id.next) {
      do_version_group_collection_to_collection(bmain, group);
    }

    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      do_version_scene_collection_to_collection(bmain, scene);
    }
  }
#endif

  /* Update Curve object Shape Key data layout to include the Radius property */
  if (!MAIN_VERSION_ATLEAST(bmain, 280, 23)) {
    for (Curve *cu = bmain->curves.first; cu; cu = cu->id.next) {
      if (!cu->key || cu->key->elemsize != sizeof(float[4])) {
        continue;
      }

      cu->key->elemstr[0] = 3; /*KEYELEM_ELEM_SIZE_CURVE*/
      cu->key->elemsize = sizeof(float[3]);

      int new_count = BKE_keyblock_curve_element_count(&cu->nurb);

      LISTBASE_FOREACH (KeyBlock *, block, &cu->key->block) {
        int old_count = block->totelem;
        void *old_data = block->data;

        if (!old_data || old_count <= 0) {
          continue;
        }

        block->totelem = new_count;
        block->data = MEM_callocN(sizeof(float[3]) * new_count, __func__);

        float *oldptr = old_data;
        float(*newptr)[3] = block->data;

        LISTBASE_FOREACH (Nurb *, nu, &cu->nurb) {
          if (nu->bezt) {
            BezTriple *bezt = nu->bezt;

            for (int a = 0; a < nu->pntsu; a++, bezt++) {
              if ((old_count -= 3) < 0) {
                memcpy(newptr, bezt->vec, sizeof(float[3][3]));
                newptr[3][0] = bezt->tilt;
              }
              else {
                memcpy(newptr, oldptr, sizeof(float[3][4]));
              }

              newptr[3][1] = bezt->radius;

              oldptr += 3 * 4;
              newptr += 4; /*KEYELEM_ELEM_LEN_BEZTRIPLE*/
            }
          }
          else if (nu->bp) {
            BPoint *bp = nu->bp;

            for (int a = 0; a < nu->pntsu * nu->pntsv; a++, bp++) {
              if (--old_count < 0) {
                copy_v3_v3(newptr[0], bp->vec);
                newptr[1][0] = bp->tilt;
              }
              else {
                memcpy(newptr, oldptr, sizeof(float[4]));
              }

              newptr[1][1] = bp->radius;

              oldptr += 4;
              newptr += 2; /*KEYELEM_ELEM_LEN_BPOINT*/
            }
          }
        }

        MEM_freeN(old_data);
      }
    }
  }

  /* Move B-Bone custom handle settings from bPoseChannel to Bone. */
  if (!MAIN_VERSION_ATLEAST(bmain, 280, 25)) {
    for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
      bArmature *arm = ob->data;

      /* If it is an armature from the same file. */
      if (ob->pose && arm && arm->id.lib == ob->id.lib) {
        bool rebuild = false;

        LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
          /* If the 2.7 flag is enabled, processing is needed. */
          if (pchan->bone && (pchan->bboneflag & PCHAN_BBONE_CUSTOM_HANDLES)) {
            /* If the settings in the Bone are not set, copy. */
            if (pchan->bone->bbone_prev_type == BBONE_HANDLE_AUTO &&
                pchan->bone->bbone_next_type == BBONE_HANDLE_AUTO &&
                pchan->bone->bbone_prev == NULL && pchan->bone->bbone_next == NULL) {
              pchan->bone->bbone_prev_type = (pchan->bboneflag & PCHAN_BBONE_CUSTOM_START_REL) ?
                                                 BBONE_HANDLE_RELATIVE :
                                                 BBONE_HANDLE_ABSOLUTE;
              pchan->bone->bbone_next_type = (pchan->bboneflag & PCHAN_BBONE_CUSTOM_END_REL) ?
                                                 BBONE_HANDLE_RELATIVE :
                                                 BBONE_HANDLE_ABSOLUTE;

              if (pchan->bbone_prev) {
                pchan->bone->bbone_prev = pchan->bbone_prev->bone;
              }
              if (pchan->bbone_next) {
                pchan->bone->bbone_next = pchan->bbone_next->bone;
              }
            }

            rebuild = true;
            pchan->bboneflag = 0;
          }
        }

        /* Tag pose rebuild for all objects that use this armature. */
        if (rebuild) {
          for (Object *ob2 = bmain->objects.first; ob2; ob2 = ob2->id.next) {
            if (ob2->pose && ob2->data == arm) {
              ob2->pose->flag |= POSE_RECALC;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 30)) {
    for (Brush *brush = bmain->brushes.first; brush; brush = brush->id.next) {
      if (brush->gpencil_settings != NULL) {
        brush->gpencil_tool = brush->gpencil_settings->brush_type;
      }
    }
    BKE_paint_toolslots_init_from_main(bmain);
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 38)) {
    /* Ensure we get valid rigidbody object/constraint data in relevant collections' objects.
     */
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      RigidBodyWorld *rbw = scene->rigidbody_world;

      if (rbw == NULL) {
        continue;
      }

      BKE_rigidbody_objects_collection_validate(scene, rbw);
      BKE_rigidbody_constraints_collection_validate(scene, rbw);
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 69)) {
    /* Unify DOF settings (EEVEE part only) */
    const int SCE_EEVEE_DOF_ENABLED = (1 << 7);
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (STREQ(scene->r.engine, RE_engine_id_BLENDER_EEVEE)) {
        if (scene->eevee.flag & SCE_EEVEE_DOF_ENABLED) {
          Object *cam_ob = scene->camera;
          if (cam_ob && cam_ob->type == OB_CAMERA) {
            Camera *cam = cam_ob->data;
            cam->dof.flag |= CAM_DOF_ENABLED;
          }
        }
      }
    }

    LISTBASE_FOREACH (Camera *, camera, &bmain->cameras) {
      camera->dof.focus_object = camera->dof_ob;
      camera->dof.focus_distance = camera->dof_distance;
      camera->dof.aperture_fstop = camera->gpu_dof.fstop;
      camera->dof.aperture_rotation = camera->gpu_dof.rotation;
      camera->dof.aperture_ratio = camera->gpu_dof.ratio;
      camera->dof.aperture_blades = camera->gpu_dof.num_blades;
      camera->dof_ob = NULL;
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 281, 2)) {
    /* Replace Multiply and Additive blend mode by Alpha Blend
     * now that we use dualsource blending. */
    /* We take care of doing only nodetrees that are always part of materials
     * with old blending modes. */
    for (Material *ma = bmain->materials.first; ma; ma = ma->id.next) {
      bNodeTree *ntree = ma->nodetree;
      if (ma->blend_method == 1 /* MA_BM_ADD */) {
        if (ma->use_nodes) {
          do_versions_material_convert_legacy_blend_mode(ntree, 1 /* MA_BM_ADD */);
        }
        ma->blend_method = MA_BM_BLEND;
      }
      else if (ma->blend_method == 2 /* MA_BM_MULTIPLY */) {
        if (ma->use_nodes) {
          do_versions_material_convert_legacy_blend_mode(ntree, 2 /* MA_BM_MULTIPLY */);
        }
        ma->blend_method = MA_BM_BLEND;
      }
    }

    /* Update all ruler layers to set new flag. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      bGPdata *gpd = scene->gpd;
      if (gpd == NULL) {
        continue;
      }
      LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
        if (STREQ(gpl->info, "RulerData3D")) {
          gpl->flag |= GP_LAYER_IS_RULER;
          break;
        }
      }
    }

    /* This versioning could probably be done only on earlier versions, not sure however
     * which exact version fully deprecated tessfaces, so think we can keep that one here, no
     * harm to be expected anyway for being over-conservative. */
    for (Mesh *me = bmain->meshes.first; me != NULL; me = me->id.next) {
      /*check if we need to convert mfaces to mpolys*/
      if (me->totface && !me->totpoly) {
        /* temporarily switch main so that reading from
         * external CustomData works */
        Main *gmain = G_MAIN;
        G_MAIN = bmain;

        BKE_mesh_do_versions_convert_mfaces_to_mpolys(me);

        G_MAIN = gmain;
      }

      /* Deprecated, only kept for conversion. */
      BKE_mesh_tessface_clear(me);

      /* Moved from do_versions because we need updated polygons for calculating normals. */
      if (MAIN_VERSION_OLDER(bmain, 256, 6)) {
        BKE_mesh_calc_normals(me);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 282, 2)) {
    /* Init all Vertex/Sculpt and Weight Paint brushes. */
    Brush *brush;
    Material *ma;
    /* Pen Soft brush. */
    brush = (Brush *)rename_id_for_versioning(bmain, ID_BR, "Draw Soft", "Pencil Soft");
    if (brush) {
      brush->gpencil_settings->icon_id = GP_BRUSH_ICON_PEN;
    }
    rename_id_for_versioning(bmain, ID_BR, "Draw Pencil", "Pencil");
    rename_id_for_versioning(bmain, ID_BR, "Draw Pen", "Pen");
    rename_id_for_versioning(bmain, ID_BR, "Draw Ink", "Ink Pen");
    rename_id_for_versioning(bmain, ID_BR, "Draw Noise", "Ink Pen Rough");
    rename_id_for_versioning(bmain, ID_BR, "Draw Marker", "Marker Bold");
    rename_id_for_versioning(bmain, ID_BR, "Draw Block", "Marker Chisel");

    ma = BLI_findstring(&bmain->materials, "Black", offsetof(ID, name) + 2);
    if (ma && ma->gp_style) {
      rename_id_for_versioning(bmain, ID_MA, "Black", "Solid Stroke");
    }
    ma = BLI_findstring(&bmain->materials, "Red", offsetof(ID, name) + 2);
    if (ma && ma->gp_style) {
      rename_id_for_versioning(bmain, ID_MA, "Red", "Squares Stroke");
    }
    ma = BLI_findstring(&bmain->materials, "Grey", offsetof(ID, name) + 2);
    if (ma && ma->gp_style) {
      rename_id_for_versioning(bmain, ID_MA, "Grey", "Solid Fill");
    }
    ma = BLI_findstring(&bmain->materials, "Black Dots", offsetof(ID, name) + 2);
    if (ma && ma->gp_style) {
      rename_id_for_versioning(bmain, ID_MA, "Black Dots", "Dots Stroke");
    }

    brush = BLI_findstring(&bmain->brushes, "Pencil", offsetof(ID, name) + 2);

    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      ToolSettings *ts = scene->toolsettings;

      /* Ensure new Paint modes. */
      BKE_paint_ensure_from_paintmode(scene, PAINT_MODE_GPENCIL);
      BKE_paint_ensure_from_paintmode(scene, PAINT_MODE_VERTEX_GPENCIL);
      BKE_paint_ensure_from_paintmode(scene, PAINT_MODE_SCULPT_GPENCIL);
      BKE_paint_ensure_from_paintmode(scene, PAINT_MODE_WEIGHT_GPENCIL);

      /* Set default Draw brush. */
      if (brush != NULL) {
        Paint *paint = &ts->gp_paint->paint;
        BKE_paint_brush_set(paint, brush);
        /* Enable cursor by default. */
        paint->flags |= PAINT_SHOW_BRUSH;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 283, 8)) {

    /* During development of Blender 2.80 the "Object.hide" property was
     * removed, and reintroduced in 5e968a996a53 as "Object.hide_viewport". */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      BKE_fcurves_id_cb(&ob->id, do_version_fcurve_hide_viewport_fix, NULL);
    }

    /* Reset all grease pencil brushes. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      /* Ensure new Paint modes. */
      BKE_paint_ensure_from_paintmode(scene, PAINT_MODE_VERTEX_GPENCIL);
      BKE_paint_ensure_from_paintmode(scene, PAINT_MODE_SCULPT_GPENCIL);
      BKE_paint_ensure_from_paintmode(scene, PAINT_MODE_WEIGHT_GPENCIL);
    }
  }

  /**
   * Versioning code until next subversion bump goes here.
   *
   * \note Be sure to check when bumping the version:
   * - #blo_do_versions_280 in this file.
   * - "versioning_userdef.c", #BLO_version_defaults_userpref_blend
   * - "versioning_userdef.c", #do_versions_theme
   *
   * \note Keep this message at the bottom of the function.
   */
  {
    /* Keep this block, even when empty. */
  }
}

/* NOTE: This version patch is intended for versions < 2.52.2,
 * but was initially introduced in 2.27 already.
 * But in 2.79 another case generating non-unique names was discovered
 * (see T55668, involving Meta strips). */
static void do_versions_seq_unique_name_all_strips(Scene *sce, ListBase *seqbasep)
{
  for (Sequence *seq = seqbasep->first; seq != NULL; seq = seq->next) {
    BKE_sequence_base_unique_name_recursive(&sce->ed->seqbase, seq);
    if (seq->seqbase.first != NULL) {
      do_versions_seq_unique_name_all_strips(sce, &seq->seqbase);
    }
  }
}

static void do_versions_seq_set_cache_defaults(Editing *ed)
{
  ed->cache_flag = SEQ_CACHE_STORE_FINAL_OUT;
  ed->cache_flag |= SEQ_CACHE_VIEW_FINAL_OUT;
  ed->cache_flag |= SEQ_CACHE_VIEW_ENABLE;
  ed->recycle_max_cost = 10.0f;
}

void blo_do_versions_280(FileData *fd, Library *UNUSED(lib), Main *bmain)
{
  bool use_collection_compat_28 = true;

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 0)) {
    use_collection_compat_28 = false;

    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      scene->r.gauss = 1.5f;
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 1)) {
    if (!DNA_struct_elem_find(fd->filesdna, "Light", "float", "bleedexp")) {
      for (Light *la = bmain->lights.first; la; la = la->id.next) {
        la->bleedexp = 2.5f;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "GPUDOFSettings", "float", "ratio")) {
      for (Camera *ca = bmain->cameras.first; ca; ca = ca->id.next) {
        ca->gpu_dof.ratio = 1.0f;
      }
    }

    /* MTexPoly now removed. */
    if (DNA_struct_find(fd->filesdna, "MTexPoly")) {
      for (Mesh *me = bmain->meshes.first; me; me = me->id.next) {
        /* If we have UV's, so this file will have MTexPoly layers too! */
        if (me->mloopuv != NULL) {
          CustomData_update_typemap(&me->pdata);
          CustomData_free_layers(&me->pdata, CD_MTEXPOLY, me->totpoly);
          BKE_mesh_update_customdata_pointers(me, false);
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 2)) {
    if (!DNA_struct_elem_find(fd->filesdna, "Light", "float", "cascade_max_dist")) {
      for (Light *la = bmain->lights.first; la; la = la->id.next) {
        la->cascade_max_dist = 1000.0f;
        la->cascade_count = 4;
        la->cascade_exponent = 0.8f;
        la->cascade_fade = 0.1f;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "Light", "float", "contact_dist")) {
      for (Light *la = bmain->lights.first; la; la = la->id.next) {
        la->contact_dist = 0.2f;
        la->contact_bias = 0.03f;
        la->contact_spread = 0.2f;
        la->contact_thickness = 0.2f;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "LightProbe", "float", "vis_bias")) {
      for (LightProbe *probe = bmain->lightprobes.first; probe; probe = probe->id.next) {
        probe->vis_bias = 1.0f;
        probe->vis_blur = 0.2f;
      }
    }

    typedef enum eNTreeDoVersionErrors {
      NTREE_DOVERSION_NO_ERROR = 0,
      NTREE_DOVERSION_NEED_OUTPUT = (1 << 0),
      NTREE_DOVERSION_TRANSPARENCY_EMISSION = (1 << 1),
    } eNTreeDoVersionErrors;

    /* Eevee shader nodes renamed because of the output node system.
     * Note that a new output node is not being added here, because it would be overkill
     * to handle this case in lib_verify_nodetree.
     *
     * Also, metallic node is now unified into the principled node. */
    eNTreeDoVersionErrors error = NTREE_DOVERSION_NO_ERROR;

    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type == 194 /* SH_NODE_EEVEE_METALLIC */ &&
              STREQ(node->idname, "ShaderNodeOutputMetallic")) {
            BLI_strncpy(node->idname, "ShaderNodeEeveeMetallic", sizeof(node->idname));
            error |= NTREE_DOVERSION_NEED_OUTPUT;
          }

          else if (node->type == SH_NODE_EEVEE_SPECULAR &&
                   STREQ(node->idname, "ShaderNodeOutputSpecular")) {
            BLI_strncpy(node->idname, "ShaderNodeEeveeSpecular", sizeof(node->idname));
            error |= NTREE_DOVERSION_NEED_OUTPUT;
          }

          else if (node->type == 196 /* SH_NODE_OUTPUT_EEVEE_MATERIAL */ &&
                   STREQ(node->idname, "ShaderNodeOutputEeveeMaterial")) {
            node->type = SH_NODE_OUTPUT_MATERIAL;
            BLI_strncpy(node->idname, "ShaderNodeOutputMaterial", sizeof(node->idname));
          }

          else if (node->type == 194 /* SH_NODE_EEVEE_METALLIC */ &&
                   STREQ(node->idname, "ShaderNodeEeveeMetallic")) {
            node->type = SH_NODE_BSDF_PRINCIPLED;
            BLI_strncpy(node->idname, "ShaderNodeBsdfPrincipled", sizeof(node->idname));
            node->custom1 = SHD_GLOSSY_MULTI_GGX;
            error |= NTREE_DOVERSION_TRANSPARENCY_EMISSION;
          }
        }
      }
    }
    FOREACH_NODETREE_END;

    if (error & NTREE_DOVERSION_NEED_OUTPUT) {
      BKE_report(fd->reports, RPT_ERROR, "Eevee material conversion problem. Error in console");
      printf(
          "You need to connect Principled and Eevee Specular shader nodes to new material "
          "output "
          "nodes.\n");
    }

    if (error & NTREE_DOVERSION_TRANSPARENCY_EMISSION) {
      BKE_report(fd->reports, RPT_ERROR, "Eevee material conversion problem. Error in console");
      printf(
          "You need to combine transparency and emission shaders to the converted Principled "
          "shader nodes.\n");
    }

#ifdef USE_COLLECTION_COMPAT_28
    if (use_collection_compat_28 &&
        (DNA_struct_elem_find(fd->filesdna, "ViewLayer", "FreestyleConfig", "freestyle_config") ==
         false) &&
        DNA_struct_elem_find(fd->filesdna, "Scene", "ListBase", "view_layers")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        ViewLayer *view_layer;
        for (view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
          view_layer->flag |= VIEW_LAYER_FREESTYLE;
          view_layer->layflag = 0x7FFF; /* solid ztra halo edge strand */
          view_layer->passflag = SCE_PASS_COMBINED | SCE_PASS_Z;
          view_layer->pass_alpha_threshold = 0.5f;
          BKE_freestyle_config_init(&view_layer->freestyle_config);
        }
      }
    }
#endif

    {
      /* Init grease pencil edit line color */
      if (!DNA_struct_elem_find(fd->filesdna, "bGPdata", "float", "line_color[4]")) {
        for (bGPdata *gpd = bmain->gpencils.first; gpd; gpd = gpd->id.next) {
          ARRAY_SET_ITEMS(gpd->line_color, 0.6f, 0.6f, 0.6f, 0.5f);
        }
      }

      /* Init grease pencil pixel size factor */
      if (!DNA_struct_elem_find(fd->filesdna, "bGPdata", "float", "pixfactor")) {
        for (bGPdata *gpd = bmain->gpencils.first; gpd; gpd = gpd->id.next) {
          gpd->pixfactor = GP_DEFAULT_PIX_FACTOR;
        }
      }

      /* Grease pencil multiframe falloff curve */
      if (!DNA_struct_elem_find(
              fd->filesdna, "GP_Sculpt_Settings", "CurveMapping", "cur_falloff")) {
        for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
          /* sculpt brushes */
          GP_Sculpt_Settings *gset = &scene->toolsettings->gp_sculpt;
          if ((gset) && (gset->cur_falloff == NULL)) {
            gset->cur_falloff = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
            BKE_curvemapping_initialize(gset->cur_falloff);
            BKE_curvemap_reset(gset->cur_falloff->cm,
                               &gset->cur_falloff->clipr,
                               CURVE_PRESET_GAUSS,
                               CURVEMAP_SLOPE_POSITIVE);
          }
        }
      }
    }

    /* 2.79 style Maintain Volume mode. */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      do_version_constraints_maintain_volume_mode_uniform(&ob->constraints);
      if (ob->pose) {
        LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
          do_version_constraints_maintain_volume_mode_uniform(&pchan->constraints);
        }
      }
    }
  }

#ifdef USE_COLLECTION_COMPAT_28
  if (use_collection_compat_28 && !MAIN_VERSION_ATLEAST(bmain, 280, 3)) {
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      ViewLayer *view_layer;
      for (view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
        do_version_view_layer_visibility(view_layer);
      }
    }

    for (Collection *group = bmain->collections.first; group; group = group->id.next) {
      if (group->view_layer != NULL) {
        do_version_view_layer_visibility(group->view_layer);
      }
    }
  }
#endif

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 3)) {
    /* init grease pencil grids and paper */
    if (!DNA_struct_elem_find(fd->filesdna, "View3DOverlay", "float", "gpencil_paper_color[3]")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->overlay.gpencil_paper_opacity = 0.5f;
              v3d->overlay.gpencil_grid_opacity = 0.9f;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 6)) {
    if (DNA_struct_elem_find(fd->filesdna, "SpaceOutliner", "int", "filter") == false) {
      bScreen *screen;
      ScrArea *area;
      SpaceLink *sl;

      /* Update files using invalid (outdated) outlinevis Outliner values. */
      for (screen = bmain->screens.first; screen; screen = screen->id.next) {
        for (area = screen->areabase.first; area; area = area->next) {
          for (sl = area->spacedata.first; sl; sl = sl->next) {
            if (sl->spacetype == SPACE_OUTLINER) {
              SpaceOutliner *so = (SpaceOutliner *)sl;

              if (!ELEM(so->outlinevis,
                        SO_SCENES,
                        SO_LIBRARIES,
                        SO_SEQUENCE,
                        SO_DATA_API,
                        SO_ID_ORPHANS)) {
                so->outlinevis = SO_VIEW_LAYER;
              }
            }
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "LightProbe", "float", "intensity")) {
      for (LightProbe *probe = bmain->lightprobes.first; probe; probe = probe->id.next) {
        probe->intensity = 1.0f;
      }
    }

    for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
      bConstraint *con, *con_next;
      con = ob->constraints.first;
      while (con) {
        con_next = con->next;
        if (con->type == 17) { /* CONSTRAINT_TYPE_RIGIDBODYJOINT */
          BLI_remlink(&ob->constraints, con);
          BKE_constraint_free_data(con);
          MEM_freeN(con);
        }
        con = con_next;
      }
    }

    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            v3d->shading.light = V3D_LIGHTING_STUDIO;
            v3d->shading.flag |= V3D_SHADING_OBJECT_OUTLINE;

            /* Assume (demo) files written with 2.8 want to show
             * Eevee renders in the viewport. */
            if (MAIN_VERSION_ATLEAST(bmain, 280, 0)) {
              v3d->drawtype = OB_MATERIAL;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 7)) {
    /* Render engine storage moved elsewhere and back during 2.8
     * development, we assume any files saved in 2.8 had Eevee set
     * as scene render engine. */
    if (MAIN_VERSION_ATLEAST(bmain, 280, 0)) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        BLI_strncpy(scene->r.engine, RE_engine_id_BLENDER_EEVEE, sizeof(scene->r.engine));
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 8)) {
    /* Blender Internal removal */
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      if (STREQ(scene->r.engine, "BLENDER_RENDER") || STREQ(scene->r.engine, "BLENDER_GAME")) {
        BLI_strncpy(scene->r.engine, RE_engine_id_BLENDER_EEVEE, sizeof(scene->r.engine));
      }

      scene->r.bake_mode = 0;
    }

    for (Tex *tex = bmain->textures.first; tex; tex = tex->id.next) {
      /* Removed envmap, pointdensity, voxeldata, ocean textures. */
      if (ELEM(tex->type, 10, 14, 15, 16)) {
        tex->type = 0;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 11)) {

    /* Remove info editor, but only if at the top of the window. */
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      /* Calculate window width/height from screen vertices */
      int win_width = 0, win_height = 0;
      LISTBASE_FOREACH (ScrVert *, vert, &screen->vertbase) {
        win_width = MAX2(win_width, vert->vec.x);
        win_height = MAX2(win_height, vert->vec.y);
      }

      for (ScrArea *area = screen->areabase.first, *area_next; area; area = area_next) {
        area_next = area->next;

        if (area->spacetype == SPACE_INFO) {
          if ((area->v2->vec.y == win_height) && (area->v1->vec.x == 0) &&
              (area->v4->vec.x == win_width)) {
            BKE_screen_area_free(area);

            BLI_remlink(&screen->areabase, area);

            BKE_screen_remove_double_scredges(screen);
            BKE_screen_remove_unused_scredges(screen);
            BKE_screen_remove_unused_scrverts(screen);

            MEM_freeN(area);
          }
        }
        /* AREA_TEMP_INFO is deprecated from now on, it should only be set for info areas
         * which are deleted above, so don't need to unset it. Its slot/bit can be reused */
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 11)) {
    for (Light *la = bmain->lights.first; la; la = la->id.next) {
      if (la->mode & (1 << 13)) { /* LA_SHAD_RAY */
        la->mode |= LA_SHADOW;
        la->mode &= ~(1 << 13);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 12)) {
    /* Remove tool property regions. */
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (ELEM(sl->spacetype, SPACE_VIEW3D, SPACE_CLIP)) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;

            for (ARegion *region = regionbase->first, *region_next; region; region = region_next) {
              region_next = region->next;

              if (region->regiontype == RGN_TYPE_TOOL_PROPS) {
                BKE_area_region_free(NULL, region);
                BLI_freelinkN(regionbase, region);
              }
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 13)) {
    /* Initialize specular factor. */
    if (!DNA_struct_elem_find(fd->filesdna, "Light", "float", "spec_fac")) {
      for (Light *la = bmain->lights.first; la; la = la->id.next) {
        la->spec_fac = 1.0f;
      }
    }

    /* Initialize new view3D options. */
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            v3d->shading.light = V3D_LIGHTING_STUDIO;
            v3d->shading.color_type = V3D_SHADING_MATERIAL_COLOR;
            copy_v3_fl(v3d->shading.single_color, 0.8f);
            v3d->shading.shadow_intensity = 0.5;

            v3d->overlay.backwire_opacity = 0.5f;
            v3d->overlay.normals_length = 0.1f;
            v3d->overlay.flag = 0;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 14)) {
    if (!DNA_struct_elem_find(fd->filesdna, "Scene", "SceneDisplay", "display")) {
      /* Initialize new scene.SceneDisplay */
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        copy_v3_v3(scene->display.light_direction, (float[3]){-M_SQRT1_3, -M_SQRT1_3, M_SQRT1_3});
      }
    }
    if (!DNA_struct_elem_find(fd->filesdna, "SceneDisplay", "float", "shadow_shift")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        scene->display.shadow_shift = 0.1;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "ToolSettings", "char", "transform_pivot_point")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        scene->toolsettings->transform_pivot_point = V3D_AROUND_CENTER_MEDIAN;
      }
    }

    if (!DNA_struct_find(fd->filesdna, "SceneEEVEE")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        /* First set the default for all the properties. */

        scene->eevee.gi_diffuse_bounces = 3;
        scene->eevee.gi_cubemap_resolution = 512;
        scene->eevee.gi_visibility_resolution = 32;

        scene->eevee.taa_samples = 16;
        scene->eevee.taa_render_samples = 64;

        scene->eevee.sss_samples = 7;
        scene->eevee.sss_jitter_threshold = 0.3f;

        scene->eevee.ssr_quality = 0.25f;
        scene->eevee.ssr_max_roughness = 0.5f;
        scene->eevee.ssr_thickness = 0.2f;
        scene->eevee.ssr_border_fade = 0.075f;
        scene->eevee.ssr_firefly_fac = 10.0f;

        scene->eevee.volumetric_start = 0.1f;
        scene->eevee.volumetric_end = 100.0f;
        scene->eevee.volumetric_tile_size = 8;
        scene->eevee.volumetric_samples = 64;
        scene->eevee.volumetric_sample_distribution = 0.8f;
        scene->eevee.volumetric_light_clamp = 0.0f;
        scene->eevee.volumetric_shadow_samples = 16;

        scene->eevee.gtao_distance = 0.2f;
        scene->eevee.gtao_factor = 1.0f;
        scene->eevee.gtao_quality = 0.25f;

        scene->eevee.bokeh_max_size = 100.0f;
        scene->eevee.bokeh_threshold = 1.0f;

        copy_v3_fl(scene->eevee.bloom_color, 1.0f);
        scene->eevee.bloom_threshold = 0.8f;
        scene->eevee.bloom_knee = 0.5f;
        scene->eevee.bloom_intensity = 0.05f;
        scene->eevee.bloom_radius = 6.5f;
        scene->eevee.bloom_clamp = 0.0f;

        scene->eevee.motion_blur_samples = 8;
        scene->eevee.motion_blur_shutter = 0.5f;

        scene->eevee.shadow_method = SHADOW_ESM;
        scene->eevee.shadow_cube_size = 512;
        scene->eevee.shadow_cascade_size = 1024;

        scene->eevee.flag = SCE_EEVEE_VOLUMETRIC_LIGHTS | SCE_EEVEE_GTAO_BENT_NORMALS |
                            SCE_EEVEE_GTAO_BOUNCE | SCE_EEVEE_TAA_REPROJECTION |
                            SCE_EEVEE_SSR_HALF_RESOLUTION;

        /* If the file is pre-2.80 move on. */
        if (scene->layer_properties == NULL) {
          continue;
        }

        /* Now we handle eventual properties that may be set in the file. */
#define EEVEE_GET_BOOL(_props, _name, _flag) \
  { \
    IDProperty *_idprop = IDP_GetPropertyFromGroup(_props, #_name); \
    if (_idprop != NULL) { \
      const int _value = IDP_Int(_idprop); \
      if (_value) { \
        scene->eevee.flag |= _flag; \
      } \
      else { \
        scene->eevee.flag &= ~_flag; \
      } \
    } \
  } \
  ((void)0)

#define EEVEE_GET_INT(_props, _name) \
  { \
    IDProperty *_idprop = IDP_GetPropertyFromGroup(_props, #_name); \
    if (_idprop != NULL) { \
      scene->eevee._name = IDP_Int(_idprop); \
    } \
  } \
  ((void)0)

#define EEVEE_GET_FLOAT(_props, _name) \
  { \
    IDProperty *_idprop = IDP_GetPropertyFromGroup(_props, #_name); \
    if (_idprop != NULL) { \
      scene->eevee._name = IDP_Float(_idprop); \
    } \
  } \
  ((void)0)

#define EEVEE_GET_FLOAT_ARRAY(_props, _name, _length) \
  { \
    IDProperty *_idprop = IDP_GetPropertyFromGroup(_props, #_name); \
    if (_idprop != NULL) { \
      const float *_values = IDP_Array(_idprop); \
      for (int _i = 0; _i < _length; _i++) { \
        scene->eevee._name[_i] = _values[_i]; \
      } \
    } \
  } \
  ((void)0)
        const int SCE_EEVEE_DOF_ENABLED = (1 << 7);
        IDProperty *props = IDP_GetPropertyFromGroup(scene->layer_properties,
                                                     RE_engine_id_BLENDER_EEVEE);
        // EEVEE_GET_BOOL(props, volumetric_enable, SCE_EEVEE_VOLUMETRIC_ENABLED);
        EEVEE_GET_BOOL(props, volumetric_lights, SCE_EEVEE_VOLUMETRIC_LIGHTS);
        EEVEE_GET_BOOL(props, volumetric_shadows, SCE_EEVEE_VOLUMETRIC_SHADOWS);
        EEVEE_GET_BOOL(props, gtao_enable, SCE_EEVEE_GTAO_ENABLED);
        EEVEE_GET_BOOL(props, gtao_use_bent_normals, SCE_EEVEE_GTAO_BENT_NORMALS);
        EEVEE_GET_BOOL(props, gtao_bounce, SCE_EEVEE_GTAO_BOUNCE);
        EEVEE_GET_BOOL(props, dof_enable, SCE_EEVEE_DOF_ENABLED);
        EEVEE_GET_BOOL(props, bloom_enable, SCE_EEVEE_BLOOM_ENABLED);
        EEVEE_GET_BOOL(props, motion_blur_enable, SCE_EEVEE_MOTION_BLUR_ENABLED);
        EEVEE_GET_BOOL(props, shadow_high_bitdepth, SCE_EEVEE_SHADOW_HIGH_BITDEPTH);
        EEVEE_GET_BOOL(props, taa_reprojection, SCE_EEVEE_TAA_REPROJECTION);
        // EEVEE_GET_BOOL(props, sss_enable, SCE_EEVEE_SSS_ENABLED);
        // EEVEE_GET_BOOL(props, sss_separate_albedo, SCE_EEVEE_SSS_SEPARATE_ALBEDO);
        EEVEE_GET_BOOL(props, ssr_enable, SCE_EEVEE_SSR_ENABLED);
        EEVEE_GET_BOOL(props, ssr_refraction, SCE_EEVEE_SSR_REFRACTION);
        EEVEE_GET_BOOL(props, ssr_halfres, SCE_EEVEE_SSR_HALF_RESOLUTION);

        EEVEE_GET_INT(props, gi_diffuse_bounces);
        EEVEE_GET_INT(props, gi_diffuse_bounces);
        EEVEE_GET_INT(props, gi_cubemap_resolution);
        EEVEE_GET_INT(props, gi_visibility_resolution);

        EEVEE_GET_INT(props, taa_samples);
        EEVEE_GET_INT(props, taa_render_samples);

        EEVEE_GET_INT(props, sss_samples);
        EEVEE_GET_FLOAT(props, sss_jitter_threshold);

        EEVEE_GET_FLOAT(props, ssr_quality);
        EEVEE_GET_FLOAT(props, ssr_max_roughness);
        EEVEE_GET_FLOAT(props, ssr_thickness);
        EEVEE_GET_FLOAT(props, ssr_border_fade);
        EEVEE_GET_FLOAT(props, ssr_firefly_fac);

        EEVEE_GET_FLOAT(props, volumetric_start);
        EEVEE_GET_FLOAT(props, volumetric_end);
        EEVEE_GET_INT(props, volumetric_tile_size);
        EEVEE_GET_INT(props, volumetric_samples);
        EEVEE_GET_FLOAT(props, volumetric_sample_distribution);
        EEVEE_GET_FLOAT(props, volumetric_light_clamp);
        EEVEE_GET_INT(props, volumetric_shadow_samples);

        EEVEE_GET_FLOAT(props, gtao_distance);
        EEVEE_GET_FLOAT(props, gtao_factor);
        EEVEE_GET_FLOAT(props, gtao_quality);

        EEVEE_GET_FLOAT(props, bokeh_max_size);
        EEVEE_GET_FLOAT(props, bokeh_threshold);

        EEVEE_GET_FLOAT_ARRAY(props, bloom_color, 3);
        EEVEE_GET_FLOAT(props, bloom_threshold);
        EEVEE_GET_FLOAT(props, bloom_knee);
        EEVEE_GET_FLOAT(props, bloom_intensity);
        EEVEE_GET_FLOAT(props, bloom_radius);
        EEVEE_GET_FLOAT(props, bloom_clamp);

        EEVEE_GET_INT(props, motion_blur_samples);
        EEVEE_GET_FLOAT(props, motion_blur_shutter);

        EEVEE_GET_INT(props, shadow_method);
        EEVEE_GET_INT(props, shadow_cube_size);
        EEVEE_GET_INT(props, shadow_cascade_size);

        /* Cleanup. */
        IDP_FreeProperty(scene->layer_properties);
        scene->layer_properties = NULL;

#undef EEVEE_GET_FLOAT_ARRAY
#undef EEVEE_GET_FLOAT
#undef EEVEE_GET_INT
#undef EEVEE_GET_BOOL
      }
    }

    if (!MAIN_VERSION_ATLEAST(bmain, 280, 15)) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        scene->display.matcap_ssao_distance = 0.2f;
        scene->display.matcap_ssao_attenuation = 1.0f;
        scene->display.matcap_ssao_samples = 16;
      }

      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_OUTLINER) {
              SpaceOutliner *soops = (SpaceOutliner *)sl;
              soops->filter_id_type = ID_GR;
              soops->outlinevis = SO_VIEW_LAYER;
            }
          }
        }
      }

      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        switch (scene->toolsettings->snap_mode) {
          case 0:
            scene->toolsettings->snap_mode = SCE_SNAP_MODE_INCREMENT;
            break;
          case 1:
            scene->toolsettings->snap_mode = SCE_SNAP_MODE_VERTEX;
            break;
          case 2:
            scene->toolsettings->snap_mode = SCE_SNAP_MODE_EDGE;
            break;
          case 3:
            scene->toolsettings->snap_mode = SCE_SNAP_MODE_FACE;
            break;
          case 4:
            scene->toolsettings->snap_mode = SCE_SNAP_MODE_VOLUME;
            break;
        }
        switch (scene->toolsettings->snap_node_mode) {
          case 5:
            scene->toolsettings->snap_node_mode = SCE_SNAP_MODE_NODE_X;
            break;
          case 6:
            scene->toolsettings->snap_node_mode = SCE_SNAP_MODE_NODE_Y;
            break;
          case 7:
            scene->toolsettings->snap_node_mode = SCE_SNAP_MODE_NODE_X | SCE_SNAP_MODE_NODE_Y;
            break;
          case 8:
            scene->toolsettings->snap_node_mode = SCE_SNAP_MODE_GRID;
            break;
        }
        switch (scene->toolsettings->snap_uv_mode) {
          case 0:
            scene->toolsettings->snap_uv_mode = SCE_SNAP_MODE_INCREMENT;
            break;
          case 1:
            scene->toolsettings->snap_uv_mode = SCE_SNAP_MODE_VERTEX;
            break;
        }
      }

      ParticleSettings *part;
      for (part = bmain->particles.first; part; part = part->id.next) {
        part->shape_flag = PART_SHAPE_CLOSE_TIP;
        part->shape = 0.0f;
        part->rad_root = 1.0f;
        part->rad_tip = 0.0f;
        part->rad_scale = 0.01f;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 18)) {
    if (!DNA_struct_elem_find(fd->filesdna, "Material", "float", "roughness")) {
      for (Material *mat = bmain->materials.first; mat; mat = mat->id.next) {
        if (mat->use_nodes) {
          if (MAIN_VERSION_ATLEAST(bmain, 280, 0)) {
            mat->roughness = mat->gloss_mir;
          }
          else {
            mat->roughness = 0.25f;
          }
        }
        else {
          mat->roughness = 1.0f - mat->gloss_mir;
        }
        mat->metallic = mat->ray_mirror;
      }

      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->shading.flag |= V3D_SHADING_SPECULAR_HIGHLIGHT;
            }
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "View3DShading", "float", "xray_alpha")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->shading.xray_alpha = 0.5f;
            }
          }
        }
      }
    }
    if (!DNA_struct_elem_find(fd->filesdna, "View3DShading", "char", "matcap[256]")) {
      StudioLight *default_matcap = BKE_studiolight_find_default(STUDIOLIGHT_TYPE_MATCAP);
      /* when loading the internal file is loaded before the matcaps */
      if (default_matcap) {
        for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
          LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
            LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
              if (sl->spacetype == SPACE_VIEW3D) {
                View3D *v3d = (View3D *)sl;
                BLI_strncpy(v3d->shading.matcap, default_matcap->name, FILE_MAXFILE);
              }
            }
          }
        }
      }
    }
    if (!DNA_struct_elem_find(fd->filesdna, "View3DOverlay", "float", "wireframe_threshold")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->overlay.wireframe_threshold = 0.5f;
            }
          }
        }
      }
    }
    if (!DNA_struct_elem_find(fd->filesdna, "View3DShading", "float", "cavity_valley_factor")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->shading.cavity_valley_factor = 1.0f;
              v3d->shading.cavity_ridge_factor = 1.0f;
            }
          }
        }
      }
    }
    if (!DNA_struct_elem_find(fd->filesdna, "View3DOverlay", "float", "xray_alpha_bone")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->overlay.xray_alpha_bone = 0.5f;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 19)) {
    if (!DNA_struct_elem_find(fd->filesdna, "Image", "ListBase", "renderslot")) {
      for (Image *ima = bmain->images.first; ima; ima = ima->id.next) {
        if (ima->type == IMA_TYPE_R_RESULT) {
          for (int i = 0; i < 8; i++) {
            RenderSlot *slot = MEM_callocN(sizeof(RenderSlot), "Image Render Slot Init");
            BLI_snprintf(slot->name, sizeof(slot->name), "Slot %d", i + 1);
            BLI_addtail(&ima->renderslots, slot);
          }
        }
      }
    }
    if (!DNA_struct_elem_find(fd->filesdna, "SpaceAction", "char", "mode_prev")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_ACTION) {
              SpaceAction *saction = (SpaceAction *)sl;
              /* "Dopesheet" should be default here,
               * unless it looks like the Action Editor was active instead. */
              if ((saction->mode_prev == 0) && (saction->action == NULL)) {
                saction->mode_prev = SACTCONT_DOPESHEET;
              }
            }
          }
        }
      }
    }

    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            if (v3d->drawtype == OB_TEXTURE) {
              v3d->drawtype = OB_SOLID;
              v3d->shading.light = V3D_LIGHTING_STUDIO;
              v3d->shading.color_type = V3D_SHADING_TEXTURE_COLOR;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 21)) {
    for (Scene *sce = bmain->scenes.first; sce != NULL; sce = sce->id.next) {
      if (sce->ed != NULL && sce->ed->seqbase.first != NULL) {
        do_versions_seq_unique_name_all_strips(sce, &sce->ed->seqbase);
      }
    }

    if (!DNA_struct_elem_find(
            fd->filesdna, "View3DOverlay", "float", "texture_paint_mode_opacity")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              enum {
                V3D_SHOW_MODE_SHADE_OVERRIDE = (1 << 15),
              };
              View3D *v3d = (View3D *)sl;
              float alpha = (v3d->flag2 & V3D_SHOW_MODE_SHADE_OVERRIDE) ? 0.0f : 1.0f;
              v3d->overlay.texture_paint_mode_opacity = alpha;
              v3d->overlay.vertex_paint_mode_opacity = alpha;
              v3d->overlay.weight_paint_mode_opacity = alpha;
            }
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "View3DShading", "char", "background_type")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              copy_v3_fl(v3d->shading.background_color, 0.05f);
            }
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "SceneEEVEE", "float", "gi_cubemap_draw_size")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        scene->eevee.gi_irradiance_draw_size = 0.1f;
        scene->eevee.gi_cubemap_draw_size = 0.3f;
      }
    }

    if (!DNA_struct_elem_find(
            fd->filesdna, "RigidBodyWorld", "RigidBodyWorld_Shared", "*shared")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        RigidBodyWorld *rbw = scene->rigidbody_world;

        if (rbw == NULL) {
          continue;
        }

        if (rbw->shared == NULL) {
          rbw->shared = MEM_callocN(sizeof(*rbw->shared), "RigidBodyWorld_Shared");
        }

        /* Move shared pointers from deprecated location to current location */
        rbw->shared->pointcache = rbw->pointcache;
        rbw->shared->ptcaches = rbw->ptcaches;

        rbw->pointcache = NULL;
        BLI_listbase_clear(&rbw->ptcaches);

        if (rbw->shared->pointcache == NULL) {
          rbw->shared->pointcache = BKE_ptcache_add(&(rbw->shared->ptcaches));
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "SoftBody", "SoftBody_Shared", "*shared")) {
      for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
        SoftBody *sb = ob->soft;
        if (sb == NULL) {
          continue;
        }
        if (sb->shared == NULL) {
          sb->shared = MEM_callocN(sizeof(*sb->shared), "SoftBody_Shared");
        }

        /* Move shared pointers from deprecated location to current location */
        sb->shared->pointcache = sb->pointcache;
        sb->shared->ptcaches = sb->ptcaches;

        sb->pointcache = NULL;
        BLI_listbase_clear(&sb->ptcaches);
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "View3DShading", "short", "type")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              if (v3d->drawtype == OB_RENDER) {
                v3d->drawtype = OB_SOLID;
              }
              v3d->shading.type = v3d->drawtype;
              v3d->shading.prev_type = OB_SOLID;
            }
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "SceneDisplay", "View3DShading", "shading")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        BKE_screen_view3d_shading_init(&scene->display.shading);
      }
    }
    /* initialize grease pencil view data */
    if (!DNA_struct_elem_find(fd->filesdna, "SpaceView3D", "float", "vertex_opacity")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->vertex_opacity = 1.0f;
              v3d->gp_flag |= V3D_GP_SHOW_EDIT_LINES;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 22)) {
    if (!DNA_struct_elem_find(fd->filesdna, "ToolSettings", "char", "annotate_v3d_align")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        scene->toolsettings->annotate_v3d_align = GP_PROJECT_VIEWSPACE | GP_PROJECT_CURSOR;
        scene->toolsettings->annotate_thickness = 3;
      }
    }
    if (!DNA_struct_elem_find(fd->filesdna, "bGPDlayer", "short", "line_change")) {
      for (bGPdata *gpd = bmain->gpencils.first; gpd; gpd = gpd->id.next) {
        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          gpl->line_change = gpl->thickness;
          if ((gpl->thickness < 1) || (gpl->thickness > 10)) {
            gpl->thickness = 3;
          }
        }
      }
    }
    if (!DNA_struct_elem_find(fd->filesdna, "View3DOverlay", "float", "gpencil_paper_opacity")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->overlay.gpencil_paper_opacity = 0.5f;
            }
          }
        }
      }
    }
    if (!DNA_struct_elem_find(fd->filesdna, "View3DOverlay", "float", "gpencil_grid_opacity")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->overlay.gpencil_grid_opacity = 0.5f;
            }
          }
        }
      }
    }

    /* default loc axis */
    if (!DNA_struct_elem_find(fd->filesdna, "GP_Sculpt_Settings", "int", "lock_axis")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        /* lock axis */
        GP_Sculpt_Settings *gset = &scene->toolsettings->gp_sculpt;
        if (gset) {
          gset->lock_axis = GP_LOCKAXIS_Y;
        }
      }
    }

    /* Versioning code for Subsurf modifier. */
    if (!DNA_struct_elem_find(fd->filesdna, "SubsurfModifier", "short", "uv_smooth")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
          if (md->type == eModifierType_Subsurf) {
            SubsurfModifierData *smd = (SubsurfModifierData *)md;
            if (smd->flags & eSubsurfModifierFlag_SubsurfUv_DEPRECATED) {
              smd->uv_smooth = SUBSURF_UV_SMOOTH_PRESERVE_CORNERS;
            }
            else {
              smd->uv_smooth = SUBSURF_UV_SMOOTH_NONE;
            }
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "SubsurfModifier", "short", "quality")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
          if (md->type == eModifierType_Subsurf) {
            SubsurfModifierData *smd = (SubsurfModifierData *)md;
            smd->quality = min_ii(smd->renderLevels, 3);
          }
        }
      }
    }
    /* Versioning code for Multires modifier. */
    if (!DNA_struct_elem_find(fd->filesdna, "MultiresModifier", "short", "quality")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
          if (md->type == eModifierType_Multires) {
            MultiresModifierData *mmd = (MultiresModifierData *)md;
            mmd->quality = 3;
            if (mmd->flags & eMultiresModifierFlag_PlainUv_DEPRECATED) {
              mmd->uv_smooth = SUBSURF_UV_SMOOTH_NONE;
            }
            else {
              mmd->uv_smooth = SUBSURF_UV_SMOOTH_PRESERVE_CORNERS;
            }
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "ClothSimSettings", "short", "bending_model")) {
      for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          ClothModifierData *clmd = NULL;
          if (md->type == eModifierType_Cloth) {
            clmd = (ClothModifierData *)md;
          }
          else if (md->type == eModifierType_ParticleSystem) {
            ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;
            ParticleSystem *psys = psmd->psys;
            clmd = psys->clmd;
          }
          if (clmd != NULL) {
            clmd->sim_parms->bending_model = CLOTH_BENDING_LINEAR;
            clmd->sim_parms->tension = clmd->sim_parms->structural;
            clmd->sim_parms->compression = clmd->sim_parms->structural;
            clmd->sim_parms->shear = clmd->sim_parms->structural;
            clmd->sim_parms->max_tension = clmd->sim_parms->max_struct;
            clmd->sim_parms->max_compression = clmd->sim_parms->max_struct;
            clmd->sim_parms->max_shear = clmd->sim_parms->max_struct;
            clmd->sim_parms->vgroup_shear = clmd->sim_parms->vgroup_struct;
            clmd->sim_parms->tension_damp = clmd->sim_parms->Cdis;
            clmd->sim_parms->compression_damp = clmd->sim_parms->Cdis;
            clmd->sim_parms->shear_damp = clmd->sim_parms->Cdis;
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "BrushGpencilSettings", "float", "era_strength_f")) {
      for (Brush *brush = bmain->brushes.first; brush; brush = brush->id.next) {
        if (brush->gpencil_settings != NULL) {
          BrushGpencilSettings *gp = brush->gpencil_settings;
          if (gp->brush_type == GPAINT_TOOL_ERASE) {
            gp->era_strength_f = 100.0f;
            gp->era_thickness_f = 10.0f;
          }
        }
      }
    }

    for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_Cloth) {
          ClothModifierData *clmd = (ClothModifierData *)md;

          if (!(clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL)) {
            clmd->sim_parms->vgroup_mass = 0;
          }

          if (!(clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_SCALING)) {
            clmd->sim_parms->vgroup_struct = 0;
            clmd->sim_parms->vgroup_shear = 0;
            clmd->sim_parms->vgroup_bend = 0;
          }

          if (!(clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_SEW)) {
            clmd->sim_parms->shrink_min = 0.0f;
            clmd->sim_parms->vgroup_shrink = 0;
          }

          if (!(clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_ENABLED)) {
            clmd->coll_parms->flags &= ~CLOTH_COLLSETTINGS_FLAG_SELF;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 24)) {
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            v3d->overlay.edit_flag |= V3D_OVERLAY_EDIT_FACES | V3D_OVERLAY_EDIT_SEAMS |
                                      V3D_OVERLAY_EDIT_SHARP | V3D_OVERLAY_EDIT_FREESTYLE_EDGE |
                                      V3D_OVERLAY_EDIT_FREESTYLE_FACE | V3D_OVERLAY_EDIT_EDGES |
                                      V3D_OVERLAY_EDIT_CREASES | V3D_OVERLAY_EDIT_BWEIGHTS;
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "ShrinkwrapModifierData", "char", "shrinkMode")) {
      for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_Shrinkwrap) {
            ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;
            if (smd->shrinkOpts & MOD_SHRINKWRAP_KEEP_ABOVE_SURFACE) {
              smd->shrinkMode = MOD_SHRINKWRAP_ABOVE_SURFACE;
              smd->shrinkOpts &= ~MOD_SHRINKWRAP_KEEP_ABOVE_SURFACE;
            }
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "PartDeflect", "float", "pdef_cfrict")) {
      for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
        if (ob->pd) {
          ob->pd->pdef_cfrict = 5.0f;
        }

        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_Cloth) {
            ClothModifierData *clmd = (ClothModifierData *)md;

            clmd->coll_parms->selfepsilon = 0.015f;
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "View3DShading", "float", "xray_alpha_wire")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->shading.flag |= V3D_SHADING_XRAY_WIREFRAME;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 25)) {
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      UnitSettings *unit = &scene->unit;
      if (unit->system != USER_UNIT_NONE) {
        unit->length_unit = bUnit_GetBaseUnitOfType(scene->unit.system, B_UNIT_LENGTH);
        unit->mass_unit = bUnit_GetBaseUnitOfType(scene->unit.system, B_UNIT_MASS);
      }
      unit->time_unit = bUnit_GetBaseUnitOfType(USER_UNIT_NONE, B_UNIT_TIME);
    }

    /* gpencil grid settings */
    for (bGPdata *gpd = bmain->gpencils.first; gpd; gpd = gpd->id.next) {
      ARRAY_SET_ITEMS(gpd->grid.color, 0.5f, 0.5f, 0.5f);  // Color
      ARRAY_SET_ITEMS(gpd->grid.scale, 1.0f, 1.0f);        // Scale
      gpd->grid.lines = GP_DEFAULT_GRID_LINES;             // Number of lines
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 28)) {
    for (Mesh *mesh = bmain->meshes.first; mesh; mesh = mesh->id.next) {
      BKE_mesh_calc_edges_loose(mesh);
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 29)) {
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            enum { V3D_OCCLUDE_WIRE = (1 << 14) };
            View3D *v3d = (View3D *)sl;
            if (v3d->flag2 & V3D_OCCLUDE_WIRE) {
              v3d->overlay.edit_flag |= V3D_OVERLAY_EDIT_OCCLUDE_WIRE;
              v3d->flag2 &= ~V3D_OCCLUDE_WIRE;
            }
          }
        }
      }
    }

    /* Files stored pre 2.5 (possibly re-saved with newer versions) may have non-visible
     * spaces without a header (visible/active ones are properly versioned).
     * Multiple version patches below assume there's always a header though. So inserting this
     * patch in-between older ones to add a header when needed.
     *
     * From here on it should be fine to assume there always is a header.
     */
    if (!MAIN_VERSION_ATLEAST(bmain, 283, 1)) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;
            ARegion *region_header = do_versions_find_region_or_null(regionbase, RGN_TYPE_HEADER);

            if (!region_header) {
              /* Headers should always be first in the region list, except if there's also a
               * tool-header. These were only introduced in later versions though, so should be
               * fine to always insert headers first. */
              BLI_assert(!do_versions_find_region_or_null(regionbase, RGN_TYPE_TOOL_HEADER));

              ARegion *region = do_versions_add_region(RGN_TYPE_HEADER,
                                                       "header 2.83.1 versioning");
              region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM :
                                                                    RGN_ALIGN_TOP;
              BLI_addhead(regionbase, region);
            }
          }
        }
      }
    }

    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_PROPERTIES) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;
            ARegion *region = MEM_callocN(sizeof(ARegion), "navigation bar for properties");
            ARegion *region_header = NULL;

            for (region_header = regionbase->first; region_header;
                 region_header = region_header->next) {
              if (region_header->regiontype == RGN_TYPE_HEADER) {
                break;
              }
            }
            BLI_assert(region_header);

            BLI_insertlinkafter(regionbase, region_header, region);

            region->regiontype = RGN_TYPE_NAV_BAR;
            region->alignment = RGN_ALIGN_LEFT;
          }
        }
      }
    }

    /* grease pencil fade layer opacity */
    if (!DNA_struct_elem_find(fd->filesdna, "View3DOverlay", "float", "gpencil_fade_layer")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->overlay.gpencil_fade_layer = 0.5f;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 30)) {
    /* grease pencil main material show switches */
    for (Material *mat = bmain->materials.first; mat; mat = mat->id.next) {
      if (mat->gp_style) {
        mat->gp_style->flag |= GP_MATERIAL_STROKE_SHOW;
        mat->gp_style->flag |= GP_MATERIAL_FILL_SHOW;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 33)) {

    if (!DNA_struct_elem_find(fd->filesdna, "SceneEEVEE", "float", "overscan")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        scene->eevee.overscan = 3.0f;
      }
    }

    for (Light *la = bmain->lights.first; la; la = la->id.next) {
      /* Removed Hemi lights. */
      if (!ELEM(la->type, LA_LOCAL, LA_SUN, LA_SPOT, LA_AREA)) {
        la->type = LA_SUN;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "SceneEEVEE", "float", "light_threshold")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        scene->eevee.light_threshold = 0.01f;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "SceneEEVEE", "float", "gi_irradiance_smoothing")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        scene->eevee.gi_irradiance_smoothing = 0.1f;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "SceneEEVEE", "float", "gi_filter_quality")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        scene->eevee.gi_filter_quality = 1.0f;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "Light", "float", "att_dist")) {
      for (Light *la = bmain->lights.first; la; la = la->id.next) {
        la->att_dist = la->clipend;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "Brush", "char", "weightpaint_tool")) {
      /* Magic defines from old files (2.7x) */

#define PAINT_BLEND_MIX 0
#define PAINT_BLEND_ADD 1
#define PAINT_BLEND_SUB 2
#define PAINT_BLEND_MUL 3
#define PAINT_BLEND_BLUR 4
#define PAINT_BLEND_LIGHTEN 5
#define PAINT_BLEND_DARKEN 6
#define PAINT_BLEND_AVERAGE 7
#define PAINT_BLEND_SMEAR 8
#define PAINT_BLEND_COLORDODGE 9
#define PAINT_BLEND_DIFFERENCE 10
#define PAINT_BLEND_SCREEN 11
#define PAINT_BLEND_HARDLIGHT 12
#define PAINT_BLEND_OVERLAY 13
#define PAINT_BLEND_SOFTLIGHT 14
#define PAINT_BLEND_EXCLUSION 15
#define PAINT_BLEND_LUMINOSITY 16
#define PAINT_BLEND_SATURATION 17
#define PAINT_BLEND_HUE 18
#define PAINT_BLEND_ALPHA_SUB 19
#define PAINT_BLEND_ALPHA_ADD 20

      for (Brush *brush = bmain->brushes.first; brush; brush = brush->id.next) {
        if (brush->ob_mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT)) {
          const char tool_init = brush->vertexpaint_tool;
          bool is_blend = false;

          {
            char tool = tool_init;
            switch (tool_init) {
              case PAINT_BLEND_MIX:
                tool = VPAINT_TOOL_DRAW;
                break;
              case PAINT_BLEND_BLUR:
                tool = VPAINT_TOOL_BLUR;
                break;
              case PAINT_BLEND_AVERAGE:
                tool = VPAINT_TOOL_AVERAGE;
                break;
              case PAINT_BLEND_SMEAR:
                tool = VPAINT_TOOL_SMEAR;
                break;
              default:
                tool = VPAINT_TOOL_DRAW;
                is_blend = true;
                break;
            }
            brush->vertexpaint_tool = tool;
          }

          if (is_blend == false) {
            brush->blend = IMB_BLEND_MIX;
          }
          else {
            short blend = IMB_BLEND_MIX;
            switch (tool_init) {
              case PAINT_BLEND_ADD:
                blend = IMB_BLEND_ADD;
                break;
              case PAINT_BLEND_SUB:
                blend = IMB_BLEND_SUB;
                break;
              case PAINT_BLEND_MUL:
                blend = IMB_BLEND_MUL;
                break;
              case PAINT_BLEND_LIGHTEN:
                blend = IMB_BLEND_LIGHTEN;
                break;
              case PAINT_BLEND_DARKEN:
                blend = IMB_BLEND_DARKEN;
                break;
              case PAINT_BLEND_COLORDODGE:
                blend = IMB_BLEND_COLORDODGE;
                break;
              case PAINT_BLEND_DIFFERENCE:
                blend = IMB_BLEND_DIFFERENCE;
                break;
              case PAINT_BLEND_SCREEN:
                blend = IMB_BLEND_SCREEN;
                break;
              case PAINT_BLEND_HARDLIGHT:
                blend = IMB_BLEND_HARDLIGHT;
                break;
              case PAINT_BLEND_OVERLAY:
                blend = IMB_BLEND_OVERLAY;
                break;
              case PAINT_BLEND_SOFTLIGHT:
                blend = IMB_BLEND_SOFTLIGHT;
                break;
              case PAINT_BLEND_EXCLUSION:
                blend = IMB_BLEND_EXCLUSION;
                break;
              case PAINT_BLEND_LUMINOSITY:
                blend = IMB_BLEND_LUMINOSITY;
                break;
              case PAINT_BLEND_SATURATION:
                blend = IMB_BLEND_SATURATION;
                break;
              case PAINT_BLEND_HUE:
                blend = IMB_BLEND_HUE;
                break;
              case PAINT_BLEND_ALPHA_SUB:
                blend = IMB_BLEND_ERASE_ALPHA;
                break;
              case PAINT_BLEND_ALPHA_ADD:
                blend = IMB_BLEND_ADD_ALPHA;
                break;
            }
            brush->blend = blend;
          }
        }
        /* For now these match, in the future new items may not. */
        brush->weightpaint_tool = brush->vertexpaint_tool;
      }

#undef PAINT_BLEND_MIX
#undef PAINT_BLEND_ADD
#undef PAINT_BLEND_SUB
#undef PAINT_BLEND_MUL
#undef PAINT_BLEND_BLUR
#undef PAINT_BLEND_LIGHTEN
#undef PAINT_BLEND_DARKEN
#undef PAINT_BLEND_AVERAGE
#undef PAINT_BLEND_SMEAR
#undef PAINT_BLEND_COLORDODGE
#undef PAINT_BLEND_DIFFERENCE
#undef PAINT_BLEND_SCREEN
#undef PAINT_BLEND_HARDLIGHT
#undef PAINT_BLEND_OVERLAY
#undef PAINT_BLEND_SOFTLIGHT
#undef PAINT_BLEND_EXCLUSION
#undef PAINT_BLEND_LUMINOSITY
#undef PAINT_BLEND_SATURATION
#undef PAINT_BLEND_HUE
#undef PAINT_BLEND_ALPHA_SUB
#undef PAINT_BLEND_ALPHA_ADD
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 34)) {
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, slink, &area->spacedata) {
          if (slink->spacetype == SPACE_USERPREF) {
            ARegion *navigation_region = BKE_spacedata_find_region_type(
                slink, area, RGN_TYPE_NAV_BAR);

            if (!navigation_region) {
              ARegion *main_region = BKE_spacedata_find_region_type(slink, area, RGN_TYPE_WINDOW);
              ListBase *regionbase = (slink == area->spacedata.first) ? &area->regionbase :
                                                                        &slink->regionbase;

              navigation_region = MEM_callocN(sizeof(ARegion),
                                              "userpref navigation-region do_versions");

              /* Order matters, addhead not addtail! */
              BLI_insertlinkbefore(regionbase, main_region, navigation_region);

              navigation_region->regiontype = RGN_TYPE_NAV_BAR;
              navigation_region->alignment = RGN_ALIGN_LEFT;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 36)) {
    if (!DNA_struct_elem_find(fd->filesdna, "View3DShading", "float", "curvature_ridge_factor")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->shading.curvature_ridge_factor = 1.0f;
              v3d->shading.curvature_valley_factor = 1.0f;
            }
          }
        }
      }
    }

    /* Rename OpenGL to Workbench. */
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      if (STREQ(scene->r.engine, "BLENDER_OPENGL")) {
        STRNCPY(scene->r.engine, RE_engine_id_BLENDER_WORKBENCH);
      }
    }

    /* init Annotations onion skin */
    if (!DNA_struct_elem_find(fd->filesdna, "bGPDlayer", "int", "gstep")) {
      for (bGPdata *gpd = bmain->gpencils.first; gpd; gpd = gpd->id.next) {
        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          ARRAY_SET_ITEMS(gpl->gcolor_prev, 0.302f, 0.851f, 0.302f);
          ARRAY_SET_ITEMS(gpl->gcolor_next, 0.250f, 0.1f, 1.0f);
        }
      }
    }

    /* Move studio_light selection to lookdev_light. */
    if (!DNA_struct_elem_find(fd->filesdna, "View3DShading", "char", "lookdev_light[256]")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              memcpy(v3d->shading.lookdev_light, v3d->shading.studio_light, sizeof(char) * 256);
            }
          }
        }
      }
    }

    /* Change Solid mode shadow orientation. */
    if (!DNA_struct_elem_find(fd->filesdna, "SceneDisplay", "float", "shadow_focus")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        float *dir = scene->display.light_direction;
        SWAP(float, dir[2], dir[1]);
        dir[2] = -dir[2];
        dir[0] = -dir[0];
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 37)) {
    for (Camera *ca = bmain->cameras.first; ca; ca = ca->id.next) {
      ca->drawsize *= 2.0f;
    }

    /* Grease pencil primitive curve */
    if (!DNA_struct_elem_find(
            fd->filesdna, "GP_Sculpt_Settings", "CurveMapping", "cur_primitive")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        GP_Sculpt_Settings *gset = &scene->toolsettings->gp_sculpt;
        if ((gset) && (gset->cur_primitive == NULL)) {
          gset->cur_primitive = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
          BKE_curvemapping_initialize(gset->cur_primitive);
          BKE_curvemap_reset(gset->cur_primitive->cm,
                             &gset->cur_primitive->clipr,
                             CURVE_PRESET_BELL,
                             CURVEMAP_SLOPE_POSITIVE);
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 38)) {
    if (DNA_struct_elem_find(fd->filesdna, "Object", "char", "empty_image_visibility_flag")) {
      for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
        ob->empty_image_visibility_flag ^= (OB_EMPTY_IMAGE_HIDE_PERSPECTIVE |
                                            OB_EMPTY_IMAGE_HIDE_ORTHOGRAPHIC |
                                            OB_EMPTY_IMAGE_HIDE_BACK);
      }
    }

    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          switch (sl->spacetype) {
            case SPACE_IMAGE: {
              SpaceImage *sima = (SpaceImage *)sl;
              sima->flag &= ~(SI_FLAG_UNUSED_0 | SI_FLAG_UNUSED_1 | SI_FLAG_UNUSED_3 |
                              SI_FLAG_UNUSED_6 | SI_FLAG_UNUSED_7 | SI_FLAG_UNUSED_8 |
                              SI_FLAG_UNUSED_17 | SI_FLAG_UNUSED_18 | SI_FLAG_UNUSED_23 |
                              SI_FLAG_UNUSED_24);
              break;
            }
            case SPACE_VIEW3D: {
              View3D *v3d = (View3D *)sl;
              v3d->flag &= ~(V3D_LOCAL_COLLECTIONS | V3D_FLAG_UNUSED_1 | V3D_FLAG_UNUSED_10 |
                             V3D_FLAG_UNUSED_12);
              v3d->flag2 &= ~(V3D_FLAG2_UNUSED_3 | V3D_FLAG2_UNUSED_6 | V3D_FLAG2_UNUSED_12 |
                              V3D_FLAG2_UNUSED_13 | V3D_FLAG2_UNUSED_14 | V3D_FLAG2_UNUSED_15);
              break;
            }
            case SPACE_OUTLINER: {
              SpaceOutliner *so = (SpaceOutliner *)sl;
              so->filter &= ~(SO_FILTER_UNUSED_1 | SO_FILTER_UNUSED_5 | SO_FILTER_UNUSED_12);
              so->storeflag &= ~(SO_TREESTORE_UNUSED_1);
              break;
            }
            case SPACE_FILE: {
              SpaceFile *sfile = (SpaceFile *)sl;
              if (sfile->params) {
                sfile->params->flag &= ~(FILE_PARAMS_FLAG_UNUSED_1 | FILE_PARAMS_FLAG_UNUSED_6 |
                                         FILE_PARAMS_FLAG_UNUSED_9);
              }
              break;
            }
            case SPACE_NODE: {
              SpaceNode *snode = (SpaceNode *)sl;
              snode->flag &= ~(SNODE_FLAG_UNUSED_6 | SNODE_FLAG_UNUSED_10 | SNODE_FLAG_UNUSED_11);
              break;
            }
            case SPACE_PROPERTIES: {
              SpaceProperties *sbuts = (SpaceProperties *)sl;
              sbuts->flag &= ~(SB_FLAG_UNUSED_2 | SB_FLAG_UNUSED_3);
              break;
            }
            case SPACE_NLA: {
              SpaceNla *snla = (SpaceNla *)sl;
              snla->flag &= ~(SNLA_FLAG_UNUSED_0 | SNLA_FLAG_UNUSED_1 | SNLA_FLAG_UNUSED_3);
              break;
            }
          }
        }
      }
    }

    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      scene->r.mode &= ~(R_MODE_UNUSED_1 | R_MODE_UNUSED_2 | R_MODE_UNUSED_3 | R_MODE_UNUSED_4 |
                         R_MODE_UNUSED_5 | R_MODE_UNUSED_6 | R_MODE_UNUSED_7 | R_MODE_UNUSED_8 |
                         R_MODE_UNUSED_10 | R_MODE_UNUSED_13 | R_MODE_UNUSED_16 |
                         R_MODE_UNUSED_17 | R_MODE_UNUSED_18 | R_MODE_UNUSED_19 |
                         R_MODE_UNUSED_20 | R_MODE_UNUSED_21 | R_MODE_UNUSED_27);

      scene->r.scemode &= ~(R_SCEMODE_UNUSED_8 | R_SCEMODE_UNUSED_11 | R_SCEMODE_UNUSED_13 |
                            R_SCEMODE_UNUSED_16 | R_SCEMODE_UNUSED_17 | R_SCEMODE_UNUSED_19);

      if (scene->toolsettings->sculpt) {
        scene->toolsettings->sculpt->flags &= ~(SCULPT_FLAG_UNUSED_0 | SCULPT_FLAG_UNUSED_1 |
                                                SCULPT_FLAG_UNUSED_2);
      }

      if (scene->ed) {
        Sequence *seq;
        SEQ_BEGIN (scene->ed, seq) {
          seq->flag &= ~(SEQ_FLAG_UNUSED_6 | SEQ_FLAG_UNUSED_18 | SEQ_FLAG_UNUSED_19 |
                         SEQ_FLAG_UNUSED_21);
          if (seq->type == SEQ_TYPE_SPEED) {
            SpeedControlVars *s = (SpeedControlVars *)seq->effectdata;
            s->flags &= ~(SEQ_SPEED_UNUSED_1);
          }
        }
        SEQ_END;
      }
    }

    for (World *world = bmain->worlds.first; world; world = world->id.next) {
      world->flag &= ~(WO_MODE_UNUSED_1 | WO_MODE_UNUSED_2 | WO_MODE_UNUSED_3 | WO_MODE_UNUSED_4 |
                       WO_MODE_UNUSED_5 | WO_MODE_UNUSED_7);
    }

    for (Image *image = bmain->images.first; image; image = image->id.next) {
      image->flag &= ~(IMA_HIGH_BITDEPTH | IMA_FLAG_UNUSED_1 | IMA_FLAG_UNUSED_4 |
                       IMA_FLAG_UNUSED_6 | IMA_FLAG_UNUSED_8 | IMA_FLAG_UNUSED_15 |
                       IMA_FLAG_UNUSED_16);
    }

    for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
      ob->flag &= ~(OB_FLAG_UNUSED_11 | OB_FLAG_UNUSED_12);
      ob->transflag &= ~(OB_TRANSFLAG_UNUSED_0 | OB_TRANSFLAG_UNUSED_1);
      ob->shapeflag &= ~OB_SHAPE_FLAG_UNUSED_1;
    }

    for (Mesh *me = bmain->meshes.first; me; me = me->id.next) {
      me->flag &= ~(ME_FLAG_UNUSED_0 | ME_FLAG_UNUSED_1 | ME_FLAG_UNUSED_3 | ME_FLAG_UNUSED_4 |
                    ME_FLAG_UNUSED_6 | ME_FLAG_UNUSED_7 | ME_FLAG_UNUSED_8);
    }

    for (Material *mat = bmain->materials.first; mat; mat = mat->id.next) {
      mat->blend_flag &= ~(1 << 2); /* UNUSED */
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 40)) {
    if (!DNA_struct_elem_find(fd->filesdna, "ToolSettings", "char", "snap_transform_mode_flag")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        scene->toolsettings->snap_transform_mode_flag = SCE_SNAP_TRANSFORM_MODE_TRANSLATE;
      }
    }

    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          switch (sl->spacetype) {
            case SPACE_VIEW3D: {
              enum { V3D_BACKFACE_CULLING = (1 << 10) };
              View3D *v3d = (View3D *)sl;
              if (v3d->flag2 & V3D_BACKFACE_CULLING) {
                v3d->flag2 &= ~V3D_BACKFACE_CULLING;
                v3d->shading.flag |= V3D_SHADING_BACKFACE_CULLING;
              }
              break;
            }
          }
        }
      }
    }

    if (!DNA_struct_find(fd->filesdna, "TransformOrientationSlot")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        for (int i = 0; i < ARRAY_SIZE(scene->orientation_slots); i++) {
          scene->orientation_slots[i].index_custom = -1;
        }
      }
    }

    /* Grease pencil cutter/select segment intersection threshold  */
    if (!DNA_struct_elem_find(fd->filesdna, "GP_Sculpt_Settings", "float", "isect_threshold")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        GP_Sculpt_Settings *gset = &scene->toolsettings->gp_sculpt;
        if (gset) {
          gset->isect_threshold = 0.1f;
        }
      }
    }

    /* Fix anamorphic bokeh eevee rna limits.*/
    for (Camera *ca = bmain->cameras.first; ca; ca = ca->id.next) {
      if (ca->gpu_dof.ratio < 0.01f) {
        ca->gpu_dof.ratio = 0.01f;
      }
    }

    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_USERPREF) {
            ARegion *execute_region = BKE_spacedata_find_region_type(sl, area, RGN_TYPE_EXECUTE);

            if (!execute_region) {
              ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                     &sl->regionbase;
              ARegion *region_navbar = BKE_spacedata_find_region_type(sl, area, RGN_TYPE_NAV_BAR);

              execute_region = MEM_callocN(sizeof(ARegion), "execute region for properties");

              BLI_assert(region_navbar);

              BLI_insertlinkafter(regionbase, region_navbar, execute_region);

              execute_region->regiontype = RGN_TYPE_EXECUTE;
              execute_region->alignment = RGN_ALIGN_BOTTOM | RGN_SPLIT_PREV;
              execute_region->flag |= RGN_FLAG_DYNAMIC_SIZE;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 43)) {
    ListBase *lb = which_libbase(bmain, ID_BR);
    BKE_main_id_repair_duplicate_names_listbase(lb);
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 44)) {
    if (!DNA_struct_elem_find(fd->filesdna, "Material", "float", "a")) {
      for (Material *mat = bmain->materials.first; mat; mat = mat->id.next) {
        mat->a = 1.0f;
      }
    }

    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      enum {
        R_ALPHAKEY = 2,
      };
      scene->r.seq_flag &= ~(R_SEQ_UNUSED_0 | R_SEQ_UNUSED_1 | R_SEQ_UNUSED_2);
      scene->r.color_mgt_flag &= ~R_COLOR_MANAGEMENT_UNUSED_1;
      if (scene->r.alphamode == R_ALPHAKEY) {
        scene->r.alphamode = R_ADDSKY;
      }
      ToolSettings *ts = scene->toolsettings;
      ts->particle.flag &= ~PE_UNUSED_6;
      if (ts->sculpt != NULL) {
        ts->sculpt->flags &= ~SCULPT_FLAG_UNUSED_6;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 46)) {
    /* Add wireframe color. */
    if (!DNA_struct_elem_find(fd->filesdna, "View3DShading", "char", "wire_color_type")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->shading.wire_color_type = V3D_SHADING_SINGLE_COLOR;
            }
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "View3DCursor", "short", "rotation_mode")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        if (is_zero_v3(scene->cursor.rotation_axis)) {
          scene->cursor.rotation_mode = ROT_MODE_XYZ;
          scene->cursor.rotation_quaternion[0] = 1.0f;
          scene->cursor.rotation_axis[1] = 1.0f;
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 47)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ParticleEditSettings *pset = &scene->toolsettings->particle;
      if (pset->brushtype < 0) {
        pset->brushtype = PE_BRUSH_COMB;
      }
    }

    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      {
        enum { PARCURVE = 1, PARKEY = 2, PAR_DEPRECATED = 16 };
        if (ELEM(ob->partype, PARCURVE, PARKEY, PAR_DEPRECATED)) {
          ob->partype = PAROBJECT;
        }
      }

      {
        enum { OB_WAVE = 21, OB_LIFE = 23, OB_SECTOR = 24 };
        if (ELEM(ob->type, OB_WAVE, OB_LIFE, OB_SECTOR)) {
          ob->type = OB_EMPTY;
        }
      }

      ob->transflag &= ~(OB_TRANSFLAG_UNUSED_0 | OB_TRANSFLAG_UNUSED_1 | OB_TRANSFLAG_UNUSED_3 |
                         OB_TRANSFLAG_UNUSED_6 | OB_TRANSFLAG_UNUSED_12);

      ob->nlaflag &= ~(OB_ADS_UNUSED_1 | OB_ADS_UNUSED_2);
    }

    LISTBASE_FOREACH (bArmature *, arm, &bmain->armatures) {
      arm->flag &= ~(ARM_FLAG_UNUSED_1 | ARM_FLAG_UNUSED_5 | ARM_FLAG_UNUSED_6 |
                     ARM_FLAG_UNUSED_7 | ARM_FLAG_UNUSED_12);
    }

    LISTBASE_FOREACH (Text *, text, &bmain->texts) {
      text->flags &= ~(TXT_FLAG_UNUSED_8 | TXT_FLAG_UNUSED_9);
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 48)) {
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      /* Those are not currently used, but are accessible through RNA API and were not
       * properly initialized previously. This is mere copy of BKE_init_scene() code. */
      if (scene->r.im_format.view_settings.look[0] == '\0') {
        BKE_color_managed_display_settings_init(&scene->r.im_format.display_settings);
        BKE_color_managed_view_settings_init_render(
            &scene->r.im_format.view_settings, &scene->r.im_format.display_settings, "Filmic");
      }

      if (scene->r.bake.im_format.view_settings.look[0] == '\0') {
        BKE_color_managed_display_settings_init(&scene->r.bake.im_format.display_settings);
        BKE_color_managed_view_settings_init_render(&scene->r.bake.im_format.view_settings,
                                                    &scene->r.bake.im_format.display_settings,
                                                    "Filmic");
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 49)) {
    /* All tool names changed, reset to defaults. */
    for (WorkSpace *workspace = bmain->workspaces.first; workspace;
         workspace = workspace->id.next) {
      while (!BLI_listbase_is_empty(&workspace->tools)) {
        BKE_workspace_tool_remove(workspace, workspace->tools.first);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 52)) {
    LISTBASE_FOREACH (ParticleSettings *, part, &bmain->particles) {
      /* Replace deprecated PART_DRAW_BB by PART_DRAW_NOT */
      if (part->ren_as == PART_DRAW_BB) {
        part->ren_as = PART_DRAW_NOT;
      }
      if (part->draw_as == PART_DRAW_BB) {
        part->draw_as = PART_DRAW_NOT;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "TriangulateModifierData", "int", "min_vertices")) {
      for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_Triangulate) {
            TriangulateModifierData *smd = (TriangulateModifierData *)md;
            smd->min_vertices = 4;
          }
        }
      }
    }

    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          /* Fix missing version patching from earlier changes. */
          if (STREQ(node->idname, "ShaderNodeOutputLamp")) {
            STRNCPY(node->idname, "ShaderNodeOutputLight");
          }
          if (node->type == SH_NODE_BSDF_PRINCIPLED && node->custom2 == 0) {
            node->custom2 = SHD_SUBSURFACE_BURLEY;
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 53)) {
    for (Material *mat = bmain->materials.first; mat; mat = mat->id.next) {
      /* Eevee: Keep material appearance consistent with previous behavior. */
      if (!mat->use_nodes || !mat->nodetree || mat->blend_method == MA_BM_SOLID) {
        mat->blend_shadow = MA_BS_SOLID;
      }
    }

    /* grease pencil default animation channel color */
    {
      for (bGPdata *gpd = bmain->gpencils.first; gpd; gpd = gpd->id.next) {
        if (gpd->flag & GP_DATA_ANNOTATIONS) {
          continue;
        }
        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          /* default channel color */
          ARRAY_SET_ITEMS(gpl->color, 0.2f, 0.2f, 0.2f);
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 54)) {
    for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
      bool is_first_subdiv = true;
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_Subsurf) {
          SubsurfModifierData *smd = (SubsurfModifierData *)md;
          if (is_first_subdiv) {
            smd->flags |= eSubsurfModifierFlag_UseCrease;
          }
          else {
            smd->flags &= ~eSubsurfModifierFlag_UseCrease;
          }
          is_first_subdiv = false;
        }
        else if (md->type == eModifierType_Multires) {
          MultiresModifierData *mmd = (MultiresModifierData *)md;
          if (is_first_subdiv) {
            mmd->flags |= eMultiresModifierFlag_UseCrease;
          }
          else {
            mmd->flags &= ~eMultiresModifierFlag_UseCrease;
          }
          is_first_subdiv = false;
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 55)) {
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_TEXT) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;

            /* Remove multiple footers that were added by mistake. */
            do_versions_remove_regions_by_type(regionbase, RGN_TYPE_FOOTER);

            /* Add footer. */
            ARegion *region = do_versions_add_region(RGN_TYPE_FOOTER, "footer for text");
            region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_TOP : RGN_ALIGN_BOTTOM;

            ARegion *region_header = do_versions_find_region(regionbase, RGN_TYPE_HEADER);
            BLI_insertlinkafter(regionbase, region_header, region);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 56)) {
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            v3d->gizmo_show_armature = V3D_GIZMO_SHOW_ARMATURE_BBONE |
                                       V3D_GIZMO_SHOW_ARMATURE_ROLL;
            v3d->gizmo_show_empty = V3D_GIZMO_SHOW_EMPTY_IMAGE | V3D_GIZMO_SHOW_EMPTY_FORCE_FIELD;
            v3d->gizmo_show_light = V3D_GIZMO_SHOW_LIGHT_SIZE | V3D_GIZMO_SHOW_LIGHT_LOOK_AT;
            v3d->gizmo_show_camera = V3D_GIZMO_SHOW_CAMERA_LENS | V3D_GIZMO_SHOW_CAMERA_DOF_DIST;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 57)) {
    /* Enable Show Interpolation in dopesheet by default. */
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_ACTION) {
            SpaceAction *saction = (SpaceAction *)sl;
            if ((saction->flag & SACTION_SHOW_EXTREMES) == 0) {
              saction->flag |= SACTION_SHOW_INTERPOLATION;
            }
          }
        }
      }
    }

    /* init grease pencil brush gradients */
    if (!DNA_struct_elem_find(fd->filesdna, "BrushGpencilSettings", "float", "hardeness")) {
      for (Brush *brush = bmain->brushes.first; brush; brush = brush->id.next) {
        if (brush->gpencil_settings != NULL) {
          BrushGpencilSettings *gp = brush->gpencil_settings;
          gp->hardeness = 1.0f;
          copy_v2_fl(gp->aspect_ratio, 1.0f);
        }
      }
    }

    /* init grease pencil stroke gradients */
    if (!DNA_struct_elem_find(fd->filesdna, "bGPDstroke", "float", "hardeness")) {
      for (bGPdata *gpd = bmain->gpencils.first; gpd; gpd = gpd->id.next) {
        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
            LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
              gps->hardeness = 1.0f;
              copy_v2_fl(gps->aspect_ratio, 1.0f);
            }
          }
        }
      }
    }

    /* enable the axis aligned ortho grid by default */
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            v3d->gridflag |= V3D_SHOW_ORTHO_GRID;
          }
        }
      }
    }
  }

  /* Keep un-versioned until we're finished adding space types. */
  {
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                 &sl->regionbase;
          /* All spaces that use tools must be eventually added. */
          ARegion *region = NULL;
          if (ELEM(sl->spacetype, SPACE_VIEW3D, SPACE_IMAGE, SPACE_SEQ) &&
              ((region = do_versions_find_region_or_null(regionbase, RGN_TYPE_TOOL_HEADER)) ==
               NULL)) {
            /* Add tool header. */
            region = do_versions_add_region(RGN_TYPE_TOOL_HEADER, "tool header");
            region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

            ARegion *region_header = do_versions_find_region(regionbase, RGN_TYPE_HEADER);
            BLI_insertlinkbefore(regionbase, region_header, region);
            /* Hide by default, enable for painting workspaces (startup only). */
            region->flag |= RGN_FLAG_HIDDEN | RGN_FLAG_HIDDEN_BY_USER;
          }
          if (region != NULL) {
            SET_FLAG_FROM_TEST(
                region->flag, region->flag & RGN_FLAG_HIDDEN_BY_USER, RGN_FLAG_HIDDEN);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 60)) {
    if (!DNA_struct_elem_find(fd->filesdna, "bSplineIKConstraint", "short", "yScaleMode")) {
      for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
        if (ob->pose) {
          LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
            LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
              if (con->type == CONSTRAINT_TYPE_SPLINEIK) {
                bSplineIKConstraint *data = (bSplineIKConstraint *)con->data;
                if ((data->flag & CONSTRAINT_SPLINEIK_SCALE_LIMITED) == 0) {
                  data->yScaleMode = CONSTRAINT_SPLINEIK_YS_FIT_CURVE;
                }
              }
            }
          }
        }
      }
    }

    if (!DNA_struct_elem_find(
            fd->filesdna, "View3DOverlay", "float", "sculpt_mode_mask_opacity")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->overlay.sculpt_mode_mask_opacity = 0.75f;
            }
          }
        }
      }
    }
    if (!DNA_struct_elem_find(fd->filesdna, "SceneDisplay", "char", "render_aa")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->display.render_aa = SCE_DISPLAY_AA_SAMPLES_8;
        scene->display.viewport_aa = SCE_DISPLAY_AA_FXAA;
      }
    }

    /* Split bbone_scalein/bbone_scaleout into x and y fields. */
    if (!DNA_struct_elem_find(fd->filesdna, "bPoseChannel", "float", "scale_out_y")) {
      /* Update armature data and pose channels. */
      LISTBASE_FOREACH (bArmature *, arm, &bmain->armatures) {
        do_version_bones_split_bbone_scale(&arm->bonebase);
      }

      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        if (ob->pose) {
          LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
            pchan->scale_in_y = pchan->scale_in_x;
            pchan->scale_out_y = pchan->scale_out_x;
          }
        }
      }

      /* Update action curves and drivers. */
      LISTBASE_FOREACH (bAction *, act, &bmain->actions) {
        LISTBASE_FOREACH_MUTABLE (FCurve *, fcu, &act->curves) {
          do_version_bbone_scale_fcurve_fix(&act->curves, fcu);
        }
      }

      BKE_animdata_main_cb(bmain, do_version_bbone_scale_animdata_cb, NULL);
    }

    for (Scene *sce = bmain->scenes.first; sce != NULL; sce = sce->id.next) {
      if (sce->ed != NULL) {
        do_versions_seq_set_cache_defaults(sce->ed);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 61)) {
    /* Added a power option to Copy Scale. */
    if (!DNA_struct_elem_find(fd->filesdna, "bSizeLikeConstraint", "float", "power")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        do_version_constraints_copy_scale_power(&ob->constraints);
        if (ob->pose) {
          LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
            do_version_constraints_copy_scale_power(&pchan->constraints);
          }
        }
      }
    }

    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (ELEM(sl->spacetype, SPACE_CLIP, SPACE_GRAPH, SPACE_SEQ)) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;

            ARegion *region = NULL;
            if (sl->spacetype == SPACE_CLIP) {
              if (((SpaceClip *)sl)->view == SC_VIEW_GRAPH) {
                region = do_versions_find_region_or_null(regionbase, RGN_TYPE_PREVIEW);
              }
            }
            else {
              region = do_versions_find_region_or_null(regionbase, RGN_TYPE_WINDOW);
            }

            if (region != NULL) {
              region->v2d.scroll &= ~V2D_SCROLL_LEFT;
              region->v2d.scroll |= V2D_SCROLL_RIGHT;
            }
          }
        }
      }
    }

    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype != SPACE_OUTLINER) {
            continue;
          }
          SpaceOutliner *so = (SpaceOutliner *)sl;
          so->filter &= ~SO_FLAG_UNUSED_1;
          so->show_restrict_flags = SO_RESTRICT_ENABLE | SO_RESTRICT_HIDE;
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 69)) {
    LISTBASE_FOREACH (bArmature *, arm, &bmain->armatures) {
      arm->flag &= ~(ARM_FLAG_UNUSED_7 | ARM_FLAG_UNUSED_9);
    }

    /* Initializes sun lights with the new angular diameter property */
    if (!DNA_struct_elem_find(fd->filesdna, "Light", "float", "sun_angle")) {
      LISTBASE_FOREACH (Light *, light, &bmain->lights) {
        light->sun_angle = 2.0f * atanf(light->area_size);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 70)) {
    /* New image alpha modes. */
    LISTBASE_FOREACH (Image *, image, &bmain->images) {
      const int IMA_IGNORE_ALPHA = (1 << 12);
      if (image->flag & IMA_IGNORE_ALPHA) {
        image->alpha_mode = IMA_ALPHA_IGNORE;
        image->flag &= ~IMA_IGNORE_ALPHA;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 71)) {
    /* This assumes the Blender builtin config. Depending on the OCIO
     * environment variable for versioning is weak, and these deprecated view
     * transforms and look names don't seem to exist in other commonly used
     * OCIO configs so .blend files created for those would be unaffected. */
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      ColorManagedViewSettings *view_settings;
      view_settings = &scene->view_settings;

      if (STREQ(view_settings->view_transform, "Default")) {
        STRNCPY(view_settings->view_transform, "Standard");
      }
      else if (STREQ(view_settings->view_transform, "RRT") ||
               STREQ(view_settings->view_transform, "Film")) {
        STRNCPY(view_settings->view_transform, "Filmic");
      }
      else if (STREQ(view_settings->view_transform, "Log")) {
        STRNCPY(view_settings->view_transform, "Filmic Log");
      }

      if (STREQ(view_settings->look, "Filmic - Base Contrast")) {
        STRNCPY(view_settings->look, "None");
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 74)) {
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      if (scene->ed != NULL) {
        do_versions_seq_alloc_transform_and_crop(&scene->ed->seqbase);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 75)) {
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      if (scene->master_collection != NULL) {
        scene->master_collection->flag &= ~(COLLECTION_RESTRICT_VIEWPORT |
                                            COLLECTION_RESTRICT_SELECT |
                                            COLLECTION_RESTRICT_RENDER);
      }

      UnitSettings *unit = &scene->unit;
      if (unit->system == USER_UNIT_NONE) {
        unit->length_unit = (char)USER_UNIT_ADAPTIVE;
        unit->mass_unit = (char)USER_UNIT_ADAPTIVE;
      }

      RenderData *render_data = &scene->r;
      switch (render_data->ffcodecdata.ffmpeg_preset) {
        case FFM_PRESET_ULTRAFAST:
        case FFM_PRESET_SUPERFAST:
          render_data->ffcodecdata.ffmpeg_preset = FFM_PRESET_REALTIME;
          break;
        case FFM_PRESET_VERYFAST:
        case FFM_PRESET_FASTER:
        case FFM_PRESET_FAST:
        case FFM_PRESET_MEDIUM:
          render_data->ffcodecdata.ffmpeg_preset = FFM_PRESET_GOOD;
          break;
        case FFM_PRESET_SLOW:
        case FFM_PRESET_SLOWER:
        case FFM_PRESET_VERYSLOW:
          render_data->ffcodecdata.ffmpeg_preset = FFM_PRESET_BEST;
      }
    }

    LISTBASE_FOREACH (bArmature *, arm, &bmain->armatures) {
      arm->flag &= ~(ARM_FLAG_UNUSED_6);
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 281, 1)) {
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_DataTransfer) {
          /* Now datatransfer's mix factor is multiplied with weights when any,
           * instead of being ignored,
           * we need to take care of that to keep 'old' files compatible. */
          DataTransferModifierData *dtmd = (DataTransferModifierData *)md;
          if (dtmd->defgrp_name[0] != '\0') {
            dtmd->mix_factor = 1.0f;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 281, 3)) {
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_TEXT) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;
            ARegion *region = do_versions_find_region_or_null(regionbase, RGN_TYPE_UI);
            if (region) {
              region->alignment = RGN_ALIGN_RIGHT;
            }
          }
          /* Mark outliners as dirty for syncing and enable synced selection */
          if (sl->spacetype == SPACE_OUTLINER) {
            SpaceOutliner *soutliner = (SpaceOutliner *)sl;
            soutliner->sync_select_dirty |= WM_OUTLINER_SYNC_SELECT_FROM_ALL;
            soutliner->flag |= SO_SYNC_SELECT;
          }
        }
      }
    }
    for (Mesh *mesh = bmain->meshes.first; mesh; mesh = mesh->id.next) {
      if (mesh->remesh_voxel_size == 0.0f) {
        mesh->remesh_voxel_size = 0.1f;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 281, 4)) {
    ID *id;
    FOREACH_MAIN_ID_BEGIN (bmain, id) {
      bNodeTree *ntree = ntreeFromID(id);
      if (ntree) {
        ntree->id.flag |= LIB_EMBEDDED_DATA;
      }
    }
    FOREACH_MAIN_ID_END;
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 281, 5)) {
    for (Brush *br = bmain->brushes.first; br; br = br->id.next) {
      if (br->ob_mode & OB_MODE_SCULPT && br->normal_radius_factor == 0.0f) {
        br->normal_radius_factor = 0.5f;
      }
    }

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      /* Older files do not have a master collection, which is then added through
       * `BKE_collection_master_add()`, so everything is fine. */
      if (scene->master_collection != NULL) {
        scene->master_collection->id.flag |= LIB_EMBEDDED_DATA;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 281, 6)) {
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            v3d->shading.flag |= V3D_SHADING_SCENE_LIGHTS_RENDER | V3D_SHADING_SCENE_WORLD_RENDER;

            /* files by default don't have studio lights selected unless interacted
             * with the shading popover. When no studio-light could be read, we will
             * select the default world one. */
            StudioLight *studio_light = BKE_studiolight_find(v3d->shading.lookdev_light,
                                                             STUDIOLIGHT_TYPE_WORLD);
            if (studio_light != NULL) {
              STRNCPY(v3d->shading.lookdev_light, studio_light->name);
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 281, 9)) {
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_FILE) {
            SpaceFile *sfile = (SpaceFile *)sl;
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;
            ARegion *region_ui = do_versions_find_region(regionbase, RGN_TYPE_UI);
            ARegion *region_header = do_versions_find_region(regionbase, RGN_TYPE_HEADER);
            ARegion *region_toolprops = do_versions_find_region_or_null(regionbase,
                                                                        RGN_TYPE_TOOL_PROPS);

            /* Reinsert UI region so that it spawns entire area width */
            BLI_remlink(regionbase, region_ui);
            BLI_insertlinkafter(regionbase, region_header, region_ui);

            region_ui->flag |= RGN_FLAG_DYNAMIC_SIZE;

            if (region_toolprops &&
                (region_toolprops->alignment == (RGN_ALIGN_BOTTOM | RGN_SPLIT_PREV))) {
              SpaceType *stype = BKE_spacetype_from_id(sl->spacetype);

              /* Remove empty region at old location. */
              BLI_assert(sfile->op == NULL);
              BKE_area_region_free(stype, region_toolprops);
              BLI_freelinkN(regionbase, region_toolprops);
            }

            if (sfile->params) {
              sfile->params->details_flags |= FILE_DETAILS_SIZE | FILE_DETAILS_DATETIME;
            }
          }
        }
      }
    }

    /* Convert the BONE_NO_SCALE flag to inherit_scale_mode enum. */
    if (!DNA_struct_elem_find(fd->filesdna, "Bone", "char", "inherit_scale_mode")) {
      LISTBASE_FOREACH (bArmature *, arm, &bmain->armatures) {
        do_version_bones_inherit_scale(&arm->bonebase);
      }
    }

    /* Convert the Offset flag to the mix mode enum. */
    if (!DNA_struct_elem_find(fd->filesdna, "bRotateLikeConstraint", "char", "mix_mode")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        do_version_constraints_copy_rotation_mix_mode(&ob->constraints);
        if (ob->pose) {
          LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
            do_version_constraints_copy_rotation_mix_mode(&pchan->constraints);
          }
        }
      }
    }

    /* Added studio-light intensity. */
    if (!DNA_struct_elem_find(fd->filesdna, "View3DShading", "float", "studiolight_intensity")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->shading.studiolight_intensity = 1.0f;
            }
          }
        }
      }
    }

    /* Elatic deform brush */
    for (Brush *br = bmain->brushes.first; br; br = br->id.next) {
      if (br->ob_mode & OB_MODE_SCULPT && br->elastic_deform_volume_preservation == 0.0f) {
        br->elastic_deform_volume_preservation = 0.5f;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 281, 15)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->toolsettings->snap_node_mode == SCE_SNAP_MODE_NODE_X) {
        scene->toolsettings->snap_node_mode = SCE_SNAP_MODE_GRID;
      }
    }

    if (!DNA_struct_elem_find(
            fd->filesdna, "LayerCollection", "short", "local_collections_bits")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
          LISTBASE_FOREACH (LayerCollection *, layer_collection, &view_layer->layer_collections) {
            do_versions_local_collection_bits_set(layer_collection);
          }
        }
      }
    }

    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;

            LISTBASE_FOREACH (ScrArea *, area_other, &screen->areabase) {
              LISTBASE_FOREACH (SpaceLink *, sl_other, &area_other->spacedata) {
                if (sl != sl_other && sl_other->spacetype == SPACE_VIEW3D) {
                  View3D *v3d_other = (View3D *)sl_other;

                  if (v3d->shading.prop == v3d_other->shading.prop) {
                    v3d_other->shading.prop = NULL;
                  }
                }
              }
            }
          }
          else if (sl->spacetype == SPACE_FILE) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;
            ARegion *region_tools = do_versions_find_region_or_null(regionbase, RGN_TYPE_TOOLS);
            ARegion *region_header = do_versions_find_region(regionbase, RGN_TYPE_HEADER);

            if (region_tools) {
              ARegion *region_next = region_tools->next;

              /* We temporarily had two tools regions, get rid of the second one. */
              if (region_next && region_next->regiontype == RGN_TYPE_TOOLS) {
                do_versions_remove_region(regionbase, region_next);
              }

              BLI_remlink(regionbase, region_tools);
              BLI_insertlinkafter(regionbase, region_header, region_tools);
            }
            else {
              region_tools = do_versions_add_region(RGN_TYPE_TOOLS,
                                                    "versioning file tools region");
              BLI_insertlinkafter(regionbase, region_header, region_tools);
              region_tools->alignment = RGN_ALIGN_LEFT;
            }
          }
        }
      }
    }

    for (Brush *br = bmain->brushes.first; br; br = br->id.next) {
      if (br->ob_mode & OB_MODE_SCULPT && br->area_radius_factor == 0.0f) {
        br->area_radius_factor = 0.5f;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 282, 2)) {
    do_version_curvemapping_walker(bmain, do_version_curvemapping_flag_extend_extrapolate);

    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        area->flag &= ~AREA_FLAG_UNUSED_6;
      }
    }

    /* Add custom curve profile to toolsettings for bevel tool */
    if (!DNA_struct_elem_find(fd->filesdna, "ToolSettings", "CurveProfile", "custom_profile")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        ToolSettings *ts = scene->toolsettings;
        if ((ts) && (ts->custom_bevel_profile_preset == NULL)) {
          ts->custom_bevel_profile_preset = BKE_curveprofile_add(PROF_PRESET_LINE);
        }
      }
    }

    /* Add custom curve profile to bevel modifier */
    if (!DNA_struct_elem_find(fd->filesdna, "BevelModifier", "CurveProfile", "custom_profile")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
          if (md->type == eModifierType_Bevel) {
            BevelModifierData *bmd = (BevelModifierData *)md;
            if (!bmd->custom_profile) {
              bmd->custom_profile = BKE_curveprofile_add(PROF_PRESET_LINE);
            }
          }
        }
      }
    }

    /* Dash Ratio and Dash Samples */
    if (!DNA_struct_elem_find(fd->filesdna, "Brush", "float", "dash_ratio")) {
      for (Brush *br = bmain->brushes.first; br; br = br->id.next) {
        br->dash_ratio = 1.0f;
        br->dash_samples = 20;
      }
    }

    /* Pose brush smooth iterations */
    if (!DNA_struct_elem_find(fd->filesdna, "Brush", "float", "pose_smooth_iterations")) {
      for (Brush *br = bmain->brushes.first; br; br = br->id.next) {
        br->pose_smooth_iterations = 4;
      }
    }

    /* Cloth pressure */
    for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_Cloth) {
          ClothModifierData *clmd = (ClothModifierData *)md;

          clmd->sim_parms->pressure_factor = 1;
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 282, 3)) {
    /* Remove Unified pressure/size and pressure/alpha */
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      ToolSettings *ts = scene->toolsettings;
      UnifiedPaintSettings *ups = &ts->unified_paint_settings;
      ups->flag &= ~(UNIFIED_PAINT_FLAG_UNUSED_0 | UNIFIED_PAINT_FLAG_UNUSED_1);
    }

    /* Set the default render pass in the viewport to Combined. */
    if (!DNA_struct_elem_find(fd->filesdna, "View3DShading", "int", "render_pass")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        scene->display.shading.render_pass = SCE_PASS_COMBINED;
      }

      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->shading.render_pass = SCE_PASS_COMBINED;
            }
          }
        }
      }
    }

    /* Make markers region visible by default. */
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          switch (sl->spacetype) {
            case SPACE_SEQ: {
              SpaceSeq *sseq = (SpaceSeq *)sl;
              sseq->flag |= SEQ_SHOW_MARKERS;
              break;
            }
            case SPACE_ACTION: {
              SpaceAction *saction = (SpaceAction *)sl;
              saction->flag |= SACTION_SHOW_MARKERS;
              break;
            }
            case SPACE_GRAPH: {
              SpaceGraph *sipo = (SpaceGraph *)sl;
              sipo->flag |= SIPO_SHOW_MARKERS;
              break;
            }
            case SPACE_NLA: {
              SpaceNla *snla = (SpaceNla *)sl;
              snla->flag |= SNLA_SHOW_MARKERS;
              break;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 283, 3)) {
    /* Color Management Look. */
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      ColorManagedViewSettings *view_settings;
      view_settings = &scene->view_settings;
      if (BLI_str_startswith(view_settings->look, "Filmic - ")) {
        char *src = view_settings->look + strlen("Filmic - ");
        memmove(view_settings->look, src, strlen(src) + 1);
      }
      else if (BLI_str_startswith(view_settings->look, "Standard - ")) {
        char *src = view_settings->look + strlen("Standard - ");
        memmove(view_settings->look, src, strlen(src) + 1);
      }
    }

    /* Sequencer Tool region */
    do_versions_area_ensure_tool_region(bmain, SPACE_SEQ, RGN_FLAG_HIDDEN);

    /* Cloth internal springs */
    for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_Cloth) {
          ClothModifierData *clmd = (ClothModifierData *)md;

          clmd->sim_parms->internal_tension = 15.0f;
          clmd->sim_parms->max_internal_tension = 15.0f;
          clmd->sim_parms->internal_compression = 15.0f;
          clmd->sim_parms->max_internal_compression = 15.0f;
          clmd->sim_parms->internal_spring_max_diversion = M_PI / 4.0f;
        }
      }
    }

    /* Add primary tile to images. */
    if (!DNA_struct_elem_find(fd->filesdna, "Image", "ListBase", "tiles")) {
      for (Image *ima = bmain->images.first; ima; ima = ima->id.next) {
        ImageTile *tile = MEM_callocN(sizeof(ImageTile), "Image Tile");
        tile->ok = 1;
        tile->tile_number = 1001;
        BLI_addtail(&ima->tiles, tile);
      }
    }

    /* UDIM Image Editor change. */
    if (!DNA_struct_elem_find(fd->filesdna, "SpaceImage", "int", "tile_grid_shape[2]")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_IMAGE) {
              SpaceImage *sima = (SpaceImage *)sl;
              sima->tile_grid_shape[0] = 1;
              sima->tile_grid_shape[1] = 1;
            }
          }
        }
      }
    }

    for (Brush *br = bmain->brushes.first; br; br = br->id.next) {
      br->add_col[3] = 0.9f;
      br->sub_col[3] = 0.9f;
    }

    /* Pose brush IK segments. */
    for (Brush *br = bmain->brushes.first; br; br = br->id.next) {
      if (br->pose_ik_segments == 0) {
        br->pose_ik_segments = 1;
      }
    }

    /* Pose brush keep anchor point. */
    for (Brush *br = bmain->brushes.first; br; br = br->id.next) {
      if (br->sculpt_tool == SCULPT_TOOL_POSE) {
        br->flag2 |= BRUSH_POSE_IK_ANCHORED;
      }
    }

    /* Tip Roundness. */
    if (!DNA_struct_elem_find(fd->filesdna, "Brush", "float", "tip_roundness")) {
      for (Brush *br = bmain->brushes.first; br; br = br->id.next) {
        if (br->ob_mode & OB_MODE_SCULPT && br->sculpt_tool == SCULPT_TOOL_CLAY_STRIPS) {
          br->tip_roundness = 0.18f;
        }
      }
    }

    /* EEVEE: Cascade shadow bias fix */
    LISTBASE_FOREACH (Light *, light, &bmain->lights) {
      if (light->type == LA_SUN) {
        /* Should be 0.0004 but for practical reason we make it bigger.
         * Correct factor is scene dependent. */
        light->bias *= 0.002f;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 283, 5)) {
    /* Alembic Transform Cache changed from world to local space. */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (bConstraint *, con, &ob->constraints) {
        if (con->type == CONSTRAINT_TYPE_TRANSFORM_CACHE) {
          con->ownspace = CONSTRAINT_SPACE_LOCAL;
        }
      }
    }

    /* Add 2D transform to UV Warp modifier. */
    if (!DNA_struct_elem_find(fd->filesdna, "UVWarpModifierData", "float", "scale[2]")) {
      for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_UVWarp) {
            UVWarpModifierData *umd = (UVWarpModifierData *)md;
            copy_v2_fl(umd->scale, 1.0f);
          }
        }
      }
    }

    /* Add Lookdev blur property. */
    if (!DNA_struct_elem_find(fd->filesdna, "View3DShading", "float", "studiolight_blur")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->shading.studiolight_blur = 0.5f;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 283, 7)) {
    /* Init default Grease Pencil Vertex paint mix factor for Viewport. */
    if (!DNA_struct_elem_find(
            fd->filesdna, "View3DOverlay", "float", "gpencil_vertex_paint_opacity")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->overlay.gpencil_vertex_paint_opacity = 1.0f;
            }
          }
        }
      }
    }

    /* Update Grease Pencil after drawing engine and code refactor.
     * It uses the seed variable of Array modifier to avoid double patching for
     * files created with a development version. */
    if (!DNA_struct_elem_find(fd->filesdna, "ArrayGpencilModifierData", "int", "seed")) {
      /* Init new Grease Pencil Paint tools. */
      {
        LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
          if (brush->gpencil_settings != NULL) {
            brush->gpencil_vertex_tool = brush->gpencil_settings->brush_type;
            brush->gpencil_sculpt_tool = brush->gpencil_settings->brush_type;
            brush->gpencil_weight_tool = brush->gpencil_settings->brush_type;
          }
        }
      }

      LISTBASE_FOREACH (Material *, mat, &bmain->materials) {
        MaterialGPencilStyle *gp_style = mat->gp_style;
        if (gp_style == NULL) {
          continue;
        }
        /* Fix Grease Pencil Material colors to Linear. */
        srgb_to_linearrgb_v4(gp_style->stroke_rgba, gp_style->stroke_rgba);
        srgb_to_linearrgb_v4(gp_style->fill_rgba, gp_style->fill_rgba);

        /* Move old gradient variables to texture. */
        if (gp_style->fill_style == GP_MATERIAL_FILL_STYLE_GRADIENT) {
          gp_style->texture_angle = gp_style->gradient_angle;
          copy_v2_v2(gp_style->texture_scale, gp_style->gradient_scale);
          copy_v2_v2(gp_style->texture_offset, gp_style->gradient_shift);
        }
        /* Set Checker material as Solid. This fill mode has been removed and replaced
         * by textures. */
        if (gp_style->fill_style == GP_MATERIAL_FILL_STYLE_CHECKER) {
          gp_style->fill_style = GP_MATERIAL_FILL_STYLE_SOLID;
        }
        /* Update Alpha channel for texture opacity. */
        if (gp_style->fill_style == GP_MATERIAL_FILL_STYLE_TEXTURE) {
          gp_style->fill_rgba[3] *= gp_style->texture_opacity;
        }
        /* Stroke stencil mask to mix = 1. */
        if (gp_style->flag & GP_MATERIAL_STROKE_PATTERN) {
          gp_style->mix_stroke_factor = 1.0f;
          gp_style->flag &= ~GP_MATERIAL_STROKE_PATTERN;
        }
        /* Mix disabled, set mix factor to 0. */
        else if ((gp_style->flag & GP_MATERIAL_STROKE_TEX_MIX) == 0) {
          gp_style->mix_stroke_factor = 0.0f;
        }
      }

      /* Fix Grease Pencil VFX and modifiers. */
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        if (ob->type != OB_GPENCIL) {
          continue;
        }

        /* VFX. */
        LISTBASE_FOREACH (ShaderFxData *, fx, &ob->shader_fx) {
          switch (fx->type) {
            case eShaderFxType_Colorize: {
              ColorizeShaderFxData *vfx = (ColorizeShaderFxData *)fx;
              if (ELEM(vfx->mode, eShaderFxColorizeMode_GrayScale, eShaderFxColorizeMode_Sepia)) {
                vfx->factor = 1.0f;
              }
              srgb_to_linearrgb_v4(vfx->low_color, vfx->low_color);
              srgb_to_linearrgb_v4(vfx->high_color, vfx->high_color);
              break;
            }
            case eShaderFxType_Pixel: {
              PixelShaderFxData *vfx = (PixelShaderFxData *)fx;
              srgb_to_linearrgb_v4(vfx->rgba, vfx->rgba);
              break;
            }
            case eShaderFxType_Rim: {
              RimShaderFxData *vfx = (RimShaderFxData *)fx;
              srgb_to_linearrgb_v3_v3(vfx->rim_rgb, vfx->rim_rgb);
              srgb_to_linearrgb_v3_v3(vfx->mask_rgb, vfx->mask_rgb);
              break;
            }
            case eShaderFxType_Shadow: {
              ShadowShaderFxData *vfx = (ShadowShaderFxData *)fx;
              srgb_to_linearrgb_v4(vfx->shadow_rgba, vfx->shadow_rgba);
              break;
            }
            case eShaderFxType_Glow: {
              GlowShaderFxData *vfx = (GlowShaderFxData *)fx;
              srgb_to_linearrgb_v3_v3(vfx->glow_color, vfx->glow_color);
              vfx->glow_color[3] = 1.0f;
              srgb_to_linearrgb_v3_v3(vfx->select_color, vfx->select_color);
              vfx->blur[1] = vfx->blur[0];
              break;
            }
            default:
              break;
          }
        }

        /* Modifiers. */
        LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
          switch ((GpencilModifierType)md->type) {
            case eGpencilModifierType_Array: {
              ArrayGpencilModifierData *mmd = (ArrayGpencilModifierData *)md;
              mmd->seed = 1;
              if ((mmd->offset[0] != 0.0f) || (mmd->offset[1] != 0.0f) ||
                  (mmd->offset[2] != 0.0f)) {
                mmd->flag |= GP_ARRAY_USE_OFFSET;
              }
              if ((mmd->shift[0] != 0.0f) || (mmd->shift[1] != 0.0f) || (mmd->shift[2] != 0.0f)) {
                mmd->flag |= GP_ARRAY_USE_OFFSET;
              }
              if (mmd->object != NULL) {
                mmd->flag |= GP_ARRAY_USE_OB_OFFSET;
              }
              break;
            }
            case eGpencilModifierType_Noise: {
              NoiseGpencilModifierData *mmd = (NoiseGpencilModifierData *)md;
              float factor = mmd->factor / 25.0f;
              mmd->factor = (mmd->flag & GP_NOISE_MOD_LOCATION) ? factor : 0.0f;
              mmd->factor_thickness = (mmd->flag & GP_NOISE_MOD_STRENGTH) ? factor : 0.0f;
              mmd->factor_strength = (mmd->flag & GP_NOISE_MOD_THICKNESS) ? factor : 0.0f;
              mmd->factor_uvs = (mmd->flag & GP_NOISE_MOD_UV) ? factor : 0.0f;

              mmd->noise_scale = (mmd->flag & GP_NOISE_FULL_STROKE) ? 0.0f : 1.0f;

              if (mmd->curve_intensity == NULL) {
                mmd->curve_intensity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
                if (mmd->curve_intensity) {
                  BKE_curvemapping_initialize(mmd->curve_intensity);
                }
              }
              break;
            }
            case eGpencilModifierType_Tint: {
              TintGpencilModifierData *mmd = (TintGpencilModifierData *)md;
              srgb_to_linearrgb_v3_v3(mmd->rgb, mmd->rgb);
              if (mmd->curve_intensity == NULL) {
                mmd->curve_intensity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
                if (mmd->curve_intensity) {
                  BKE_curvemapping_initialize(mmd->curve_intensity);
                }
              }
              break;
            }
            case eGpencilModifierType_Smooth: {
              SmoothGpencilModifierData *mmd = (SmoothGpencilModifierData *)md;
              if (mmd->curve_intensity == NULL) {
                mmd->curve_intensity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
                if (mmd->curve_intensity) {
                  BKE_curvemapping_initialize(mmd->curve_intensity);
                }
              }
              break;
            }
            case eGpencilModifierType_Opacity: {
              OpacityGpencilModifierData *mmd = (OpacityGpencilModifierData *)md;
              if (mmd->curve_intensity == NULL) {
                mmd->curve_intensity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
                if (mmd->curve_intensity) {
                  BKE_curvemapping_initialize(mmd->curve_intensity);
                }
              }
              break;
            }
            case eGpencilModifierType_Color: {
              ColorGpencilModifierData *mmd = (ColorGpencilModifierData *)md;
              if (mmd->curve_intensity == NULL) {
                mmd->curve_intensity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
                if (mmd->curve_intensity) {
                  BKE_curvemapping_initialize(mmd->curve_intensity);
                }
              }
              break;
            }
            case eGpencilModifierType_Thick: {
              if (!DNA_struct_elem_find(
                      fd->filesdna, "ThickGpencilModifierData", "float", "thickness_fac")) {
                ThickGpencilModifierData *mmd = (ThickGpencilModifierData *)md;
                mmd->thickness_fac = mmd->thickness;
              }
              break;
            }
            case eGpencilModifierType_Multiply: {
              MultiplyGpencilModifierData *mmd = (MultiplyGpencilModifierData *)md;
              mmd->fading_opacity = 1.0 - mmd->fading_opacity;
              break;
            }
            case eGpencilModifierType_Subdiv: {
              const short simple = (1 << 0);
              SubdivGpencilModifierData *mmd = (SubdivGpencilModifierData *)md;
              if (mmd->flag & simple) {
                mmd->flag &= ~simple;
                mmd->type = GP_SUBDIV_SIMPLE;
              }
              break;
            }
            default:
              break;
          }
        }
      }

      /* Fix Layers Colors and Vertex Colors to Linear.
       * Also set lights to on for layers. */
      LISTBASE_FOREACH (bGPdata *, gpd, &bmain->gpencils) {
        if (gpd->flag & GP_DATA_ANNOTATIONS) {
          continue;
        }
        /* Onion colors. */
        srgb_to_linearrgb_v3_v3(gpd->gcolor_prev, gpd->gcolor_prev);
        srgb_to_linearrgb_v3_v3(gpd->gcolor_next, gpd->gcolor_next);
        /* Z-depth Offset. */
        gpd->zdepth_offset = 0.150f;

        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          gpl->flag |= GP_LAYER_USE_LIGHTS;
          srgb_to_linearrgb_v4(gpl->tintcolor, gpl->tintcolor);
          gpl->vertex_paint_opacity = 1.0f;

          LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
            LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
              /* Set initial opacity for fill color. */
              gps->fill_opacity_fac = 1.0f;

              /* Calc geometry data because in old versions this data was not saved. */
              BKE_gpencil_stroke_geometry_update(gps);

              srgb_to_linearrgb_v4(gps->vert_color_fill, gps->vert_color_fill);
              int i;
              bGPDspoint *pt;
              for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
                srgb_to_linearrgb_v4(pt->vert_color, pt->vert_color);
              }
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 283, 8)) {
    if (!DNA_struct_elem_find(
            fd->filesdna, "View3DOverlay", "float", "sculpt_mode_face_sets_opacity")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->overlay.sculpt_mode_face_sets_opacity = 1.0f;
            }
          }
        }
      }
    }

    /* Alembic Transform Cache changed from local to world space. */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (bConstraint *, con, &ob->constraints) {
        if (con->type == CONSTRAINT_TYPE_TRANSFORM_CACHE) {
          con->ownspace = CONSTRAINT_SPACE_WORLD;
        }
      }
    }

    /* Boundary Edges Automasking. */
    if (!DNA_struct_elem_find(
            fd->filesdna, "Brush", "int", "automasking_boundary_edges_propagation_steps")) {
      for (Brush *br = bmain->brushes.first; br; br = br->id.next) {
        br->automasking_boundary_edges_propagation_steps = 1;
      }
    }

    /* Corrective smooth modifier scale*/
    if (!DNA_struct_elem_find(fd->filesdna, "CorrectiveSmoothModifierData", "float", "scale")) {
      for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_CorrectiveSmooth) {
            CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)md;
            csmd->scale = 1.0f;
          }
        }
      }
    }

    /* Default Face Set Color. */
    for (Mesh *me = bmain->meshes.first; me != NULL; me = me->id.next) {
      if (me->totpoly > 0) {
        int *face_sets = CustomData_get_layer(&me->pdata, CD_SCULPT_FACE_SETS);
        if (face_sets) {
          me->face_sets_color_default = abs(face_sets[0]);
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 283, 11)) {
    if (!DNA_struct_elem_find(fd->filesdna, "OceanModifierData", "float", "fetch_jonswap")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
          if (md->type == eModifierType_Ocean) {
            OceanModifierData *omd = (OceanModifierData *)md;
            omd->fetch_jonswap = 120.0f;
          }
        }
      }
    }

    if (!DNA_struct_find(fd->filesdna, "XrSessionSettings")) {
      for (wmWindowManager *wm = bmain->wm.first; wm; wm = wm->id.next) {
        const View3D *v3d_default = DNA_struct_default_get(View3D);

        wm->xr.session_settings.shading = v3d_default->shading;
        /* Don't rotate light with the viewer by default, make it fixed. */
        wm->xr.session_settings.shading.flag |= V3D_SHADING_WORLD_ORIENTATION;
        wm->xr.session_settings.draw_flags = (V3D_OFSDRAW_SHOW_GRIDFLOOR |
                                              V3D_OFSDRAW_SHOW_ANNOTATION);
        wm->xr.session_settings.clip_start = v3d_default->clip_start;
        wm->xr.session_settings.clip_end = v3d_default->clip_end;

        wm->xr.session_settings.flag = XR_SESSION_USE_POSITION_TRACKING;
      }
    }

    /* Surface deform modifier strength*/
    if (!DNA_struct_elem_find(fd->filesdna, "SurfaceDeformModifierData", "float", "strength")) {
      for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_SurfaceDeform) {
            SurfaceDeformModifierData *sdmd = (SurfaceDeformModifierData *)md;
            sdmd->strength = 1.0f;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 283, 12)) {
    /* Activate f-curve drawing in the sequencer. */
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      for (ScrArea *area = screen->areabase.first; area; area = area->next) {
        for (SpaceLink *sl = area->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_SEQ) {
            SpaceSeq *sseq = (SpaceSeq *)sl;
            sseq->flag |= SEQ_SHOW_FCURVES;
          }
        }
      }
    }

    /* Remesh Modifier Voxel Mode. */
    if (!DNA_struct_elem_find(fd->filesdna, "RemeshModifierData", "float", "voxel_size")) {
      for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_Remesh) {
            RemeshModifierData *rmd = (RemeshModifierData *)md;
            rmd->voxel_size = 0.1f;
            rmd->adaptivity = 0.0f;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 283, 14)) {
    /* Solidify modifier merge tolerance. */
    if (!DNA_struct_elem_find(fd->filesdna, "SolidifyModifierData", "float", "merge_tolerance")) {
      for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
        for (ModifierData *md = ob->modifiers.first; md; md = md->next) {
          if (md->type == eModifierType_Solidify) {
            SolidifyModifierData *smd = (SolidifyModifierData *)md;
            /* set to 0.0003 since that is what was used before, default now is 0.0001 */
            smd->merge_tolerance = 0.0003f;
          }
        }
      }
    }

    /* Enumerator was incorrect for a time in 2.83 development.
     * Note that this only corrects values known to be invalid. */
    for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
      RigidBodyCon *rbc = ob->rigidbody_constraint;
      if (rbc != NULL) {
        enum {
          INVALID_RBC_TYPE_SLIDER = 2,
          INVALID_RBC_TYPE_6DOF_SPRING = 4,
          INVALID_RBC_TYPE_MOTOR = 7,
        };
        switch (rbc->type) {
          case INVALID_RBC_TYPE_SLIDER:
            rbc->type = RBC_TYPE_SLIDER;
            break;
          case INVALID_RBC_TYPE_6DOF_SPRING:
            rbc->type = RBC_TYPE_6DOF_SPRING;
            break;
          case INVALID_RBC_TYPE_MOTOR:
            rbc->type = RBC_TYPE_MOTOR;
            break;
        }
      }
    }
  }

  /* Match scale of fluid modifier gravity with scene gravity. */
  if (!MAIN_VERSION_ATLEAST(bmain, 283, 15)) {
    for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
      for (ModifierData *md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Fluid) {
          FluidModifierData *fmd = (FluidModifierData *)md;
          if (fmd->domain != NULL) {
            mul_v3_fl(fmd->domain->gravity, 9.81f);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 283, 16)) {
    /* Init SMAA threshold for grease pencil render. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->grease_pencil_settings.smaa_threshold = 1.0f;
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 283, 17)) {
    /* Reset the cloth mass to 1.0 in brushes with an invalid value. */
    for (Brush *br = bmain->brushes.first; br; br = br->id.next) {
      if (br->sculpt_tool == SCULPT_TOOL_CLOTH) {
        if (br->cloth_mass == 0.0f) {
          br->cloth_mass = 1.0f;
        }
      }
    }

    /* Set Brush default color for grease pencil. */
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if (brush->gpencil_settings) {
        brush->rgb[0] = 0.498f;
        brush->rgb[1] = 1.0f;
        brush->rgb[2] = 0.498f;
      }
    }
  }

  /**
   * Versioning code until next subversion bump goes here.
   *
   * \note Be sure to check when bumping the version:
   * - #do_versions_after_linking_280 in this file.
   * - "versioning_userdef.c", #BLO_version_defaults_userpref_blend
   * - "versioning_userdef.c", #do_versions_theme
   *
   * \note Keep this message at the bottom of the function.
   */
  {
    /* Set the cloth wind factor to 1 for old forces. */
    if (!DNA_struct_elem_find(fd->filesdna, "PartDeflect", "float", "f_wind_factor")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        if (ob->pd) {
          ob->pd->f_wind_factor = 1.0f;
        }
      }
      LISTBASE_FOREACH (ParticleSettings *, part, &bmain->particles) {
        if (part->pd) {
          part->pd->f_wind_factor = 1.0f;
        }
        if (part->pd2) {
          part->pd2->f_wind_factor = 1.0f;
        }
      }
    }

    /* Keep this block, even when empty. */
  }
}
