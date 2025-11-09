/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 */

#include "BLI_utildefines.h"

#include <string>

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

/* Define macros in `DNA_genfile.h`. */
#define DNA_GENFILE_VERSIONING_MACROS

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_cloth_types.h"
#include "DNA_constraint_types.h"
#include "DNA_fluid_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_light_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_mask_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_pointcache_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "DNA_genfile.h"

#undef DNA_GENFILE_VERSIONING_MACROS

#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_colortools.hh"
#include "BKE_customdata.hh"
#include "BKE_fcurve_driver.h"
#include "BKE_main.hh"
#include "BKE_mask.h"
#include "BKE_modifier.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "DNA_material_types.h"

#include "SEQ_effects.hh"
#include "SEQ_iterator.hh"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"

#include "BLT_translation.hh"

#include "BLO_readfile.hh"

#include "NOD_composite.hh"
#include "NOD_socket.hh"

#include "readfile.hh"

#include "MEM_guardedalloc.h"

/* Make preferences read-only, use `versioning_userdef.cc`. */
#define U (*((const UserDef *)&U))

/* ************************************************** */
/* GP Palettes API (Deprecated) */

/* add a new gp-palette */
static bGPDpalette *BKE_gpencil_palette_addnew(bGPdata *gpd, const char *name)
{
  bGPDpalette *palette;

  /* check that list is ok */
  if (gpd == nullptr) {
    return nullptr;
  }

  /* allocate memory and add to end of list */
  palette = MEM_callocN<bGPDpalette>("bGPDpalette");

  /* add to datablock */
  BLI_addtail(&gpd->palettes, palette);

  /* set basic settings */
  /* auto-name */
  STRNCPY_UTF8(palette->info, name);
  BLI_uniquename(&gpd->palettes,
                 palette,
                 DATA_("GP_Palette"),
                 '.',
                 offsetof(bGPDpalette, info),
                 sizeof(palette->info));

  /* return palette */
  return palette;
}

/* add a new gp-palettecolor */
static bGPDpalettecolor *BKE_gpencil_palettecolor_addnew(bGPDpalette *palette, const char *name)
{
  bGPDpalettecolor *palcolor;

  /* check that list is ok */
  if (palette == nullptr) {
    return nullptr;
  }

  /* allocate memory and add to end of list */
  palcolor = MEM_callocN<bGPDpalettecolor>("bGPDpalettecolor");

  /* add to datablock */
  BLI_addtail(&palette->colors, palcolor);

  /* set basic settings */
  copy_v4_v4(palcolor->color, U.gpencil_new_layer_col);
  ARRAY_SET_ITEMS(palcolor->fill, 1.0f, 1.0f, 1.0f);

  /* auto-name */
  STRNCPY_UTF8(palcolor->info, name);
  BLI_uniquename(&palette->colors,
                 palcolor,
                 DATA_("Color"),
                 '.',
                 offsetof(bGPDpalettecolor, info),
                 sizeof(palcolor->info));

  /* return palette color */
  return palcolor;
}

/**
 * Setup rotation stabilization from ancient single track spec.
 * Former Version of 2D stabilization used a single tracking marker to determine the rotation
 * to be compensated. Now several tracks can contribute to rotation detection and this feature
 * is enabled by the MovieTrackingTrack#flag on a per track base.
 */
static void migrate_single_rot_stabilization_track_settings(MovieTrackingStabilization *stab)
{
  if (stab->rot_track_legacy) {
    if (!(stab->rot_track_legacy->flag & TRACK_USE_2D_STAB_ROT)) {
      stab->tot_rot_track++;
      stab->rot_track_legacy->flag |= TRACK_USE_2D_STAB_ROT;
    }
  }
  stab->rot_track_legacy = nullptr; /* this field is now ignored */
}

static void do_version_constraints_radians_degrees_270_1(ListBase *lb)
{
  LISTBASE_FOREACH (bConstraint *, con, lb) {
    if (con->type == CONSTRAINT_TYPE_TRANSFORM) {
      bTransformConstraint *data = (bTransformConstraint *)con->data;
      const float deg_to_rad_f = DEG2RADF(1.0f);

      if (data->from == TRANS_ROTATION) {
        mul_v3_fl(data->from_min, deg_to_rad_f);
        mul_v3_fl(data->from_max, deg_to_rad_f);
      }

      if (data->to == TRANS_ROTATION) {
        mul_v3_fl(data->to_min, deg_to_rad_f);
        mul_v3_fl(data->to_max, deg_to_rad_f);
      }
    }
  }
}

static void do_version_constraints_radians_degrees_270_5(ListBase *lb)
{
  LISTBASE_FOREACH (bConstraint *, con, lb) {
    if (con->type == CONSTRAINT_TYPE_TRANSFORM) {
      bTransformConstraint *data = (bTransformConstraint *)con->data;

      if (data->from == TRANS_ROTATION) {
        copy_v3_v3(data->from_min_rot, data->from_min);
        copy_v3_v3(data->from_max_rot, data->from_max);
      }
      else if (data->from == TRANS_SCALE) {
        copy_v3_v3(data->from_min_scale, data->from_min);
        copy_v3_v3(data->from_max_scale, data->from_max);
      }

      if (data->to == TRANS_ROTATION) {
        copy_v3_v3(data->to_min_rot, data->to_min);
        copy_v3_v3(data->to_max_rot, data->to_max);
      }
      else if (data->to == TRANS_SCALE) {
        copy_v3_v3(data->to_min_scale, data->to_min);
        copy_v3_v3(data->to_max_scale, data->to_max);
      }
    }
  }
}

static void do_version_constraints_stretch_to_limits(ListBase *lb)
{
  LISTBASE_FOREACH (bConstraint *, con, lb) {
    if (con->type == CONSTRAINT_TYPE_STRETCHTO) {
      bStretchToConstraint *data = (bStretchToConstraint *)con->data;
      data->bulge_min = 1.0f;
      data->bulge_max = 1.0f;
    }
  }
}

static void do_version_action_editor_properties_region(ListBase *regionbase)
{
  LISTBASE_FOREACH (ARegion *, region, regionbase) {
    if (region->regiontype == RGN_TYPE_UI) {
      /* already exists */
      return;
    }
    if (region->regiontype == RGN_TYPE_WINDOW) {
      /* add new region here */
      ARegion *region_new = BKE_area_region_new();

      BLI_insertlinkbefore(regionbase, region, region_new);

      region_new->regiontype = RGN_TYPE_UI;
      region_new->alignment = RGN_ALIGN_RIGHT;
      region_new->flag = RGN_FLAG_HIDDEN;

      return;
    }
  }
}

static void do_version_bones_super_bbone(ListBase *lb)
{
  LISTBASE_FOREACH (Bone *, bone, lb) {
    bone->scale_in_x = bone->scale_in_z = 1.0f;
    bone->scale_out_x = bone->scale_out_z = 1.0f;

    do_version_bones_super_bbone(&bone->childbase);
  }
}

/* TODO(sergey): Consider making it somewhat more generic function in BLI_anim.h. */
static void anim_change_prop_name(FCurve *fcu,
                                  const char *prefix,
                                  const char *old_prop_name,
                                  const char *new_prop_name)
{
  const char *old_path = BLI_sprintfN("%s.%s", prefix, old_prop_name);
  if (STREQ(fcu->rna_path, old_path)) {
    MEM_freeN(fcu->rna_path);
    fcu->rna_path = BLI_sprintfN("%s.%s", prefix, new_prop_name);
  }
  MEM_freeN((char *)old_path);
}

static void do_version_hue_sat_node(bNodeTree *ntree, bNode *node)
{
  if (node->storage == nullptr) {
    return;
  }

  /* Convert value from old storage to new sockets. */
  NodeHueSat *nhs = static_cast<NodeHueSat *>(node->storage);
  bNodeSocket *hue = blender::bke::node_find_socket(*node, SOCK_IN, "Hue");
  bNodeSocket *saturation = blender::bke::node_find_socket(*node, SOCK_IN, "Saturation");
  bNodeSocket *value = blender::bke::node_find_socket(*node, SOCK_IN, "Value");
  if (hue == nullptr) {
    hue = blender::bke::node_add_static_socket(
        *ntree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Hue", "Hue");
  }
  if (saturation == nullptr) {
    saturation = blender::bke::node_add_static_socket(
        *ntree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Saturation", "Saturation");
  }
  if (value == nullptr) {
    value = blender::bke::node_add_static_socket(
        *ntree, *node, SOCK_IN, SOCK_FLOAT, PROP_FACTOR, "Value", "Value");
  }

  ((bNodeSocketValueFloat *)hue->default_value)->value = nhs->hue;
  ((bNodeSocketValueFloat *)saturation->default_value)->value = nhs->sat;
  ((bNodeSocketValueFloat *)value->default_value)->value = nhs->val;
  /* Take care of possible animation. */
  AnimData *adt = BKE_animdata_from_id(&ntree->id);
  if (adt != nullptr && adt->action != nullptr) {
    char node_name_esc[sizeof(node->name) * 2];
    BLI_str_escape(node_name_esc, node->name, sizeof(node_name_esc));
    const char *prefix = BLI_sprintfN("nodes[\"%s\"]", node_name_esc);
    LISTBASE_FOREACH (FCurve *, fcu, &adt->action->curves) {
      if (STRPREFIX(fcu->rna_path, prefix)) {
        anim_change_prop_name(fcu, prefix, "color_hue", "inputs[1].default_value");
        anim_change_prop_name(fcu, prefix, "color_saturation", "inputs[2].default_value");
        anim_change_prop_name(fcu, prefix, "color_value", "inputs[3].default_value");
      }
    }
    MEM_freeN((char *)prefix);
  }
  /* Free storage, it is no longer used. */
  MEM_freeN(node->storage);
  node->storage = nullptr;
}

static void do_versions_compositor_render_passes_storage(bNode *node)
{
  int pass_index = 0;
  const char *sockname;
  for (bNodeSocket *sock = static_cast<bNodeSocket *>(node->outputs.first);
       sock && pass_index < 31;
       sock = static_cast<bNodeSocket *>(sock->next), pass_index++)
  {
    if (sock->storage == nullptr) {
      NodeImageLayer *sockdata = MEM_callocN<NodeImageLayer>("node image layer");
      sock->storage = sockdata;
      STRNCPY_UTF8(sockdata->pass_name, node_cmp_rlayers_sock_to_pass(pass_index));

      if (pass_index == 0) {
        sockname = "Image";
      }
      else if (pass_index == 1) {
        sockname = "Alpha";
      }
      else {
        sockname = node_cmp_rlayers_sock_to_pass(pass_index);
      }
      STRNCPY_UTF8(sock->name, sockname);
    }
  }
}

static void do_versions_compositor_render_passes(bNodeTree *ntree)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type_legacy == CMP_NODE_R_LAYERS) {
      /* First we make sure existing sockets have proper names.
       * This is important because otherwise verification will
       * drop links from sockets which were renamed.
       */
      do_versions_compositor_render_passes_storage(node);
      /* Make sure new sockets are properly created. */
      node_verify_sockets(ntree, node, false);
      /* Make sure all possibly created sockets have proper storage. */
      do_versions_compositor_render_passes_storage(node);
    }
  }
}

static char *replace_bbone_easing_rnapath(char *old_path)
{
  char *new_path = nullptr;

  /* NOTE: This will break paths for any bones/custom-properties
   * which happen be named after the bbone property id's
   */
  if (strstr(old_path, "bbone_in")) {
    new_path = BLI_string_replaceN(old_path, "bbone_in", "bbone_easein");
  }
  else if (strstr(old_path, "bbone_out")) {
    new_path = BLI_string_replaceN(old_path, "bbone_out", "bbone_easeout");
  }

  if (new_path) {
    MEM_freeN(old_path);
    return new_path;
  }

  return old_path;
}

static void do_version_bbone_easing_fcurve_fix(ID * /*id*/, FCurve *fcu)
{
  /* F-Curve's path (for bbone_in/out) */
  if (fcu->rna_path) {
    fcu->rna_path = replace_bbone_easing_rnapath(fcu->rna_path);
  }

  /* Driver -> Driver Vars (for bbone_in/out) */
  if (fcu->driver) {
    LISTBASE_FOREACH (DriverVar *, dvar, &fcu->driver->variables) {
      DRIVER_TARGETS_LOOPER_BEGIN (dvar) {
        if (dtar->rna_path) {
          dtar->rna_path = replace_bbone_easing_rnapath(dtar->rna_path);
        }
      }
      DRIVER_TARGETS_LOOPER_END;
    }
  }

  /* FModifiers -> Stepped (for frame_start/end) */
  if (fcu->modifiers.first) {
    LISTBASE_FOREACH (FModifier *, fcm, &fcu->modifiers) {
      if (fcm->type == FMODIFIER_TYPE_STEPPED) {
        FMod_Stepped *data = static_cast<FMod_Stepped *>(fcm->data);

        /* Modifier doesn't work if the modifier's copy of start/end frame are both 0
         * as those were only getting written to the fcm->data copy (#52009)
         */
        if ((fcm->sfra == fcm->efra) && (fcm->sfra == 0)) {
          fcm->sfra = data->start_frame;
          fcm->efra = data->end_frame;
        }
      }
    }
  }
}

static bool strip_update_proxy_cb(Strip *strip, void * /*user_data*/)
{
  strip->stereo3d_format = MEM_callocN<Stereo3dFormat>("Stereo Display 3d Format");

#define STRIP_USE_PROXY_CUSTOM_DIR (1 << 19)
#define STRIP_USE_PROXY_CUSTOM_FILE (1 << 21)
  if (strip->data && strip->data->proxy && !strip->data->proxy->storage) {
    if (strip->flag & STRIP_USE_PROXY_CUSTOM_DIR) {
      strip->data->proxy->storage = SEQ_STORAGE_PROXY_CUSTOM_DIR;
    }
    if (strip->flag & STRIP_USE_PROXY_CUSTOM_FILE) {
      strip->data->proxy->storage = SEQ_STORAGE_PROXY_CUSTOM_FILE;
    }
  }
#undef STRIP_USE_PROXY_CUSTOM_DIR
#undef STRIP_USE_PROXY_CUSTOM_FILE
  return true;
}

static bool strip_init_text_effect_data(Strip *strip, void * /*user_data*/)
{
  if (strip->type != STRIP_TYPE_TEXT) {
    return true;
  }

  blender::seq::effect_ensure_initialized(strip);
  TextVars *data = static_cast<TextVars *>(strip->effectdata);
  if (data->color[3] == 0.0f) {
    copy_v4_fl(data->color, 1.0f);
    data->shadow_color[3] = 1.0f;
  }
  return true;
}

/* NOLINTNEXTLINE: readability-function-size */
void blo_do_versions_270(FileData *fd, Library * /*lib*/, Main *bmain)
{
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 270, 0)) {

    if (!DNA_struct_member_exists(fd->filesdna, "BevelModifierData", "float", "profile")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_Bevel) {
            BevelModifierData *bmd = (BevelModifierData *)md;
            bmd->profile = 0.5f;
            bmd->val_flags = MOD_BEVEL_AMT_OFFSET;
          }
        }
      }
    }

    /* nodes don't use fixed node->id any more, clean up */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (ELEM(node->type_legacy, CMP_NODE_COMPOSITE_DEPRECATED, CMP_NODE_OUTPUT_FILE)) {
            node->id = nullptr;
          }
        }
      }
    }
    FOREACH_NODETREE_END;

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, space_link, &area->spacedata) {
          if (space_link->spacetype == SPACE_CLIP) {
            SpaceClip *space_clip = (SpaceClip *)space_link;
            if (space_clip->mode != SC_MODE_MASKEDIT) {
              space_clip->mode = SC_MODE_TRACKING;
            }
          }
        }
      }
    }

    if (!DNA_struct_member_exists(
            fd->filesdna, "MovieTrackingSettings", "float", "default_weight"))
    {
      LISTBASE_FOREACH (MovieClip *, clip, &bmain->movieclips) {
        clip->tracking.settings.default_weight = 1.0f;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 270, 1)) {
    /* Update Transform constraint (another deg -> rad stuff). */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      do_version_constraints_radians_degrees_270_1(&ob->constraints);

      if (ob->pose) {
        /* Bones constraints! */
        LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
          do_version_constraints_radians_degrees_270_1(&pchan->constraints);
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 270, 2)) {
    /* Mesh smoothresh_legacy deg->rad. */
    LISTBASE_FOREACH (Mesh *, me, &bmain->meshes) {
      me->smoothresh_legacy = DEG2RADF(me->smoothresh_legacy);
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 270, 3)) {
    LISTBASE_FOREACH (FreestyleLineStyle *, linestyle, &bmain->linestyles) {
      linestyle->flag |= LS_NO_SORTING;
      linestyle->sort_key = LS_SORT_KEY_DISTANCE_FROM_CAMERA;
      linestyle->integration_type = LS_INTEGRATION_MEAN;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 270, 4)) {
    /* ui_previews were not handled correctly when copying areas,
     * leading to corrupted files (see #39847).
     * This will always reset situation to a valid state.
     */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          ListBase *lb = (sl == area->spacedata.first) ? &area->regionbase : &sl->regionbase;
          LISTBASE_FOREACH (ARegion *, region, lb) {
            BLI_listbase_clear(&region->ui_previews);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 270, 5)) {
    /* Update Transform constraint (again :|). */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      do_version_constraints_radians_degrees_270_5(&ob->constraints);

      if (ob->pose) {
        /* Bones constraints! */
        LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
          do_version_constraints_radians_degrees_270_5(&pchan->constraints);
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 271, 0)) {
    if (!DNA_struct_member_exists(fd->filesdna, "RenderData", "BakeData", "bake")) {
      LISTBASE_FOREACH (Scene *, sce, &bmain->scenes) {
        sce->r.bake.flag = R_BAKE_CLEAR;
        sce->r.bake.width = 512;
        sce->r.bake.height = 512;
        sce->r.bake.margin = 16;
        sce->r.bake.normal_space = R_BAKE_SPACE_TANGENT;
        sce->r.bake.normal_swizzle[0] = R_BAKE_POSX;
        sce->r.bake.normal_swizzle[1] = R_BAKE_POSY;
        sce->r.bake.normal_swizzle[2] = R_BAKE_POSZ;
        STRNCPY(sce->r.bake.filepath, U.renderdir);

        sce->r.bake.im_format.planes = R_IMF_PLANES_RGBA;
        sce->r.bake.im_format.imtype = R_IMF_IMTYPE_PNG;
        sce->r.bake.im_format.depth = R_IMF_CHAN_DEPTH_8;
        sce->r.bake.im_format.quality = 90;
        sce->r.bake.im_format.compress = 15;
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "FreestyleLineStyle", "float", "texstep")) {
      LISTBASE_FOREACH (FreestyleLineStyle *, linestyle, &bmain->linestyles) {
        linestyle->flag |= LS_TEXTURE;
        linestyle->texstep = 1.0;
      }
    }

    {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        int num_layers = BLI_listbase_count(&scene->r.layers);
        scene->r.actlay = min_ff(scene->r.actlay, num_layers - 1);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 271, 1)) {
    if (!DNA_struct_member_exists(fd->filesdna, "Material", "float", "line_col[4]")) {
      LISTBASE_FOREACH (Material *, mat, &bmain->materials) {
        mat->line_col[0] = mat->line_col[1] = mat->line_col[2] = 0.0f;
        mat->line_col[3] = mat->alpha;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 271, 3)) {
    LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
      br->fill_threshold = 0.2f;
    }

    if (!DNA_struct_member_exists(fd->filesdna, "BevelModifierData", "int", "mat")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_Bevel) {
            BevelModifierData *bmd = (BevelModifierData *)md;
            bmd->mat = -1;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 271, 6)) {
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_ParticleSystem) {
          ParticleSystemModifierData *pmd = (ParticleSystemModifierData *)md;
          if (pmd->psys && pmd->psys->clmd) {
            pmd->psys->clmd->sim_parms->vel_damping = 1.0f;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 272, 1)) {
    LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
      if ((br->ob_mode & OB_MODE_SCULPT) &&
          ELEM(br->sculpt_brush_type, SCULPT_BRUSH_TYPE_GRAB, SCULPT_BRUSH_TYPE_SNAKE_HOOK))
      {
        br->alpha = 1.0f;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 272, 2)) {
    if (!DNA_struct_member_exists(fd->filesdna, "Image", "float", "gen_color")) {
      LISTBASE_FOREACH (Image *, image, &bmain->images) {
        image->gen_color[3] = 1.0f;
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "bStretchToConstraint", "float", "bulge_min")) {
      /* Update Transform constraint (again :|). */
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        do_version_constraints_stretch_to_limits(&ob->constraints);

        if (ob->pose) {
          /* Bones constraints! */
          LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
            do_version_constraints_stretch_to_limits(&pchan->constraints);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 273, 1)) {
#define BRUSH_RAKE (1 << 7)
#define BRUSH_RANDOM_ROTATION (1 << 25)

    LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
      if (br->flag & BRUSH_RAKE) {
        br->mtex.brush_angle_mode |= MTEX_ANGLE_RAKE;
        br->mask_mtex.brush_angle_mode |= MTEX_ANGLE_RAKE;
      }
      else if (br->flag & BRUSH_RANDOM_ROTATION) {
        br->mtex.brush_angle_mode |= MTEX_ANGLE_RANDOM;
        br->mask_mtex.brush_angle_mode |= MTEX_ANGLE_RANDOM;
      }
      br->mtex.random_angle = 2.0 * M_PI;
      br->mask_mtex.random_angle = 2.0 * M_PI;
    }
  }

#undef BRUSH_RAKE
#undef BRUSH_RANDOM_ROTATION

  /* Customizable Safe Areas */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 273, 2)) {
    if (!DNA_struct_member_exists(fd->filesdna, "Scene", "DisplaySafeAreas", "safe_areas")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        copy_v2_fl2(scene->safe_areas.title, 3.5f / 100.0f, 3.5f / 100.0f);
        copy_v2_fl2(scene->safe_areas.action, 10.0f / 100.0f, 5.0f / 100.0f);
        copy_v2_fl2(scene->safe_areas.title_center, 17.5f / 100.0f, 5.0f / 100.0f);
        copy_v2_fl2(scene->safe_areas.action_center, 15.0f / 100.0f, 5.0f / 100.0f);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 273, 3)) {
    LISTBASE_FOREACH (ParticleSettings *, part, &bmain->particles) {
      if (part->clumpcurve) {
        part->child_flag |= PART_CHILD_USE_CLUMP_CURVE;
      }
      if (part->roughcurve) {
        part->child_flag |= PART_CHILD_USE_ROUGH_CURVE;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 273, 6)) {
    if (!DNA_struct_member_exists(fd->filesdna, "ClothSimSettings", "float", "bending_damping")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_Cloth) {
            ClothModifierData *clmd = (ClothModifierData *)md;
            clmd->sim_parms->bending_damping = 0.5f;
          }
          else if (md->type == eModifierType_ParticleSystem) {
            ParticleSystemModifierData *pmd = (ParticleSystemModifierData *)md;
            if (pmd->psys->clmd) {
              pmd->psys->clmd->sim_parms->bending_damping = 0.5f;
            }
          }
        }
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "ParticleSettings", "float", "clump_noise_size")) {
      LISTBASE_FOREACH (ParticleSettings *, part, &bmain->particles) {
        part->clump_noise_size = 1.0f;
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "ParticleSettings", "int", "kink_extra_steps")) {
      LISTBASE_FOREACH (ParticleSettings *, part, &bmain->particles) {
        part->kink_extra_steps = 4;
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "MTex", "float", "kinkampfac")) {
      LISTBASE_FOREACH (ParticleSettings *, part, &bmain->particles) {
        for (int a = 0; a < MAX_MTEX; a++) {
          MTex *mtex = part->mtex[a];
          if (mtex) {
            mtex->kinkampfac = 1.0f;
          }
        }
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "HookModifierData", "char", "flag")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_Hook) {
            HookModifierData *hmd = (HookModifierData *)md;
            hmd->falloff_type = eHook_Falloff_InvSquare;
          }
        }
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "NodePlaneTrackDeformData", "char", "flag")) {
      FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
        if (ntree->type == NTREE_COMPOSIT) {
          LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
            if (ELEM(node->type_legacy, CMP_NODE_PLANETRACKDEFORM)) {
              NodePlaneTrackDeformData *data = static_cast<NodePlaneTrackDeformData *>(
                  node->storage);
              data->flag = 0;
              data->motion_blur_samples = 16;
              data->motion_blur_shutter = 0.5f;
            }
          }
        }
      }
      FOREACH_NODETREE_END;
    }

    if (!DNA_struct_member_exists(fd->filesdna, "Camera", "GPUDOFSettings", "gpu_dof")) {
      LISTBASE_FOREACH (Camera *, ca, &bmain->cameras) {
        ca->gpu_dof.fstop = 128.0f;
        ca->gpu_dof.focal_length = 1.0f;
        ca->gpu_dof.focus_distance = 1.0f;
        ca->gpu_dof.sensor = 1.0f;
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 273, 8)) {
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        const std::string old_name = md->name;
        BKE_modifier_unique_name(&ob->modifiers, md);
        if (old_name != md->name) {
          printf(
              "Warning: Object '%s' had several modifiers with the "
              "same name, renamed one of them to '%s'.\n",
              ob->id.name + 2,
              md->name);
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 273, 9)) {
    /* Make sure sequencer preview area limits zoom */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SEQ) {
            LISTBASE_FOREACH (ARegion *, region, &sl->regionbase) {
              if (region->regiontype == RGN_TYPE_PREVIEW) {
                region->v2d.keepzoom |= V2D_LIMITZOOM;
                region->v2d.minzoom = 0.001f;
                region->v2d.maxzoom = 1000.0f;
                break;
              }
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 274, 1)) {
    /* particle systems need to be forced to redistribute for jitter mode fix */
    {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
          if ((psys->pointcache->flag & PTCACHE_BAKED) == 0) {
            psys->recalc |= ID_RECALC_PSYS_RESET;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 274, 4)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      BKE_scene_add_render_view(scene, STEREO_LEFT_NAME);
      SceneRenderView *srv = static_cast<SceneRenderView *>(scene->r.views.first);
      STRNCPY_UTF8(srv->suffix, STEREO_LEFT_SUFFIX);

      BKE_scene_add_render_view(scene, STEREO_RIGHT_NAME);
      srv = static_cast<SceneRenderView *>(scene->r.views.last);
      STRNCPY_UTF8(srv->suffix, STEREO_RIGHT_SUFFIX);

      if (scene->ed) {
        blender::seq::foreach_strip(&scene->ed->seqbase, strip_update_proxy_cb, nullptr);
      }
    }

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          switch (sl->spacetype) {
            case SPACE_VIEW3D: {
              View3D *v3d = (View3D *)sl;
              v3d->stereo3d_camera = STEREO_3D_ID;
              v3d->stereo3d_flag |= V3D_S3D_DISPPLANE;
              v3d->stereo3d_convergence_alpha = 0.15f;
              v3d->stereo3d_volume_alpha = 0.05f;
              break;
            }
            case SPACE_IMAGE: {
              SpaceImage *sima = (SpaceImage *)sl;
              sima->iuser.flag |= IMA_SHOW_STEREO;
              break;
            }
          }
        }
      }
    }

    LISTBASE_FOREACH (Camera *, cam, &bmain->cameras) {
      cam->stereo.interocular_distance = 0.065f;
      cam->stereo.convergence_distance = 30.0f * 0.065f;
    }

    LISTBASE_FOREACH (Image *, ima, &bmain->images) {
      ima->stereo3d_format = MEM_callocN<Stereo3dFormat>("Image Stereo 3d Format");

      if (ima->packedfile) {
        ImagePackedFile *imapf = MEM_mallocN<ImagePackedFile>("Image Packed File");
        BLI_addtail(&ima->packedfiles, imapf);

        imapf->packedfile = ima->packedfile;
        STRNCPY(imapf->filepath, ima->filepath);
        ima->packedfile = nullptr;
      }
    }

    LISTBASE_FOREACH (wmWindowManager *, wm, &bmain->wm) {
      LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
        win->stereo3d_format = MEM_callocN<Stereo3dFormat>("Stereo Display 3d Format");
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 274, 6)) {
    if (!DNA_struct_member_exists(fd->filesdna, "FileSelectParams", "int", "thumbnail_size")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_FILE) {
              SpaceFile *sfile = (SpaceFile *)sl;

              if (sfile->params) {
                sfile->params->thumbnail_size = 128;
              }
            }
          }
        }
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "RenderData", "short", "simplify_subsurf_render"))
    {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->r.simplify_subsurf_render = scene->r.simplify_subsurf;
        scene->r.simplify_particles_render = scene->r.simplify_particles;
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "DecimateModifierData", "float", "defgrp_factor"))
    {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_Decimate) {
            DecimateModifierData *dmd = (DecimateModifierData *)md;
            dmd->defgrp_factor = 1.0f;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 275, 3)) {
#define BRUSH_TORUS (1 << 1)
    LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
      br->flag &= ~BRUSH_TORUS;
    }
#undef BRUSH_TORUS
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 276, 2)) {
    if (!DNA_struct_member_exists(fd->filesdna, "bPoseChannel", "float", "custom_scale")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        if (ob->pose) {
          LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
            pchan->custom_scale = 1.0f;
          }
        }
      }
    }

#define RV3D_VIEW_PERSPORTHO 7
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_VIEW3D) {
            ListBase *lb = (sl == area->spacedata.first) ? &area->regionbase : &sl->regionbase;
            LISTBASE_FOREACH (ARegion *, region, lb) {
              if (region->regiontype == RGN_TYPE_WINDOW) {
                if (region->regiondata) {
                  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
                  if (rv3d->view == RV3D_VIEW_PERSPORTHO) {
                    rv3d->view = RV3D_VIEW_USER;
                  }
                }
              }
            }
            break;
          }
        }
      }
    }
#undef RV3D_VIEW_PERSPORTHO

#define LA_YF_PHOTON 5
    LISTBASE_FOREACH (Light *, la, &bmain->lights) {
      if (la->type == LA_YF_PHOTON) {
        la->type = LA_LOCAL;
      }
    }
#undef LA_YF_PHOTON
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 276, 3)) {
    if (!DNA_struct_member_exists(
            fd->filesdna, "RenderData", "CurveMapping", "mblur_shutter_curve"))
    {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        CurveMapping *curve_mapping = &scene->r.mblur_shutter_curve;
        BKE_curvemapping_set_defaults(curve_mapping, 1, 0.0f, 0.0f, 1.0f, 1.0f, HD_AUTO);
        BKE_curvemapping_init(curve_mapping);
        BKE_curvemap_reset(curve_mapping->cm,
                           &curve_mapping->clipr,
                           CURVE_PRESET_MAX,
                           CurveMapSlopeType::PositiveNegative);
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 276, 4)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ToolSettings *ts = scene->toolsettings;
      if (!DNA_struct_member_exists(fd->filesdna, "ToolSettings", "char", "gpencil_v3d_align")) {
        ts->gpencil_v3d_align = GP_PROJECT_VIEWSPACE;
        ts->gpencil_v2d_align = GP_PROJECT_VIEWSPACE;
      }
    }

    LISTBASE_FOREACH (bGPdata *, gpd, &bmain->gpencils) {
      bool enabled = false;

      /* Ensure that the data-block's onion-skinning toggle flag
       * stays in sync with the status of the actual layers. */
      LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
        if (gpl->flag & GP_LAYER_ONIONSKIN) {
          enabled = true;
        }
      }

      if (enabled) {
        gpd->flag |= GP_DATA_SHOW_ONIONSKINS;
      }
      else {
        gpd->flag &= ~GP_DATA_SHOW_ONIONSKINS;
      }
    }
  }
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 276, 5)) {
    /* Important to clear all non-persistent flags from older versions here,
     * otherwise they could collide with any new persistent flag we may add in the future. */
    MainListsArray lbarray = BKE_main_lists_get(*bmain);
    int a = lbarray.size();
    while (a--) {
      LISTBASE_FOREACH (ID *, id, lbarray[a]) {
        id->flag &= ID_FLAG_FAKEUSER;

        /* NOTE: This is added in 4.1 code.
         *
         * Original commit (3fcf535d2e) forgot to handle embedded IDs. Fortunately, back then, the
         * only embedded IDs that existed were the NodeTree ones, and the current API to access
         * them should still be valid on code from 9 years ago. */
        bNodeTree *node_tree = blender::bke::node_tree_from_id(id);
        if (node_tree) {
          node_tree->id.flag &= ID_FLAG_FAKEUSER;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 276, 7)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->r.bake.pass_filter = R_BAKE_PASS_FILTER_ALL;
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 277, 1)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      ParticleEditSettings *pset = &scene->toolsettings->particle;
      for (int a = 0; a < ARRAY_SIZE(pset->brush); a++) {
        if (pset->brush[a].strength > 1.0f) {
          pset->brush[a].strength *= 0.01f;
        }
      }
    }

    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase :
                                                                 &sl->regionbase;
          /* Bug: Was possible to add preview region to sequencer view by using AZones. */
          if (sl->spacetype == SPACE_SEQ) {
            SpaceSeq *sseq = (SpaceSeq *)sl;
            if (sseq->view == SEQ_VIEW_SEQUENCE) {
              LISTBASE_FOREACH (ARegion *, region, regionbase) {
                /* remove preview region for sequencer-only view! */
                if (region->regiontype == RGN_TYPE_PREVIEW) {
                  region->flag |= RGN_FLAG_HIDDEN;
                  region->alignment = RGN_ALIGN_NONE;
                  break;
                }
              }
            }
          }
          /* Remove old deprecated region from file-browsers. */
          else if (sl->spacetype == SPACE_FILE) {
            LISTBASE_FOREACH (ARegion *, region, regionbase) {
              if (region->regiontype == RGN_TYPE_CHANNELS) {
                /* Free old deprecated 'channel' region... */
                BKE_area_region_free(nullptr, region);
                BLI_freelinkN(regionbase, region);
                break;
              }
            }
          }
        }
      }
    }

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      CurvePaintSettings *cps = &scene->toolsettings->curve_paint_settings;
      if (cps->error_threshold == 0) {
        cps->curve_type = CU_BEZIER;
        cps->flag |= CURVE_PAINT_FLAG_CORNERS_DETECT;
        cps->error_threshold = 8;
        cps->radius_max = 1.0f;
        cps->corner_angle = DEG2RADF(70.0f);
      }
    }

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed) {
        blender::seq::foreach_strip(&scene->ed->seqbase, strip_init_text_effect_data, nullptr);
      }
    }

    /* Adding "Properties" region to DopeSheet */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        /* handle pushed-back space data first */
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_ACTION) {
            SpaceAction *saction = (SpaceAction *)sl;
            do_version_action_editor_properties_region(&saction->regionbase);
          }
        }

        /* active spacedata info must be handled too... */
        if (area->spacetype == SPACE_ACTION) {
          do_version_action_editor_properties_region(&area->regionbase);
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 277, 2)) {
    if (!DNA_struct_member_exists(fd->filesdna, "Bone", "float", "scale_in_x")) {
      LISTBASE_FOREACH (bArmature *, arm, &bmain->armatures) {
        do_version_bones_super_bbone(&arm->bonebase);
      }
    }
    if (!DNA_struct_member_exists(fd->filesdna, "bPoseChannel", "float", "scale_in_x")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        if (ob->pose) {
          LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
            /* see do_version_bones_super_bbone()... */
            pchan->scale_in_x = pchan->scale_in_z = 1.0f;
            pchan->scale_out_x = pchan->scale_out_z = 1.0f;

            /* also make sure some legacy (unused for over a decade) flags are unset,
             * so that we can reuse them for stuff that matters now...
             * (i.e. POSE_IK_MAT, (unknown/unused x 4), POSE_HAS_IK)
             *
             * These seem to have been runtime flags used by the IK solver, but that stuff
             * should be able to be recalculated automatically anyway, so it should be fine.
             */
            pchan->flag &= ~((1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8));
          }
        }
      }
    }

    LISTBASE_FOREACH (Camera *, camera, &bmain->cameras) {
      if (camera->stereo.pole_merge_angle_from == 0.0f &&
          camera->stereo.pole_merge_angle_to == 0.0f)
      {
        camera->stereo.pole_merge_angle_from = DEG2RADF(60.0f);
        camera->stereo.pole_merge_angle_to = DEG2RADF(75.0f);
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "NormalEditModifierData", "float", "mix_limit")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_NormalEdit) {
            NormalEditModifierData *nemd = (NormalEditModifierData *)md;
            nemd->mix_limit = DEG2RADF(180.0f);
          }
        }
      }
    }

    if (!DNA_struct_member_exists(
            fd->filesdna, "BooleanModifierData", "float", "double_threshold"))
    {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_Boolean) {
            BooleanModifierData *bmd = (BooleanModifierData *)md;
            bmd->double_threshold = 1e-6f;
          }
        }
      }
    }

    LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
      if (br->sculpt_brush_type == SCULPT_BRUSH_TYPE_FLATTEN) {
        br->flag |= BRUSH_ACCUMULATE;
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "ClothSimSettings", "float", "time_scale")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_Cloth) {
            ClothModifierData *clmd = (ClothModifierData *)md;
            clmd->sim_parms->time_scale = 1.0f;
          }
          else if (md->type == eModifierType_ParticleSystem) {
            ParticleSystemModifierData *pmd = (ParticleSystemModifierData *)md;
            if (pmd->psys->clmd) {
              pmd->psys->clmd->sim_parms->time_scale = 1.0f;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 277, 3)) {
    /* ------- init of grease pencil initialization --------------- */
    if (!DNA_struct_member_exists(fd->filesdna, "bGPDstroke", "bGPDpalettecolor", "*palcolor")) {
      /* Convert Grease Pencil to new palettes/brushes
       * Loop all strokes and create the palette and all colors
       */
      LISTBASE_FOREACH (bGPdata *, gpd, &bmain->gpencils) {
        if (BLI_listbase_is_empty(&gpd->palettes)) {
          /* create palette */
          bGPDpalette *palette = BKE_gpencil_palette_addnew(gpd, "GP_Palette");
          LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
            /* create color using layer name */
            bGPDpalettecolor *palcolor = BKE_gpencil_palettecolor_addnew(palette, gpl->info);
            if (palcolor != nullptr) {
              /* set color attributes */
              copy_v4_v4(palcolor->color, gpl->color);
              copy_v4_v4(palcolor->fill, gpl->fill);

              if (gpl->flag & GP_LAYER_HIDE) {
                palcolor->flag |= PC_COLOR_HIDE;
              }
              if (gpl->flag & GP_LAYER_LOCKED) {
                palcolor->flag |= PC_COLOR_LOCKED;
              }
              if (gpl->flag & GP_LAYER_ONIONSKIN) {
                palcolor->flag |= PC_COLOR_ONIONSKIN;
              }
              if (gpl->flag & GP_LAYER_VOLUMETRIC) {
                palcolor->flag |= PC_COLOR_VOLUMETRIC;
              }

              /* set layer opacity to 1 */
              gpl->opacity = 1.0f;

              /* set tint color */
              ARRAY_SET_ITEMS(gpl->tintcolor, 0.0f, 0.0f, 0.0f, 0.0f);

              /* flush relevant layer-settings to strokes */
              LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
                LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
                  /* set stroke to palette and force recalculation */
                  STRNCPY_UTF8(gps->colorname, gpl->info);
                  gps->thickness = gpl->thickness;

                  /* set alpha strength to 1 */
                  for (int i = 0; i < gps->totpoints; i++) {
                    gps->points[i].strength = 1.0f;
                  }
                }
              }
            }
          }
        }
      }
    }
    /* ------- end of grease pencil initialization --------------- */
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 278, 0)) {
    if (!DNA_struct_member_exists(fd->filesdna, "MovieTrackingTrack", "float", "weight_stab")) {
      LISTBASE_FOREACH (MovieClip *, clip, &bmain->movieclips) {
        const MovieTracking *tracking = &clip->tracking;
        LISTBASE_FOREACH (MovieTrackingObject *, tracking_object, &tracking->objects) {
          const ListBase *tracksbase = (tracking_object->flag & TRACKING_OBJECT_CAMERA) ?
                                           &tracking->tracks_legacy :
                                           &tracking_object->tracks;
          LISTBASE_FOREACH (MovieTrackingTrack *, track, tracksbase) {
            track->weight_stab = track->weight;
          }
        }
      }
    }

    if (!DNA_struct_member_exists(
            fd->filesdna, "MovieTrackingStabilization", "int", "tot_rot_track"))
    {
      LISTBASE_FOREACH (MovieClip *, clip, &bmain->movieclips) {
        if (clip->tracking.stabilization.rot_track_legacy) {
          migrate_single_rot_stabilization_track_settings(&clip->tracking.stabilization);
        }
        if (clip->tracking.stabilization.scale == 0.0f) {
          /* ensure init.
           * Was previously used for auto-scale only,
           * now used always (as "target scale") */
          clip->tracking.stabilization.scale = 1.0f;
        }
        /* blender prefers 1-based frame counting;
         * thus using frame 1 as reference typically works best */
        clip->tracking.stabilization.anchor_frame = 1;
        /* by default show the track lists expanded, to improve "discoverability" */
        clip->tracking.stabilization.flag |= TRACKING_SHOW_STAB_TRACKS;
      }
    }
  }
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 278, 2)) {
    if (!DNA_struct_member_exists(fd->filesdna, "FFMpegCodecData", "int", "ffmpeg_preset")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        /* "medium" is the preset FFmpeg uses when no presets are given. */
        scene->r.ffcodecdata.ffmpeg_preset = FFM_PRESET_MEDIUM;
      }
    }
    if (!DNA_struct_member_exists(fd->filesdna, "FFMpegCodecData", "int", "constant_rate_factor"))
    {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        /* fall back to behavior from before we introduced CRF for old files */
        scene->r.ffcodecdata.constant_rate_factor = FFM_CRF_NONE;
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "FluidModifierData", "float", "slice_per_voxel")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_Fluid) {
            FluidModifierData *fmd = (FluidModifierData *)md;
            if (fmd->domain) {
              fmd->domain->slice_per_voxel = 5.0f;
              fmd->domain->slice_depth = 0.5f;
              fmd->domain->display_thickness = 1.0f;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 278, 3)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->toolsettings != nullptr) {
        ToolSettings *ts = scene->toolsettings;
        ParticleEditSettings *pset = &ts->particle;
        for (int a = 0; a < ARRAY_SIZE(pset->brush); a++) {
          if (pset->brush[a].count == 0) {
            pset->brush[a].count = 10;
          }
        }
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "RigidBodyCon", "float", "spring_stiffness_ang_x"))
    {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        RigidBodyCon *rbc = ob->rigidbody_constraint;
        if (rbc) {
          rbc->spring_stiffness_ang_x = 10.0;
          rbc->spring_stiffness_ang_y = 10.0;
          rbc->spring_stiffness_ang_z = 10.0;
          rbc->spring_damping_ang_x = 0.5;
          rbc->spring_damping_ang_y = 0.5;
          rbc->spring_damping_ang_z = 0.5;
        }
      }
    }

    /* constant detail for sculpting is now a resolution value instead of
     * a percentage, we reuse old DNA struct member but convert it */
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->toolsettings != nullptr) {
        ToolSettings *ts = scene->toolsettings;
        if (ts->sculpt && ts->sculpt->constant_detail != 0.0f) {
          ts->sculpt->constant_detail = 100.0f / ts->sculpt->constant_detail;
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 278, 4)) {
    const float sqrt_3 = float(M_SQRT3);
    LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
      br->fill_threshold /= sqrt_3;
    }

    /* Custom motion paths */
    if (!DNA_struct_member_exists(fd->filesdna, "bMotionPath", "int", "line_thickness")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        if (bMotionPath *mpath = ob->mpath) {
          mpath->color[0] = 1.0f;
          mpath->color[1] = 0.0f;
          mpath->color[2] = 0.0f;
          mpath->line_thickness = 1;
          mpath->flag |= MOTIONPATH_FLAG_LINES;
        }
        /* bones motion path */
        if (ob->pose) {
          LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
            if (bMotionPath *mpath = pchan->mpath) {
              mpath->color[0] = 1.0f;
              mpath->color[1] = 0.0f;
              mpath->color[2] = 0.0f;
              mpath->line_thickness = 1;
              mpath->flag |= MOTIONPATH_FLAG_LINES;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 278, 5)) {
    /* Mask primitive adding code was not initializing correctly id_type of its points' parent. */
    LISTBASE_FOREACH (Mask *, mask, &bmain->masks) {
      LISTBASE_FOREACH (MaskLayer *, mlayer, &mask->masklayers) {
        LISTBASE_FOREACH (MaskSpline *, mspline, &mlayer->splines) {
          int i = 0;
          for (MaskSplinePoint *mspoint = mspline->points; i < mspline->tot_point; mspoint++, i++)
          {
            if (mspoint->parent.id_type == 0) {
              BKE_mask_parent_init(&mspoint->parent);
            }
          }
        }
      }
    }

    /* Fix for #50736, Glare comp node using same var for two different things. */
    if (!DNA_struct_member_exists(fd->filesdna, "NodeGlare", "char", "star_45")) {
      FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
        if (ntree->type == NTREE_COMPOSIT) {
          blender::bke::node_tree_set_type(*ntree);
          LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
            if (node->type_legacy == CMP_NODE_GLARE) {
              NodeGlare *ndg = static_cast<NodeGlare *>(node->storage);
              switch (ndg->type) {
                case CMP_NODE_GLARE_STREAKS:
                  ndg->streaks = ndg->angle;
                  break;
                case CMP_NODE_GLARE_SIMPLE_STAR:
                  ndg->star_45 = ndg->angle != 0;
                  break;
                default:
                  break;
              }
            }
          }
        }
      }
      FOREACH_NODETREE_END;
    }

    if (!DNA_struct_member_exists(fd->filesdna, "SurfaceDeformModifierData", "float", "mat[4][4]"))
    {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_SurfaceDeform) {
            SurfaceDeformModifierData *smd = (SurfaceDeformModifierData *)md;
            unit_m4(smd->mat);
          }
        }
      }
    }

    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        do_versions_compositor_render_passes(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 279, 0)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->r.im_format.exr_codec == R_IMF_EXR_CODEC_DWAB) {
        scene->r.im_format.exr_codec = R_IMF_EXR_CODEC_DWAA;
      }
    }

    /* Fix related to VGroup modifiers creating named defgroup CD layers! See #51520. */
    LISTBASE_FOREACH (Mesh *, me, &bmain->meshes) {
      CustomData_set_layer_name(&me->vert_data, CD_MDEFORMVERT, 0, "");
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 279, 3)) {
    if (!DNA_struct_member_exists(fd->filesdna, "FluidDomainSettings", "float", "clipping")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_Fluid) {
            FluidModifierData *fmd = (FluidModifierData *)md;
            if (fmd->domain) {
              fmd->domain->clipping = 1e-3f;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 279, 4)) {
    /* Fix for invalid state of screen due to bug in older versions. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        if (area->full && screen->state == SCREENNORMAL) {
          area->full = nullptr;
        }
      }
    }

    if (!DNA_struct_member_exists(fd->filesdna, "Brush", "float", "falloff_angle")) {
      LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
        br->falloff_angle = DEG2RADF(80);
        /* These flags are used for new features. They are not related to `falloff_angle`. */
        br->flag &= ~(BRUSH_INVERT_TO_SCRAPE_FILL | BRUSH_ORIGINAL_PLANE |
                      BRUSH_GRAB_ACTIVE_VERTEX | BRUSH_SCENE_SPACING | BRUSH_FRONTFACE_FALLOFF);
      }

      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        ToolSettings *ts = scene->toolsettings;
        for (int i = 0; i < 2; i++) {
          VPaint *vp = i ? ts->vpaint : ts->wpaint;
          if (vp != nullptr) {
            /* remove all other flags */
            vp->flag &= (VP_FLAG_VGROUP_RESTRICT);
          }
        }
      }
    }

    /* Simple deform modifier no longer assumes Z axis (X for bend type).
     * Must set previous defaults. */
    if (!DNA_struct_member_exists(fd->filesdna, "SimpleDeformModifierData", "char", "deform_axis"))
    {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_SimpleDeform) {
            SimpleDeformModifierData *smd = (SimpleDeformModifierData *)md;
            smd->deform_axis = 2;
          }
        }
      }
    }

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      int preset = scene->r.ffcodecdata.ffmpeg_preset;
      if (preset == FFM_PRESET_NONE || preset >= FFM_PRESET_GOOD) {
        continue;
      }
      if (preset <= FFM_PRESET_FAST) {
        preset = FFM_PRESET_REALTIME;
      }
      else if (preset >= FFM_PRESET_SLOW) {
        preset = FFM_PRESET_BEST;
      }
      else {
        preset = FFM_PRESET_GOOD;
      }
      scene->r.ffcodecdata.ffmpeg_preset = preset;
    }

    if (!DNA_struct_member_exists(
            fd->filesdna, "ParticleInstanceModifierData", "float", "particle_amount"))
    {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_ParticleInstance) {
            ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *)md;
            pimd->space = eParticleInstanceSpace_World;
            pimd->particle_amount = 1.0f;
          }
        }
      }
    }
  }
}

void do_versions_after_linking_270(Main *bmain)
{
  /* To be added to next subversion bump! */
  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 279, 0)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        blender::bke::node_tree_set_type(*ntree);
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type_legacy == CMP_NODE_HUE_SAT) {
            do_version_hue_sat_node(ntree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_FILE_ATLEAST(bmain, 279, 2)) {
    /* B-Bones (bbone_in/out -> bbone_easein/out) + Stepped FMod Frame Start/End fix */
    BKE_fcurves_main_cb(bmain, do_version_bbone_easing_fcurve_fix);
  }
}
