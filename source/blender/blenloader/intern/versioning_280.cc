/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include <algorithm>
#include <cfloat>
#include <cstring>

#include "BLI_enum_flags.hh"
#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_mempool.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

/* Define macros in `DNA_genfile.h`. */
#define DNA_GENFILE_VERSIONING_MACROS

#include "DNA_defaults.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_cloth_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_curveprofile_types.h"
#include "DNA_fluid_types.h"
#include "DNA_freestyle_types.h"
#include "DNA_genfile.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
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
#include "DNA_sequence_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_text_types.h"
#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"
#include "DNA_world_types.h"

#undef DNA_GENFILE_VERSIONING_MACROS

#include "BKE_anim_data.hh"
#include "BKE_blender.hh"
#include "BKE_collection.hh"
#include "BKE_colortools.hh"
#include "BKE_constraint.h"
#include "BKE_curveprofile.h"
#include "BKE_customdata.hh"
#include "BKE_fcurve.hh"
#include "BKE_fcurve_driver.h"
#include "BKE_freestyle.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_idprop.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"
#include "BKE_pointcache.h"
#include "BKE_report.hh"
#include "BKE_rigidbody.h"
#include "BKE_screen.hh"
#include "BKE_studiolight.h"
#include "BKE_unit.hh"
#include "BKE_workspace.hh"

#include "SEQ_iterator.hh"
#include "SEQ_modifier.hh"
#include "SEQ_utils.hh"

#include "NOD_shader.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"

#include "BLT_translation.hh"

#include "BLO_read_write.hh"
#include "BLO_readfile.hh"
#include "readfile.hh"

#include "versioning_common.hh"

#include "MEM_guardedalloc.h"

/* Make preferences read-only, use `versioning_userdef.cc`. */
#define U (*((const UserDef *)&U))

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

  return nullptr;
}

static void do_version_workspaces_create_from_screens(Main *bmain)
{
  bmain->is_locked_for_linking = false;

  LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
    const bScreen *screen_parent = screen_parent_find(screen);
    WorkSpace *workspace;
    if (screen->temp) {
      continue;
    }

    if (screen_parent) {
      /* Full-screen with "Back to Previous" option, don't create
       * a new workspace, add layout workspace containing parent. */
      workspace = static_cast<WorkSpace *>(
          BLI_findstring(&bmain->workspaces, screen_parent->id.name + 2, offsetof(ID, name) + 2));
    }
    else {
      workspace = BKE_workspace_add(bmain, screen->id.name + 2);
    }
    if (workspace == nullptr) {
      continue; /* Not much we can do. */
    }
    BKE_workspace_layout_add(bmain, workspace, screen, screen->id.name + 2);
  }

  bmain->is_locked_for_linking = true;
}

static void do_version_area_change_space_to_space_action(ScrArea *area, const Scene *scene)
{
  SpaceType *stype = BKE_spacetype_from_id(SPACE_ACTION);
  SpaceAction *saction = (SpaceAction *)stype->create(area, scene);
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

  LISTBASE_FOREACH (wmWindowManager *, wm, &bmain->wm) {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      bScreen *screen_parent = screen_parent_find(win->screen);
      bScreen *screen = screen_parent ? screen_parent : win->screen;

      if (screen->temp) {
        /* We do not generate a new workspace for those screens...
         * still need to set some data in win. */
        win->workspace_hook = BKE_workspace_instance_hook_create(bmain, win->winid);
        win->scene = screen->scene;
        /* Deprecated from now on! */
        win->screen = nullptr;
        continue;
      }

      WorkSpace *workspace = static_cast<WorkSpace *>(
          BLI_findstring(&bmain->workspaces, screen->id.name + 2, offsetof(ID, name) + 2));
      BLI_assert(workspace != nullptr);
      WorkSpaceLayout *layout = BKE_workspace_layout_find(workspace, win->screen);
      BLI_assert(layout != nullptr);

      win->workspace_hook = BKE_workspace_instance_hook_create(bmain, win->winid);

      BKE_workspace_active_set(win->workspace_hook, workspace);
      BKE_workspace_active_layout_set(win->workspace_hook, win->winid, workspace, layout);

      /* Move scene and view layer to window. */
      Scene *scene = screen->scene;
      ViewLayer *layer = static_cast<ViewLayer *>(
          BLI_findlink(&scene->view_layers, scene->r.actlay));
      if (!layer) {
        layer = BKE_view_layer_default_view(scene);
      }

      win->scene = scene;
      STRNCPY_UTF8(win->view_layer_name, layer->name);

      /* Deprecated from now on! */
      win->screen = nullptr;
    }
  }

  LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
    /* Deprecated from now on! */
    BLI_freelistN(&screen->scene->transform_spaces);
    screen->scene = nullptr;
  }
}

static void do_version_layers_to_collections(Main *bmain, Scene *scene)
{
  /* Since we don't have access to FileData we check the (always valid) first
   * render layer instead. */
  if (!scene->master_collection) {
    scene->master_collection = BKE_collection_master_add(scene);
  }

  if (scene->view_layers.first) {
    return;
  }

  /* Create collections from layers. */
  Collection *collection_master = scene->master_collection;
  Collection *collections[20] = {nullptr};

  for (int layer = 0; layer < 20; layer++) {
    LISTBASE_FOREACH (Base *, base, &scene->base) {
      if (base->lay & (1 << layer)) {
        /* Create collections when needed only. */
        if (collections[layer] == nullptr) {
          char name[MAX_ID_NAME - 2];

          SNPRINTF_UTF8(name, DATA_("Collection %d"), layer + 1);

          Collection *collection = BKE_collection_add(bmain, collection_master, name);
          collection->id.lib = scene->id.lib;
          if (ID_IS_LINKED(collection)) {
            collection->id.tag |= ID_TAG_INDIRECT;
          }
          collections[layer] = collection;

          if (!(scene->lay & (1 << layer))) {
            collection->flag |= COLLECTION_HIDE_VIEWPORT | COLLECTION_HIDE_RENDER;
          }
        }

        /* Note usually this would do slow collection syncing for view layers,
         * but since no view layers exists yet at this point it's fast. */
        BKE_collection_object_add_notest(bmain, collections[layer], base->object);
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
  const bool need_default_renderlayer = scene->r.layers.first == nullptr;

  LISTBASE_FOREACH (SceneRenderLayer *, srl, &scene->r.layers) {
    ViewLayer *view_layer = BKE_view_layer_add(scene, srl->name, nullptr, VIEWLAYER_ADD_NEW);

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
    view_layer->world_override = srl->world_override;

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

    BKE_view_layer_synced_ensure(scene, view_layer);
    /* for convenience set the same active object in all the layers */
    if (scene->basact) {
      view_layer->basact = BKE_view_layer_base_find(view_layer, scene->basact->object);
    }

    LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
      if ((base->flag & BASE_SELECTABLE) && (base->object->flag & SELECT)) {
        base->flag |= BASE_SELECTED;
      }
    }
  }

  BLI_freelistN(&scene->r.layers);

  /* If render layers included overrides, or there are no render layers,
   * we also create a vanilla viewport layer. */
  if (have_override || need_default_renderlayer) {
    ViewLayer *view_layer = BKE_view_layer_add(scene, "Viewport", nullptr, VIEWLAYER_ADD_NEW);

    /* If we ported all the original render layers,
     * we don't need to make the viewport layer renderable. */
    if (!BLI_listbase_is_single(&scene->view_layers)) {
      view_layer->flag &= ~VIEW_LAYER_RENDER;
    }

    BKE_view_layer_synced_ensure(scene, view_layer);
    /* convert active base */
    if (scene->basact) {
      view_layer->basact = BKE_view_layer_base_find(view_layer, scene->basact->object);
    }

    /* convert selected bases */
    LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
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
  scene->basact = nullptr;
}

static void do_version_collection_propagate_lib_to_children(Collection *collection)
{
  if (ID_IS_LINKED(collection)) {
    LISTBASE_FOREACH (CollectionChild *, collection_child, &collection->children) {
      if (!ID_IS_LINKED(collection_child->collection)) {
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
            if ((gps->colorname[0] != '\0') && STREQ(gps->colorname, palcolor->info)) {
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
  MEM_delete(region->runtime);
  BLI_freelinkN(regionbase, region);
}

static void do_versions_remove_regions_by_type(ListBase *regionbase, int regiontype)
{
  ARegion *region, *region_next;
  for (region = static_cast<ARegion *>(regionbase->first); region; region = region_next) {
    region_next = static_cast<ARegion *>(region->next);
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
  return nullptr;
}

static ARegion *do_versions_find_region(ListBase *regionbase, int regiontype)
{
  ARegion *region = do_versions_find_region_or_null(regionbase, regiontype);
  if (region == nullptr) {
    BLI_assert_msg(0, "Did not find expected region in versioning");
  }
  return region;
}

static void do_versions_area_ensure_tool_region(Main *bmain,
                                                const short space_type,
                                                const short region_flag)
{
  LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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
    bone->scale_in_z = bone->scale_in_x;
    bone->scale_out_z = bone->scale_out_x;

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

  if (old_path == nullptr) {
    return false;
  }

  if (BLI_str_endswith(old_path, "bbone_scalein") || BLI_str_endswith(old_path, "bbone_scaleout"))
  {
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

    if (fcu->grp != nullptr && fcu->grp->channels.last == fcu) {
      fcu->grp->channels.last = second;
    }
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
  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    if (ELEM(strip->type, STRIP_TYPE_SOUND_RAM, STRIP_TYPE_SOUND_HD) == 0) {
      if (strip->data->transform == nullptr) {
        strip->data->transform = MEM_callocN<StripTransform>("StripTransform");
      }

      if (strip->data->crop == nullptr) {
        strip->data->crop = MEM_callocN<StripCrop>("StripCrop");
      }

      if (strip->seqbase.first != nullptr) {
        do_versions_seq_alloc_transform_and_crop(&strip->seqbase);
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
  for (bNodeLink *link = static_cast<bNodeLink *>(ntree->links.last); link; link = prevlink) {
    prevlink = static_cast<bNodeLink *>(link->prev);

    /* Detect link to replace. */
    bNode *fromnode = link->fromnode;
    bNodeSocket *fromsock = link->fromsock;
    bNode *tonode = link->tonode;
    bNodeSocket *tosock = link->tosock;

    if (!(tonode->type_legacy == SH_NODE_OUTPUT_MATERIAL && STREQ(tosock->identifier, "Surface")))
    {
      continue;
    }

    /* Only do outputs that are enabled for EEVEE */
    if (!ELEM(tonode->custom1, SHD_OUTPUT_ALL, SHD_OUTPUT_EEVEE)) {
      continue;
    }

    enum {
      MA_BM_ADD = 1,
      MA_BM_MULTIPLY = 2,
    };
    if (blend_method == MA_BM_ADD) {
      blender::bke::node_remove_link(ntree, *link);

      bNode *add_node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_ADD_SHADER);
      add_node->locx_legacy = 0.5f * (fromnode->locx_legacy + tonode->locx_legacy);
      add_node->locy_legacy = 0.5f * (fromnode->locy_legacy + tonode->locy_legacy);

      bNodeSocket *shader1_socket = static_cast<bNodeSocket *>(add_node->inputs.first);
      bNodeSocket *shader2_socket = static_cast<bNodeSocket *>(add_node->inputs.last);
      bNodeSocket *add_socket = blender::bke::node_find_socket(*add_node, SOCK_OUT, "Shader");

      bNode *transp_node = blender::bke::node_add_static_node(
          nullptr, *ntree, SH_NODE_BSDF_TRANSPARENT);
      transp_node->locx_legacy = add_node->locx_legacy;
      transp_node->locy_legacy = add_node->locy_legacy - 110.0f;

      bNodeSocket *transp_socket = blender::bke::node_find_socket(*transp_node, SOCK_OUT, "BSDF");

      /* Link to input and material output node. */
      blender::bke::node_add_link(*ntree, *fromnode, *fromsock, *add_node, *shader1_socket);
      blender::bke::node_add_link(
          *ntree, *transp_node, *transp_socket, *add_node, *shader2_socket);
      blender::bke::node_add_link(*ntree, *add_node, *add_socket, *tonode, *tosock);

      need_update = true;
    }
    else if (blend_method == MA_BM_MULTIPLY) {
      blender::bke::node_remove_link(ntree, *link);

      bNode *transp_node = blender::bke::node_add_static_node(
          nullptr, *ntree, SH_NODE_BSDF_TRANSPARENT);

      bNodeSocket *color_socket = blender::bke::node_find_socket(*transp_node, SOCK_IN, "Color");
      bNodeSocket *transp_socket = blender::bke::node_find_socket(*transp_node, SOCK_OUT, "BSDF");

      /* If incoming link is from a closure socket, we need to convert it. */
      if (fromsock->type == SOCK_SHADER) {
        transp_node->locx_legacy = 0.33f * fromnode->locx_legacy + 0.66f * tonode->locx_legacy;
        transp_node->locy_legacy = 0.33f * fromnode->locy_legacy + 0.66f * tonode->locy_legacy;

        bNode *shtorgb_node = blender::bke::node_add_static_node(
            nullptr, *ntree, SH_NODE_SHADERTORGB);
        shtorgb_node->locx_legacy = 0.66f * fromnode->locx_legacy + 0.33f * tonode->locx_legacy;
        shtorgb_node->locy_legacy = 0.66f * fromnode->locy_legacy + 0.33f * tonode->locy_legacy;

        bNodeSocket *shader_socket = blender::bke::node_find_socket(
            *shtorgb_node, SOCK_IN, "Shader");
        bNodeSocket *rgba_socket = blender::bke::node_find_socket(
            *shtorgb_node, SOCK_OUT, "Color");

        blender::bke::node_add_link(*ntree, *fromnode, *fromsock, *shtorgb_node, *shader_socket);
        blender::bke::node_add_link(
            *ntree, *shtorgb_node, *rgba_socket, *transp_node, *color_socket);
      }
      else {
        transp_node->locx_legacy = 0.5f * (fromnode->locx_legacy + tonode->locx_legacy);
        transp_node->locy_legacy = 0.5f * (fromnode->locy_legacy + tonode->locy_legacy);

        blender::bke::node_add_link(*ntree, *fromnode, *fromsock, *transp_node, *color_socket);
      }

      /* Link to input and material output node. */
      blender::bke::node_add_link(*ntree, *transp_node, *transp_socket, *tonode, *tosock);

      need_update = true;
    }
  }

  if (need_update) {
    version_socket_update_is_used(ntree);
  }
}

static void do_versions_local_collection_bits_set(LayerCollection *layer_collection)
{
  layer_collection->local_collections_bits = ~0;
  LISTBASE_FOREACH (LayerCollection *, child, &layer_collection->layer_collections) {
    do_versions_local_collection_bits_set(child);
  }
}

static void do_version_curvemapping_flag_extend_extrapolate(CurveMapping *cumap)
{
  if (cumap == nullptr) {
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

    if (scene->ed != nullptr) {
      LISTBASE_FOREACH (Strip *, strip, &scene->ed->seqbase) {
        LISTBASE_FOREACH (StripModifierData *, smd, &strip->modifiers) {
          const blender::seq::StripModifierTypeInfo *smti = blender::seq::modifier_type_info_get(
              smd->type);

          if (smti) {
            if (smd->type == eSeqModifierType_Curves) {
              CurvesModifierData *cmd = (CurvesModifierData *)smd;
              callback(&cmd->curve_mapping);
            }
            else if (smd->type == eSeqModifierType_HueCorrect) {
              HueCorrectModifierData *hcmd = (HueCorrectModifierData *)smd;
              callback(&hcmd->curve_mapping);
            }
          }
        }
      }
    }

    /* toolsettings */
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
      if (ELEM(node->type_legacy,
               SH_NODE_CURVE_VEC,
               SH_NODE_CURVE_RGB,
               CMP_NODE_CURVE_VEC_DEPRECATED,
               CMP_NODE_CURVE_RGB,
               CMP_NODE_TIME,
               CMP_NODE_HUECORRECT,
               TEX_NODE_CURVE_RGB,
               TEX_NODE_CURVE_TIME))
      {
        callback((CurveMapping *)node->storage);
      }
    }
  }
  FOREACH_NODETREE_END;

  LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
    if (brush->curve_distance_falloff) {
      callback(brush->curve_distance_falloff);
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
  LISTBASE_FOREACH (FreestyleLineStyle *, linestyle, &bmain->linestyles) {
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

static void displacement_node_insert(bNodeTree *ntree)
{
  bool need_update = false;

  /* Iterate backwards from end so we don't encounter newly added links. */
  LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
    /* Detect link to replace. */
    bNode *fromnode = link->fromnode;
    bNodeSocket *fromsock = link->fromsock;
    bNode *tonode = link->tonode;
    bNodeSocket *tosock = link->tosock;

    if (!(tonode->type_legacy == SH_NODE_OUTPUT_MATERIAL &&
          fromnode->type_legacy != SH_NODE_DISPLACEMENT &&
          STREQ(tosock->identifier, "Displacement")))
    {
      continue;
    }

    /* Replace link with displacement node. */
    blender::bke::node_remove_link(ntree, *link);

    /* Add displacement node. */
    bNode *node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_DISPLACEMENT);
    node->locx_legacy = 0.5f * (fromnode->locx_legacy + tonode->locx_legacy);
    node->locy_legacy = 0.5f * (fromnode->locy_legacy + tonode->locy_legacy);

    bNodeSocket *scale_socket = blender::bke::node_find_socket(*node, SOCK_IN, "Scale");
    bNodeSocket *midlevel_socket = blender::bke::node_find_socket(*node, SOCK_IN, "Midlevel");
    bNodeSocket *height_socket = blender::bke::node_find_socket(*node, SOCK_IN, "Height");
    bNodeSocket *displacement_socket = blender::bke::node_find_socket(
        *node, SOCK_OUT, "Displacement");

    /* Set default values for compatibility. */
    *version_cycles_node_socket_float_value(scale_socket) = 0.1f;
    *version_cycles_node_socket_float_value(midlevel_socket) = 0.0f;

    /* Link to input and material output node. */
    blender::bke::node_add_link(*ntree, *fromnode, *fromsock, *node, *height_socket);
    blender::bke::node_add_link(*ntree, *node, *displacement_socket, *tonode, *tosock);

    need_update = true;
  }

  if (need_update) {
    version_socket_update_is_used(ntree);
  }
}

static void displacement_principled_nodes(bNode *node)
{
  if (node->type_legacy == SH_NODE_DISPLACEMENT) {
    if (node->custom1 != SHD_SPACE_WORLD) {
      node->custom1 = SHD_SPACE_OBJECT;
    }
  }
  else if (node->type_legacy == SH_NODE_BSDF_PRINCIPLED) {
    if (node->custom2 != SHD_SUBSURFACE_RANDOM_WALK_SKIN) {
      node->custom2 = SHD_SUBSURFACE_BURLEY;
    }
  }
}

static void square_roughness_node_insert(bNodeTree *ntree)
{
  auto check_node = [](const bNode *node) {
    return ELEM(node->type_legacy,
                SH_NODE_BSDF_GLASS,
                SH_NODE_BSDF_GLOSSY_LEGACY,
                SH_NODE_BSDF_GLOSSY,
                SH_NODE_BSDF_REFRACTION);
  };
  auto update_input = [](const bNode *, bNodeSocket *input) {
    float *value = version_cycles_node_socket_float_value(input);
    *value = sqrtf(max_ff(*value, 0.0f));
  };
  auto update_input_link = [ntree](bNode *fromnode,
                                   bNodeSocket *fromsock,
                                   bNode *tonode,
                                   bNodeSocket *tosock) {
    /* Add `sqrt` node. */
    bNode *node = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
    node->custom1 = NODE_MATH_POWER;
    node->locx_legacy = 0.5f * (fromnode->locx_legacy + tonode->locx_legacy);
    node->locy_legacy = 0.5f * (fromnode->locy_legacy + tonode->locy_legacy);

    /* Link to input and material output node. */
    *version_cycles_node_socket_float_value(static_cast<bNodeSocket *>(node->inputs.last)) = 0.5f;
    blender::bke::node_add_link(
        *ntree, *fromnode, *fromsock, *node, *static_cast<bNodeSocket *>(node->inputs.first));
    blender::bke::node_add_link(
        *ntree, *node, *static_cast<bNodeSocket *>(node->outputs.first), *tonode, *tosock);
  };

  version_update_node_input(ntree, check_node, "Roughness", update_input, update_input_link);
}

static void mapping_node_order_flip(bNode *node)
{
  /* Flip euler order of mapping shader node */
  if (node->type_legacy == SH_NODE_MAPPING && node->storage) {
    TexMapping *texmap = static_cast<TexMapping *>(node->storage);

    float quat[4];
    eulO_to_quat(quat, texmap->rot, EULER_ORDER_ZYX);
    quat_to_eulO(texmap->rot, EULER_ORDER_XYZ, quat);
  }
}

static void vector_curve_node_remap(bNode *node)
{
  /* Remap values of vector curve node from normalized to absolute values */
  if (node->type_legacy == SH_NODE_CURVE_VEC && node->storage) {
    CurveMapping *mapping = static_cast<CurveMapping *>(node->storage);
    mapping->flag &= ~CUMA_DO_CLIP;

    for (int curve_index = 0; curve_index < CM_TOT; curve_index++) {
      CurveMap *cm = &mapping->cm[curve_index];
      if (cm->curve) {
        for (int i = 0; i < cm->totpoint; i++) {
          cm->curve[i].x = (cm->curve[i].x * 2.0f) - 1.0f;
          cm->curve[i].y = (cm->curve[i].y - 0.5f) * 2.0f;
        }
      }
    }

    BKE_curvemapping_changed_all(mapping);
  }
}

static void ambient_occlusion_node_relink(bNodeTree *ntree)
{
  bool need_update = false;

  /* Set default values. */
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_AMBIENT_OCCLUSION) {
      node->custom1 = 1; /* samples */
      node->custom2 &= ~SHD_AO_LOCAL;

      bNodeSocket *distance_socket = blender::bke::node_find_socket(*node, SOCK_IN, "Distance");
      *version_cycles_node_socket_float_value(distance_socket) = 0.0f;
    }
  }

  /* Iterate backwards from end so we don't encounter newly added links. */
  LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
    /* Detect link to replace. */
    bNode *fromnode = link->fromnode;
    bNode *tonode = link->tonode;
    bNodeSocket *tosock = link->tosock;

    if (!(fromnode->type_legacy == SH_NODE_AMBIENT_OCCLUSION)) {
      continue;
    }

    /* Replace links with color socket. */
    blender::bke::node_remove_link(ntree, *link);
    bNodeSocket *color_socket = blender::bke::node_find_socket(*fromnode, SOCK_OUT, "Color");
    blender::bke::node_add_link(*ntree, *fromnode, *color_socket, *tonode, *tosock);

    need_update = true;
  }

  if (need_update) {
    version_socket_update_is_used(ntree);
  }
}

static void image_node_colorspace(bNode *node)
{
  if (node->id == nullptr) {
    return;
  }

  int color_space;
  if (node->type_legacy == SH_NODE_TEX_IMAGE && node->storage) {
    NodeTexImage *tex = static_cast<NodeTexImage *>(node->storage);
    color_space = tex->color_space;
  }
  else if (node->type_legacy == SH_NODE_TEX_ENVIRONMENT && node->storage) {
    NodeTexEnvironment *tex = static_cast<NodeTexEnvironment *>(node->storage);
    color_space = tex->color_space;
  }
  else {
    return;
  }

  enum { SHD_COLORSPACE_NONE = 0 };
  Image *image = (Image *)node->id;
  if (color_space == SHD_COLORSPACE_NONE) {
    STRNCPY_UTF8(image->colorspace_settings.name,
                 IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DATA));
  }
}

static void light_emission_node_to_energy(Light *light, float *energy, float color[3])
{
  *energy = 1.0;
  copy_v3_fl(color, 1.0f);

  /* If nodetree has animation or drivers, don't try to convert. */
  bNodeTree *ntree = light->nodetree;
  if (ntree == nullptr || ntree->adt) {
    return;
  }

  /* Find emission node */
  bNode *output_node = ntreeShaderOutputNode(ntree, SHD_OUTPUT_CYCLES);
  if (output_node == nullptr) {
    return;
  }

  bNode *emission_node = nullptr;
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    if (link->tonode == output_node && link->fromnode->type_legacy == SH_NODE_EMISSION) {
      emission_node = link->fromnode;
      break;
    }
  }

  if (emission_node == nullptr) {
    return;
  }

  /* Don't convert if anything is linked */
  bNodeSocket *strength_socket = blender::bke::node_find_socket(
      *emission_node, SOCK_IN, "Strength");
  bNodeSocket *color_socket = blender::bke::node_find_socket(*emission_node, SOCK_IN, "Color");

  if ((strength_socket->flag & SOCK_IS_LINKED) || (color_socket->flag & SOCK_IS_LINKED)) {
    return;
  }

  float *strength_value = version_cycles_node_socket_float_value(strength_socket);
  float *color_value = version_cycles_node_socket_rgba_value(color_socket);

  *energy = *strength_value;
  copy_v3_v3(color, color_value);

  *strength_value = 1.0f;
  copy_v4_fl(color_value, 1.0f);
  light->use_nodes = false;
}

static void light_emission_unify(Light *light, const char *engine)
{
  if (light->type != LA_SUN) {
    light->energy_deprecated *= 100.0f;
  }

  /* Attempt to extract constant energy and color from nodes. */
  bool use_nodes = light->use_nodes;
  float energy, color[3];
  light_emission_node_to_energy(light, &energy, color);

  if (STREQ(engine, "CYCLES")) {
    if (use_nodes) {
      /* Energy extracted from nodes */
      light->energy_deprecated = energy;
      copy_v3_v3(&light->r, color);
    }
    else {
      /* Default cycles multipliers if there are no nodes */
      if (light->type == LA_SUN) {
        light->energy_deprecated = 1.0f;
      }
      else {
        light->energy_deprecated = 100.0f;
      }
    }
  }
  else {
    /* Disable nodes if scene was configured for Eevee */
    light->use_nodes = false;
  }
}

/* The B input of the Math node is no longer used for single-operand operators.
 * Previously, if the B input was linked and the A input was not, the B input
 * was used as the input of the operator. To correct this, we move the link
 * from B to A if B is linked and A is not.
 */
static void update_math_node_single_operand_operators(bNodeTree *ntree)
{
  bool need_update = false;

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_MATH) {
      if (ELEM(node->custom1,
               NODE_MATH_SQRT,
               NODE_MATH_CEIL,
               NODE_MATH_SINE,
               NODE_MATH_ROUND,
               NODE_MATH_FLOOR,
               NODE_MATH_COSINE,
               NODE_MATH_ARCSINE,
               NODE_MATH_TANGENT,
               NODE_MATH_ABSOLUTE,
               NODE_MATH_FRACTION,
               NODE_MATH_ARCCOSINE,
               NODE_MATH_ARCTANGENT))
      {
        bNodeSocket *sockA = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 0));
        bNodeSocket *sockB = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 1));
        if (!sockA->link && sockB->link) {
          blender::bke::node_add_link(
              *ntree, *sockB->link->fromnode, *sockB->link->fromsock, *node, *sockA);
          blender::bke::node_remove_link(ntree, *sockB->link);
          need_update = true;
        }
      }
    }
  }

  if (need_update) {
    version_socket_update_is_used(ntree);
  }
}

/* The Value output of the Vector Math node is no longer available in the Add
 * and Subtract operators. Previously, this Value output was computed from the
 * Vector output V as follows:
 *
 *   Value = (abs(V.x) + abs(V.y) + abs(V.z)) / 3
 *
 * Or more compactly using vector operators:
 *
 *   Value = dot(abs(V), (1 / 3, 1 / 3, 1 / 3))
 *
 * To correct this, if the Value output was used, we are going to compute
 * it using the second equation by adding an absolute and a dot node, and
 * then connect them appropriately.
 */
static void update_vector_math_node_add_and_subtract_operators(bNodeTree *ntree)
{
  bool need_update = false;

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_VECTOR_MATH) {
      bNodeSocket *sockOutValue = blender::bke::node_find_socket(*node, SOCK_OUT, "Value");
      if (version_node_socket_is_used(sockOutValue) &&
          ELEM(node->custom1, NODE_VECTOR_MATH_ADD, NODE_VECTOR_MATH_SUBTRACT))
      {

        bNode *absNode = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_VECTOR_MATH);
        absNode->custom1 = NODE_VECTOR_MATH_ABSOLUTE;
        absNode->locx_legacy = node->locx_legacy + node->width + 20.0f;
        absNode->locy_legacy = node->locy_legacy;

        bNode *dotNode = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_VECTOR_MATH);
        dotNode->custom1 = NODE_VECTOR_MATH_DOT_PRODUCT;
        dotNode->locx_legacy = absNode->locx_legacy + absNode->width + 20.0f;
        dotNode->locy_legacy = absNode->locy_legacy;
        bNodeSocket *sockDotB = static_cast<bNodeSocket *>(BLI_findlink(&dotNode->inputs, 1));
        bNodeSocket *sockDotOutValue = blender::bke::node_find_socket(*dotNode, SOCK_OUT, "Value");
        copy_v3_fl(version_cycles_node_socket_vector_value(sockDotB), 1 / 3.0f);

        LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
          if (link->fromsock == sockOutValue) {
            blender::bke::node_add_link(
                *ntree, *dotNode, *sockDotOutValue, *link->tonode, *link->tosock);
            blender::bke::node_remove_link(ntree, *link);
          }
        }

        bNodeSocket *sockAbsA = static_cast<bNodeSocket *>(BLI_findlink(&absNode->inputs, 0));
        bNodeSocket *sockDotA = static_cast<bNodeSocket *>(BLI_findlink(&dotNode->inputs, 0));
        bNodeSocket *sockOutVector = blender::bke::node_find_socket(*node, SOCK_OUT, "Vector");
        bNodeSocket *sockAbsOutVector = blender::bke::node_find_socket(
            *absNode, SOCK_OUT, "Vector");

        blender::bke::node_add_link(*ntree, *node, *sockOutVector, *absNode, *sockAbsA);
        blender::bke::node_add_link(*ntree, *absNode, *sockAbsOutVector, *dotNode, *sockDotA);

        need_update = true;
      }
    }
  }

  if (need_update) {
    version_socket_update_is_used(ntree);
  }
}

/* The Vector output of the Vector Math node is no longer available in the Dot
 * Product operator. Previously, this Vector was always zero initialized. To
 * correct this, we zero out any socket the Vector Output was connected to.
 */
static void update_vector_math_node_dot_product_operator(bNodeTree *ntree)
{
  bool need_update = false;

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_VECTOR_MATH) {
      bNodeSocket *sockOutVector = blender::bke::node_find_socket(*node, SOCK_OUT, "Vector");
      if (version_node_socket_is_used(sockOutVector) &&
          node->custom1 == NODE_VECTOR_MATH_DOT_PRODUCT)
      {
        LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
          if (link->fromsock == sockOutVector) {
            switch (link->tosock->type) {
              case SOCK_FLOAT:
                *version_cycles_node_socket_float_value(link->tosock) = 0.0f;
                break;
              case SOCK_VECTOR:
                copy_v3_fl(version_cycles_node_socket_vector_value(link->tosock), 0.0f);
                break;
              case SOCK_RGBA:
                copy_v4_fl(version_cycles_node_socket_rgba_value(link->tosock), 0.0f);
                break;
            }
            blender::bke::node_remove_link(ntree, *link);
          }
        }
        need_update = true;
      }
    }
  }

  if (need_update) {
    version_socket_update_is_used(ntree);
  }
}

/* Previously, the Vector output of the cross product operator was normalized.
 * To correct this, a Normalize node is added to normalize the output if used.
 * Moreover, the Value output was removed. This Value was equal to the length
 * of the cross product. To correct this, a Length node is added if needed.
 */
static void update_vector_math_node_cross_product_operator(bNodeTree *ntree)
{
  bool need_update = false;

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_VECTOR_MATH) {
      if (node->custom1 == NODE_VECTOR_MATH_CROSS_PRODUCT) {
        bNodeSocket *sockOutVector = blender::bke::node_find_socket(*node, SOCK_OUT, "Vector");
        if (version_node_socket_is_used(sockOutVector)) {
          bNode *normalizeNode = blender::bke::node_add_static_node(
              nullptr, *ntree, SH_NODE_VECTOR_MATH);
          normalizeNode->custom1 = NODE_VECTOR_MATH_NORMALIZE;
          normalizeNode->locx_legacy = node->locx_legacy + node->width + 20.0f;
          normalizeNode->locy_legacy = node->locy_legacy;
          bNodeSocket *sockNormalizeOut = blender::bke::node_find_socket(
              *normalizeNode, SOCK_OUT, "Vector");

          LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
            if (link->fromsock == sockOutVector) {
              blender::bke::node_add_link(
                  *ntree, *normalizeNode, *sockNormalizeOut, *link->tonode, *link->tosock);
              blender::bke::node_remove_link(ntree, *link);
            }
          }
          bNodeSocket *sockNormalizeA = static_cast<bNodeSocket *>(
              BLI_findlink(&normalizeNode->inputs, 0));
          blender::bke::node_add_link(
              *ntree, *node, *sockOutVector, *normalizeNode, *sockNormalizeA);

          need_update = true;
        }

        bNodeSocket *sockOutValue = blender::bke::node_find_socket(*node, SOCK_OUT, "Value");
        if (version_node_socket_is_used(sockOutValue)) {
          bNode *lengthNode = blender::bke::node_add_static_node(
              nullptr, *ntree, SH_NODE_VECTOR_MATH);
          lengthNode->custom1 = NODE_VECTOR_MATH_LENGTH;
          lengthNode->locx_legacy = node->locx_legacy + node->width + 20.0f;
          if (version_node_socket_is_used(sockOutVector)) {
            lengthNode->locy_legacy = node->locy_legacy - lengthNode->height - 20.0f;
          }
          else {
            lengthNode->locy_legacy = node->locy_legacy;
          }
          bNodeSocket *sockLengthOut = blender::bke::node_find_socket(
              *lengthNode, SOCK_OUT, "Value");

          LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
            if (link->fromsock == sockOutValue) {
              blender::bke::node_add_link(
                  *ntree, *lengthNode, *sockLengthOut, *link->tonode, *link->tosock);
              blender::bke::node_remove_link(ntree, *link);
            }
          }
          bNodeSocket *sockLengthA = static_cast<bNodeSocket *>(
              BLI_findlink(&lengthNode->inputs, 0));
          blender::bke::node_add_link(*ntree, *node, *sockOutVector, *lengthNode, *sockLengthA);

          need_update = true;
        }
      }
    }
  }

  if (need_update) {
    version_socket_update_is_used(ntree);
  }
}

/* The Value output of the Vector Math node is no longer available in the
 * Normalize operator. This Value output was equal to the length of the
 * the input vector A. To correct this, we either add a Length node or
 * convert the Normalize node into a Length node, depending on if the
 * Vector output is needed.
 */
static void update_vector_math_node_normalize_operator(bNodeTree *ntree)
{
  bool need_update = false;

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_VECTOR_MATH) {
      bNodeSocket *sockOutValue = blender::bke::node_find_socket(*node, SOCK_OUT, "Value");
      if (node->custom1 == NODE_VECTOR_MATH_NORMALIZE && version_node_socket_is_used(sockOutValue))
      {
        bNodeSocket *sockOutVector = blender::bke::node_find_socket(*node, SOCK_OUT, "Vector");
        if (version_node_socket_is_used(sockOutVector)) {
          bNode *lengthNode = blender::bke::node_add_static_node(
              nullptr, *ntree, SH_NODE_VECTOR_MATH);
          lengthNode->custom1 = NODE_VECTOR_MATH_LENGTH;
          lengthNode->locx_legacy = node->locx_legacy + node->width + 20.0f;
          lengthNode->locy_legacy = node->locy_legacy;
          bNodeSocket *sockLengthValue = blender::bke::node_find_socket(
              *lengthNode, SOCK_OUT, "Value");

          LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
            if (link->fromsock == sockOutValue) {
              blender::bke::node_add_link(
                  *ntree, *lengthNode, *sockLengthValue, *link->tonode, *link->tosock);
              blender::bke::node_remove_link(ntree, *link);
            }
          }
          bNodeSocket *sockA = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 0));
          bNodeSocket *sockLengthA = static_cast<bNodeSocket *>(
              BLI_findlink(&lengthNode->inputs, 0));
          if (sockA->link) {
            bNodeLink *link = sockA->link;
            blender::bke::node_add_link(
                *ntree, *link->fromnode, *link->fromsock, *lengthNode, *sockLengthA);
          }
          else {
            copy_v3_v3(version_cycles_node_socket_vector_value(sockLengthA),
                       version_cycles_node_socket_vector_value(sockA));
          }

          need_update = true;
        }
        else {
          node->custom1 = NODE_VECTOR_MATH_LENGTH;
        }
      }
    }
  }
  if (need_update) {
    version_socket_update_is_used(ntree);
  }
}

/* The Vector Math operator types didn't have an enum, but rather, their
 * values were hard coded into the code. After the enum was created and
 * after more vector operators were added, the hard coded values needs
 * to be remapped to their correct enum values. To fix this, we remap
 * the values according to the following rules:
 *
 * Dot Product Operator : 3 -> 7
 * Normalize Operator   : 5 -> 11
 *
 * Additionally, since the Average operator was removed, it is assigned
 * a value of -1 just to be identified later in the versioning code:
 *
 * Average Operator : 2 -> -1
 */
static void update_vector_math_node_operators_enum_mapping(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_VECTOR_MATH) {
      switch (node->custom1) {
        case 2:
          node->custom1 = -1;
          break;
        case 3:
          node->custom1 = 7;
          break;
        case 5:
          node->custom1 = 11;
          break;
      }
    }
  }
}

/* The Average operator is no longer available in the Vector Math node.
 * The Vector output was equal to the normalized sum of input vectors while
 * the Value output was equal to the length of the sum of input vectors.
 * To correct this, we convert the node into an Add node and add a length
 * node or a normalize node if needed.
 */
static void update_vector_math_node_average_operator(bNodeTree *ntree)
{
  bool need_update = false;

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_VECTOR_MATH) {
      /* See update_vector_math_node_operators_enum_mapping. */
      if (node->custom1 == -1) {
        node->custom1 = NODE_VECTOR_MATH_ADD;
        bNodeSocket *sockOutVector = blender::bke::node_find_socket(*node, SOCK_OUT, "Vector");
        if (version_node_socket_is_used(sockOutVector)) {
          bNode *normalizeNode = blender::bke::node_add_static_node(
              nullptr, *ntree, SH_NODE_VECTOR_MATH);
          normalizeNode->custom1 = NODE_VECTOR_MATH_NORMALIZE;
          normalizeNode->locx_legacy = node->locx_legacy + node->width + 20.0f;
          normalizeNode->locy_legacy = node->locy_legacy;
          bNodeSocket *sockNormalizeOut = blender::bke::node_find_socket(
              *normalizeNode, SOCK_OUT, "Vector");

          LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
            if (link->fromsock == sockOutVector) {
              blender::bke::node_add_link(
                  *ntree, *normalizeNode, *sockNormalizeOut, *link->tonode, *link->tosock);
              blender::bke::node_remove_link(ntree, *link);
            }
          }
          bNodeSocket *sockNormalizeA = static_cast<bNodeSocket *>(
              BLI_findlink(&normalizeNode->inputs, 0));
          blender::bke::node_add_link(
              *ntree, *node, *sockOutVector, *normalizeNode, *sockNormalizeA);

          need_update = true;
        }

        bNodeSocket *sockOutValue = blender::bke::node_find_socket(*node, SOCK_OUT, "Value");
        if (version_node_socket_is_used(sockOutValue)) {
          bNode *lengthNode = blender::bke::node_add_static_node(
              nullptr, *ntree, SH_NODE_VECTOR_MATH);
          lengthNode->custom1 = NODE_VECTOR_MATH_LENGTH;
          lengthNode->locx_legacy = node->locx_legacy + node->width + 20.0f;
          if (version_node_socket_is_used(sockOutVector)) {
            lengthNode->locy_legacy = node->locy_legacy - lengthNode->height - 20.0f;
          }
          else {
            lengthNode->locy_legacy = node->locy_legacy;
          }
          bNodeSocket *sockLengthOut = blender::bke::node_find_socket(
              *lengthNode, SOCK_OUT, "Value");

          LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
            if (link->fromsock == sockOutValue) {
              blender::bke::node_add_link(
                  *ntree, *lengthNode, *sockLengthOut, *link->tonode, *link->tosock);
              blender::bke::node_remove_link(ntree, *link);
            }
          }
          bNodeSocket *sockLengthA = static_cast<bNodeSocket *>(
              BLI_findlink(&lengthNode->inputs, 0));
          blender::bke::node_add_link(*ntree, *node, *sockOutVector, *lengthNode, *sockLengthA);

          need_update = true;
        }
      }
    }
  }

  if (need_update) {
    version_socket_update_is_used(ntree);
  }
}

/* The Noise node now have a dimension property. This property should be
 * initialized to 3 by default.
 */
static void update_noise_node_dimensions(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_TEX_NOISE && node->storage) {
      NodeTexNoise *tex = (NodeTexNoise *)node->storage;
      tex->dimensions = 3;
    }
  }
}

/* This callback function is used by update_mapping_node_inputs_and_properties.
 * It is executed on every fcurve in the nodetree id updating its RNA paths. The
 * paths needs to be updated because the node properties became inputs.
 *
 * nodes["Mapping"].translation --> nodes["Mapping"].inputs[1].default_value
 * nodes["Mapping"].rotation --> nodes["Mapping"].inputs[2].default_value
 * nodes["Mapping"].scale --> nodes["Mapping"].inputs[3].default_value
 * nodes["Mapping"].max --> nodes["Maximum"].inputs[1].default_value
 * nodes["Mapping"].min --> nodes["Minimum"].inputs[1].default_value
 *
 * The fcurve can be that of any node or property in the nodetree, so we only
 * update if the rna path starts with the rna path of the mapping node and
 * doesn't end with "default_value", that is, not the Vector input.
 */
static void update_mapping_node_fcurve_rna_path_callback(FCurve *fcurve,
                                                         const char *nodePath,
                                                         const bNode *minimumNode,
                                                         const bNode *maximumNode)
{
  if (!STRPREFIX(fcurve->rna_path, nodePath) ||
      BLI_str_endswith(fcurve->rna_path, "default_value"))
  {
    return;
  }
  char *old_fcurve_rna_path = fcurve->rna_path;

  if (BLI_str_endswith(old_fcurve_rna_path, "translation")) {
    fcurve->rna_path = BLI_sprintfN("%s.%s", nodePath, "inputs[1].default_value");
  }
  else if (BLI_str_endswith(old_fcurve_rna_path, "rotation")) {
    fcurve->rna_path = BLI_sprintfN("%s.%s", nodePath, "inputs[2].default_value");
  }
  else if (BLI_str_endswith(old_fcurve_rna_path, "scale")) {
    fcurve->rna_path = BLI_sprintfN("%s.%s", nodePath, "inputs[3].default_value");
  }
  else if (minimumNode && BLI_str_endswith(old_fcurve_rna_path, "max")) {
    char node_name_esc[sizeof(minimumNode->name) * 2];
    BLI_str_escape(node_name_esc, minimumNode->name, sizeof(node_name_esc));
    fcurve->rna_path = BLI_sprintfN("nodes[\"%s\"].%s", node_name_esc, "inputs[1].default_value");
  }
  else if (maximumNode && BLI_str_endswith(old_fcurve_rna_path, "min")) {
    char node_name_esc[sizeof(maximumNode->name) * 2];
    BLI_str_escape(node_name_esc, maximumNode->name, sizeof(node_name_esc));
    fcurve->rna_path = BLI_sprintfN("nodes[\"%s\"].%s", node_name_esc, "inputs[1].default_value");
  }

  if (fcurve->rna_path != old_fcurve_rna_path) {
    MEM_freeN(old_fcurve_rna_path);
  }
}

/* The Mapping node has been rewritten to support dynamic inputs. Previously,
 * the transformation information was stored in a TexMapping struct in the
 * node->storage member of bNode. Currently, the transformation information
 * is stored in input sockets. To correct this, we transfer the information
 * from the TexMapping struct to the input sockets.
 *
 * Additionally, the Minimum and Maximum properties are no longer available
 * in the node. To correct this, a Vector Minimum and/or a Vector Maximum
 * nodes are added if needed.
 *
 * Finally, the #TexMapping struct is freed and `node->storage` is set to null.
 *
 * Since the RNA paths of the properties changed, we also have to update the
 * rna_path of the FCurves if they exist. To do that, we loop over FCurves
 * and check if they control a property of the node, if they do, we update
 * the path to be that of the corresponding socket in the node or the added
 * minimum/maximum node.
 */
static void update_mapping_node_inputs_and_properties(bNodeTree *ntree)
{
  bool need_update = false;

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    /* If `node->storage` is null, then conversion has already taken place.
     * This can happen if a file with the new mapping node [saved from (2, 81, 8) or newer]
     * is opened in a blender version prior to (2, 81, 8) and saved from there again. */
    if (node->type_legacy == SH_NODE_MAPPING && node->storage) {
      TexMapping *mapping = (TexMapping *)node->storage;
      node->custom1 = mapping->type;
      node->width = 140.0f;

      bNodeSocket *sockLocation = blender::bke::node_find_socket(*node, SOCK_IN, "Location");
      copy_v3_v3(version_cycles_node_socket_vector_value(sockLocation), mapping->loc);
      bNodeSocket *sockRotation = blender::bke::node_find_socket(*node, SOCK_IN, "Rotation");
      copy_v3_v3(version_cycles_node_socket_vector_value(sockRotation), mapping->rot);
      bNodeSocket *sockScale = blender::bke::node_find_socket(*node, SOCK_IN, "Scale");
      copy_v3_v3(version_cycles_node_socket_vector_value(sockScale), mapping->size);

      bNode *maximumNode = nullptr;
      if (mapping->flag & TEXMAP_CLIP_MIN) {
        maximumNode = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_VECTOR_MATH);
        maximumNode->custom1 = NODE_VECTOR_MATH_MAXIMUM;
        if (mapping->flag & TEXMAP_CLIP_MAX) {
          maximumNode->locx_legacy = node->locx_legacy + (node->width + 20.0f) * 2.0f;
        }
        else {
          maximumNode->locx_legacy = node->locx_legacy + node->width + 20.0f;
        }
        maximumNode->locy_legacy = node->locy_legacy;
        bNodeSocket *sockMaximumB = static_cast<bNodeSocket *>(
            BLI_findlink(&maximumNode->inputs, 1));
        copy_v3_v3(version_cycles_node_socket_vector_value(sockMaximumB), mapping->min);
        bNodeSocket *sockMappingResult = blender::bke::node_find_socket(*node, SOCK_OUT, "Vector");

        LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
          if (link->fromsock == sockMappingResult) {
            bNodeSocket *sockMaximumResult = blender::bke::node_find_socket(
                *maximumNode, SOCK_OUT, "Vector");
            blender::bke::node_add_link(
                *ntree, *maximumNode, *sockMaximumResult, *link->tonode, *link->tosock);
            blender::bke::node_remove_link(ntree, *link);
          }
        }
        if (!(mapping->flag & TEXMAP_CLIP_MAX)) {
          bNodeSocket *sockMaximumA = static_cast<bNodeSocket *>(
              BLI_findlink(&maximumNode->inputs, 0));
          blender::bke::node_add_link(
              *ntree, *node, *sockMappingResult, *maximumNode, *sockMaximumA);
        }

        need_update = true;
      }

      bNode *minimumNode = nullptr;
      if (mapping->flag & TEXMAP_CLIP_MAX) {
        minimumNode = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_VECTOR_MATH);
        minimumNode->custom1 = NODE_VECTOR_MATH_MINIMUM;
        minimumNode->locx_legacy = node->locx_legacy + node->width + 20.0f;
        minimumNode->locy_legacy = node->locy_legacy;
        bNodeSocket *sockMinimumB = static_cast<bNodeSocket *>(
            BLI_findlink(&minimumNode->inputs, 1));
        copy_v3_v3(version_cycles_node_socket_vector_value(sockMinimumB), mapping->max);

        bNodeSocket *sockMinimumResult = blender::bke::node_find_socket(
            *minimumNode, SOCK_OUT, "Vector");
        bNodeSocket *sockMappingResult = blender::bke::node_find_socket(*node, SOCK_OUT, "Vector");

        if (maximumNode) {
          bNodeSocket *sockMaximumA = static_cast<bNodeSocket *>(
              BLI_findlink(&maximumNode->inputs, 0));
          blender::bke::node_add_link(
              *ntree, *minimumNode, *sockMinimumResult, *maximumNode, *sockMaximumA);
        }
        else {
          LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
            if (link->fromsock == sockMappingResult) {
              blender::bke::node_add_link(
                  *ntree, *minimumNode, *sockMinimumResult, *link->tonode, *link->tosock);
              blender::bke::node_remove_link(ntree, *link);
            }
          }
        }
        bNodeSocket *sockMinimumA = static_cast<bNodeSocket *>(
            BLI_findlink(&minimumNode->inputs, 0));
        blender::bke::node_add_link(
            *ntree, *node, *sockMappingResult, *minimumNode, *sockMinimumA);

        need_update = true;
      }

      MEM_freeN(node->storage);
      node->storage = nullptr;

      char node_name_esc[sizeof(node->name) * 2];
      BLI_str_escape(node_name_esc, node->name, sizeof(node_name_esc));

      char *nodePath = BLI_sprintfN("nodes[\"%s\"]", node_name_esc);
      BKE_fcurves_id_cb(&ntree->id, [&](ID * /*id*/, FCurve *fcu) {
        update_mapping_node_fcurve_rna_path_callback(fcu, nodePath, minimumNode, maximumNode);
      });
      MEM_freeN(nodePath);
    }
  }

  if (need_update) {
    version_socket_update_is_used(ntree);
  }
}

/* The Musgrave node now has a dimension property. This property should
 * be initialized to 3 by default.
 */
static void update_musgrave_node_dimensions(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_TEX_MUSGRAVE_DEPRECATED && node->storage) {
      NodeTexMusgrave *tex = (NodeTexMusgrave *)node->storage;
      tex->dimensions = 3;
    }
  }
}

/* The Color output of the Musgrave node has been removed. Previously, this
 * output was just equal to the `Fac` output. To correct this, we move links
 * from the Color output to the `Fac` output if they exist.
 */
static void update_musgrave_node_color_output(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNodeLink *, link, &ntree->links) {
    if (link->fromnode && link->fromnode->type_legacy == SH_NODE_TEX_MUSGRAVE_DEPRECATED) {
      if (link->fromsock->type == SOCK_RGBA) {
        link->fromsock = link->fromsock->next;
      }
    }
  }
}

/* The Voronoi node now have a dimension property. This property should be
 * initialized to 3 by default.
 */
static void update_voronoi_node_dimensions(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_TEX_VORONOI && node->storage) {
      NodeTexVoronoi *tex = (NodeTexVoronoi *)node->storage;
      tex->dimensions = 3;
    }
  }
}

/* The F3 and F4 features of the Voronoi node have been removed.
 * To correct this, we set the feature type to be F2 if it is F3
 * or F4. The SHD_VORONOI_F3 and SHD_VORONOI_F4 enum values were
 * 2 and 3 respectively.
 */
static void update_voronoi_node_f3_and_f4(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_TEX_VORONOI && node->storage) {
      NodeTexVoronoi *tex = (NodeTexVoronoi *)node->storage;
      if (ELEM(tex->feature, 2, 3)) {
        tex->feature = SHD_VORONOI_F2;
      }
    }
  }
}

/* The `Fac` output of the Voronoi node has been removed. Previously, this
 * output was the voronoi distance in the Intensity mode and the Cell ID
 * in the Cell mode. To correct this, we update the identifier and name
 * of the `Fac` socket such that it gets mapped to the Distance socket.
 * This is supposed to work with update_voronoi_node_coloring.
 */
static void update_voronoi_node_fac_output(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_TEX_VORONOI) {
      bNodeSocket *facOutput = static_cast<bNodeSocket *>(BLI_findlink(&node->outputs, 1));
      STRNCPY_UTF8(facOutput->identifier, "Distance");
      STRNCPY_UTF8(facOutput->name, "Distance");
    }
  }
}

/* The Crackle feature of the Voronoi node has been removed. Previously,
 * this feature returned the F2 distance minus the F1 distance. The
 * crackle feature had an enum value of 4. To fix this we do the
 * following:
 *
 * 1. The node feature is set to F1.
 * 2. A new Voronoi node is added and its feature is set to F2.
 * 3. The properties, input values, and connections are copied
 *    from the node to the new Voronoi node so that they match
 *    exactly.
 * 4. A Subtract node is added.
 * 5. The outputs of the F1 and F2 voronoi are connected to
 *    the inputs of the subtract node.
 * 6. The output of the subtract node is connected to the
 *    appropriate sockets.
 */
static void update_voronoi_node_crackle(bNodeTree *ntree)
{
  bool need_update = false;

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_TEX_VORONOI && node->storage) {
      NodeTexVoronoi *tex = (NodeTexVoronoi *)node->storage;
      bNodeSocket *sockDistance = blender::bke::node_find_socket(*node, SOCK_OUT, "Distance");
      bNodeSocket *sockColor = blender::bke::node_find_socket(*node, SOCK_OUT, "Color");
      if (tex->feature == 4 &&
          (version_node_socket_is_used(sockDistance) || version_node_socket_is_used(sockColor)))
      {
        tex->feature = SHD_VORONOI_F1;

        bNode *voronoiNode = blender::bke::node_add_static_node(
            nullptr, *ntree, SH_NODE_TEX_VORONOI);
        NodeTexVoronoi *texVoronoi = (NodeTexVoronoi *)voronoiNode->storage;
        texVoronoi->feature = SHD_VORONOI_F2;
        texVoronoi->distance = tex->distance;
        texVoronoi->dimensions = 3;
        voronoiNode->locx_legacy = node->locx_legacy + node->width + 20.0f;
        voronoiNode->locy_legacy = node->locy_legacy;

        bNodeSocket *sockVector = blender::bke::node_find_socket(*node, SOCK_IN, "Vector");
        bNodeSocket *sockScale = blender::bke::node_find_socket(*node, SOCK_IN, "Scale");
        bNodeSocket *sockExponent = blender::bke::node_find_socket(*node, SOCK_IN, "Exponent");
        bNodeSocket *sockVoronoiVector = blender::bke::node_find_socket(
            *voronoiNode, SOCK_IN, "Vector");
        bNodeSocket *sockVoronoiScale = blender::bke::node_find_socket(
            *voronoiNode, SOCK_IN, "Scale");
        bNodeSocket *sockVoronoiExponent = blender::bke::node_find_socket(
            *voronoiNode, SOCK_IN, "Exponent");
        if (sockVector->link) {
          blender::bke::node_add_link(*ntree,
                                      *sockVector->link->fromnode,
                                      *sockVector->link->fromsock,
                                      *voronoiNode,
                                      *sockVoronoiVector);
        }
        *version_cycles_node_socket_float_value(
            sockVoronoiScale) = *version_cycles_node_socket_float_value(sockScale);
        if (sockScale->link) {
          blender::bke::node_add_link(*ntree,
                                      *sockScale->link->fromnode,
                                      *sockScale->link->fromsock,
                                      *voronoiNode,
                                      *sockVoronoiScale);
        }
        *version_cycles_node_socket_float_value(
            sockVoronoiExponent) = *version_cycles_node_socket_float_value(sockExponent);
        if (sockExponent->link) {
          blender::bke::node_add_link(*ntree,
                                      *sockExponent->link->fromnode,
                                      *sockExponent->link->fromsock,
                                      *voronoiNode,
                                      *sockVoronoiExponent);
        }

        bNode *subtractNode = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
        subtractNode->custom1 = NODE_MATH_SUBTRACT;
        subtractNode->locx_legacy = voronoiNode->locx_legacy + voronoiNode->width + 20.0f;
        subtractNode->locy_legacy = voronoiNode->locy_legacy;
        bNodeSocket *sockSubtractOutValue = blender::bke::node_find_socket(
            *subtractNode, SOCK_OUT, "Value");

        LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
          if (link->fromnode == node) {
            blender::bke::node_add_link(
                *ntree, *subtractNode, *sockSubtractOutValue, *link->tonode, *link->tosock);
            blender::bke::node_remove_link(ntree, *link);
          }
        }

        bNodeSocket *sockDistanceF1 = blender::bke::node_find_socket(*node, SOCK_OUT, "Distance");
        bNodeSocket *sockDistanceF2 = blender::bke::node_find_socket(
            *voronoiNode, SOCK_OUT, "Distance");
        bNodeSocket *sockSubtractA = static_cast<bNodeSocket *>(
            BLI_findlink(&subtractNode->inputs, 0));
        bNodeSocket *sockSubtractB = static_cast<bNodeSocket *>(
            BLI_findlink(&subtractNode->inputs, 1));

        blender::bke::node_add_link(*ntree, *node, *sockDistanceF1, *subtractNode, *sockSubtractB);
        blender::bke::node_add_link(
            *ntree, *voronoiNode, *sockDistanceF2, *subtractNode, *sockSubtractA);

        need_update = true;
      }
    }
  }

  if (need_update) {
    version_socket_update_is_used(ntree);
  }
}

/**
 * The coloring property of the Voronoi node was removed. Previously,
 * if the coloring enum was set to Intensity (0), the voronoi distance
 * was returned in all outputs, otherwise, the Cell ID was returned.
 * Since we remapped the `Fac` output in update_voronoi_node_fac_output,
 * then to fix this, we relink the Color output to the Distance
 * output if coloring was set to 0, and the other way around otherwise.
 */
static void update_voronoi_node_coloring(bNodeTree *ntree)
{
  bool need_update = false;

  LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
    bNode *node = link->fromnode;
    if (node && node->type_legacy == SH_NODE_TEX_VORONOI && node->storage) {
      NodeTexVoronoi *tex = (NodeTexVoronoi *)node->storage;
      if (tex->coloring == 0) {
        bNodeSocket *sockColor = blender::bke::node_find_socket(*node, SOCK_OUT, "Color");
        if (link->fromsock == sockColor) {
          bNodeSocket *sockDistance = blender::bke::node_find_socket(*node, SOCK_OUT, "Distance");
          blender::bke::node_add_link(*ntree, *node, *sockDistance, *link->tonode, *link->tosock);
          blender::bke::node_remove_link(ntree, *link);
          need_update = true;
        }
      }
      else {
        bNodeSocket *sockDistance = blender::bke::node_find_socket(*node, SOCK_OUT, "Distance");
        if (link->fromsock == sockDistance) {
          bNodeSocket *sockColor = blender::bke::node_find_socket(*node, SOCK_OUT, "Color");
          blender::bke::node_add_link(*ntree, *node, *sockColor, *link->tonode, *link->tosock);
          blender::bke::node_remove_link(ntree, *link);
          need_update = true;
        }
      }
    }
  }

  if (need_update) {
    version_socket_update_is_used(ntree);
  }
}

/* Previously, the output euclidean distance was actually the squared
 * euclidean distance. To fix this, we square the output distance
 * socket if the distance metric is set to SHD_VORONOI_EUCLIDEAN.
 */
static void update_voronoi_node_square_distance(bNodeTree *ntree)
{
  bool need_update = false;

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_TEX_VORONOI && node->storage) {
      NodeTexVoronoi *tex = (NodeTexVoronoi *)node->storage;
      bNodeSocket *sockDistance = blender::bke::node_find_socket(*node, SOCK_OUT, "Distance");
      if (tex->distance == SHD_VORONOI_EUCLIDEAN &&
          ELEM(tex->feature, SHD_VORONOI_F1, SHD_VORONOI_F2) &&
          version_node_socket_is_used(sockDistance))
      {
        bNode *multiplyNode = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
        multiplyNode->custom1 = NODE_MATH_MULTIPLY;
        multiplyNode->locx_legacy = node->locx_legacy + node->width + 20.0f;
        multiplyNode->locy_legacy = node->locy_legacy;

        bNodeSocket *sockValue = blender::bke::node_find_socket(*multiplyNode, SOCK_OUT, "Value");
        LISTBASE_FOREACH_BACKWARD_MUTABLE (bNodeLink *, link, &ntree->links) {
          if (link->fromsock == sockDistance) {
            blender::bke::node_add_link(
                *ntree, *multiplyNode, *sockValue, *link->tonode, *link->tosock);
            blender::bke::node_remove_link(ntree, *link);
          }
        }

        bNodeSocket *sockMultiplyA = static_cast<bNodeSocket *>(
            BLI_findlink(&multiplyNode->inputs, 0));
        bNodeSocket *sockMultiplyB = static_cast<bNodeSocket *>(
            BLI_findlink(&multiplyNode->inputs, 1));

        blender::bke::node_add_link(*ntree, *node, *sockDistance, *multiplyNode, *sockMultiplyA);
        blender::bke::node_add_link(*ntree, *node, *sockDistance, *multiplyNode, *sockMultiplyB);

        need_update = true;
      }
    }
  }

  if (need_update) {
    version_socket_update_is_used(ntree);
  }
}

/**
 * Noise and Wave Texture nodes: Restore previous Distortion range.
 * In 2.81 we used `noise()` for distortion, now we use `snoise()` which has twice the range.
 * To fix this we halve distortion value, directly or by adding multiply node for used sockets.
 */
static void update_noise_and_wave_distortion(bNodeTree *ntree)
{
  bool need_update = false;

  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (ELEM(node->type_legacy, SH_NODE_TEX_NOISE, SH_NODE_TEX_WAVE)) {

      bNodeSocket *sockDistortion = blender::bke::node_find_socket(*node, SOCK_IN, "Distortion");
      float *distortion = version_cycles_node_socket_float_value(sockDistortion);

      if (version_node_socket_is_used(sockDistortion) && sockDistortion->link != nullptr) {
        bNode *distortionInputNode = sockDistortion->link->fromnode;
        bNodeSocket *distortionInputSock = sockDistortion->link->fromsock;

        bNode *mulNode = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_MATH);
        mulNode->custom1 = NODE_MATH_MULTIPLY;
        mulNode->locx_legacy = node->locx_legacy;
        mulNode->locy_legacy = node->locy_legacy - 240.0f;
        mulNode->flag |= NODE_COLLAPSED;
        bNodeSocket *mulSockA = static_cast<bNodeSocket *>(BLI_findlink(&mulNode->inputs, 0));
        bNodeSocket *mulSockB = static_cast<bNodeSocket *>(BLI_findlink(&mulNode->inputs, 1));
        *version_cycles_node_socket_float_value(mulSockB) = 0.5f;
        bNodeSocket *mulSockOut = blender::bke::node_find_socket(*mulNode, SOCK_OUT, "Value");

        blender::bke::node_remove_link(ntree, *sockDistortion->link);
        blender::bke::node_add_link(
            *ntree, *distortionInputNode, *distortionInputSock, *mulNode, *mulSockA);
        blender::bke::node_add_link(*ntree, *mulNode, *mulSockOut, *node, *sockDistortion);

        need_update = true;
      }
      else if (*distortion != 0.0f) {
        *distortion = *distortion * 0.5f;
      }
    }
  }

  if (need_update) {
    version_socket_update_is_used(ntree);
  }
}

/**
 * Wave Texture node: Restore previous texture directions and offset.
 * 1. In 2.81, Wave texture had fixed diagonal direction (Bands) or
 *    mapping along distance (Rings). Now, directions are customizable
 *    properties, with X axis being new default. To fix this we set new
 *    direction options to Diagonal and Spherical.
 * 2. Sine profile is now negatively offset by PI/2 to better match
 *    other profiles. To fix this we set new Phase Offset input to PI/2
 *    in nodes with Sine profile.
 */
static void update_wave_node_directions_and_offset(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == SH_NODE_TEX_WAVE) {
      NodeTexWave *tex = (NodeTexWave *)node->storage;
      tex->bands_direction = SHD_WAVE_BANDS_DIRECTION_DIAGONAL;
      tex->rings_direction = SHD_WAVE_RINGS_DIRECTION_SPHERICAL;

      if (tex->wave_profile == SHD_WAVE_PROFILE_SIN) {
        bNodeSocket *sockPhaseOffset = blender::bke::node_find_socket(
            *node, SOCK_IN, "Phase Offset");
        *version_cycles_node_socket_float_value(sockPhaseOffset) = M_PI_2;
      }
    }
  }
}

void do_versions_after_linking_280(FileData *fd, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 0)) {
    /* Convert group layer visibility flags to hidden nested collection. */
    LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
      /* Add fake user for all existing groups. */
      id_fake_user_set(&collection->id);

      if (collection->flag & (COLLECTION_HIDE_VIEWPORT | COLLECTION_HIDE_RENDER)) {
        continue;
      }

      Collection *hidden_collection_array[20] = {nullptr};
      for (CollectionObject *cob = static_cast<CollectionObject *>(collection->gobject.first),
                            *cob_next = nullptr;
           cob;
           cob = cob_next)
      {
        cob_next = cob->next;
        Object *ob = cob->ob;

        if (!(ob->lay & collection->layer)) {
          /* Find or create hidden collection matching object's first layer. */
          Collection **collection_hidden = nullptr;
          int coll_idx = 0;
          for (; coll_idx < 20; coll_idx++) {
            if (ob->lay & (1 << coll_idx)) {
              collection_hidden = &hidden_collection_array[coll_idx];
              break;
            }
          }
          if (collection_hidden == nullptr) {
            /* This should never happen (objects are always supposed to be instantiated in a
             * scene), but it does sometimes, see e.g. #81168.
             * Just put them in first hidden collection in those cases. */
            collection_hidden = &hidden_collection_array[0];
          }

          if (*collection_hidden == nullptr) {
            char name[MAX_ID_NAME];
            SNPRINTF_UTF8(name, DATA_("Hidden %d"), coll_idx + 1);
            *collection_hidden = BKE_collection_add(bmain, collection, name);
            (*collection_hidden)->flag |= COLLECTION_HIDE_VIEWPORT | COLLECTION_HIDE_RENDER;
          }

          BKE_collection_object_add_notest(bmain, *collection_hidden, ob);
          BKE_collection_object_remove(bmain, collection, ob, true);
        }
      }
    }

    /* We need to assign lib pointer to generated hidden collections *after* all have been
     * created, otherwise we'll end up with several data-blocks sharing same name/library,
     * which is FORBIDDEN! NOTE: we need this to be recursive, since a child collection may be
     * sorted before its parent in bmain. */
    LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
      do_version_collection_propagate_lib_to_children(collection);
    }

    /* Convert layers to collections. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      do_version_layers_to_collections(bmain, scene);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 0)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      BLO_read_assert_message(screen->scene == nullptr,
                              ,
                              (BlendHandle *)fd,
                              bmain,
                              "No Screen data-block should ever have a nullptr `scene` pointer");

      /* same render-layer as do_version_workspaces_after_lib_link will activate,
       * so same layer as BKE_view_layer_default_view would return */
      ViewLayer *layer = static_cast<ViewLayer *>(screen->scene->view_layers.first);

      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
          if (space->spacetype == SPACE_OUTLINER) {
            SpaceOutliner *space_outliner = (SpaceOutliner *)space;

            space_outliner->outlinevis = SO_VIEW_LAYER;

            if (BLI_listbase_is_single(&layer->layer_collections)) {
              if (space_outliner->treestore == nullptr) {
                space_outliner->treestore = BLI_mempool_create(
                    sizeof(TreeStoreElem), 1, 512, BLI_MEMPOOL_ALLOW_ITER);
              }

              /* Create a tree store element for the collection. This is normally
               * done in check_persistent `outliner_tree.cc`, but we need to access
               * it here :/ (expand element if it's the only one) */
              TreeStoreElem *tselem = static_cast<TreeStoreElem *>(
                  BLI_mempool_calloc(space_outliner->treestore));
              tselem->type = TSE_LAYER_COLLECTION;
              tselem->id = &((LayerCollection *)(layer->layer_collections.first))->collection->id;
              tselem->nr = tselem->used = 0;
              tselem->flag &= ~TSE_CLOSED;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 0)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

  /* New workspace design. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 1)) {
    do_version_workspaces_after_lib_link(bmain);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 2)) {
    /* Cleanup any remaining SceneRenderLayer data for files that were created
     * with Blender 2.8 before the SceneRenderLayer > RenderLayer refactor. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      LISTBASE_FOREACH (SceneRenderLayer *, srl, &scene->r.layers) {
        if (srl->prop) {
          IDP_FreeProperty(srl->prop);
        }
        BKE_freestyle_config_free(&srl->freestyleConfig, true);
      }
      BLI_freelistN(&scene->r.layers);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 3)) {
    /* Due to several changes to particle RNA and draw code particles from older files may
     * no longer be visible.
     * Here we correct this by setting a default draw size for those files. */
    LISTBASE_FOREACH (Object *, object, &bmain->objects) {
      LISTBASE_FOREACH (ParticleSystem *, psys, &object->particlesystem) {
        if (psys->part->draw_size == 0.0f) {
          psys->part->draw_size = 0.1f;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 4)) {
    LISTBASE_FOREACH (Object *, object, &bmain->objects) {
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

    /* Cleanup deprecated flag from particle-settings data-blocks. */
    LISTBASE_FOREACH (ParticleSettings *, part, &bmain->particles) {
      part->draw &= ~PART_DRAW_EMITTER;
    }
  }

  /* SpaceTime & SpaceLogic removal/replacing */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 9)) {
    const wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
    const Scene *scene = static_cast<Scene *>(bmain->scenes.first);

    if (wm != nullptr) {
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
    if (scene != nullptr) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 14)) {
    /* This code fixes crashes when loading early 2.80 development files, due to the lack of a
     * master collection after removal of the versioning code handling the 'SceneCollection' data
     * that was part of the very early 2.80 development (commit 23835a393c).
     *
     * NOTE: This code only ensures that there is no crash, since the whole collection hierarchy
     * from these files remain lost, these files will still need a lot of manual work if one want
     * to get them working properly again. Or just open and save them with an older release of
     * Blender (up to 3.6 included). */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->master_collection == nullptr) {
        scene->master_collection = BKE_collection_master_add(scene);
        /* #BKE_layer_collection_sync accepts missing view-layer in a scene, but not invalid ones
         * where the first view-layer's layer-collection would not be for the Scene's master
         * collection. */
        LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
          if (LayerCollection *first_layer_collection = static_cast<LayerCollection *>(
                  view_layer->layer_collections.first))
          {
            first_layer_collection->collection = scene->master_collection;
          }
        }
      }
    }
  }

  /* Update Curve object Shape Key data layout to include the Radius property */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 23)) {
    LISTBASE_FOREACH (Curve *, cu, &bmain->curves) {
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
        block->data = MEM_calloc_arrayN<float[3]>(new_count, __func__);

        float *oldptr = static_cast<float *>(old_data);
        float (*newptr)[3] = static_cast<float (*)[3]>(block->data);

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
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 25)) {
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      bArmature *arm = static_cast<bArmature *>(ob->data);

      /* If it is an armature from the same file. */
      if (ob->pose && arm && arm->id.lib == ob->id.lib) {
        bool rebuild = false;

        LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
          /* If the 2.7 flag is enabled, processing is needed. */
          if (pchan->bone && (pchan->bboneflag & PCHAN_BBONE_CUSTOM_HANDLES)) {
            /* If the settings in the Bone are not set, copy. */
            if (pchan->bone->bbone_prev_type == BBONE_HANDLE_AUTO &&
                pchan->bone->bbone_next_type == BBONE_HANDLE_AUTO &&
                pchan->bone->bbone_prev == nullptr && pchan->bone->bbone_next == nullptr)
            {
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
          LISTBASE_FOREACH (Object *, ob2, &bmain->objects) {
            if (ob2->pose && ob2->data == arm) {
              ob2->pose->flag |= POSE_RECALC;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 30)) {
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if (brush->gpencil_settings != nullptr) {
        brush->gpencil_brush_type = brush->gpencil_settings->brush_type;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 38)) {
    /* Ensure we get valid rigidbody object/constraint data in relevant collections' objects.
     */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      RigidBodyWorld *rbw = scene->rigidbody_world;

      if (rbw == nullptr) {
        continue;
      }

      BKE_rigidbody_objects_collection_validate(bmain, scene, rbw);
      BKE_rigidbody_constraints_collection_validate(scene, rbw);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 69)) {
    /* Unify DOF settings (EEVEE part only) */
    enum { SCE_EEVEE_DOF_ENABLED = (1 << 7) };
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (STREQ(scene->r.engine, RE_engine_id_BLENDER_EEVEE)) {
        if (scene->eevee.flag & SCE_EEVEE_DOF_ENABLED) {
          Object *cam_ob = scene->camera;
          if (cam_ob && cam_ob->type == OB_CAMERA) {
            Camera *cam = static_cast<Camera *>(cam_ob->data);
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
      camera->dof_ob = nullptr;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 66)) {
    /* Shader node tree changes. After lib linking so we have all the type-info
     * pointers and updated sockets and we can use the high level node API to
     * manipulate nodes. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_SHADER) {
        continue;
      }

      if (!MAIN_VERSION_FILE_ATLEAST(bmain, 273, 5)) {
        /* Euler order was ZYX in previous versions. */
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          mapping_node_order_flip(node);
        }
      }

      if (!MAIN_VERSION_FILE_ATLEAST(bmain, 276, 6)) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          vector_curve_node_remap(node);
        }
      }

      if (!MAIN_VERSION_FILE_ATLEAST(bmain, 279, 2) ||
          (MAIN_VERSION_FILE_ATLEAST(bmain, 280, 0) && !MAIN_VERSION_FILE_ATLEAST(bmain, 280, 4)))
      {
        displacement_node_insert(ntree);
      }

      if (!MAIN_VERSION_FILE_ATLEAST(bmain, 279, 3)) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          displacement_principled_nodes(node);
        }
      }

      if (!MAIN_VERSION_FILE_ATLEAST(bmain, 279, 4) ||
          (MAIN_VERSION_FILE_ATLEAST(bmain, 280, 0) && !MAIN_VERSION_FILE_ATLEAST(bmain, 280, 5)))
      {
        /* Switch to squared roughness convention */
        square_roughness_node_insert(ntree);
      }

      if (!MAIN_VERSION_FILE_ATLEAST(bmain, 279, 5)) {
        ambient_occlusion_node_relink(ntree);
      }

      if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 66)) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          image_node_colorspace(node);
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 64)) {
    /* Unify Cycles and Eevee settings. */
    Scene *scene = static_cast<Scene *>(bmain->scenes.first);
    const char *engine = (scene) ? scene->r.engine : "CYCLES";

    LISTBASE_FOREACH (Light *, light, &bmain->lights) {
      light_emission_unify(light, engine);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 69)) {
    /* Unify Cycles and Eevee depth of field. */
    Scene *scene = static_cast<Scene *>(bmain->scenes.first);
    const char *engine = (scene) ? scene->r.engine : "CYCLES";

    if (STREQ(engine, RE_engine_id_CYCLES)) {
      LISTBASE_FOREACH (Camera *, camera, &bmain->cameras) {
        IDProperty *ccamera = version_cycles_properties_from_ID(&camera->id);
        if (ccamera) {
          const bool is_fstop = version_cycles_property_int(ccamera, "aperture_type", 0) == 1;

          camera->dof.aperture_fstop = version_cycles_property_float(
              ccamera, "aperture_fstop", 5.6f);
          camera->dof.aperture_blades = version_cycles_property_int(ccamera, "aperture_blades", 0);
          camera->dof.aperture_rotation = version_cycles_property_float(
              ccamera, "aperture_rotation", 0.0);
          camera->dof.aperture_ratio = version_cycles_property_float(
              ccamera, "aperture_ratio", 1.0f);
          camera->dof.flag |= CAM_DOF_ENABLED;

          float aperture_size = version_cycles_property_float(ccamera, "aperture_size", 0.0f);

          if (is_fstop) {
            continue;
          }
          if (aperture_size > 0.0f) {
            if (camera->type == CAM_ORTHO) {
              camera->dof.aperture_fstop = 1.0f / (2.0f * aperture_size);
            }
            else {
              camera->dof.aperture_fstop = (camera->lens * 1e-3f) / (2.0f * aperture_size);
            }

            continue;
          }
        }

        /* No depth of field, set default settings. */
        camera->dof.aperture_fstop = 2.8f;
        camera->dof.aperture_blades = 0;
        camera->dof.aperture_rotation = 0.0f;
        camera->dof.aperture_ratio = 1.0f;
        camera->dof.flag &= ~CAM_DOF_ENABLED;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 281, 2)) {
    /* Replace Multiply and Additive blend mode by Alpha Blend
     * now that we use dual-source blending. */
    /* We take care of doing only node-trees that are always part of materials
     * with old blending modes. */
    enum {
      MA_BM_ADD = 1,
      MA_BM_MULTIPLY = 2,
    };
    LISTBASE_FOREACH (Material *, ma, &bmain->materials) {
      bNodeTree *ntree = ma->nodetree;
      if (ma->blend_method == MA_BM_ADD) {
        if (ma->use_nodes) {
          do_versions_material_convert_legacy_blend_mode(ntree, MA_BM_ADD);
        }
        ma->blend_method = MA_BM_BLEND;
      }
      else if (ma->blend_method == MA_BM_MULTIPLY) {
        if (ma->use_nodes) {
          do_versions_material_convert_legacy_blend_mode(ntree, MA_BM_MULTIPLY);
        }
        ma->blend_method = MA_BM_BLEND;
      }
    }

    /* Update all ruler layers to set new flag. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      bGPdata *gpd = scene->gpd;
      if (gpd == nullptr) {
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
    LISTBASE_FOREACH (Mesh *, me, &bmain->meshes) {
      /* Check if we need to convert mfaces to polys. */
      if (me->totface_legacy && !me->faces_num) {
        /* temporarily switch main so that reading from
         * external CustomData works */
        Main *orig_gmain = BKE_blender_globals_main_swap(bmain);

        BKE_mesh_do_versions_convert_mfaces_to_mpolys(me);

        Main *tmp_gmain = BKE_blender_globals_main_swap(orig_gmain);
        BLI_assert(tmp_gmain == bmain);
        UNUSED_VARS_NDEBUG(tmp_gmain);
      }

      /* Deprecated, only kept for conversion. */
      BKE_mesh_tessface_clear(me);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 281, 2)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        update_math_node_single_operand_operators(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 281, 3)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        update_vector_math_node_add_and_subtract_operators(ntree);
        update_vector_math_node_dot_product_operator(ntree);
        update_vector_math_node_cross_product_operator(ntree);
        update_vector_math_node_normalize_operator(ntree);
        update_vector_math_node_average_operator(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 281, 7)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        update_noise_node_dimensions(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 281, 8)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        update_mapping_node_inputs_and_properties(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 281, 10)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        update_musgrave_node_dimensions(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 281, 11)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        update_voronoi_node_dimensions(ntree);
        update_voronoi_node_crackle(ntree);
        update_voronoi_node_coloring(ntree);
        update_voronoi_node_square_distance(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 282, 2)) {
    /* Init all Vertex/Sculpt and Weight Paint brushes. */
    Material *ma;
    /* Pen Soft brush. */
    do_versions_rename_id(bmain, ID_BR, "Draw Soft", "Pencil Soft");
    do_versions_rename_id(bmain, ID_BR, "Draw Pencil", "Pencil");
    do_versions_rename_id(bmain, ID_BR, "Draw Pen", "Pen");
    do_versions_rename_id(bmain, ID_BR, "Draw Ink", "Ink Pen");
    do_versions_rename_id(bmain, ID_BR, "Draw Noise", "Ink Pen Rough");
    do_versions_rename_id(bmain, ID_BR, "Draw Marker", "Marker Bold");
    do_versions_rename_id(bmain, ID_BR, "Draw Block", "Marker Chisel");

    ma = static_cast<Material *>(
        BLI_findstring(&bmain->materials, "Black", offsetof(ID, name) + 2));
    if (ma && ma->gp_style) {
      do_versions_rename_id(bmain, ID_MA, "Black", "Solid Stroke");
    }
    ma = static_cast<Material *>(BLI_findstring(&bmain->materials, "Red", offsetof(ID, name) + 2));
    if (ma && ma->gp_style) {
      do_versions_rename_id(bmain, ID_MA, "Red", "Squares Stroke");
    }
    ma = static_cast<Material *>(
        BLI_findstring(&bmain->materials, "Grey", offsetof(ID, name) + 2));
    if (ma && ma->gp_style) {
      do_versions_rename_id(bmain, ID_MA, "Grey", "Solid Fill");
    }
    ma = static_cast<Material *>(
        BLI_findstring(&bmain->materials, "Black Dots", offsetof(ID, name) + 2));
    if (ma && ma->gp_style) {
      do_versions_rename_id(bmain, ID_MA, "Black Dots", "Dots Stroke");
    }

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *ts = scene->toolsettings;

      /* Ensure new Paint modes. */
      BKE_paint_ensure_from_paintmode(scene, PaintMode::GPencil);
      BKE_paint_ensure_from_paintmode(scene, PaintMode::VertexGPencil);
      BKE_paint_ensure_from_paintmode(scene, PaintMode::SculptGPencil);
      BKE_paint_ensure_from_paintmode(scene, PaintMode::WeightGPencil);

      /* Enable cursor by default. */
      Paint *paint = &ts->gp_paint->paint;
      paint->flags |= PAINT_SHOW_BRUSH;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 282, 4)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        update_noise_and_wave_distortion(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 283, 4)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        update_wave_node_directions_and_offset(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 283, 8)) {

    /* During development of Blender 2.80 the "Object.hide" property was
     * removed, and reintroduced in 5e968a996a53 as "Object.hide_viewport". */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      BKE_fcurves_id_cb(&ob->id, [&](ID * /*id*/, FCurve *fcu) {
        if (fcu->rna_path == nullptr || !STREQ(fcu->rna_path, "hide")) {
          return;
        }

        MEM_freeN(fcu->rna_path);
        fcu->rna_path = BLI_strdupn("hide_viewport", 13);
      });
    }

    /* Reset all grease pencil brushes. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      /* Ensure new Paint modes. */
      BKE_paint_ensure_from_paintmode(scene, PaintMode::VertexGPencil);
      BKE_paint_ensure_from_paintmode(scene, PaintMode::SculptGPencil);
      BKE_paint_ensure_from_paintmode(scene, PaintMode::WeightGPencil);
    }
  }

  /* Old forgotten versioning code. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 39)) {
    /* Paint Brush. This ensure that the brush paints by default. Used during the development and
     * patch review of the initial Sculpt Vertex Colors implementation (D5975) */
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if (brush->ob_mode & OB_MODE_SCULPT && brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_PAINT) {
        brush->tip_roundness = 1.0f;
        brush->flow = 1.0f;
        brush->density = 1.0f;
        brush->tip_scale_x = 1.0f;
      }
    }

    /* Pose Brush with support for loose parts. */
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if (brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_POSE &&
          brush->disconnected_distance_max == 0.0f)
      {
        brush->flag2 |= BRUSH_USE_CONNECTED_ONLY;
        brush->disconnected_distance_max = 0.1f;
      }
    }

    /* 2.8x dropped support for non-empty dupli instances. but proper do-versioning was never
     * done correctly. So added here as a 'safe' place version wise, always better than in
     * readfile lib-linking code! */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      if (ob->type != OB_EMPTY && ob->instance_collection != nullptr) {
        BLO_reportf_wrap(fd->reports,
                         RPT_INFO,
                         RPT_("Non-Empty object '%s' cannot duplicate collection '%s' "
                              "anymore in Blender 2.80 and later, removed instancing"),
                         ob->id.name + 2,
                         ob->instance_collection->id.name + 2);
        ob->instance_collection = nullptr;
        ob->transflag &= ~OB_DUPLICOLLECTION;
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

/* NOTE: This version patch is intended for versions < 2.52.2,
 * but was initially introduced in 2.27 already.
 * But in 2.79 another case generating non-unique names was discovered
 * (see #55668, involving Meta strips). */
static void do_versions_seq_unique_name_all_strips(Scene *sce, ListBase *seqbasep)
{
  LISTBASE_FOREACH (Strip *, strip, seqbasep) {
    blender::seq::strip_unique_name_set(sce, &sce->ed->seqbase, strip);
    if (strip->seqbase.first != nullptr) {
      do_versions_seq_unique_name_all_strips(sce, &strip->seqbase);
    }
  }
}

static void do_versions_seq_set_cache_defaults(Editing *ed)
{
  ed->cache_flag = SEQ_CACHE_STORE_FINAL_OUT;
}

static bool strip_update_flags_cb(Strip *strip, void * /*user_data*/)
{
  strip->flag &= ~((1 << 6) | (1 << 18) | (1 << 19) | (1 << 21));
  if (strip->type == STRIP_TYPE_SPEED) {
    SpeedControlVars *s = (SpeedControlVars *)strip->effectdata;
    s->flags &= ~SEQ_SPEED_UNUSED_1;
  }
  return true;
}

enum class eNTreeDoVersionErrors : int8_t {
  NTREE_DOVERSION_NO_ERROR = 0,
  NTREE_DOVERSION_NEED_OUTPUT = (1 << 0),
  NTREE_DOVERSION_TRANSPARENCY_EMISSION = (1 << 1),
};
ENUM_OPERATORS(eNTreeDoVersionErrors);

/* NOLINTNEXTLINE: readability-function-size */
void blo_do_versions_280(FileData *fd, Library * /*lib*/, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 0)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->r.gauss = 1.5f;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 1)) {
    if (!DNA_struct_member_exists(fd->filesdna, "GPUDOFSettings", "float", "ratio")) {
      LISTBASE_FOREACH (Camera *, ca, &bmain->cameras) {
        ca->gpu_dof.ratio = 1.0f;
      }
    }

    /* MTexPoly now removed. */
    if (DNA_struct_exists(fd->filesdna, "MTexPoly")) {
      LISTBASE_FOREACH (Mesh *, me, &bmain->meshes) {
        /* If we have UVs, so this file will have MTexPoly layers too! */
        if (CustomData_has_layer(&me->corner_data, CD_MLOOPUV) ||
            CustomData_has_layer(&me->corner_data, CD_PROP_FLOAT2))
        {
          CustomData_update_typemap(&me->face_data);
          CustomData_free_layers(&me->face_data, CD_MTEXPOLY);
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 2)) {
    if (!DNA_struct_member_exists(fd->filesdna, "Light", "float", "cascade_max_dist")) {
      LISTBASE_FOREACH (Light *, la, &bmain->lights) {
        la->cascade_max_dist = 1000.0f;
        la->cascade_count = 4;
        la->cascade_exponent = 0.8f;
        la->cascade_fade = 0.1f;
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "LightProbe", "float", "vis_bias")) {
      LISTBASE_FOREACH (LightProbe *, probe, &bmain->lightprobes) {
        probe->vis_bias = 1.0f;
        probe->vis_blur = 0.2f;
      }
    }

    /* Eevee shader nodes renamed because of the output node system.
     * Note that a new output node is not being added here, because it would be overkill
     * to handle this case in lib_verify_nodetree.
     *
     * Also, metallic node is now unified into the principled node. */
    eNTreeDoVersionErrors error = eNTreeDoVersionErrors::NTREE_DOVERSION_NO_ERROR;

    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type_legacy == 194 /* SH_NODE_EEVEE_METALLIC */ &&
              STREQ(node->idname, "ShaderNodeOutputMetallic"))
          {
            STRNCPY_UTF8(node->idname, "ShaderNodeEeveeMetallic");
            error |= eNTreeDoVersionErrors::NTREE_DOVERSION_NEED_OUTPUT;
          }

          else if (node->type_legacy == SH_NODE_EEVEE_SPECULAR &&
                   STREQ(node->idname, "ShaderNodeOutputSpecular"))
          {
            STRNCPY_UTF8(node->idname, "ShaderNodeEeveeSpecular");
            error |= eNTreeDoVersionErrors::NTREE_DOVERSION_NEED_OUTPUT;
          }

          else if (node->type_legacy == 196 /* SH_NODE_OUTPUT_EEVEE_MATERIAL */ &&
                   STREQ(node->idname, "ShaderNodeOutputEeveeMaterial"))
          {
            node->type_legacy = SH_NODE_OUTPUT_MATERIAL;
            STRNCPY_UTF8(node->idname, "ShaderNodeOutputMaterial");
          }

          else if (node->type_legacy == 194 /* SH_NODE_EEVEE_METALLIC */ &&
                   STREQ(node->idname, "ShaderNodeEeveeMetallic"))
          {
            node->type_legacy = SH_NODE_BSDF_PRINCIPLED;
            STRNCPY_UTF8(node->idname, "ShaderNodeBsdfPrincipled");
            node->custom1 = SHD_GLOSSY_MULTI_GGX;
            error |= eNTreeDoVersionErrors::NTREE_DOVERSION_TRANSPARENCY_EMISSION;
          }
        }
      }
    }
    FOREACH_NODETREE_END;

    if (flag_is_set(error, eNTreeDoVersionErrors::NTREE_DOVERSION_NEED_OUTPUT)) {
      BKE_report(fd->reports != nullptr ? fd->reports->reports : nullptr,
                 RPT_ERROR,
                 "Eevee material conversion problem. Error in console");
      printf(
          "You need to connect Principled and Eevee Specular shader nodes to new material "
          "output "
          "nodes.\n");
    }

    if (flag_is_set(error, eNTreeDoVersionErrors::NTREE_DOVERSION_TRANSPARENCY_EMISSION)) {
      BKE_report(fd->reports != nullptr ? fd->reports->reports : nullptr,
                 RPT_ERROR,
                 "Eevee material conversion problem. Error in console");
      printf(
          "You need to combine transparency and emission shaders to the converted Principled "
          "shader nodes.\n");
    }

    {
      /* Init grease pencil edit line color */
      if (!DNA_struct_member_exists(fd->filesdna, "bGPdata", "float", "line_color[4]")) {
        LISTBASE_FOREACH (bGPdata *, gpd, &bmain->gpencils) {
          ARRAY_SET_ITEMS(gpd->line_color, 0.6f, 0.6f, 0.6f, 0.5f);
        }
      }

      /* Init grease pencil pixel size factor */
      if (!DNA_struct_member_exists(fd->filesdna, "bGPdata", "float", "pixfactor")) {
        LISTBASE_FOREACH (bGPdata *, gpd, &bmain->gpencils) {
          gpd->pixfactor = GP_DEFAULT_PIX_FACTOR;
        }
      }

      /* Grease pencil multi-frame falloff curve. */
      if (!DNA_struct_member_exists(
              fd->filesdna, "GP_Sculpt_Settings", "CurveMapping", "cur_falloff"))
      {
        LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
          /* sculpt brushes */
          GP_Sculpt_Settings &gset = scene->toolsettings->gp_sculpt;
          if (gset.cur_falloff == nullptr) {
            gset.cur_falloff = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
            BKE_curvemapping_init(gset.cur_falloff);
            BKE_curvemap_reset(gset.cur_falloff->cm,
                               &gset.cur_falloff->clipr,
                               CURVE_PRESET_GAUSS,
                               CurveMapSlopeType::Positive);
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

  /* Files from this version included do get a valid `win->screen` pointer written for backward
   * compatibility, however this should never be used nor needed, so clear these pointers here. */
  if (MAIN_VERSION_FILE_ATLEAST(bmain, 280, 1)) {
    LISTBASE_FOREACH (wmWindowManager *, wm, &bmain->wm) {
      LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
        win->screen = nullptr;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 3)) {
    /* init grease pencil grids and paper */
    if (!DNA_struct_member_exists(
            fd->filesdna, "View3DOverlay", "float", "gpencil_paper_color[3]"))
    {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = reinterpret_cast<View3D *>(sl);
              v3d->overlay.gpencil_paper_opacity = 0.5f;
              v3d->overlay.gpencil_grid_opacity = 0.9f;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 6)) {
    if (DNA_struct_member_exists(fd->filesdna, "SpaceOutliner", "int", "filter") == false) {
      /* Update files using invalid (outdated) outlinevis Outliner values. */
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_OUTLINER) {
              SpaceOutliner *space_outliner = (SpaceOutliner *)sl;

              if (!ELEM(space_outliner->outlinevis,
                        SO_SCENES,
                        SO_LIBRARIES,
                        SO_SEQUENCE,
                        SO_DATA_API,
                        SO_ID_ORPHANS))
              {
                space_outliner->outlinevis = SO_VIEW_LAYER;
              }
            }
          }
        }
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "LightProbe", "float", "intensity")) {
      LISTBASE_FOREACH (LightProbe *, probe, &bmain->lightprobes) {
        probe->intensity = 1.0f;
      }
    }

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            v3d->shading.light = V3D_LIGHTING_STUDIO;
            v3d->shading.flag |= V3D_SHADING_OBJECT_OUTLINE;

            /* Assume (demo) files written with 2.8 want to show
             * Eevee renders in the viewport. */
            if (MAIN_VERSION_FILE_ATLEAST(bmain, 280, 0)) {
              v3d->drawtype = OB_MATERIAL;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 7)) {
    /* Render engine storage moved elsewhere and back during 2.8
     * development, we assume any files saved in 2.8 had Eevee set
     * as scene render engine. */
    if (MAIN_VERSION_FILE_ATLEAST(bmain, 280, 0)) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        STRNCPY_UTF8(scene->r.engine, RE_engine_id_BLENDER_EEVEE);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 8)) {
    /* Blender Internal removal */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (STR_ELEM(scene->r.engine, "BLENDER_RENDER", "BLENDER_GAME")) {
        STRNCPY_UTF8(scene->r.engine, RE_engine_id_BLENDER_EEVEE);
      }
    }

    LISTBASE_FOREACH (Tex *, tex, &bmain->textures) {
      /* Removed environment map, point-density, voxel-data, ocean textures. */
      if (ELEM(tex->type, 10, 14, 15, 16)) {
        tex->type = 0;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 11)) {

    /* Remove info editor, but only if at the top of the window. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      /* Calculate window width/height from screen vertices */
      int win_width = 0, win_height = 0;
      LISTBASE_FOREACH (ScrVert *, vert, &screen->vertbase) {
        win_width = std::max<int>(win_width, vert->vec.x);
        win_height = std::max<int>(win_height, vert->vec.y);
      }

      for (ScrArea *area = static_cast<ScrArea *>(screen->areabase.first), *area_next; area;
           area = area_next)
      {
        area_next = static_cast<ScrArea *>(area->next);

        if (area->spacetype == SPACE_INFO) {
          if ((area->v2->vec.y == win_height) && (area->v1->vec.x == 0) &&
              (area->v4->vec.x == win_width))
          {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 11)) {
    LISTBASE_FOREACH (Light *, la, &bmain->lights) {
      if (la->mode & (1 << 13)) { /* LA_SHAD_RAY */
        la->mode |= LA_SHADOW;
        la->mode &= ~(1 << 13);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 12)) {
    /* Remove tool property regions. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (ELEM(sl->spacetype, SPACE_VIEW3D, SPACE_CLIP)) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;

            for (ARegion *region = static_cast<ARegion *>(regionbase->first), *region_next; region;
                 region = region_next)
            {
              region_next = static_cast<ARegion *>(region->next);

              if (region->regiontype == RGN_TYPE_TOOL_PROPS) {
                BKE_area_region_free(nullptr, region);
                BLI_freelinkN(regionbase, region);
              }
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 13)) {
    /* Initialize specular factor. */
    if (!DNA_struct_member_exists(fd->filesdna, "Light", "float", "spec_fac")) {
      LISTBASE_FOREACH (Light *, la, &bmain->lights) {
        la->spec_fac = 1.0f;
      }
    }

    /* Initialize new view3D options. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            v3d->shading.light = V3D_LIGHTING_STUDIO;
            v3d->shading.color_type = V3D_SHADING_MATERIAL_COLOR;
            copy_v3_fl(v3d->shading.single_color, 0.8f);
            v3d->shading.shadow_intensity = 0.5;

            v3d->overlay.normals_length = 0.1f;
            v3d->overlay.flag = 0;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 14)) {
    if (!DNA_struct_member_exists(fd->filesdna, "Scene", "SceneDisplay", "display")) {
      /* Initialize new scene.SceneDisplay */
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        const float vector[3] = {-M_SQRT1_3, -M_SQRT1_3, M_SQRT1_3};
        copy_v3_v3(scene->display.light_direction, vector);
      }
    }
    if (!DNA_struct_member_exists(fd->filesdna, "SceneDisplay", "float", "shadow_shift")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->display.shadow_shift = 0.1;
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "ToolSettings", "char", "transform_pivot_point")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->toolsettings->transform_pivot_point = V3D_AROUND_CENTER_MEDIAN;
      }
    }

    if (!DNA_struct_exists(fd->filesdna, "SceneEEVEE")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        /* First set the default for all the properties. */

        scene->eevee.gi_diffuse_bounces = 3;
        scene->eevee.gi_cubemap_resolution = 512;
        scene->eevee.gi_visibility_resolution = 32;

        scene->eevee.taa_samples = 16;
        scene->eevee.taa_render_samples = 64;

        scene->eevee.volumetric_start = 0.1f;
        scene->eevee.volumetric_end = 100.0f;
        scene->eevee.volumetric_tile_size = 8;
        scene->eevee.volumetric_samples = 64;
        scene->eevee.volumetric_sample_distribution = 0.8f;
        scene->eevee.volumetric_light_clamp = 0.0f;
        scene->eevee.volumetric_shadow_samples = 16;

        scene->eevee.gtao_distance = 0.2f;
        scene->eevee.fast_gi_quality = 0.25f;

        scene->eevee.bokeh_max_size = 100.0f;
        scene->eevee.bokeh_threshold = 1.0f;

        scene->eevee.motion_blur_samples = 8;
        scene->eevee.motion_blur_shutter_deprecated = 0.5f;

        scene->eevee.shadow_cube_size_deprecated = 512;

        scene->eevee.flag = SCE_EEVEE_TAA_REPROJECTION;

        /* If the file is pre-2.80 move on. */
        if (scene->layer_properties == nullptr) {
          continue;
        }

        /* Now we handle eventual properties that may be set in the file. */
#define EEVEE_GET_BOOL(_props, _name, _flag) \
  { \
    IDProperty *_idprop = IDP_GetPropertyFromGroup(_props, #_name); \
    if (_idprop != nullptr) { \
      const int _value = IDP_int_get(_idprop); \
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
    if (_idprop != nullptr) { \
      scene->eevee._name = IDP_int_get(_idprop); \
    } \
  } \
  ((void)0)

#define EEVEE_GET_FLOAT(_props, _name) \
  { \
    IDProperty *_idprop = IDP_GetPropertyFromGroup(_props, #_name); \
    if (_idprop != nullptr) { \
      scene->eevee._name = IDP_float_get(_idprop); \
    } \
  } \
  ((void)0)

#define EEVEE_GET_FLOAT_ARRAY(_props, _name, _length) \
  { \
    IDProperty *_idprop = IDP_GetPropertyFromGroup(_props, #_name); \
    if (_idprop != nullptr) { \
      const float *_values = static_cast<float *>(IDP_Array(_idprop)); \
      for (int _i = 0; _i < _length; _i++) { \
        scene->eevee._name[_i] = _values[_i]; \
      } \
    } \
  } \
  ((void)0)
        enum { SCE_EEVEE_DOF_ENABLED = (1 << 7) };
        IDProperty *props = IDP_GetPropertyFromGroup(scene->layer_properties,
                                                     RE_engine_id_BLENDER_EEVEE);
        // EEVEE_GET_BOOL(props, volumetric_enable, SCE_EEVEE_VOLUMETRIC_ENABLED);
        // EEVEE_GET_BOOL(props, volumetric_lights, SCE_EEVEE_VOLUMETRIC_LIGHTS);
        // EEVEE_GET_BOOL(props, volumetric_shadows, SCE_EEVEE_VOLUMETRIC_SHADOWS);
        EEVEE_GET_BOOL(props, gtao_enable, SCE_EEVEE_GTAO_ENABLED);
        // EEVEE_GET_BOOL(props, gtao_use_bent_normals, SCE_EEVEE_GTAO_BENT_NORMALS);
        // EEVEE_GET_BOOL(props, gtao_bounce, SCE_EEVEE_GTAO_BOUNCE);
        EEVEE_GET_BOOL(props, dof_enable, SCE_EEVEE_DOF_ENABLED);
        // EEVEE_GET_BOOL(props, bloom_enable, SCE_EEVEE_BLOOM_ENABLED);
        EEVEE_GET_BOOL(props, motion_blur_enable, SCE_EEVEE_MOTION_BLUR_ENABLED_DEPRECATED);
        // EEVEE_GET_BOOL(props, shadow_high_bitdepth, SCE_EEVEE_SHADOW_HIGH_BITDEPTH);
        EEVEE_GET_BOOL(props, taa_reprojection, SCE_EEVEE_TAA_REPROJECTION);
        // EEVEE_GET_BOOL(props, sss_enable, SCE_EEVEE_SSS_ENABLED);
        // EEVEE_GET_BOOL(props, sss_separate_albedo, SCE_EEVEE_SSS_SEPARATE_ALBEDO);
        EEVEE_GET_BOOL(props, ssr_enable, SCE_EEVEE_SSR_ENABLED);
        // EEVEE_GET_BOOL(props, ssr_refraction, SCE_EEVEE_SSR_REFRACTION);
        // EEVEE_GET_BOOL(props, ssr_halfres, SCE_EEVEE_SSR_HALF_RESOLUTION);

        EEVEE_GET_INT(props, gi_diffuse_bounces);
        EEVEE_GET_INT(props, gi_diffuse_bounces);
        EEVEE_GET_INT(props, gi_cubemap_resolution);
        EEVEE_GET_INT(props, gi_visibility_resolution);

        EEVEE_GET_INT(props, taa_samples);
        EEVEE_GET_INT(props, taa_render_samples);

        // EEVEE_GET_INT(props, sss_samples);
        // EEVEE_GET_FLOAT(props, sss_jitter_threshold);

        // EEVEE_GET_FLOAT(props, ssr_quality);
        // EEVEE_GET_FLOAT(props, ssr_max_roughness);
        // EEVEE_GET_FLOAT(props, ssr_thickness);
        // EEVEE_GET_FLOAT(props, ssr_border_fade);
        // EEVEE_GET_FLOAT(props, ssr_firefly_fac);

        EEVEE_GET_FLOAT(props, volumetric_start);
        EEVEE_GET_FLOAT(props, volumetric_end);
        EEVEE_GET_INT(props, volumetric_tile_size);
        EEVEE_GET_INT(props, volumetric_samples);
        EEVEE_GET_FLOAT(props, volumetric_sample_distribution);
        EEVEE_GET_FLOAT(props, volumetric_light_clamp);
        EEVEE_GET_INT(props, volumetric_shadow_samples);

        // EEVEE_GET_FLOAT(props, gtao_distance);
        // EEVEE_GET_FLOAT(props, gtao_factor);
        EEVEE_GET_FLOAT(props, fast_gi_quality);

        EEVEE_GET_FLOAT(props, bokeh_max_size);
        EEVEE_GET_FLOAT(props, bokeh_threshold);

        // EEVEE_GET_FLOAT_ARRAY(props, bloom_color, 3);
        // EEVEE_GET_FLOAT(props, bloom_threshold);
        // EEVEE_GET_FLOAT(props, bloom_knee);
        // EEVEE_GET_FLOAT(props, bloom_intensity);
        // EEVEE_GET_FLOAT(props, bloom_radius);
        // EEVEE_GET_FLOAT(props, bloom_clamp);

        EEVEE_GET_INT(props, motion_blur_samples);
        EEVEE_GET_FLOAT(props, motion_blur_shutter_deprecated);

        // EEVEE_GET_INT(props, shadow_method);
        EEVEE_GET_INT(props, shadow_cube_size_deprecated);
        // EEVEE_GET_INT(props, shadow_cascade_size);

        /* Cleanup. */
        IDP_FreeProperty(scene->layer_properties);
        scene->layer_properties = nullptr;

#undef EEVEE_GET_FLOAT_ARRAY
#undef EEVEE_GET_FLOAT
#undef EEVEE_GET_INT
#undef EEVEE_GET_BOOL
      }
    }

    if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 15)) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->display.matcap_ssao_distance = 0.2f;
        scene->display.matcap_ssao_attenuation = 1.0f;
        scene->display.matcap_ssao_samples = 16;
      }

      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_OUTLINER) {
              SpaceOutliner *space_outliner = (SpaceOutliner *)sl;
              space_outliner->filter_id_type = ID_GR;
              space_outliner->outlinevis = SO_VIEW_LAYER;
            }
          }
        }
      }

      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        switch (scene->toolsettings->snap_mode) {
          case 0:
            scene->toolsettings->snap_mode = (1 << 4); /* SCE_SNAP_TO_INCREMENT */
            break;
          case 1:
            scene->toolsettings->snap_mode = (1 << 0); /* SCE_SNAP_TO_VERTEX */
            break;
          case 2:
            scene->toolsettings->snap_mode = (1 << 1); /* SCE_SNAP_TO_EDGE */
            break;
          case 3:
            scene->toolsettings->snap_mode = (1 << 2); /* SCE_SNAP_INDIVIDUAL_PROJECT */
            break;
          case 4:
            scene->toolsettings->snap_mode = (1 << 3); /* SCE_SNAP_TO_VOLUME */
            break;
        }
        switch (scene->toolsettings->snap_node_mode) {
          case 5:
            scene->toolsettings->snap_node_mode = (1 << 5); /* SCE_SNAP_TO_NODE_X */
            break;
          case 6:
            scene->toolsettings->snap_node_mode = (1 << 6); /* SCE_SNAP_TO_NODE_Y */
            break;
          case 7:
            scene->toolsettings->snap_node_mode =
                (1 << 5) | (1 << 6); /* SCE_SNAP_TO_NODE_X | SCE_SNAP_TO_NODE_Y */
            break;
          case 8:
            scene->toolsettings->snap_node_mode = (1 << 7); /* SCE_SNAP_TO_GRID */
            break;
        }
        switch (scene->toolsettings->snap_uv_mode) {
          case 0:
            scene->toolsettings->snap_uv_mode = (1 << 4); /* SCE_SNAP_TO_INCREMENT */
            break;
          case 1:
            scene->toolsettings->snap_uv_mode = (1 << 0); /* SCE_SNAP_TO_VERTEX */
            break;
        }
      }

      LISTBASE_FOREACH (ParticleSettings *, part, &bmain->particles) {
        part->shape_flag = PART_SHAPE_CLOSE_TIP;
        part->shape = 0.0f;
        part->rad_root = 1.0f;
        part->rad_tip = 0.0f;
        part->rad_scale = 0.01f;
      }
    }
  }

  /* Particle shape shared with Eevee. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 16)) {
    LISTBASE_FOREACH (ParticleSettings *, part, &bmain->particles) {
      IDProperty *cpart = version_cycles_properties_from_ID(&part->id);

      if (cpart) {
        part->shape = version_cycles_property_float(cpart, "shape", 0.0);
        part->rad_root = version_cycles_property_float(cpart, "root_width", 1.0);
        part->rad_tip = version_cycles_property_float(cpart, "tip_width", 0.0);
        part->rad_scale = version_cycles_property_float(cpart, "radius_scale", 0.01);
        if (version_cycles_property_boolean(cpart, "use_closetip", true)) {
          part->shape_flag |= PART_SHAPE_CLOSE_TIP;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 18)) {
    if (!DNA_struct_member_exists(fd->filesdna, "Material", "float", "roughness")) {
      LISTBASE_FOREACH (Material *, mat, &bmain->materials) {
        if (mat->use_nodes) {
          if (MAIN_VERSION_FILE_ATLEAST(bmain, 280, 0)) {
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

      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

    if (!DNA_struct_member_exists(fd->filesdna, "View3DShading", "float", "xray_alpha")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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
    if (!DNA_struct_member_exists(fd->filesdna, "View3DShading", "char", "matcap[256]")) {
      StudioLight *default_matcap = BKE_studiolight_find_default(STUDIOLIGHT_TYPE_MATCAP);
      /* when loading the internal file is loaded before the matcaps */
      if (default_matcap) {
        LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
          LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
            LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
              if (sl->spacetype == SPACE_VIEW3D) {
                View3D *v3d = (View3D *)sl;
                STRNCPY(v3d->shading.matcap, default_matcap->name);
              }
            }
          }
        }
      }
    }
    if (!DNA_struct_member_exists(fd->filesdna, "View3DOverlay", "float", "wireframe_threshold")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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
    if (!DNA_struct_member_exists(fd->filesdna, "View3DShading", "float", "cavity_valley_factor"))
    {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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
    if (!DNA_struct_member_exists(fd->filesdna, "View3DOverlay", "float", "xray_alpha_bone")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 19)) {
    if (!DNA_struct_member_exists(fd->filesdna, "Image", "ListBase", "renderslot")) {
      LISTBASE_FOREACH (Image *, ima, &bmain->images) {
        if (ima->type == IMA_TYPE_R_RESULT) {
          for (int i = 0; i < 8; i++) {
            RenderSlot *slot = MEM_callocN<RenderSlot>("Image Render Slot Init");
            SNPRINTF_UTF8(slot->name, "Slot %d", i + 1);
            BLI_addtail(&ima->renderslots, slot);
          }
        }
      }
    }
    if (!DNA_struct_member_exists(fd->filesdna, "SpaceAction", "char", "mode_prev")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_ACTION) {
              SpaceAction *saction = (SpaceAction *)sl;
              /* "Dope-sheet" should be default here,
               * unless it looks like the Action Editor was active instead. */
              if ((saction->mode_prev == 0) && (saction->action == nullptr)) {
                saction->mode_prev = SACTCONT_DOPESHEET;
              }
            }
          }
        }
      }
    }

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 21)) {
    LISTBASE_FOREACH (Scene *, sce, &bmain->scenes) {
      if (sce->ed != nullptr && sce->ed->seqbase.first != nullptr) {
        do_versions_seq_unique_name_all_strips(sce, &sce->ed->seqbase);
      }
    }

    if (!DNA_struct_member_exists(
            fd->filesdna, "View3DOverlay", "float", "texture_paint_mode_opacity"))
    {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

    if (!DNA_struct_member_exists(fd->filesdna, "View3DShading", "char", "background_type")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

    if (!DNA_struct_member_exists(
            fd->filesdna, "RigidBodyWorld", "RigidBodyWorld_Shared", "*shared"))
    {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        RigidBodyWorld *rbw = scene->rigidbody_world;

        if (rbw == nullptr) {
          continue;
        }

        if (rbw->shared == nullptr) {
          rbw->shared = MEM_callocN<RigidBodyWorld_Shared>("RigidBodyWorld_Shared");
          BKE_rigidbody_world_init_runtime(rbw);
        }

        /* Move shared pointers from deprecated location to current location */
        rbw->shared->pointcache = rbw->pointcache;
        rbw->shared->ptcaches = rbw->ptcaches;

        rbw->pointcache = nullptr;
        BLI_listbase_clear(&rbw->ptcaches);

        if (rbw->shared->pointcache == nullptr) {
          rbw->shared->pointcache = BKE_ptcache_add(&(rbw->shared->ptcaches));
        }
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "SoftBody", "SoftBody_Shared", "*shared")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        SoftBody *sb = ob->soft;
        if (sb == nullptr) {
          continue;
        }
        if (sb->shared == nullptr) {
          sb->shared = MEM_callocN<SoftBody_Shared>("SoftBody_Shared");
        }

        /* Move shared pointers from deprecated location to current location */
        sb->shared->pointcache = sb->pointcache;
        sb->shared->ptcaches = sb->ptcaches;

        sb->pointcache = nullptr;
        BLI_listbase_clear(&sb->ptcaches);
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "View3DShading", "short", "type")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

    if (!DNA_struct_member_exists(fd->filesdna, "SceneDisplay", "View3DShading", "shading")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        BKE_screen_view3d_shading_init(&scene->display.shading);
      }
    }
    /* initialize grease pencil view data */
    if (!DNA_struct_member_exists(fd->filesdna, "SpaceView3D", "float", "vertex_opacity")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 22)) {
    if (!DNA_struct_member_exists(fd->filesdna, "ToolSettings", "char", "annotate_v3d_align")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->toolsettings->annotate_v3d_align = GP_PROJECT_VIEWSPACE | GP_PROJECT_CURSOR;
        scene->toolsettings->annotate_thickness = 3;
      }
    }
    if (!DNA_struct_member_exists(fd->filesdna, "bGPDlayer", "short", "line_change")) {
      LISTBASE_FOREACH (bGPdata *, gpd, &bmain->gpencils) {
        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          gpl->line_change = gpl->thickness;
          if ((gpl->thickness < 1) || (gpl->thickness > 10)) {
            gpl->thickness = 3;
          }
        }
      }
    }
    if (!DNA_struct_member_exists(fd->filesdna, "View3DOverlay", "float", "gpencil_paper_opacity"))
    {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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
    if (!DNA_struct_member_exists(fd->filesdna, "View3DOverlay", "float", "gpencil_grid_opacity"))
    {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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
    if (!DNA_struct_member_exists(fd->filesdna, "GP_Sculpt_Settings", "int", "lock_axis")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        /* lock axis */
        GP_Sculpt_Settings &gset = scene->toolsettings->gp_sculpt;
        gset.lock_axis = GP_LOCKAXIS_Y;
      }
    }

    /* Versioning code for Subsurf modifier. */
    if (!DNA_struct_member_exists(fd->filesdna, "SubsurfModifier", "short", "uv_smooth")) {
      LISTBASE_FOREACH (Object *, object, &bmain->objects) {
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

    if (!DNA_struct_member_exists(fd->filesdna, "SubsurfModifier", "short", "quality")) {
      LISTBASE_FOREACH (Object *, object, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
          if (md->type == eModifierType_Subsurf) {
            SubsurfModifierData *smd = (SubsurfModifierData *)md;
            smd->quality = min_ii(smd->renderLevels, 3);
          }
        }
      }
    }
    /* Versioning code for Multires modifier. */
    if (!DNA_struct_member_exists(fd->filesdna, "MultiresModifier", "short", "quality")) {
      LISTBASE_FOREACH (Object *, object, &bmain->objects) {
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

    if (!DNA_struct_member_exists(fd->filesdna, "ClothSimSettings", "short", "bending_model")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          ClothModifierData *clmd = nullptr;
          if (md->type == eModifierType_Cloth) {
            clmd = (ClothModifierData *)md;
          }
          else if (md->type == eModifierType_ParticleSystem) {
            ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)md;
            ParticleSystem *psys = psmd->psys;
            clmd = psys->clmd;
          }
          if (clmd != nullptr) {
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

    if (!DNA_struct_member_exists(fd->filesdna, "BrushGpencilSettings", "float", "era_strength_f"))
    {
      LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
        if (brush->gpencil_settings != nullptr) {
          BrushGpencilSettings *gp = brush->gpencil_settings;
          if (gp->brush_type == GPAINT_BRUSH_TYPE_ERASE) {
            gp->era_strength_f = 100.0f;
            gp->era_thickness_f = 10.0f;
          }
        }
      }
    }

    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 24)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;
            v3d->overlay.edit_flag |= V3D_OVERLAY_EDIT_FACES | V3D_OVERLAY_EDIT_SEAMS |
                                      V3D_OVERLAY_EDIT_SHARP | V3D_OVERLAY_EDIT_FREESTYLE_EDGE |
                                      V3D_OVERLAY_EDIT_FREESTYLE_FACE | V3D_OVERLAY_EDIT_CREASES |
                                      V3D_OVERLAY_EDIT_BWEIGHTS;
          }
        }
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "ShrinkwrapModifierData", "char", "shrinkMode")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
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

    if (!DNA_struct_member_exists(fd->filesdna, "PartDeflect", "float", "pdef_cfrict")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
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

    if (!DNA_struct_member_exists(fd->filesdna, "View3DShading", "float", "xray_alpha_wire")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 25)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      UnitSettings *unit = &scene->unit;
      if (unit->system != USER_UNIT_NONE) {
        unit->length_unit = BKE_unit_base_of_type_get(scene->unit.system, B_UNIT_LENGTH);
        unit->mass_unit = BKE_unit_base_of_type_get(scene->unit.system, B_UNIT_MASS);
      }
      unit->time_unit = BKE_unit_base_of_type_get(USER_UNIT_NONE, B_UNIT_TIME);
    }

    /* gpencil grid settings */
    LISTBASE_FOREACH (bGPdata *, gpd, &bmain->gpencils) {
      ARRAY_SET_ITEMS(gpd->grid.color, 0.5f, 0.5f, 0.5f); /* Color */
      ARRAY_SET_ITEMS(gpd->grid.scale, 1.0f, 1.0f);       /* Scale */
      gpd->grid.lines = GP_DEFAULT_GRID_LINES;            /* Number of lines */
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 29)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            enum { V3D_OCCLUDE_WIRE = (1 << 14) };
            View3D *v3d = (View3D *)sl;
            if (v3d->flag2 & V3D_OCCLUDE_WIRE) {
              v3d->overlay.edit_flag |= V3D_OVERLAY_EDIT_RETOPOLOGY;
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
    if (!MAIN_VERSION_FILE_ATLEAST(bmain, 283, 1)) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_PROPERTIES) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;
            ARegion *region = BKE_area_region_new();
            ARegion *region_header = nullptr;

            for (region_header = static_cast<ARegion *>(regionbase->first);
                 region_header != nullptr;
                 region_header = static_cast<ARegion *>(region_header->next))
            {
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
    if (!DNA_struct_member_exists(fd->filesdna, "View3DOverlay", "float", "gpencil_fade_layer")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 30)) {
    /* grease pencil main material show switches */
    LISTBASE_FOREACH (Material *, mat, &bmain->materials) {
      if (mat->gp_style) {
        mat->gp_style->flag |= GP_MATERIAL_STROKE_SHOW;
        mat->gp_style->flag |= GP_MATERIAL_FILL_SHOW;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 33)) {

    if (!DNA_struct_member_exists(fd->filesdna, "SceneEEVEE", "float", "overscan")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->eevee.overscan = 3.0f;
      }
    }

    LISTBASE_FOREACH (Light *, la, &bmain->lights) {
      /* Removed Hemi lights. */
      if (!ELEM(la->type, LA_LOCAL, LA_SUN, LA_SPOT, LA_AREA)) {
        la->type = LA_SUN;
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "SceneEEVEE", "float", "light_threshold")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->eevee.light_threshold = 0.01f;
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "Light", "float", "att_dist")) {
      LISTBASE_FOREACH (Light *, la, &bmain->lights) {
        la->att_dist = la->clipend_deprecated;
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "Brush", "char", "weight_brush_type")) {
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

      LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
        if (brush->ob_mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT)) {
          const char tool_init = brush->vertex_brush_type;
          bool is_blend = false;

          {
            char tool;
            switch (tool_init) {
              case PAINT_BLEND_MIX:
                tool = VPAINT_BRUSH_TYPE_DRAW;
                break;
              case PAINT_BLEND_BLUR:
                tool = VPAINT_BRUSH_TYPE_BLUR;
                break;
              case PAINT_BLEND_AVERAGE:
                tool = VPAINT_BRUSH_TYPE_AVERAGE;
                break;
              case PAINT_BLEND_SMEAR:
                tool = VPAINT_BRUSH_TYPE_SMEAR;
                break;
              default:
                tool = VPAINT_BRUSH_TYPE_DRAW;
                is_blend = true;
                break;
            }
            brush->vertex_brush_type = tool;
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
        brush->weight_brush_type = brush->vertex_brush_type;
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 34)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, slink, &area->spacedata) {
          if (slink->spacetype == SPACE_USERPREF) {
            ARegion *navigation_region = BKE_spacedata_find_region_type(
                slink, area, RGN_TYPE_NAV_BAR);

            if (!navigation_region) {
              ARegion *main_region = BKE_spacedata_find_region_type(slink, area, RGN_TYPE_WINDOW);
              ListBase *regionbase = (slink == area->spacedata.first) ? &area->regionbase :
                                                                        &slink->regionbase;

              navigation_region = BKE_area_region_new();

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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 36)) {
    if (!DNA_struct_member_exists(
            fd->filesdna, "View3DShading", "float", "curvature_ridge_factor"))
    {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (STREQ(scene->r.engine, "BLENDER_OPENGL")) {
        STRNCPY(scene->r.engine, RE_engine_id_BLENDER_WORKBENCH);
      }
    }

    /* init Annotations onion skin */
    if (!DNA_struct_member_exists(fd->filesdna, "bGPDlayer", "int", "gstep")) {
      LISTBASE_FOREACH (bGPdata *, gpd, &bmain->gpencils) {
        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          ARRAY_SET_ITEMS(gpl->gcolor_prev, 0.302f, 0.851f, 0.302f);
          ARRAY_SET_ITEMS(gpl->gcolor_next, 0.250f, 0.1f, 1.0f);
        }
      }
    }

    /* Move studio_light selection to lookdev_light. */
    if (!DNA_struct_member_exists(fd->filesdna, "View3DShading", "char", "lookdev_light[256]")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              memcpy(v3d->shading.lookdev_light, v3d->shading.studio_light, sizeof(char[256]));
            }
          }
        }
      }
    }

    /* Change Solid mode shadow orientation. */
    if (!DNA_struct_member_exists(fd->filesdna, "SceneDisplay", "float", "shadow_focus")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        float *dir = scene->display.light_direction;
        std::swap(dir[2], dir[1]);
        dir[2] = -dir[2];
        dir[0] = -dir[0];
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 37)) {
    LISTBASE_FOREACH (Camera *, ca, &bmain->cameras) {
      ca->drawsize *= 2.0f;
    }

    /* Grease pencil primitive curve */
    if (!DNA_struct_member_exists(
            fd->filesdna, "GP_Sculpt_Settings", "CurveMapping", "cur_primitive"))
    {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        GP_Sculpt_Settings &gset = scene->toolsettings->gp_sculpt;
        if (gset.cur_primitive == nullptr) {
          gset.cur_primitive = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
          BKE_curvemapping_init(gset.cur_primitive);
          BKE_curvemap_reset(gset.cur_primitive->cm,
                             &gset.cur_primitive->clipr,
                             CURVE_PRESET_BELL,
                             CurveMapSlopeType::Positive);
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 38)) {
    if (DNA_struct_member_exists(fd->filesdna, "Object", "char", "empty_image_visibility_flag")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        ob->empty_image_visibility_flag ^= (OB_EMPTY_IMAGE_HIDE_PERSPECTIVE |
                                            OB_EMPTY_IMAGE_HIDE_ORTHOGRAPHIC |
                                            OB_EMPTY_IMAGE_HIDE_BACK);
      }
    }

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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
              v3d->flag2 &= ~((1 << 3) | V3D_FLAG2_UNUSED_6 | V3D_FLAG2_UNUSED_12 |
                              V3D_FLAG2_UNUSED_13 | V3D_FLAG2_UNUSED_14 | V3D_FLAG2_UNUSED_15);
              break;
            }
            case SPACE_OUTLINER: {
              SpaceOutliner *space_outliner = (SpaceOutliner *)sl;
              space_outliner->filter &= ~(SO_FILTER_CLEARED_1 | SO_FILTER_UNUSED_5 |
                                          SO_FILTER_OB_STATE_SELECTABLE);
              space_outliner->storeflag &= ~SO_TREESTORE_UNUSED_1;
              break;
            }
            case SPACE_FILE: {
              SpaceFile *sfile = (SpaceFile *)sl;
              if (sfile->params) {
                sfile->params->flag &= ~(FILE_PARAMS_FLAG_UNUSED_1 | FILE_PARAMS_FLAG_UNUSED_2 |
                                         FILE_PARAMS_FLAG_UNUSED_3);
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

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->r.mode &= ~(R_SIMPLIFY_NORMALS | R_MODE_UNUSED_2 | R_MODE_UNUSED_3 | R_MODE_UNUSED_4 |
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
        blender::seq::foreach_strip(&scene->ed->seqbase, strip_update_flags_cb, nullptr);
      }
    }

    LISTBASE_FOREACH (World *, world, &bmain->worlds) {
      world->flag &= ~(WO_MODE_UNUSED_1 | WO_MODE_UNUSED_2 | WO_MODE_UNUSED_3 | WO_MODE_UNUSED_4 |
                       WO_MODE_UNUSED_5 | WO_MODE_UNUSED_7);
    }

    LISTBASE_FOREACH (Image *, image, &bmain->images) {
      image->flag &= ~(IMA_HIGH_BITDEPTH | IMA_FLAG_UNUSED_1 | IMA_FLAG_UNUSED_4 |
                       IMA_FLAG_UNUSED_6 | IMA_FLAG_UNUSED_8 | IMA_FLAG_UNUSED_15 |
                       IMA_FLAG_UNUSED_16);
    }

    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      ob->flag &= ~(OB_FLAG_USE_SIMULATION_CACHE | OB_FLAG_ACTIVE_CLIPBOARD);
      ob->transflag &= ~(OB_TRANSFORM_ADJUST_ROOT_PARENT_FOR_VIEW_LOCK | OB_TRANSFLAG_UNUSED_1);
      ob->shapeflag &= ~OB_SHAPE_FLAG_UNUSED_1;
    }

    LISTBASE_FOREACH (Mesh *, me, &bmain->meshes) {
      me->flag &= ~(ME_FLAG_UNUSED_0 | ME_FLAG_UNUSED_1 | ME_FLAG_UV_SELECT_SYNC_VALID |
                    ME_FLAG_UNUSED_4 | ME_FLAG_UNUSED_6 | ME_FLAG_UNUSED_7 |
                    ME_REMESH_REPROJECT_ATTRIBUTES);
    }

    LISTBASE_FOREACH (Material *, mat, &bmain->materials) {
      mat->blend_flag &= ~(1 << 2); /* UNUSED */
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 40)) {
    if (!DNA_struct_member_exists(
            fd->filesdna, "ToolSettings", "char", "snap_transform_mode_flag"))
    {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->toolsettings->snap_transform_mode_flag = SCE_SNAP_TRANSFORM_MODE_TRANSLATE;
      }
    }

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

    if (!DNA_struct_exists(fd->filesdna, "TransformOrientationSlot")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        for (int i = 0; i < ARRAY_SIZE(scene->orientation_slots); i++) {
          scene->orientation_slots[i].index_custom = -1;
        }
      }
    }

    /* Grease pencil cutter/select segment intersection threshold. */
    if (!DNA_struct_member_exists(fd->filesdna, "GP_Sculpt_Settings", "float", "isect_threshold"))
    {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        GP_Sculpt_Settings &gset = scene->toolsettings->gp_sculpt;
        gset.isect_threshold = 0.1f;
      }
    }

    /* Fix anamorphic bokeh eevee rna limits. */
    LISTBASE_FOREACH (Camera *, ca, &bmain->cameras) {
      ca->gpu_dof.ratio = std::max(ca->gpu_dof.ratio, 0.01f);
    }

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_USERPREF) {
            ARegion *execute_region = BKE_spacedata_find_region_type(sl, area, RGN_TYPE_EXECUTE);

            if (!execute_region) {
              ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                     &sl->regionbase;
              ARegion *region_navbar = BKE_spacedata_find_region_type(sl, area, RGN_TYPE_NAV_BAR);

              execute_region = BKE_area_region_new();

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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 43)) {
    ListBase *lb = which_libbase(bmain, ID_BR);
    BKE_main_id_repair_duplicate_names_listbase(bmain, lb);
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 44)) {
    if (!DNA_struct_member_exists(fd->filesdna, "Material", "float", "a")) {
      LISTBASE_FOREACH (Material *, mat, &bmain->materials) {
        mat->a = 1.0f;
      }
    }

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
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
      if (ts->sculpt != nullptr) {
        ts->sculpt->flags &= ~SCULPT_FLAG_UNUSED_6;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 46)) {
    /* Add wireframe color. */
    if (!DNA_struct_member_exists(fd->filesdna, "View3DShading", "char", "wire_color_type")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

    if (!DNA_struct_member_exists(fd->filesdna, "View3DCursor", "short", "rotation_mode")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        if (is_zero_v3(scene->cursor.rotation_axis)) {
          scene->cursor.rotation_mode = ROT_MODE_XYZ;
          scene->cursor.rotation_quaternion[0] = 1.0f;
          scene->cursor.rotation_axis[1] = 1.0f;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 47)) {
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

      ob->transflag &= ~(OB_TRANSFORM_ADJUST_ROOT_PARENT_FOR_VIEW_LOCK | OB_TRANSFLAG_UNUSED_1 |
                         OB_TRANSFLAG_UNUSED_3 | OB_TRANSFLAG_UNUSED_6 | OB_TRANSFLAG_UNUSED_12);

      ob->nlaflag &= ~(OB_ADS_UNUSED_1 | OB_ADS_UNUSED_2);
    }

    LISTBASE_FOREACH (bArmature *, arm, &bmain->armatures) {
      arm->flag &= ~(ARM_FLAG_UNUSED_1 | ARM_DRAW_RELATION_FROM_HEAD | ARM_BCOLL_SOLO_ACTIVE |
                     ARM_FLAG_UNUSED_7 | ARM_FLAG_UNUSED_12);
    }

    LISTBASE_FOREACH (Text *, text, &bmain->texts) {
      text->flags &= ~(TXT_FLAG_UNUSED_8 | TXT_FLAG_UNUSED_9);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 48)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      /* Those are not currently used, but are accessible through RNA API and were not
       * properly initialized previously. This is mere copy of #scene_init_data code. */
      if (scene->r.im_format.view_settings.look[0] == '\0') {
        BKE_color_managed_display_settings_init(&scene->r.im_format.display_settings);
        BKE_color_managed_view_settings_init(
            &scene->r.im_format.view_settings, &scene->r.im_format.display_settings, "Filmic");
      }

      if (scene->r.bake.im_format.view_settings.look[0] == '\0') {
        BKE_color_managed_display_settings_init(&scene->r.bake.im_format.display_settings);
        BKE_color_managed_view_settings_init(&scene->r.bake.im_format.view_settings,
                                             &scene->r.bake.im_format.display_settings,
                                             "Filmic");
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 49)) {
    /* All tool names changed, reset to defaults. */
    LISTBASE_FOREACH (WorkSpace *, workspace, &bmain->workspaces) {
      while (!BLI_listbase_is_empty(&workspace->tools)) {
        BKE_workspace_tool_remove(workspace, static_cast<bToolRef *>(workspace->tools.first));
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 52)) {
    LISTBASE_FOREACH (ParticleSettings *, part, &bmain->particles) {
      /* Replace deprecated PART_DRAW_BB by PART_DRAW_NOT */
      if (part->ren_as == PART_DRAW_BB) {
        part->ren_as = PART_DRAW_NOT;
      }
      if (part->draw_as == PART_DRAW_BB) {
        part->draw_as = PART_DRAW_NOT;
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "TriangulateModifierData", "int", "min_vertices"))
    {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
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
            STRNCPY_UTF8(node->idname, "ShaderNodeOutputLight");
          }
          if (node->type_legacy == SH_NODE_BSDF_PRINCIPLED && node->custom2 == 0) {
            node->custom2 = SHD_SUBSURFACE_BURLEY;
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 53)) {
    LISTBASE_FOREACH (Material *, mat, &bmain->materials) {
      /* Eevee: Keep material appearance consistent with previous behavior. */
      if (!mat->use_nodes || !mat->nodetree || mat->blend_method == MA_BM_SOLID) {
        mat->blend_shadow = MA_BS_SOLID;
      }
    }

    /* grease pencil default animation channel color */
    {
      LISTBASE_FOREACH (bGPdata *, gpd, &bmain->gpencils) {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 54)) {
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 55)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 56)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 57)) {
    /* Enable Show Interpolation in dope-sheet by default. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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
    if (!DNA_struct_member_exists(fd->filesdna, "BrushGpencilSettings", "float", "hardness")) {
      LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
        if (brush->gpencil_settings != nullptr) {
          BrushGpencilSettings *gp = brush->gpencil_settings;
          gp->hardness = 1.0f;
          copy_v2_fl(gp->aspect_ratio, 1.0f);
        }
      }
    }

    /* init grease pencil stroke gradients */
    if (!DNA_struct_member_exists(fd->filesdna, "bGPDstroke", "float", "hardness")) {
      LISTBASE_FOREACH (bGPdata *, gpd, &bmain->gpencils) {
        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
            LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
              gps->hardness = 1.0f;
              copy_v2_fl(gps->aspect_ratio, 1.0f);
            }
          }
        }
      }
    }

    /* enable the axis aligned ortho grid by default */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                 &sl->regionbase;
          /* All spaces that use tools must be eventually added. */
          ARegion *region = nullptr;
          if (ELEM(sl->spacetype, SPACE_VIEW3D, SPACE_IMAGE, SPACE_SEQ) &&
              ((region = do_versions_find_region_or_null(regionbase, RGN_TYPE_TOOL_HEADER)) ==
               nullptr))
          {
            /* Add tool header. */
            region = do_versions_add_region(RGN_TYPE_TOOL_HEADER, "tool header");
            region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

            ARegion *region_header = do_versions_find_region(regionbase, RGN_TYPE_HEADER);
            BLI_insertlinkbefore(regionbase, region_header, region);
            /* Hide by default, enable for painting workspaces (startup only). */
            region->flag |= RGN_FLAG_HIDDEN | RGN_FLAG_HIDDEN_BY_USER;
          }
          if (region != nullptr) {
            SET_FLAG_FROM_TEST(
                region->flag, region->flag & RGN_FLAG_HIDDEN_BY_USER, RGN_FLAG_HIDDEN);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 60)) {
    if (!DNA_struct_member_exists(fd->filesdna, "bSplineIKConstraint", "short", "yScaleMode")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
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

    if (!DNA_struct_member_exists(
            fd->filesdna, "View3DOverlay", "float", "sculpt_mode_mask_opacity"))
    {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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
    if (!DNA_struct_member_exists(fd->filesdna, "SceneDisplay", "char", "render_aa")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->display.render_aa = SCE_DISPLAY_AA_SAMPLES_8;
        scene->display.viewport_aa = SCE_DISPLAY_AA_FXAA;
      }
    }

    /* Split bbone_scalein/bbone_scaleout into x and y fields. */
    if (!DNA_struct_member_exists(fd->filesdna, "bPoseChannel", "float", "scale_out_z")) {
      /* Update armature data and pose channels. */
      LISTBASE_FOREACH (bArmature *, arm, &bmain->armatures) {
        do_version_bones_split_bbone_scale(&arm->bonebase);
      }

      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        if (ob->pose) {
          LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
            pchan->scale_in_z = pchan->scale_in_x;
            pchan->scale_out_z = pchan->scale_out_x;
          }
        }
      }

      /* Update action curves and drivers. */
      LISTBASE_FOREACH (bAction *, act, &bmain->actions) {
        LISTBASE_FOREACH_MUTABLE (FCurve *, fcu, &act->curves) {
          do_version_bbone_scale_fcurve_fix(&act->curves, fcu);
        }
      }

      BKE_animdata_main_cb(bmain, [](ID * /*id*/, AnimData *adt) {
        LISTBASE_FOREACH_MUTABLE (FCurve *, fcu, &adt->drivers) {
          do_version_bbone_scale_fcurve_fix(&adt->drivers, fcu);
        }
      });
    }

    LISTBASE_FOREACH (Scene *, sce, &bmain->scenes) {
      if (sce->ed != nullptr) {
        do_versions_seq_set_cache_defaults(sce->ed);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 61)) {
    /* Added a power option to Copy Scale. */
    if (!DNA_struct_member_exists(fd->filesdna, "bSizeLikeConstraint", "float", "power")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        do_version_constraints_copy_scale_power(&ob->constraints);
        if (ob->pose) {
          LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
            do_version_constraints_copy_scale_power(&pchan->constraints);
          }
        }
      }
    }

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (ELEM(sl->spacetype, SPACE_CLIP, SPACE_GRAPH, SPACE_SEQ)) {
            ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                   &sl->regionbase;

            ARegion *region = nullptr;
            if (sl->spacetype == SPACE_CLIP) {
              if (((SpaceClip *)sl)->view == SC_VIEW_GRAPH) {
                region = do_versions_find_region_or_null(regionbase, RGN_TYPE_PREVIEW);
              }
            }
            else {
              region = do_versions_find_region_or_null(regionbase, RGN_TYPE_WINDOW);
            }

            if (region != nullptr) {
              region->v2d.scroll &= ~V2D_SCROLL_LEFT;
              region->v2d.scroll |= V2D_SCROLL_RIGHT;
            }
          }
        }
      }
    }

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype != SPACE_OUTLINER) {
            continue;
          }
          SpaceOutliner *space_outliner = (SpaceOutliner *)sl;
          space_outliner->filter &= ~SO_FLAG_UNUSED_1;
          space_outliner->show_restrict_flags = SO_RESTRICT_ENABLE | SO_RESTRICT_HIDE;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 68)) {
    /* Unify Cycles and Eevee film transparency. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (STREQ(scene->r.engine, RE_engine_id_CYCLES)) {
        IDProperty *cscene = version_cycles_properties_from_ID(&scene->id);
        if (cscene) {
          bool cycles_film_transparency = version_cycles_property_boolean(
              cscene, "film_transparent", false);
          scene->r.alphamode = cycles_film_transparency ? R_ALPHAPREMUL : R_ADDSKY;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 69)) {
    LISTBASE_FOREACH (bArmature *, arm, &bmain->armatures) {
      arm->flag &= ~(ARM_FLAG_UNUSED_7 | ARM_FLAG_UNUSED_9);
    }

    /* Initializes sun lights with the new angular diameter property */
    if (!DNA_struct_member_exists(fd->filesdna, "Light", "float", "sun_angle")) {
      LISTBASE_FOREACH (Light *, light, &bmain->lights) {
        light->sun_angle = 2.0f * atanf(light->area_size);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 70)) {
    /* New image alpha modes. */
    LISTBASE_FOREACH (Image *, image, &bmain->images) {
      enum { IMA_IGNORE_ALPHA = (1 << 12) };
      if (image->flag & IMA_IGNORE_ALPHA) {
        image->alpha_mode = IMA_ALPHA_IGNORE;
        image->flag &= ~IMA_IGNORE_ALPHA;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 71)) {
    /* This assumes the Blender builtin config. Depending on the OCIO
     * environment variable for versioning is weak, and these deprecated view
     * transforms and look names don't seem to exist in other commonly used
     * OCIO configs so .blend files created for those would be unaffected. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ColorManagedViewSettings *view_settings;
      view_settings = &scene->view_settings;

      if (STREQ(view_settings->view_transform, "Default")) {
        STRNCPY_UTF8(view_settings->view_transform, "Standard");
      }
      else if (STR_ELEM(view_settings->view_transform, "RRT", "Film")) {
        STRNCPY_UTF8(view_settings->view_transform, "Filmic");
      }
      else if (STREQ(view_settings->view_transform, "Log")) {
        STRNCPY_UTF8(view_settings->view_transform, "Filmic Log");
      }

      if (STREQ(view_settings->look, "Filmic - Base Contrast")) {
        STRNCPY_UTF8(view_settings->look, "None");
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 74)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed != nullptr) {
        do_versions_seq_alloc_transform_and_crop(&scene->ed->seqbase);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 280, 75)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->master_collection != nullptr) {
        scene->master_collection->flag &= ~(COLLECTION_HIDE_VIEWPORT | COLLECTION_HIDE_SELECT |
                                            COLLECTION_HIDE_RENDER);
      }

      UnitSettings *unit = &scene->unit;
      if (unit->system == USER_UNIT_NONE) {
        unit->length_unit = char(USER_UNIT_ADAPTIVE);
        unit->mass_unit = char(USER_UNIT_ADAPTIVE);
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
      arm->flag &= ~ARM_BCOLL_SOLO_ACTIVE;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 281, 1)) {
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_DataTransfer) {
          /* Now data-transfer's mix factor is multiplied with weights when any,
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 281, 3)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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
            SpaceOutliner *space_outliner = (SpaceOutliner *)sl;
            space_outliner->sync_select_dirty |= WM_OUTLINER_SYNC_SELECT_FROM_ALL;
            space_outliner->flag |= SO_SYNC_SELECT;
          }
        }
      }
    }
    LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
      if (mesh->remesh_voxel_size == 0.0f) {
        mesh->remesh_voxel_size = 0.1f;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 281, 3)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        update_vector_math_node_operators_enum_mapping(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 281, 4)) {
    ID *id;
    FOREACH_MAIN_ID_BEGIN (bmain, id) {
      bNodeTree *ntree = blender::bke::node_tree_from_id(id);
      if (ntree) {
        ntree->id.flag |= ID_FLAG_EMBEDDED_DATA;
      }
    }
    FOREACH_MAIN_ID_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 281, 5)) {
    LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
      if (br->ob_mode & OB_MODE_SCULPT && br->normal_radius_factor == 0.0f) {
        br->normal_radius_factor = 0.5f;
      }
    }

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      /* Older files do not have a master collection, which is then added through
       * `BKE_collection_master_add()`, so everything is fine. */
      if (scene->master_collection != nullptr) {
        scene->master_collection->id.flag |= ID_FLAG_EMBEDDED_DATA;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 281, 6)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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
            if (studio_light != nullptr) {
              STRNCPY(v3d->shading.lookdev_light, studio_light->name);
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 281, 9)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

            /* Check, even though this is expected to be valid. */
            if (region_ui) {
              /* Reinsert UI region so that it spawns entire area width. */
              BLI_remlink(regionbase, region_ui);
              BLI_insertlinkafter(regionbase, region_header, region_ui);

              region_ui->flag |= RGN_FLAG_DYNAMIC_SIZE;
            }

            if (region_toolprops &&
                (region_toolprops->alignment == (RGN_ALIGN_BOTTOM | RGN_SPLIT_PREV)))
            {
              SpaceType *stype = BKE_spacetype_from_id(sl->spacetype);

              /* Remove empty region at old location. */
              BLI_assert(sfile->op == nullptr);
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
    if (!DNA_struct_member_exists(fd->filesdna, "Bone", "char", "inherit_scale_mode")) {
      LISTBASE_FOREACH (bArmature *, arm, &bmain->armatures) {
        do_version_bones_inherit_scale(&arm->bonebase);
      }
    }

    /* Convert the Offset flag to the mix mode enum. */
    if (!DNA_struct_member_exists(fd->filesdna, "bRotateLikeConstraint", "char", "mix_mode")) {
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
    if (!DNA_struct_member_exists(fd->filesdna, "View3DShading", "float", "studiolight_intensity"))
    {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

    /* Elastic deform brush */
    LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
      if (br->ob_mode & OB_MODE_SCULPT && br->elastic_deform_volume_preservation == 0.0f) {
        br->elastic_deform_volume_preservation = 0.5f;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 281, 10)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        update_musgrave_node_color_output(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 281, 11)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        update_voronoi_node_f3_and_f4(ntree);
        update_voronoi_node_fac_output(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 281, 15)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      constexpr char SCE_SNAP_TO_NODE_X = (1 << 1);
      if (scene->toolsettings->snap_node_mode == SCE_SNAP_TO_NODE_X) {
        scene->toolsettings->snap_node_mode = SCE_SNAP_TO_GRID;
      }
    }

    if (!DNA_struct_member_exists(
            fd->filesdna, "LayerCollection", "short", "local_collections_bits"))
    {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
          LISTBASE_FOREACH (LayerCollection *, layer_collection, &view_layer->layer_collections) {
            do_versions_local_collection_bits_set(layer_collection);
          }
        }
      }
    }

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            View3D *v3d = (View3D *)sl;

            LISTBASE_FOREACH (ScrArea *, area_other, &screen->areabase) {
              LISTBASE_FOREACH (SpaceLink *, sl_other, &area_other->spacedata) {
                if (sl != sl_other && sl_other->spacetype == SPACE_VIEW3D) {
                  View3D *v3d_other = (View3D *)sl_other;

                  if (v3d->shading.prop == v3d_other->shading.prop) {
                    v3d_other->shading.prop = nullptr;
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

    LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
      if (br->ob_mode & OB_MODE_SCULPT && br->area_radius_factor == 0.0f) {
        br->area_radius_factor = 0.5f;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 282, 2)) {
    do_version_curvemapping_walker(bmain, do_version_curvemapping_flag_extend_extrapolate);

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        area->flag &= ~AREA_FLAG_UNUSED_6;
      }
    }

    /* Add custom curve profile to toolsettings for bevel tool */
    if (!DNA_struct_member_exists(fd->filesdna, "ToolSettings", "CurveProfile", "custom_profile"))
    {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        ToolSettings *ts = scene->toolsettings;
        if ((ts) && (ts->custom_bevel_profile_preset == nullptr)) {
          ts->custom_bevel_profile_preset = BKE_curveprofile_add(PROF_PRESET_LINE);
        }
      }
    }

    /* Add custom curve profile to bevel modifier */
    if (!DNA_struct_member_exists(fd->filesdna, "BevelModifier", "CurveProfile", "custom_profile"))
    {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
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
    if (!DNA_struct_member_exists(fd->filesdna, "Brush", "float", "dash_ratio")) {
      LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
        br->dash_ratio = 1.0f;
        br->dash_samples = 20;
      }
    }

    /* Pose brush smooth iterations */
    if (!DNA_struct_member_exists(fd->filesdna, "Brush", "float", "pose_smooth_iterations")) {
      LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
        br->pose_smooth_iterations = 4;
      }
    }

    /* Cloth pressure */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_Cloth) {
          ClothModifierData *clmd = (ClothModifierData *)md;

          clmd->sim_parms->pressure_factor = 1;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 282, 3)) {
    /* Remove Unified pressure/size and pressure/alpha */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *ts = scene->toolsettings;
      UnifiedPaintSettings *ups = &ts->unified_paint_settings;
      ups->flag &= ~(UNIFIED_PAINT_FLAG_UNUSED_0 | UNIFIED_PAINT_FLAG_UNUSED_1);
    }

    /* Set the default render pass in the viewport to Combined. */
    if (!DNA_struct_member_exists(fd->filesdna, "View3DShading", "int", "render_pass")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->display.shading.render_pass = SCE_PASS_COMBINED;
      }

      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 283, 3)) {
    /* Color Management Look. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
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
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_Cloth) {
          ClothModifierData *clmd = (ClothModifierData *)md;

          clmd->sim_parms->internal_tension = 15.0f;
          clmd->sim_parms->max_internal_tension = 15.0f;
          clmd->sim_parms->internal_compression = 15.0f;
          clmd->sim_parms->max_internal_compression = 15.0f;
          clmd->sim_parms->internal_spring_max_diversion = M_PI_4;
        }
      }
    }

    /* Add primary tile to images. */
    if (!DNA_struct_member_exists(fd->filesdna, "Image", "ListBase", "tiles")) {
      LISTBASE_FOREACH (Image *, ima, &bmain->images) {
        ImageTile *tile = MEM_callocN<ImageTile>("Image Tile");
        tile->tile_number = 1001;
        BLI_addtail(&ima->tiles, tile);
      }
    }

    /* UDIM Image Editor change. */
    if (!DNA_struct_member_exists(fd->filesdna, "SpaceImage", "int", "tile_grid_shape[2]")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

    LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
      br->add_col[3] = 0.9f;
      br->sub_col[3] = 0.9f;
    }

    /* Pose brush IK segments. */
    LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
      if (br->pose_ik_segments == 0) {
        br->pose_ik_segments = 1;
      }
    }

    /* Pose brush keep anchor point. */
    LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
      if (br->sculpt_brush_type == SCULPT_BRUSH_TYPE_POSE) {
        br->flag2 |= BRUSH_POSE_IK_ANCHORED;
      }
    }

    /* Tip Roundness. */
    if (!DNA_struct_member_exists(fd->filesdna, "Brush", "float", "tip_roundness")) {
      LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
        if (br->ob_mode & OB_MODE_SCULPT && br->sculpt_brush_type == SCULPT_BRUSH_TYPE_CLAY_STRIPS)
        {
          br->tip_roundness = 0.18f;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 283, 5)) {
    /* Alembic Transform Cache changed from world to local space. */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (bConstraint *, con, &ob->constraints) {
        if (con->type == CONSTRAINT_TYPE_TRANSFORM_CACHE) {
          con->ownspace = CONSTRAINT_SPACE_LOCAL;
        }
      }
    }

    /* Add 2D transform to UV Warp modifier. */
    if (!DNA_struct_member_exists(fd->filesdna, "UVWarpModifierData", "float", "scale[2]")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_UVWarp) {
            UVWarpModifierData *umd = (UVWarpModifierData *)md;
            copy_v2_fl(umd->scale, 1.0f);
          }
        }
      }
    }

    /* Add Lookdev blur property. */
    if (!DNA_struct_member_exists(fd->filesdna, "View3DShading", "float", "studiolight_blur")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 283, 7)) {
    /* Init default Grease Pencil Vertex paint mix factor for Viewport. */
    if (!DNA_struct_member_exists(
            fd->filesdna, "View3DOverlay", "float", "gpencil_vertex_paint_opacity"))
    {
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
    if (!DNA_struct_member_exists(fd->filesdna, "ArrayGpencilModifierData", "int", "seed")) {
      /* Init new Grease Pencil Paint tools. */
      {
        LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
          if (brush->gpencil_settings != nullptr) {
            brush->gpencil_vertex_brush_type = brush->gpencil_settings->brush_type;
            brush->gpencil_sculpt_brush_type = brush->gpencil_settings->brush_type;
            brush->gpencil_weight_brush_type = brush->gpencil_settings->brush_type;
          }
        }
      }

      LISTBASE_FOREACH (Material *, mat, &bmain->materials) {
        MaterialGPencilStyle *gp_style = mat->gp_style;
        if (gp_style == nullptr) {
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
        if (ob->type != OB_GPENCIL_LEGACY) {
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
              if ((mmd->offset[0] != 0.0f) || (mmd->offset[1] != 0.0f) || (mmd->offset[2] != 0.0f))
              {
                mmd->flag |= GP_ARRAY_USE_OFFSET;
              }
              if ((mmd->shift[0] != 0.0f) || (mmd->shift[1] != 0.0f) || (mmd->shift[2] != 0.0f)) {
                mmd->flag |= GP_ARRAY_USE_OFFSET;
              }
              if (mmd->object != nullptr) {
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

              if (mmd->curve_intensity == nullptr) {
                mmd->curve_intensity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
                if (mmd->curve_intensity) {
                  BKE_curvemapping_init(mmd->curve_intensity);
                }
              }
              break;
            }
            case eGpencilModifierType_Tint: {
              TintGpencilModifierData *mmd = (TintGpencilModifierData *)md;
              srgb_to_linearrgb_v3_v3(mmd->rgb, mmd->rgb);
              if (mmd->curve_intensity == nullptr) {
                mmd->curve_intensity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
                if (mmd->curve_intensity) {
                  BKE_curvemapping_init(mmd->curve_intensity);
                }
              }
              break;
            }
            case eGpencilModifierType_Smooth: {
              SmoothGpencilModifierData *mmd = (SmoothGpencilModifierData *)md;
              if (mmd->curve_intensity == nullptr) {
                mmd->curve_intensity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
                if (mmd->curve_intensity) {
                  BKE_curvemapping_init(mmd->curve_intensity);
                }
              }
              break;
            }
            case eGpencilModifierType_Opacity: {
              OpacityGpencilModifierData *mmd = (OpacityGpencilModifierData *)md;
              if (mmd->curve_intensity == nullptr) {
                mmd->curve_intensity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
                if (mmd->curve_intensity) {
                  BKE_curvemapping_init(mmd->curve_intensity);
                }
              }
              break;
            }
            case eGpencilModifierType_Color: {
              ColorGpencilModifierData *mmd = (ColorGpencilModifierData *)md;
              if (mmd->curve_intensity == nullptr) {
                mmd->curve_intensity = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
                if (mmd->curve_intensity) {
                  BKE_curvemapping_init(mmd->curve_intensity);
                }
              }
              break;
            }
            case eGpencilModifierType_Thick: {
              if (!DNA_struct_member_exists(
                      fd->filesdna, "ThickGpencilModifierData", "float", "thickness_fac"))
              {
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
              BKE_gpencil_stroke_geometry_update(gpd, gps);

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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 283, 8)) {
    if (!DNA_struct_member_exists(
            fd->filesdna, "View3DOverlay", "float", "sculpt_mode_face_sets_opacity"))
    {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
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

    /* Boundary Edges Auto-masking. */
    if (!DNA_struct_member_exists(
            fd->filesdna, "Brush", "int", "automasking_boundary_edges_propagation_steps"))
    {
      LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
        br->automasking_boundary_edges_propagation_steps = 1;
      }
    }

    /* Corrective smooth modifier scale. */
    if (!DNA_struct_member_exists(fd->filesdna, "CorrectiveSmoothModifierData", "float", "scale"))
    {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_CorrectiveSmooth) {
            CorrectiveSmoothModifierData *csmd = (CorrectiveSmoothModifierData *)md;
            csmd->scale = 1.0f;
          }
        }
      }
    }

    /* Default Face Set Color. */
    LISTBASE_FOREACH (Mesh *, me, &bmain->meshes) {
      if (me->faces_num > 0) {
        const int *face_sets = static_cast<const int *>(
            CustomData_get_layer(&me->face_data, CD_SCULPT_FACE_SETS));
        if (face_sets) {
          me->face_sets_color_default = abs(face_sets[0]);
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 283, 11)) {
    if (!DNA_struct_member_exists(fd->filesdna, "OceanModifierData", "float", "fetch_jonswap")) {
      LISTBASE_FOREACH (Object *, object, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
          if (md->type == eModifierType_Ocean) {
            OceanModifierData *omd = (OceanModifierData *)md;
            omd->fetch_jonswap = 120.0f;
          }
        }
      }
    }

    if (!DNA_struct_exists(fd->filesdna, "XrSessionSettings")) {
      LISTBASE_FOREACH (wmWindowManager *, wm, &bmain->wm) {
        const View3D *v3d_default = DNA_struct_default_get(View3D);

        wm->xr.session_settings.shading = v3d_default->shading;
        wm->xr.session_settings.draw_flags = (V3D_OFSDRAW_SHOW_GRIDFLOOR |
                                              V3D_OFSDRAW_SHOW_ANNOTATION);
        wm->xr.session_settings.clip_start = v3d_default->clip_start;
        wm->xr.session_settings.clip_end = v3d_default->clip_end;

        wm->xr.session_settings.flag = XR_SESSION_USE_POSITION_TRACKING;
      }
    }

    /* Surface deform modifier strength. */
    if (!DNA_struct_member_exists(fd->filesdna, "SurfaceDeformModifierData", "float", "strength"))
    {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_SurfaceDeform) {
            SurfaceDeformModifierData *sdmd = (SurfaceDeformModifierData *)md;
            sdmd->strength = 1.0f;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 283, 12)) {
    /* Activate f-curve drawing in the sequencer. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SEQ) {
            SpaceSeq *sseq = (SpaceSeq *)sl;
            sseq->flag |= SEQ_TIMELINE_SHOW_FCURVES;
          }
        }
      }
    }

    /* Remesh Modifier Voxel Mode. */
    if (!DNA_struct_member_exists(fd->filesdna, "RemeshModifierData", "float", "voxel_size")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
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

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 283, 14)) {
    /* Solidify modifier merge tolerance. */
    if (!DNA_struct_member_exists(
            fd->filesdna, "SolidifyModifierData", "float", "merge_tolerance"))
    {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
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
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      RigidBodyCon *rbc = ob->rigidbody_constraint;
      if (rbc != nullptr) {
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
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 283, 15)) {
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_Fluid) {
          FluidModifierData *fmd = (FluidModifierData *)md;
          if (fmd->domain != nullptr) {
            mul_v3_fl(fmd->domain->gravity, 9.81f);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 283, 16)) {
    /* Init SMAA threshold for grease pencil render. */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->grease_pencil_settings.smaa_threshold = 1.0f;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 283, 17)) {
    /* Reset the cloth mass to 1.0 in brushes with an invalid value. */
    LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
      if (br->sculpt_brush_type == SCULPT_BRUSH_TYPE_CLOTH) {
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

  /* Old forgotten versioning code. */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 300, 39)) {
    /* Set the cloth wind factor to 1 for old forces. */
    if (!DNA_struct_member_exists(fd->filesdna, "PartDeflect", "float", "f_wind_factor")) {
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

    LISTBASE_FOREACH (wmWindowManager *, wm, &bmain->wm) {
      /* Don't rotate light with the viewer by default, make it fixed. Shading settings can't be
       * edited and this flag should always be set. */
      wm->xr.session_settings.shading.flag |= V3D_SHADING_WORLD_ORIENTATION;
    }
  }

  /**
   * Always bump subversion in BKE_blender_version.h when adding versioning
   * code here, and wrap it inside a MAIN_VERSION_FILE_ATLEAST check.
   *
   * \note Keep this message at the bottom of the function.
   */
}
