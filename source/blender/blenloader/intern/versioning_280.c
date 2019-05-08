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

#include <string.h>
#include <float.h>

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_mempool.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_camera_types.h"
#include "DNA_cloth_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpu_types.h"
#include "DNA_light_types.h"
#include "DNA_layer_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_genfile.h"
#include "DNA_gpencil_types.h"
#include "DNA_workspace_types.h"
#include "DNA_key_types.h"
#include "DNA_curve_types.h"
#include "DNA_armature_types.h"
#include "DNA_text_types.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_cloth.h"
#include "BKE_collection.h"
#include "BKE_constraint.h"
#include "BKE_colortools.h"
#include "BKE_customdata.h"
#include "BKE_fcurve.h"
#include "BKE_freestyle.h"
#include "BKE_gpencil.h"
#include "BKE_idprop.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"
#include "BKE_rigidbody.h"
#include "BKE_scene.h"
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

static bScreen *screen_parent_find(const bScreen *screen)
{
  /* Can avoid lookup if screen state isn't maximized/full
   * (parent and child store the same state). */
  if (ELEM(screen->state, SCREENMAXIMIZED, SCREENFULL)) {
    for (const ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
      if (sa->full && sa->full != screen) {
        BLI_assert(sa->full->state == screen->state);
        return sa->full;
      }
    }
  }

  return NULL;
}

static void do_version_workspaces_create_from_screens(Main *bmain)
{
  for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
    const bScreen *screen_parent = screen_parent_find(screen);
    WorkSpace *workspace;
    if (screen->temp) {
      continue;
    }

    if (screen_parent) {
      /* fullscreen with "Back to Previous" option, don't create
       * a new workspace, add layout workspace containing parent */
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
}

static void do_version_area_change_space_to_space_action(ScrArea *area, const Scene *scene)
{
  SpaceType *stype = BKE_spacetype_from_id(SPACE_ACTION);
  SpaceAction *saction = (SpaceAction *)stype->new (area, scene);
  ARegion *region_channels;

  /* Properly free current regions */
  for (ARegion *region = area->regionbase.first; region; region = region->next) {
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
    for (wmWindow *win = wm->windows.first; win; win = win->next) {
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
      BKE_workspace_active_layout_set(win->workspace_hook, layout);

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
  for (LayerCollection *lc = lb->first; lc; lc = lc->next) {
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
  for (LayerCollection *lc = lb->first; lc; lc = lc->next) {
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

  for (LinkData *link = sc->objects.first; link; link = link->next) {
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
  Collection *collection_master = BKE_collection_master(scene);
  Collection *collections[20] = {NULL};

  for (int layer = 0; layer < 20; layer++) {
    for (Base *base = scene->base.first; base; base = base->next) {
      if (base->lay & (1 << layer)) {
        /* Create collections when needed only. */
        if (collections[layer] == NULL) {
          char name[MAX_NAME];

          BLI_snprintf(
              name, sizeof(collection_master->id.name), DATA_("Collection %d"), layer + 1);

          Collection *collection = BKE_collection_add(bmain, collection_master, name);
          collection->id.lib = scene->id.lib;
          collections[layer] = collection;

          if (!(scene->lay & (1 << layer))) {
            collection->flag |= COLLECTION_RESTRICT_VIEW | COLLECTION_RESTRICT_RENDER;
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

  for (SceneRenderLayer *srl = scene->r.layers.first; srl; srl = srl->next) {
    ViewLayer *view_layer = BKE_view_layer_add(scene, srl->name);

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
          for (LayerCollection *nlc = lc->layer_collections.first; nlc; nlc = nlc->next) {
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

    for (Base *base = view_layer->object_bases.first; base; base = base->next) {
      if ((base->flag & BASE_SELECTABLE) && (base->object->flag & SELECT)) {
        base->flag |= BASE_SELECTED;
      }
    }
  }

  BLI_freelistN(&scene->r.layers);

  /* If render layers included overrides, or there are no render layers,
   * we also create a vanilla viewport layer. */
  if (have_override || need_default_renderlayer) {
    ViewLayer *view_layer = BKE_view_layer_add(scene, "Viewport");

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
    for (Base *base = view_layer->object_bases.first; base; base = base->next) {
      if ((base->flag & BASE_SELECTABLE) && (base->object->flag & SELECT)) {
        base->flag |= BASE_SELECTED;
      }

      /* keep lay around for forward compatibility (open those files in 2.79) */
      base->lay = base->object->lay;
    }
  }

  /* remove bases once and for all */
  for (Base *base = scene->base.first; base; base = base->next) {
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
  for (const bGPDpalette *palette = gpd->palettes.first; palette; palette = palette->next) {
    for (bGPDpalettecolor *palcolor = palette->colors.first; palcolor; palcolor = palcolor->next) {
      /* fix layers */
      for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
        /* unlock/unhide layer */
        gpl->flag &= ~GP_LAYER_LOCKED;
        gpl->flag &= ~GP_LAYER_HIDE;
        /* set opacity to 1 */
        gpl->opacity = 1.0f;
        /* disable tint */
        gpl->tintcolor[3] = 0.0f;

        for (bGPDframe *gpf = gpl->frames.first; gpf; gpf = gpf->next) {
          for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
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

static void do_versions_remove_region(ListBase *regionbase, int regiontype)
{
  ARegion *ar, *ar_next;
  for (ar = regionbase->first; ar; ar = ar_next) {
    ar_next = ar->next;
    if (ar->regiontype == regiontype) {
      BLI_freelinkN(regionbase, ar);
    }
  }
}

static ARegion *do_versions_find_region_or_null(ListBase *regionbase, int regiontype)
{
  for (ARegion *ar = regionbase->first; ar; ar = ar->next) {
    if (ar->regiontype == regiontype) {
      return ar;
    }
  }
  return NULL;
}

static ARegion *do_versions_find_region(ListBase *regionbase, int regiontype)
{
  ARegion *ar = do_versions_find_region_or_null(regionbase, regiontype);
  if (ar == NULL) {
    BLI_assert(!"Did not find expected region in versioning");
  }
  return ar;
}

static ARegion *do_versions_add_region(int regiontype, const char *name)
{
  ARegion *ar = MEM_callocN(sizeof(ARegion), name);
  ar->regiontype = regiontype;
  return ar;
}

static void do_version_bones_split_bbone_scale(ListBase *lb)
{
  for (Bone *bone = lb->first; bone; bone = bone->next) {
    bone->scale_in_y = bone->scale_in_x;
    bone->scale_out_y = bone->scale_out_x;

    do_version_bones_split_bbone_scale(&bone->childbase);
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
    FCurve *second = copy_fcurve(fcu);

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
  for (bConstraint *con = lb->first; con; con = con->next) {
    if (con->type == CONSTRAINT_TYPE_SAMEVOL) {
      bSameVolumeConstraint *data = (bSameVolumeConstraint *)con->data;
      data->mode = SAMEVOL_UNIFORM;
    }
  }
}

static void do_version_constraints_copy_scale_power(ListBase *lb)
{
  for (bConstraint *con = lb->first; con; con = con->next) {
    if (con->type == CONSTRAINT_TYPE_SIZELIKE) {
      bSizeLikeConstraint *data = (bSizeLikeConstraint *)con->data;
      data->power = 1.0f;
    }
  }
}

void do_versions_after_linking_280(Main *bmain)
{
  bool use_collection_compat_28 = true;

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 0)) {
    use_collection_compat_28 = false;

    /* Convert group layer visibility flags to hidden nested collection. */
    for (Collection *collection = bmain->collections.first; collection;
         collection = collection->id.next) {
      /* Add fake user for all existing groups. */
      id_fake_user_set(&collection->id);

      if (collection->flag & (COLLECTION_RESTRICT_VIEW | COLLECTION_RESTRICT_RENDER)) {
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
            (*collection_hidden)->flag |= COLLECTION_RESTRICT_VIEW | COLLECTION_RESTRICT_RENDER;
          }

          BKE_collection_object_add(bmain, *collection_hidden, ob);
          BKE_collection_object_remove(bmain, collection, ob, true);
        }
      }
    }

    /* We need to assign lib pointer to generated hidden collections *after* all have been created,
     * otherwise we'll end up with several datablocks sharing same name/library,
     * which is FORBIDDEN!
     * Note: we need this to be recursive,
     * since a child collection may be sorted before its parent in bmain. */
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

      for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
        for (SpaceLink *space = sa->spacedata.first; space; space = space->next) {
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
      for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
        for (SpaceLink *space = sa->spacedata.first; space; space = space->next) {
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
      for (SceneRenderLayer *srl = scene->r.layers.first; srl; srl = srl->next) {
        if (srl->prop) {
          IDP_FreeProperty(srl->prop);
          MEM_freeN(srl->prop);
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
      for (ParticleSystem *psys = object->particlesystem.first; psys; psys = psys->next) {
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
        for (ParticleSystem *psys = object->particlesystem.first; psys; psys = psys->next) {
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

      for (wmWindow *win = wm->windows.first; win; win = win->next) {
        const bScreen *screen = BKE_workspace_active_screen_get(win->workspace_hook);
        for (ScrArea *area = screen->areabase.first; area; area = area->next) {
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
        for (ScrArea *area = screen->areabase.first; area; area = area->next) {
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

      for (KeyBlock *block = cu->key->block.first; block; block = block->next) {
        int old_count = block->totelem;
        void *old_data = block->data;

        if (!old_data || old_count <= 0) {
          continue;
        }

        block->totelem = new_count;
        block->data = MEM_callocN(sizeof(float[3]) * new_count, __func__);

        float *oldptr = old_data;
        float(*newptr)[3] = block->data;

        for (Nurb *nu = cu->nurb.first; nu; nu = nu->next) {
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

        for (bPoseChannel *pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
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
    /* Ensure we get valid rigidbody object/constraint data in relevant collections' objects. */
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      RigidBodyWorld *rbw = scene->rigidbody_world;

      if (rbw == NULL) {
        continue;
      }

      BKE_rigidbody_objects_collection_validate(scene, rbw);
      BKE_rigidbody_constraints_collection_validate(scene, rbw);
    }
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
      const int cd_mtexpoly = 15; /* CD_MTEXPOLY, deprecated */
      for (Mesh *me = bmain->meshes.first; me; me = me->id.next) {
        /* If we have UV's, so this file will have MTexPoly layers too! */
        if (me->mloopuv != NULL) {
          CustomData_update_typemap(&me->pdata);
          CustomData_free_layers(&me->pdata, cd_mtexpoly, me->totpoly);
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
        for (bNode *node = ntree->nodes.first; node; node = node->next) {
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
          "You need to connect Principled and Eevee Specular shader nodes to new material output "
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
      /* Grease pencil sculpt and paint cursors */
      if (!DNA_struct_elem_find(fd->filesdna, "GP_Sculpt_Settings", "int", "weighttype")) {
        for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
          /* sculpt brushes */
          GP_Sculpt_Settings *gset = &scene->toolsettings->gp_sculpt;
          if (gset) {
            gset->weighttype = GP_SCULPT_TYPE_WEIGHT;
          }
        }
      }

      {
        float curcolor_add[3], curcolor_sub[3];
        ARRAY_SET_ITEMS(curcolor_add, 1.0f, 0.6f, 0.6f);
        ARRAY_SET_ITEMS(curcolor_sub, 0.6f, 0.6f, 1.0f);
        GP_Sculpt_Data *gp_brush;

        for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
          ToolSettings *ts = scene->toolsettings;
          /* sculpt brushes */
          GP_Sculpt_Settings *gset = &ts->gp_sculpt;
          for (int i = 0; i < GP_SCULPT_TYPE_MAX; ++i) {
            gp_brush = &gset->brush[i];
            gp_brush->flag |= GP_SCULPT_FLAG_ENABLE_CURSOR;
            copy_v3_v3(gp_brush->curcolor_add, curcolor_add);
            copy_v3_v3(gp_brush->curcolor_sub, curcolor_sub);
          }
        }
      }

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
            gset->cur_falloff = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
            curvemapping_initialize(gset->cur_falloff);
            curvemap_reset(gset->cur_falloff->cm,
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
    if (!DNA_struct_elem_find(
            fd->filesdna, "gp_paper_opacity", "float", "gpencil_paper_color[3]")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        for (ScrArea *area = screen->areabase.first; area; area = area->next) {
          for (SpaceLink *sl = area->spacedata.first; sl; sl = sl->next) {
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
      bScreen *sc;
      ScrArea *sa;
      SpaceLink *sl;

      /* Update files using invalid (outdated) outlinevis Outliner values. */
      for (sc = bmain->screens.first; sc; sc = sc->id.next) {
        for (sa = sc->areabase.first; sa; sa = sa->next) {
          for (sl = sa->spacedata.first; sl; sl = sl->next) {
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

    for (bScreen *sc = bmain->screens.first; sc; sc = sc->id.next) {
      for (ScrArea *sa = sc->areabase.first; sa; sa = sa->next) {
        for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
      for (ScrVert *vert = screen->vertbase.first; vert; vert = vert->next) {
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
      for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
        for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
          if (ELEM(sl->spacetype, SPACE_VIEW3D, SPACE_CLIP)) {
            ListBase *regionbase = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;

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
      for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
        for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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

        IDProperty *props = IDP_GetPropertyFromGroup(scene->layer_properties,
                                                     RE_engine_id_BLENDER_EEVEE);
        EEVEE_GET_BOOL(props, volumetric_enable, SCE_EEVEE_VOLUMETRIC_ENABLED);
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
        EEVEE_GET_BOOL(props, sss_enable, SCE_EEVEE_SSS_ENABLED);
        EEVEE_GET_BOOL(props, sss_separate_albedo, SCE_EEVEE_SSS_SEPARATE_ALBEDO);
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
        MEM_freeN(scene->layer_properties);
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
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
          for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
            for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
      for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
        for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
            if (sl->spacetype == SPACE_VIEW3D) {
              enum {
                V3D_SHOW_MODE_SHADE_OVERRIDE = (1 << 15),
              };
              View3D *v3d = (View3D *)sl;
              float alpha = v3d->flag2 & V3D_SHOW_MODE_SHADE_OVERRIDE ? 0.0f : 1.0f;
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
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
      for (bScreen *sc = bmain->screens.first; sc; sc = sc->id.next) {
        for (ScrArea *sa = sc->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
        for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
          gpl->line_change = gpl->thickness;
          if ((gpl->thickness < 1) || (gpl->thickness > 10)) {
            gpl->thickness = 3;
          }
        }
      }
    }
    if (!DNA_struct_elem_find(fd->filesdna, "View3DOverlay", "float", "gpencil_paper_opacity")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
        for (ModifierData *md = object->modifiers.first; md; md = md->next) {
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
        for (ModifierData *md = object->modifiers.first; md; md = md->next) {
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
        for (ModifierData *md = object->modifiers.first; md; md = md->next) {
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
        for (ModifierData *md = ob->modifiers.first; md; md = md->next) {
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
      for (ModifierData *md = ob->modifiers.first; md; md = md->next) {
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
      for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
        for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            v3d->overlay.edit_flag |= V3D_OVERLAY_EDIT_FACES | V3D_OVERLAY_EDIT_SEAMS |
                                      V3D_OVERLAY_EDIT_SHARP | V3D_OVERLAY_EDIT_FREESTYLE_EDGE |
                                      V3D_OVERLAY_EDIT_FREESTYLE_FACE | V3D_OVERLAY_EDIT_EDGES |
                                      V3D_OVERLAY_EDIT_CREASES | V3D_OVERLAY_EDIT_BWEIGHTS |
                                      V3D_OVERLAY_EDIT_CU_HANDLES | V3D_OVERLAY_EDIT_CU_NORMALS;
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "ShrinkwrapModifierData", "char", "shrinkMode")) {
      for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
        for (ModifierData *md = ob->modifiers.first; md; md = md->next) {
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

        for (ModifierData *md = ob->modifiers.first; md; md = md->next) {
          if (md->type == eModifierType_Cloth) {
            ClothModifierData *clmd = (ClothModifierData *)md;

            clmd->coll_parms->selfepsilon = 0.015f;
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "View3DShading", "float", "xray_alpha_wire")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->shading.xray_alpha_wire = 0.5f;
            }
          }
        }
      }

      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->shading.flag |= V3D_SHADING_XRAY_BONE;
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
      for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
        for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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

    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
        for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_PROPERTIES) {
            ListBase *regionbase = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;
            ARegion *ar = MEM_callocN(sizeof(ARegion), "navigation bar for properties");
            ARegion *ar_header = NULL;

            for (ar_header = regionbase->first; ar_header; ar_header = ar_header->next) {
              if (ar_header->regiontype == RGN_TYPE_HEADER) {
                break;
              }
            }
            BLI_assert(ar_header);

            BLI_insertlinkafter(regionbase, ar_header, ar);

            ar->regiontype = RGN_TYPE_NAV_BAR;
            ar->alignment = RGN_ALIGN_LEFT;
          }
        }
      }
    }

    /* grease pencil fade layer opacity */
    if (!DNA_struct_elem_find(fd->filesdna, "View3DOverlay", "float", "gpencil_fade_layer")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
        mat->gp_style->flag |= GP_STYLE_STROKE_SHOW;
        mat->gp_style->flag |= GP_STYLE_FILL_SHOW;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 33)) {
    /* Grease pencil reset sculpt brushes after struct rename  */
    if (!DNA_struct_elem_find(fd->filesdna, "GP_Sculpt_Settings", "int", "weighttype")) {
      float curcolor_add[3], curcolor_sub[3];
      ARRAY_SET_ITEMS(curcolor_add, 1.0f, 0.6f, 0.6f);
      ARRAY_SET_ITEMS(curcolor_sub, 0.6f, 0.6f, 1.0f);

      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        /* sculpt brushes */
        GP_Sculpt_Settings *gset = &scene->toolsettings->gp_sculpt;
        if (gset) {
          for (int i = 0; i < GP_SCULPT_TYPE_MAX; i++) {
            GP_Sculpt_Data *gp_brush = &gset->brush[i];
            gp_brush->size = 30;
            gp_brush->strength = 0.5f;
            gp_brush->flag = GP_SCULPT_FLAG_USE_FALLOFF | GP_SCULPT_FLAG_ENABLE_CURSOR;
            copy_v3_v3(gp_brush->curcolor_add, curcolor_add);
            copy_v3_v3(gp_brush->curcolor_sub, curcolor_sub);
          }
        }
      }
    }

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
      for (ScrArea *area = screen->areabase.first; area; area = area->next) {
        for (SpaceLink *slink = area->spacedata.first; slink; slink = slink->next) {
          if (slink->spacetype == SPACE_USERPREF) {
            ARegion *navigation_region = BKE_spacedata_find_region_type(
                slink, area, RGN_TYPE_NAV_BAR);

            if (!navigation_region) {
              ARegion *main_region = BKE_spacedata_find_region_type(slink, area, RGN_TYPE_WINDOW);
              ListBase *regionbase = (slink == area->spacedata.first) ? &area->regionbase :
                                                                        &slink->regionbase;

              navigation_region = MEM_callocN(sizeof(ARegion),
                                              "userpref navigation-region do_versions");

              BLI_insertlinkbefore(regionbase,
                                   main_region,
                                   navigation_region); /* order matters, addhead not addtail! */
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
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
        for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
          ARRAY_SET_ITEMS(gpl->gcolor_prev, 0.302f, 0.851f, 0.302f);
          ARRAY_SET_ITEMS(gpl->gcolor_next, 0.250f, 0.1f, 1.0f);
        }
      }
    }

    /* Move studio_light selection to lookdev_light. */
    if (!DNA_struct_elem_find(fd->filesdna, "View3DShading", "char", "lookdev_light[256]")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
    for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
      if (ob->type != OB_EMPTY) {
        if (UNLIKELY(ob->transflag & OB_DUPLICOLLECTION)) {
          BKE_object_type_set_empty_for_versioning(ob);
        }
      }
    }

    /* Grease pencil primitive curve */
    if (!DNA_struct_elem_find(
            fd->filesdna, "GP_Sculpt_Settings", "CurveMapping", "cur_primitive")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        GP_Sculpt_Settings *gset = &scene->toolsettings->gp_sculpt;
        if ((gset) && (gset->cur_primitive == NULL)) {
          gset->cur_primitive = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
          curvemapping_initialize(gset->cur_primitive);
          curvemap_reset(gset->cur_primitive->cm,
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
      for (ScrArea *area = screen->areabase.first; area; area = area->next) {
        for (SpaceLink *sl = area->spacedata.first; sl; sl = sl->next) {
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
              v3d->flag &= ~(V3D_FLAG_UNUSED_0 | V3D_FLAG_UNUSED_1 | V3D_FLAG_UNUSED_10 |
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
      image->flag &= ~(IMA_FLAG_UNUSED_0 | IMA_FLAG_UNUSED_1 | IMA_FLAG_UNUSED_4 |
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
      mat->blend_flag &= ~(MA_BL_FLAG_UNUSED_2);
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 40)) {
    if (!DNA_struct_elem_find(fd->filesdna, "ToolSettings", "char", "snap_transform_mode_flag")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        scene->toolsettings->snap_transform_mode_flag = SCE_SNAP_TRANSFORM_MODE_TRANSLATE;
      }
    }

    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      for (ScrArea *area = screen->areabase.first; area; area = area->next) {
        for (SpaceLink *sl = area->spacedata.first; sl; sl = sl->next) {
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

    /* Grease pencil target weight  */
    if (!DNA_struct_elem_find(fd->filesdna, "GP_Sculpt_Settings", "float", "weight")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        /* sculpt brushes */
        GP_Sculpt_Settings *gset = &scene->toolsettings->gp_sculpt;
        if (gset) {
          for (int i = 0; i < GP_SCULPT_TYPE_MAX; i++) {
            GP_Sculpt_Data *gp_brush = &gset->brush[i];
            gp_brush->weight = 1.0f;
          }
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
      for (ScrArea *area = screen->areabase.first; area; area = area->next) {
        for (SpaceLink *sl = area->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_USERPREF) {
            ARegion *execute_region = BKE_spacedata_find_region_type(sl, area, RGN_TYPE_EXECUTE);

            if (!execute_region) {
              ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                     &sl->regionbase;
              ARegion *ar_navbar = BKE_spacedata_find_region_type(sl, area, RGN_TYPE_NAV_BAR);

              execute_region = MEM_callocN(sizeof(ARegion), "execute region for properties");

              BLI_assert(ar_navbar);

              BLI_insertlinkafter(regionbase, ar_navbar, execute_region);

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

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 45)) {
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      for (ScrArea *area = screen->areabase.first; area; area = area->next) {
        for (SpaceLink *sl = area->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_SEQ) {
            SpaceSeq *sseq = (SpaceSeq *)sl;
            sseq->flag |= SEQ_SHOW_MARKER_LINES;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 46)) {
    /* Add wireframe color. */
    if (!DNA_struct_elem_find(fd->filesdna, "View3DShading", "char", "wire_color_type")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
      arm->flag &= ~(ARM_FLAG_UNUSED_1 | ARM_FLAG_UNUSED_5 | ARM_MIRROR_RELATIVE |
                     ARM_FLAG_UNUSED_12);
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
        for (ModifierData *md = ob->modifiers.first; md; md = md->next) {
          if (md->type == eModifierType_Triangulate) {
            TriangulateModifierData *smd = (TriangulateModifierData *)md;
            smd->min_vertices = 4;
          }
        }
      }
    }

    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        for (bNode *node = ntree->nodes.first; node; node = node->next) {
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
        for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
          /* default channel color */
          ARRAY_SET_ITEMS(gpl->color, 0.2f, 0.2f, 0.2f);
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 54)) {
    for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
      bool is_first_subdiv = true;
      for (ModifierData *md = ob->modifiers.first; md; md = md->next) {
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
      for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
        for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_TEXT) {
            ListBase *regionbase = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;

            /* Remove multiple footers that were added by mistake. */
            do_versions_remove_region(regionbase, RGN_TYPE_FOOTER);

            /* Add footer. */
            ARegion *ar = do_versions_add_region(RGN_TYPE_FOOTER, "footer for text");
            ar->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_TOP : RGN_ALIGN_BOTTOM;

            ARegion *ar_header = do_versions_find_region(regionbase, RGN_TYPE_HEADER);
            BLI_insertlinkafter(regionbase, ar_header, ar);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 56)) {
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      for (ScrArea *area = screen->areabase.first; area; area = area->next) {
        for (SpaceLink *sl = area->spacedata.first; sl; sl = sl->next) {
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
      for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
        for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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
    if (!DNA_struct_elem_find(fd->filesdna, "BrushGpencilSettings", "float", "gradient_f")) {
      for (Brush *brush = bmain->brushes.first; brush; brush = brush->id.next) {
        if (brush->gpencil_settings != NULL) {
          BrushGpencilSettings *gp = brush->gpencil_settings;
          gp->gradient_f = 1.0f;
          gp->gradient_s[0] = 1.0f;
          gp->gradient_s[1] = 1.0f;
        }
      }
    }

    /* init grease pencil stroke gradients */
    if (!DNA_struct_elem_find(fd->filesdna, "bGPDstroke", "float", "gradient_f")) {
      for (bGPdata *gpd = bmain->gpencils.first; gpd; gpd = gpd->id.next) {
        for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
          for (bGPDframe *gpf = gpl->frames.first; gpf; gpf = gpf->next) {
            for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
              gps->gradient_f = 1.0f;
              gps->gradient_s[0] = 1.0f;
              gps->gradient_s[1] = 1.0f;
            }
          }
        }
      }
    }

    /* enable the axis aligned ortho grid by default */
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      for (ScrArea *area = screen->areabase.first; area; area = area->next) {
        for (SpaceLink *sl = area->spacedata.first; sl; sl = sl->next) {
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
      for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
        for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
          ListBase *regionbase = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;
          /* All spaces that use tools must be eventually added. */
          ARegion *ar = NULL;
          if (ELEM(sl->spacetype, SPACE_VIEW3D, SPACE_IMAGE) &&
              ((ar = do_versions_find_region_or_null(regionbase, RGN_TYPE_TOOL_HEADER)) == NULL)) {
            /* Add tool header. */
            ar = do_versions_add_region(RGN_TYPE_TOOL_HEADER, "tool header");
            ar->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

            ARegion *ar_header = do_versions_find_region(regionbase, RGN_TYPE_HEADER);
            BLI_insertlinkbefore(regionbase, ar_header, ar);
            /* Hide by default, enable for painting workspaces (startup only). */
            ar->flag |= RGN_FLAG_HIDDEN | RGN_FLAG_HIDDEN_BY_USER;
          }
          if (ar != NULL) {
            SET_FLAG_FROM_TEST(ar->flag, ar->flag & RGN_FLAG_HIDDEN_BY_USER, RGN_FLAG_HIDDEN);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 280, 60)) {
    if (!DNA_struct_elem_find(fd->filesdna, "bSplineIKConstraint", "short", "yScaleMode")) {
      for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
        if (ob->pose) {
          for (bPoseChannel *pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
            for (bConstraint *con = pchan->constraints.first; con; con = con->next) {
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
        for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
          for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
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

  {
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

    /* Versioning code until next subversion bump goes here. */
  }
}
