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

#include "BLI_utildefines.h"
#include "BLI_compiler_attrs.h"

/* for MinGW32 definition of NULL, could use BLI_blenlib.h instead too */
#include <stddef.h>

/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_cloth_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_sdna_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_mask_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_view3d_types.h"
#include "DNA_smoke_types.h"
#include "DNA_rigidbody_types.h"

#include "DNA_genfile.h"

#include "BKE_animsys.h"
#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_fcurve.h"
#include "BKE_gpencil.h"
#include "BKE_main.h"
#include "BKE_mask.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_scene.h"
#include "BKE_sequencer.h"
#include "BKE_screen.h"
#include "BKE_tracking.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"

#include "BLT_translation.h"

#include "BLO_readfile.h"

#include "NOD_common.h"
#include "NOD_socket.h"
#include "NOD_composite.h"

#include "readfile.h"

#include "MEM_guardedalloc.h"

/* ************************************************** */
/* GP Palettes API (Deprecated) */

/* add a new gp-palette */
static bGPDpalette *BKE_gpencil_palette_addnew(bGPdata *gpd, const char *name)
{
  bGPDpalette *palette;

  /* check that list is ok */
  if (gpd == NULL) {
    return NULL;
  }

  /* allocate memory and add to end of list */
  palette = MEM_callocN(sizeof(bGPDpalette), "bGPDpalette");

  /* add to datablock */
  BLI_addtail(&gpd->palettes, palette);

  /* set basic settings */
  /* auto-name */
  BLI_strncpy(palette->info, name, sizeof(palette->info));
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
  if (palette == NULL) {
    return NULL;
  }

  /* allocate memory and add to end of list */
  palcolor = MEM_callocN(sizeof(bGPDpalettecolor), "bGPDpalettecolor");

  /* add to datablock */
  BLI_addtail(&palette->colors, palcolor);

  /* set basic settings */
  copy_v4_v4(palcolor->color, U.gpencil_new_layer_col);
  ARRAY_SET_ITEMS(palcolor->fill, 1.0f, 1.0f, 1.0f);

  /* auto-name */
  BLI_strncpy(palcolor->info, name, sizeof(palcolor->info));
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
  if (stab->rot_track) {
    if (!(stab->rot_track->flag & TRACK_USE_2D_STAB_ROT)) {
      stab->tot_rot_track++;
      stab->rot_track->flag |= TRACK_USE_2D_STAB_ROT;
    }
  }
  stab->rot_track = NULL; /* this field is now ignored */
}

static void do_version_constraints_radians_degrees_270_1(ListBase *lb)
{
  bConstraint *con;

  for (con = lb->first; con; con = con->next) {
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
  bConstraint *con;

  for (con = lb->first; con; con = con->next) {
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
  bConstraint *con;

  for (con = lb->first; con; con = con->next) {
    if (con->type == CONSTRAINT_TYPE_STRETCHTO) {
      bStretchToConstraint *data = (bStretchToConstraint *)con->data;
      data->bulge_min = 1.0f;
      data->bulge_max = 1.0f;
    }
  }
}

static void do_version_action_editor_properties_region(ListBase *regionbase)
{
  ARegion *ar;

  for (ar = regionbase->first; ar; ar = ar->next) {
    if (ar->regiontype == RGN_TYPE_UI) {
      /* already exists */
      return;
    }
    else if (ar->regiontype == RGN_TYPE_WINDOW) {
      /* add new region here */
      ARegion *arnew = MEM_callocN(sizeof(ARegion), "buttons for action");

      BLI_insertlinkbefore(regionbase, ar, arnew);

      arnew->regiontype = RGN_TYPE_UI;
      arnew->alignment = RGN_ALIGN_RIGHT;
      arnew->flag = RGN_FLAG_HIDDEN;

      return;
    }
  }
}

static void do_version_bones_super_bbone(ListBase *lb)
{
  for (Bone *bone = lb->first; bone; bone = bone->next) {
    bone->scale_in_x = bone->scale_in_y = 1.0f;
    bone->scale_out_x = bone->scale_out_y = 1.0f;

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
  if (node->storage == NULL) {
    return;
  }

  /* Make sure new sockets are properly created. */
  node_verify_socket_templates(ntree, node);
  /* Convert value from old storage to new sockets. */
  NodeHueSat *nhs = node->storage;
  bNodeSocket *hue = nodeFindSocket(node, SOCK_IN, "Hue"),
              *saturation = nodeFindSocket(node, SOCK_IN, "Saturation"),
              *value = nodeFindSocket(node, SOCK_IN, "Value");
  ((bNodeSocketValueFloat *)hue->default_value)->value = nhs->hue;
  ((bNodeSocketValueFloat *)saturation->default_value)->value = nhs->sat;
  ((bNodeSocketValueFloat *)value->default_value)->value = nhs->val;
  /* Take care of possible animation. */
  AnimData *adt = BKE_animdata_from_id(&ntree->id);
  if (adt != NULL && adt->action != NULL) {
    const char *prefix = BLI_sprintfN("nodes[\"%s\"]", node->name);
    for (FCurve *fcu = adt->action->curves.first; fcu != NULL; fcu = fcu->next) {
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
  node->storage = NULL;
}

static void do_versions_compositor_render_passes_storage(bNode *node)
{
  int pass_index = 0;
  const char *sockname;
  for (bNodeSocket *sock = node->outputs.first; sock && pass_index < 31;
       sock = sock->next, pass_index++) {
    if (sock->storage == NULL) {
      NodeImageLayer *sockdata = MEM_callocN(sizeof(NodeImageLayer), "node image layer");
      sock->storage = sockdata;
      BLI_strncpy(sockdata->pass_name,
                  node_cmp_rlayers_sock_to_pass(pass_index),
                  sizeof(sockdata->pass_name));

      if (pass_index == 0) {
        sockname = "Image";
      }
      else if (pass_index == 1) {
        sockname = "Alpha";
      }
      else {
        sockname = node_cmp_rlayers_sock_to_pass(pass_index);
      }
      BLI_strncpy(sock->name, sockname, sizeof(sock->name));
    }
  }
}

static void do_versions_compositor_render_passes(bNodeTree *ntree)
{
  for (bNode *node = ntree->nodes.first; node; node = node->next) {
    if (node->type == CMP_NODE_R_LAYERS) {
      /* First we make sure existing sockets have proper names.
       * This is important because otherwise verification will
       * drop links from sockets which were renamed.
       */
      do_versions_compositor_render_passes_storage(node);
      /* Make sure new sockets are properly created. */
      node_verify_socket_templates(ntree, node);
      /* Make sure all possibly created sockets have proper storage. */
      do_versions_compositor_render_passes_storage(node);
    }
  }
}

static char *replace_bbone_easing_rnapath(char *old_path)
{
  char *new_path = NULL;

  /* NOTE: This will break paths for any bones/custom-properties
   * which happen be named after the bbone property id's
   */
  if (strstr(old_path, "bbone_in")) {
    new_path = BLI_str_replaceN(old_path, "bbone_in", "bbone_easein");
  }
  else if (strstr(old_path, "bbone_out")) {
    new_path = BLI_str_replaceN(old_path, "bbone_out", "bbone_easeout");
  }

  if (new_path) {
    MEM_freeN(old_path);
    return new_path;
  }
  else {
    return old_path;
  }
}

static void do_version_bbone_easing_fcurve_fix(ID *UNUSED(id),
                                               FCurve *fcu,
                                               void *UNUSED(user_data))
{
  /* F-Curve's path (for bbone_in/out) */
  if (fcu->rna_path) {
    fcu->rna_path = replace_bbone_easing_rnapath(fcu->rna_path);
  }

  /* Driver -> Driver Vars (for bbone_in/out) */
  if (fcu->driver) {
    for (DriverVar *dvar = fcu->driver->variables.first; dvar; dvar = dvar->next) {
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
    for (FModifier *fcm = fcu->modifiers.first; fcm; fcm = fcm->next) {
      if (fcm->type == FMODIFIER_TYPE_STEPPED) {
        FMod_Stepped *data = fcm->data;

        /* Modifier doesn't work if the modifier's copy of start/end frame are both 0
         * as those were only getting written to the fcm->data copy (T52009)
         */
        if ((fcm->sfra == fcm->efra) && (fcm->sfra == 0)) {
          fcm->sfra = data->start_frame;
          fcm->efra = data->end_frame;
        }
      }
    }
  }
}

void blo_do_versions_270(FileData *fd, Library *UNUSED(lib), Main *bmain)
{
  if (!MAIN_VERSION_ATLEAST(bmain, 270, 0)) {

    if (!DNA_struct_elem_find(fd->filesdna, "BevelModifierData", "float", "profile")) {
      Object *ob;

      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        ModifierData *md;
        for (md = ob->modifiers.first; md; md = md->next) {
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
        bNode *node;
        for (node = ntree->nodes.first; node; node = node->next) {
          if (ELEM(node->type, CMP_NODE_COMPOSITE, CMP_NODE_OUTPUT_FILE)) {
            node->id = NULL;
          }
        }
      }
    }
    FOREACH_NODETREE_END;

    {
      bScreen *screen;

      for (screen = bmain->screens.first; screen; screen = screen->id.next) {
        ScrArea *area;
        for (area = screen->areabase.first; area; area = area->next) {
          SpaceLink *space_link;
          for (space_link = area->spacedata.first; space_link; space_link = space_link->next) {
            if (space_link->spacetype == SPACE_CLIP) {
              SpaceClip *space_clip = (SpaceClip *)space_link;
              if (space_clip->mode != SC_MODE_MASKEDIT) {
                space_clip->mode = SC_MODE_TRACKING;
              }
            }
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "MovieTrackingSettings", "float", "default_weight")) {
      MovieClip *clip;
      for (clip = bmain->movieclips.first; clip; clip = clip->id.next) {
        clip->tracking.settings.default_weight = 1.0f;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 270, 1)) {
    Object *ob;

    /* Update Transform constraint (another deg -> rad stuff). */
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      do_version_constraints_radians_degrees_270_1(&ob->constraints);

      if (ob->pose) {
        /* Bones constraints! */
        bPoseChannel *pchan;
        for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
          do_version_constraints_radians_degrees_270_1(&pchan->constraints);
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 270, 2)) {
    Mesh *me;

    /* Mesh smoothresh deg->rad. */
    for (me = bmain->meshes.first; me; me = me->id.next) {
      me->smoothresh = DEG2RADF(me->smoothresh);
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 270, 3)) {
    FreestyleLineStyle *linestyle;

    for (linestyle = bmain->linestyles.first; linestyle; linestyle = linestyle->id.next) {
      linestyle->flag |= LS_NO_SORTING;
      linestyle->sort_key = LS_SORT_KEY_DISTANCE_FROM_CAMERA;
      linestyle->integration_type = LS_INTEGRATION_MEAN;
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 270, 4)) {
    /* ui_previews were not handled correctly when copying areas,
     * leading to corrupted files (see T39847).
     * This will always reset situation to a valid state.
     */
    bScreen *sc;

    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      ScrArea *sa;
      for (sa = sc->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;

        for (sl = sa->spacedata.first; sl; sl = sl->next) {
          ARegion *ar;
          ListBase *lb = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;

          for (ar = lb->first; ar; ar = ar->next) {
            BLI_listbase_clear(&ar->ui_previews);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 270, 5)) {
    Object *ob;

    /* Update Transform constraint (again :|). */
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      do_version_constraints_radians_degrees_270_5(&ob->constraints);

      if (ob->pose) {
        /* Bones constraints! */
        bPoseChannel *pchan;
        for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
          do_version_constraints_radians_degrees_270_5(&pchan->constraints);
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 271, 0)) {
    if (!DNA_struct_elem_find(fd->filesdna, "RenderData", "BakeData", "bake")) {
      Scene *sce;

      for (sce = bmain->scenes.first; sce; sce = sce->id.next) {
        sce->r.bake.flag = R_BAKE_CLEAR;
        sce->r.bake.width = 512;
        sce->r.bake.height = 512;
        sce->r.bake.margin = 16;
        sce->r.bake.normal_space = R_BAKE_SPACE_TANGENT;
        sce->r.bake.normal_swizzle[0] = R_BAKE_POSX;
        sce->r.bake.normal_swizzle[1] = R_BAKE_POSY;
        sce->r.bake.normal_swizzle[2] = R_BAKE_POSZ;
        BLI_strncpy(sce->r.bake.filepath, U.renderdir, sizeof(sce->r.bake.filepath));

        sce->r.bake.im_format.planes = R_IMF_PLANES_RGBA;
        sce->r.bake.im_format.imtype = R_IMF_IMTYPE_PNG;
        sce->r.bake.im_format.depth = R_IMF_CHAN_DEPTH_8;
        sce->r.bake.im_format.quality = 90;
        sce->r.bake.im_format.compress = 15;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "FreestyleLineStyle", "float", "texstep")) {
      FreestyleLineStyle *linestyle;

      for (linestyle = bmain->linestyles.first; linestyle; linestyle = linestyle->id.next) {
        linestyle->flag |= LS_TEXTURE;
        linestyle->texstep = 1.0;
      }
    }

    {
      Scene *scene;
      for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
        int num_layers = BLI_listbase_count(&scene->r.layers);
        scene->r.actlay = min_ff(scene->r.actlay, num_layers - 1);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 271, 1)) {
    if (!DNA_struct_elem_find(fd->filesdna, "Material", "float", "line_col[4]")) {
      Material *mat;

      for (mat = bmain->materials.first; mat; mat = mat->id.next) {
        mat->line_col[0] = mat->line_col[1] = mat->line_col[2] = 0.0f;
        mat->line_col[3] = mat->alpha;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "RenderData", "int", "preview_start_resolution")) {
      Scene *scene;
      for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
        scene->r.preview_start_resolution = 64;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 271, 3)) {
    Brush *br;

    for (br = bmain->brushes.first; br; br = br->id.next) {
      br->fill_threshold = 0.2f;
    }

    if (!DNA_struct_elem_find(fd->filesdna, "BevelModifierData", "int", "mat")) {
      Object *ob;
      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        ModifierData *md;

        for (md = ob->modifiers.first; md; md = md->next) {
          if (md->type == eModifierType_Bevel) {
            BevelModifierData *bmd = (BevelModifierData *)md;
            bmd->mat = -1;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 271, 6)) {
    Object *ob;
    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      ModifierData *md;

      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_ParticleSystem) {
          ParticleSystemModifierData *pmd = (ParticleSystemModifierData *)md;
          if (pmd->psys && pmd->psys->clmd) {
            pmd->psys->clmd->sim_parms->vel_damping = 1.0f;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 272, 0)) {
    if (!DNA_struct_elem_find(fd->filesdna, "RenderData", "int", "preview_start_resolution")) {
      Scene *scene;
      for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
        scene->r.preview_start_resolution = 64;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 272, 1)) {
    Brush *br;
    for (br = bmain->brushes.first; br; br = br->id.next) {
      if ((br->ob_mode & OB_MODE_SCULPT) &&
          ELEM(br->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_SNAKE_HOOK)) {
        br->alpha = 1.0f;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 272, 2)) {
    if (!DNA_struct_elem_find(fd->filesdna, "Image", "float", "gen_color")) {
      Image *image;
      for (image = bmain->images.first; image != NULL; image = image->id.next) {
        image->gen_color[3] = 1.0f;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "bStretchToConstraint", "float", "bulge_min")) {
      Object *ob;

      /* Update Transform constraint (again :|). */
      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        do_version_constraints_stretch_to_limits(&ob->constraints);

        if (ob->pose) {
          /* Bones constraints! */
          bPoseChannel *pchan;
          for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
            do_version_constraints_stretch_to_limits(&pchan->constraints);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 273, 1)) {
#define BRUSH_RAKE (1 << 7)
#define BRUSH_RANDOM_ROTATION (1 << 25)

    Brush *br;

    for (br = bmain->brushes.first; br; br = br->id.next) {
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
  if (!MAIN_VERSION_ATLEAST(bmain, 273, 2)) {
    if (!DNA_struct_elem_find(fd->filesdna, "Scene", "DisplaySafeAreas", "safe_areas")) {
      Scene *scene;

      for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
        copy_v2_fl2(scene->safe_areas.title, 3.5f / 100.0f, 3.5f / 100.0f);
        copy_v2_fl2(scene->safe_areas.action, 10.0f / 100.0f, 5.0f / 100.0f);
        copy_v2_fl2(scene->safe_areas.title_center, 17.5f / 100.0f, 5.0f / 100.0f);
        copy_v2_fl2(scene->safe_areas.action_center, 15.0f / 100.0f, 5.0f / 100.0f);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 273, 3)) {
    ParticleSettings *part;
    for (part = bmain->particles.first; part; part = part->id.next) {
      if (part->clumpcurve) {
        part->child_flag |= PART_CHILD_USE_CLUMP_CURVE;
      }
      if (part->roughcurve) {
        part->child_flag |= PART_CHILD_USE_ROUGH_CURVE;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 273, 6)) {
    if (!DNA_struct_elem_find(fd->filesdna, "ClothSimSettings", "float", "bending_damping")) {
      Object *ob;
      ModifierData *md;
      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        for (md = ob->modifiers.first; md; md = md->next) {
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

    if (!DNA_struct_elem_find(fd->filesdna, "ParticleSettings", "float", "clump_noise_size")) {
      ParticleSettings *part;
      for (part = bmain->particles.first; part; part = part->id.next) {
        part->clump_noise_size = 1.0f;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "ParticleSettings", "int", "kink_extra_steps")) {
      ParticleSettings *part;
      for (part = bmain->particles.first; part; part = part->id.next) {
        part->kink_extra_steps = 4;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "MTex", "float", "kinkampfac")) {
      ParticleSettings *part;
      for (part = bmain->particles.first; part; part = part->id.next) {
        int a;
        for (a = 0; a < MAX_MTEX; a++) {
          MTex *mtex = part->mtex[a];
          if (mtex) {
            mtex->kinkampfac = 1.0f;
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "HookModifierData", "char", "flag")) {
      Object *ob;

      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        ModifierData *md;
        for (md = ob->modifiers.first; md; md = md->next) {
          if (md->type == eModifierType_Hook) {
            HookModifierData *hmd = (HookModifierData *)md;
            hmd->falloff_type = eHook_Falloff_InvSquare;
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "NodePlaneTrackDeformData", "char", "flag")) {
      FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
        if (ntree->type == NTREE_COMPOSIT) {
          bNode *node;
          for (node = ntree->nodes.first; node; node = node->next) {
            if (ELEM(node->type, CMP_NODE_PLANETRACKDEFORM)) {
              NodePlaneTrackDeformData *data = node->storage;
              data->flag = 0;
              data->motion_blur_samples = 16;
              data->motion_blur_shutter = 0.5f;
            }
          }
        }
      }
      FOREACH_NODETREE_END;
    }

    if (!DNA_struct_elem_find(fd->filesdna, "Camera", "GPUDOFSettings", "gpu_dof")) {
      Camera *ca;
      for (ca = bmain->cameras.first; ca; ca = ca->id.next) {
        ca->gpu_dof.fstop = 128.0f;
        ca->gpu_dof.focal_length = 1.0f;
        ca->gpu_dof.focus_distance = 1.0f;
        ca->gpu_dof.sensor = 1.0f;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 273, 8)) {
    Object *ob;
    for (ob = bmain->objects.first; ob != NULL; ob = ob->id.next) {
      ModifierData *md;
      for (md = ob->modifiers.last; md != NULL; md = md->prev) {
        if (modifier_unique_name(&ob->modifiers, md)) {
          printf(
              "Warning: Object '%s' had several modifiers with the "
              "same name, renamed one of them to '%s'.\n",
              ob->id.name + 2,
              md->name);
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 273, 9)) {
    bScreen *scr;
    ScrArea *sa;
    SpaceLink *sl;
    ARegion *ar;

    /* Make sure sequencer preview area limits zoom */
    for (scr = bmain->screens.first; scr; scr = scr->id.next) {
      for (sa = scr->areabase.first; sa; sa = sa->next) {
        for (sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_SEQ) {
            for (ar = sl->regionbase.first; ar; ar = ar->next) {
              if (ar->regiontype == RGN_TYPE_PREVIEW) {
                ar->v2d.keepzoom |= V2D_LIMITZOOM;
                ar->v2d.minzoom = 0.001f;
                ar->v2d.maxzoom = 1000.0f;
                break;
              }
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 274, 1)) {
    /* particle systems need to be forced to redistribute for jitter mode fix */
    {
      Object *ob;
      ParticleSystem *psys;
      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        for (psys = ob->particlesystem.first; psys; psys = psys->next) {
          if ((psys->pointcache->flag & PTCACHE_BAKED) == 0) {
            psys->recalc |= ID_RECALC_PSYS_RESET;
          }
        }
      }
    }

    /* hysteresis setted to 10% but not actived */
    if (!DNA_struct_elem_find(fd->filesdna, "LodLevel", "int", "obhysteresis")) {
      Object *ob;
      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        LodLevel *level;
        for (level = ob->lodlevels.first; level; level = level->next) {
          level->obhysteresis = 10;
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 274, 4)) {
    SceneRenderView *srv;
    wmWindowManager *wm;
    bScreen *screen;
    wmWindow *win;
    Scene *scene;
    Camera *cam;
    Image *ima;

    for (scene = bmain->scenes.first; scene; scene = scene->id.next) {
      Sequence *seq;

      BKE_scene_add_render_view(scene, STEREO_LEFT_NAME);
      srv = scene->r.views.first;
      BLI_strncpy(srv->suffix, STEREO_LEFT_SUFFIX, sizeof(srv->suffix));

      BKE_scene_add_render_view(scene, STEREO_RIGHT_NAME);
      srv = scene->r.views.last;
      BLI_strncpy(srv->suffix, STEREO_RIGHT_SUFFIX, sizeof(srv->suffix));

      SEQ_BEGIN (scene->ed, seq) {
        seq->stereo3d_format = MEM_callocN(sizeof(Stereo3dFormat), "Stereo Display 3d Format");

#define SEQ_USE_PROXY_CUSTOM_DIR (1 << 19)
#define SEQ_USE_PROXY_CUSTOM_FILE (1 << 21)
        if (seq->strip && seq->strip->proxy && !seq->strip->proxy->storage) {
          if (seq->flag & SEQ_USE_PROXY_CUSTOM_DIR) {
            seq->strip->proxy->storage = SEQ_STORAGE_PROXY_CUSTOM_DIR;
          }
          if (seq->flag & SEQ_USE_PROXY_CUSTOM_FILE) {
            seq->strip->proxy->storage = SEQ_STORAGE_PROXY_CUSTOM_FILE;
          }
        }
#undef SEQ_USE_PROXY_CUSTOM_DIR
#undef SEQ_USE_PROXY_CUSTOM_FILE
      }
      SEQ_END;
    }

    for (screen = bmain->screens.first; screen; screen = screen->id.next) {
      ScrArea *sa;
      for (sa = screen->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;

        for (sl = sa->spacedata.first; sl; sl = sl->next) {
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

    for (cam = bmain->cameras.first; cam; cam = cam->id.next) {
      cam->stereo.interocular_distance = 0.065f;
      cam->stereo.convergence_distance = 30.0f * 0.065f;
    }

    for (ima = bmain->images.first; ima; ima = ima->id.next) {
      ima->stereo3d_format = MEM_callocN(sizeof(Stereo3dFormat), "Image Stereo 3d Format");

      if (ima->packedfile) {
        ImagePackedFile *imapf = MEM_mallocN(sizeof(ImagePackedFile), "Image Packed File");
        BLI_addtail(&ima->packedfiles, imapf);

        imapf->packedfile = ima->packedfile;
        BLI_strncpy(imapf->filepath, ima->name, FILE_MAX);
        ima->packedfile = NULL;
      }
    }

    for (wm = bmain->wm.first; wm; wm = wm->id.next) {
      for (win = wm->windows.first; win; win = win->next) {
        win->stereo3d_format = MEM_callocN(sizeof(Stereo3dFormat), "Stereo Display 3d Format");
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 274, 6)) {
    bScreen *screen;

    if (!DNA_struct_elem_find(fd->filesdna, "FileSelectParams", "int", "thumbnail_size")) {
      for (screen = bmain->screens.first; screen; screen = screen->id.next) {
        ScrArea *sa;

        for (sa = screen->areabase.first; sa; sa = sa->next) {
          SpaceLink *sl;

          for (sl = sa->spacedata.first; sl; sl = sl->next) {
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

    if (!DNA_struct_elem_find(fd->filesdna, "RenderData", "short", "simplify_subsurf_render")) {
      Scene *scene;
      for (scene = bmain->scenes.first; scene != NULL; scene = scene->id.next) {
        scene->r.simplify_subsurf_render = scene->r.simplify_subsurf;
        scene->r.simplify_particles_render = scene->r.simplify_particles;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "DecimateModifierData", "float", "defgrp_factor")) {
      Object *ob;

      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        ModifierData *md;
        for (md = ob->modifiers.first; md; md = md->next) {
          if (md->type == eModifierType_Decimate) {
            DecimateModifierData *dmd = (DecimateModifierData *)md;
            dmd->defgrp_factor = 1.0f;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 275, 3)) {
    Brush *br;
#define BRUSH_TORUS (1 << 1)
    for (br = bmain->brushes.first; br; br = br->id.next) {
      br->flag &= ~BRUSH_TORUS;
    }
#undef BRUSH_TORUS
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 276, 2)) {
    if (!DNA_struct_elem_find(fd->filesdna, "bPoseChannel", "float", "custom_scale")) {
      Object *ob;

      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        if (ob->pose) {
          bPoseChannel *pchan;
          for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
            pchan->custom_scale = 1.0f;
          }
        }
      }
    }

    {
      bScreen *screen;
#define RV3D_VIEW_PERSPORTHO 7
      for (screen = bmain->screens.first; screen; screen = screen->id.next) {
        ScrArea *sa;
        for (sa = screen->areabase.first; sa; sa = sa->next) {
          SpaceLink *sl;
          for (sl = sa->spacedata.first; sl; sl = sl->next) {
            if (sl->spacetype == SPACE_VIEW3D) {
              ARegion *ar;
              ListBase *lb = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;
              for (ar = lb->first; ar; ar = ar->next) {
                if (ar->regiontype == RGN_TYPE_WINDOW) {
                  if (ar->regiondata) {
                    RegionView3D *rv3d = ar->regiondata;
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
    }

    {
#define LA_YF_PHOTON 5
      for (Light *la = bmain->lights.first; la; la = la->id.next) {
        if (la->type == LA_YF_PHOTON) {
          la->type = LA_LOCAL;
        }
      }
#undef LA_YF_PHOTON
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 276, 3)) {
    if (!DNA_struct_elem_find(fd->filesdna, "RenderData", "CurveMapping", "mblur_shutter_curve")) {
      Scene *scene;
      for (scene = bmain->scenes.first; scene != NULL; scene = scene->id.next) {
        CurveMapping *curve_mapping = &scene->r.mblur_shutter_curve;
        curvemapping_set_defaults(curve_mapping, 1, 0.0f, 0.0f, 1.0f, 1.0f);
        curvemapping_initialize(curve_mapping);
        curvemap_reset(
            curve_mapping->cm, &curve_mapping->clipr, CURVE_PRESET_MAX, CURVEMAP_SLOPE_POS_NEG);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 276, 4)) {
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      ToolSettings *ts = scene->toolsettings;

      if (ts->gp_sculpt.brush[0].size == 0) {
        GP_Sculpt_Settings *gset = &ts->gp_sculpt;
        GP_Sculpt_Data *brush;

        brush = &gset->brush[GP_SCULPT_TYPE_SMOOTH];
        brush->size = 25;
        brush->strength = 0.3f;
        brush->flag = GP_SCULPT_FLAG_USE_FALLOFF | GP_SCULPT_FLAG_SMOOTH_PRESSURE;

        brush = &gset->brush[GP_SCULPT_TYPE_THICKNESS];
        brush->size = 25;
        brush->strength = 0.5f;
        brush->flag = GP_SCULPT_FLAG_USE_FALLOFF;

        brush = &gset->brush[GP_SCULPT_TYPE_GRAB];
        brush->size = 50;
        brush->strength = 0.3f;
        brush->flag = GP_SCULPT_FLAG_USE_FALLOFF;

        brush = &gset->brush[GP_SCULPT_TYPE_PUSH];
        brush->size = 25;
        brush->strength = 0.3f;
        brush->flag = GP_SCULPT_FLAG_USE_FALLOFF;

        brush = &gset->brush[GP_SCULPT_TYPE_TWIST];
        brush->size = 50;
        brush->strength = 0.3f;  // XXX?
        brush->flag = GP_SCULPT_FLAG_USE_FALLOFF;

        brush = &gset->brush[GP_SCULPT_TYPE_PINCH];
        brush->size = 50;
        brush->strength = 0.5f;  // XXX?
        brush->flag = GP_SCULPT_FLAG_USE_FALLOFF;

        brush = &gset->brush[GP_SCULPT_TYPE_RANDOMIZE];
        brush->size = 25;
        brush->strength = 0.5f;
        brush->flag = GP_SCULPT_FLAG_USE_FALLOFF;

        brush = &gset->brush[GP_SCULPT_TYPE_CLONE];
        brush->size = 50;
        brush->strength = 1.0f;
      }

      if (!DNA_struct_elem_find(fd->filesdna, "ToolSettings", "char", "gpencil_v3d_align")) {
#if 0 /* XXX: Cannot do this, as we get random crashes... */
        if (scene->gpd) {
          bGPdata *gpd = scene->gpd;

          /* Copy over the settings stored in the GP data-block linked to the scene,
           * for minimal disruption. */
          ts->gpencil_v3d_align = 0;

          if (gpd->flag & GP_DATA_VIEWALIGN) {
            ts->gpencil_v3d_align |= GP_PROJECT_VIEWSPACE;
          }
          if (gpd->flag & GP_DATA_DEPTH_VIEW) {
            ts->gpencil_v3d_align |= GP_PROJECT_DEPTH_VIEW;
          }
          if (gpd->flag & GP_DATA_DEPTH_STROKE) {
            ts->gpencil_v3d_align |= GP_PROJECT_DEPTH_STROKE;
          }

          if (gpd->flag & GP_DATA_DEPTH_STROKE_ENDPOINTS) {
            ts->gpencil_v3d_align |= GP_PROJECT_DEPTH_STROKE_ENDPOINTS;
          }
        }
        else {
          /* Default to cursor for all standard 3D views */
          ts->gpencil_v3d_align = GP_PROJECT_VIEWSPACE;
        }
#endif

        ts->gpencil_v3d_align = GP_PROJECT_VIEWSPACE;
        ts->gpencil_v2d_align = GP_PROJECT_VIEWSPACE;
        ts->gpencil_seq_align = GP_PROJECT_VIEWSPACE;
        ts->gpencil_ima_align = GP_PROJECT_VIEWSPACE;
      }
    }

    for (bGPdata *gpd = bmain->gpencils.first; gpd; gpd = gpd->id.next) {
      bool enabled = false;

      /* Ensure that the datablock's onionskinning toggle flag
       * stays in sync with the status of the actual layers
       */
      for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
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
  if (!MAIN_VERSION_ATLEAST(bmain, 276, 5)) {
    ListBase *lbarray[MAX_LIBARRAY];
    int a;

    /* Important to clear all non-persistent flags from older versions here,
     * otherwise they could collide with any new persistent flag we may add in the future. */
    a = set_listbasepointers(bmain, lbarray);
    while (a--) {
      for (ID *id = lbarray[a]->first; id; id = id->next) {
        id->flag &= LIB_FAKEUSER;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 276, 7)) {
    Scene *scene;
    for (scene = bmain->scenes.first; scene != NULL; scene = scene->id.next) {
      scene->r.bake.pass_filter = R_BAKE_PASS_FILTER_ALL;
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 277, 1)) {
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      ParticleEditSettings *pset = &scene->toolsettings->particle;
      for (int a = 0; a < ARRAY_SIZE(pset->brush); a++) {
        if (pset->brush[a].strength > 1.0f) {
          pset->brush[a].strength *= 0.01f;
        }
      }
    }

    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
        for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
          ListBase *regionbase = (sl == sa->spacedata.first) ? &sa->regionbase : &sl->regionbase;
          /* Bug: Was possible to add preview region to sequencer view by using AZones. */
          if (sl->spacetype == SPACE_SEQ) {
            SpaceSeq *sseq = (SpaceSeq *)sl;
            if (sseq->view == SEQ_VIEW_SEQUENCE) {
              for (ARegion *ar = regionbase->first; ar; ar = ar->next) {
                /* remove preview region for sequencer-only view! */
                if (ar->regiontype == RGN_TYPE_PREVIEW) {
                  ar->flag |= RGN_FLAG_HIDDEN;
                  ar->alignment = RGN_ALIGN_NONE;
                  break;
                }
              }
            }
          }
          /* Remove old deprecated region from filebrowsers */
          else if (sl->spacetype == SPACE_FILE) {
            for (ARegion *ar = regionbase->first; ar; ar = ar->next) {
              if (ar->regiontype == RGN_TYPE_CHANNELS) {
                /* Free old deprecated 'channel' region... */
                BKE_area_region_free(NULL, ar);
                BLI_freelinkN(regionbase, ar);
                break;
              }
            }
          }
        }
      }
    }

    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      CurvePaintSettings *cps = &scene->toolsettings->curve_paint_settings;
      if (cps->error_threshold == 0) {
        cps->curve_type = CU_BEZIER;
        cps->flag |= CURVE_PAINT_FLAG_CORNERS_DETECT;
        cps->error_threshold = 8;
        cps->radius_max = 1.0f;
        cps->corner_angle = DEG2RADF(70.0f);
      }
    }

    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      Sequence *seq;

      SEQ_BEGIN (scene->ed, seq) {
        if (seq->type != SEQ_TYPE_TEXT) {
          continue;
        }

        if (seq->effectdata == NULL) {
          struct SeqEffectHandle effect_handle = BKE_sequence_get_effect(seq);
          effect_handle.init(seq);
        }

        TextVars *data = seq->effectdata;
        if (data->color[3] == 0.0f) {
          copy_v4_fl(data->color, 1.0f);
          data->shadow_color[3] = 1.0f;
        }
      }
      SEQ_END;
    }

    /* Adding "Properties" region to DopeSheet */
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
        /* handle pushed-back space data first */
        for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_ACTION) {
            SpaceAction *saction = (SpaceAction *)sl;
            do_version_action_editor_properties_region(&saction->regionbase);
          }
        }

        /* active spacedata info must be handled too... */
        if (sa->spacetype == SPACE_ACTION) {
          do_version_action_editor_properties_region(&sa->regionbase);
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 277, 2)) {
    if (!DNA_struct_elem_find(fd->filesdna, "Bone", "float", "scaleIn")) {
      for (bArmature *arm = bmain->armatures.first; arm; arm = arm->id.next) {
        do_version_bones_super_bbone(&arm->bonebase);
      }
    }
    if (!DNA_struct_elem_find(fd->filesdna, "bPoseChannel", "float", "scaleIn")) {
      for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
        if (ob->pose) {
          for (bPoseChannel *pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
            /* see do_version_bones_super_bbone()... */
            pchan->scale_in_x = pchan->scale_in_y = 1.0f;
            pchan->scale_out_x = pchan->scale_out_y = 1.0f;

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

    for (Camera *camera = bmain->cameras.first; camera != NULL; camera = camera->id.next) {
      if (camera->stereo.pole_merge_angle_from == 0.0f &&
          camera->stereo.pole_merge_angle_to == 0.0f) {
        camera->stereo.pole_merge_angle_from = DEG2RADF(60.0f);
        camera->stereo.pole_merge_angle_to = DEG2RADF(75.0f);
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "NormalEditModifierData", "float", "mix_limit")) {
      Object *ob;

      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        ModifierData *md;
        for (md = ob->modifiers.first; md; md = md->next) {
          if (md->type == eModifierType_NormalEdit) {
            NormalEditModifierData *nemd = (NormalEditModifierData *)md;
            nemd->mix_limit = DEG2RADF(180.0f);
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "BooleanModifierData", "float", "double_threshold")) {
      Object *ob;
      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        ModifierData *md;
        for (md = ob->modifiers.first; md; md = md->next) {
          if (md->type == eModifierType_Boolean) {
            BooleanModifierData *bmd = (BooleanModifierData *)md;
            bmd->double_threshold = 1e-6f;
          }
        }
      }
    }

    for (Brush *br = bmain->brushes.first; br; br = br->id.next) {
      if (br->sculpt_tool == SCULPT_TOOL_FLATTEN) {
        br->flag |= BRUSH_ACCUMULATE;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "ClothSimSettings", "float", "time_scale")) {
      Object *ob;
      ModifierData *md;
      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        for (md = ob->modifiers.first; md; md = md->next) {
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

  if (!MAIN_VERSION_ATLEAST(bmain, 277, 3)) {
    /* ------- init of grease pencil initialization --------------- */
    if (!DNA_struct_elem_find(fd->filesdna, "bGPDstroke", "bGPDpalettecolor", "*palcolor")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        ToolSettings *ts = scene->toolsettings;
        /* initialize use position for sculpt brushes */
        ts->gp_sculpt.flag |= GP_SCULPT_SETT_FLAG_APPLY_POSITION;

        /* new strength sculpt brush */
        if (ts->gp_sculpt.brush[0].size >= 11) {
          GP_Sculpt_Settings *gset = &ts->gp_sculpt;
          GP_Sculpt_Data *brush;

          brush = &gset->brush[GP_SCULPT_TYPE_STRENGTH];
          brush->size = 25;
          brush->strength = 0.5f;
          brush->flag = GP_SCULPT_FLAG_USE_FALLOFF;
        }
      }
      /* Convert Grease Pencil to new palettes/brushes
       * Loop all strokes and create the palette and all colors
       */
      for (bGPdata *gpd = bmain->gpencils.first; gpd; gpd = gpd->id.next) {
        if (BLI_listbase_is_empty(&gpd->palettes)) {
          /* create palette */
          bGPDpalette *palette = BKE_gpencil_palette_addnew(gpd, "GP_Palette");
          for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
            /* create color using layer name */
            bGPDpalettecolor *palcolor = BKE_gpencil_palettecolor_addnew(palette, gpl->info);
            if (palcolor != NULL) {
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
              for (bGPDframe *gpf = gpl->frames.first; gpf; gpf = gpf->next) {
                for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
                  /* set stroke to palette and force recalculation */
                  BLI_strncpy(gps->colorname, gpl->info, sizeof(gps->colorname));
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

  if (!MAIN_VERSION_ATLEAST(bmain, 278, 0)) {
    if (!DNA_struct_elem_find(fd->filesdna, "MovieTrackingTrack", "float", "weight_stab")) {
      MovieClip *clip;
      for (clip = bmain->movieclips.first; clip; clip = clip->id.next) {
        MovieTracking *tracking = &clip->tracking;
        MovieTrackingObject *tracking_object;
        for (tracking_object = tracking->objects.first; tracking_object != NULL;
             tracking_object = tracking_object->next) {
          ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, tracking_object);
          MovieTrackingTrack *track;
          for (track = tracksbase->first; track != NULL; track = track->next) {
            track->weight_stab = track->weight;
          }
        }
      }
    }

    if (!DNA_struct_elem_find(
            fd->filesdna, "MovieTrackingStabilization", "int", "tot_rot_track")) {
      MovieClip *clip;
      for (clip = bmain->movieclips.first; clip != NULL; clip = clip->id.next) {
        if (clip->tracking.stabilization.rot_track) {
          migrate_single_rot_stabilization_track_settings(&clip->tracking.stabilization);
        }
        if (clip->tracking.stabilization.scale == 0.0f) {
          /* ensure init.
           * Was previously used for autoscale only,
           * now used always (as "target scale") */
          clip->tracking.stabilization.scale = 1.0f;
        }
        /* blender prefers 1-based frame counting;
         * thus using frame 1 as reference typically works best */
        clip->tracking.stabilization.anchor_frame = 1;
        /* by default show the track lists expanded, to improve "discoverability" */
        clip->tracking.stabilization.flag |= TRACKING_SHOW_STAB_TRACKS;
        /* deprecated, not used anymore */
        clip->tracking.stabilization.ok = false;
      }
    }
  }
  if (!MAIN_VERSION_ATLEAST(bmain, 278, 2)) {
    if (!DNA_struct_elem_find(fd->filesdna, "FFMpegCodecData", "int", "ffmpeg_preset")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        /* "medium" is the preset FFmpeg uses when no presets are given. */
        scene->r.ffcodecdata.ffmpeg_preset = FFM_PRESET_MEDIUM;
      }
    }
    if (!DNA_struct_elem_find(fd->filesdna, "FFMpegCodecData", "int", "constant_rate_factor")) {
      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        /* fall back to behavior from before we introduced CRF for old files */
        scene->r.ffcodecdata.constant_rate_factor = FFM_CRF_NONE;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "SmokeModifierData", "float", "slice_per_voxel")) {
      Object *ob;
      ModifierData *md;

      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        for (md = ob->modifiers.first; md; md = md->next) {
          if (md->type == eModifierType_Smoke) {
            SmokeModifierData *smd = (SmokeModifierData *)md;
            if (smd->domain) {
              smd->domain->slice_per_voxel = 5.0f;
              smd->domain->slice_depth = 0.5f;
              smd->domain->display_thickness = 1.0f;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 278, 3)) {
    for (Scene *scene = bmain->scenes.first; scene != NULL; scene = scene->id.next) {
      if (scene->toolsettings != NULL) {
        ToolSettings *ts = scene->toolsettings;
        ParticleEditSettings *pset = &ts->particle;
        for (int a = 0; a < ARRAY_SIZE(pset->brush); a++) {
          if (pset->brush[a].count == 0) {
            pset->brush[a].count = 10;
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "RigidBodyCon", "float", "spring_stiffness_ang_x")) {
      Object *ob;
      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
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
    for (Scene *scene = bmain->scenes.first; scene != NULL; scene = scene->id.next) {
      if (scene->toolsettings != NULL) {
        ToolSettings *ts = scene->toolsettings;
        if (ts->sculpt && ts->sculpt->constant_detail != 0.0f) {
          ts->sculpt->constant_detail = 100.0f / ts->sculpt->constant_detail;
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 278, 4)) {
    const float sqrt_3 = (float)M_SQRT3;
    for (Brush *br = bmain->brushes.first; br; br = br->id.next) {
      br->fill_threshold /= sqrt_3;
    }

    /* Custom motion paths */
    if (!DNA_struct_elem_find(fd->filesdna, "bMotionPath", "int", "line_thickness")) {
      Object *ob;
      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        bMotionPath *mpath;
        bPoseChannel *pchan;
        mpath = ob->mpath;
        if (mpath) {
          mpath->color[0] = 1.0f;
          mpath->color[1] = 0.0f;
          mpath->color[2] = 0.0f;
          mpath->line_thickness = 1;
          mpath->flag |= MOTIONPATH_FLAG_LINES;
        }
        /* bones motion path */
        if (ob->pose) {
          for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
            mpath = pchan->mpath;
            if (mpath) {
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

  if (!MAIN_VERSION_ATLEAST(bmain, 278, 5)) {
    /* Mask primitive adding code was not initializing correctly id_type of its points' parent. */
    for (Mask *mask = bmain->masks.first; mask; mask = mask->id.next) {
      for (MaskLayer *mlayer = mask->masklayers.first; mlayer; mlayer = mlayer->next) {
        for (MaskSpline *mspline = mlayer->splines.first; mspline; mspline = mspline->next) {
          int i = 0;
          for (MaskSplinePoint *mspoint = mspline->points; i < mspline->tot_point;
               mspoint++, i++) {
            if (mspoint->parent.id_type == 0) {
              BKE_mask_parent_init(&mspoint->parent);
            }
          }
        }
      }
    }

    /* Fix for T50736, Glare comp node using same var for two different things. */
    if (!DNA_struct_elem_find(fd->filesdna, "NodeGlare", "char", "star_45")) {
      FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
        if (ntree->type == NTREE_COMPOSIT) {
          ntreeSetTypes(NULL, ntree);
          for (bNode *node = ntree->nodes.first; node; node = node->next) {
            if (node->type == CMP_NODE_GLARE) {
              NodeGlare *ndg = node->storage;
              switch (ndg->type) {
                case 2: /* Grrrr! magic numbers :( */
                  ndg->streaks = ndg->angle;
                  break;
                case 0:
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

    if (!DNA_struct_elem_find(fd->filesdna, "SurfaceDeformModifierData", "float", "mat[4][4]")) {
      for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
        for (ModifierData *md = ob->modifiers.first; md; md = md->next) {
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

  if (!MAIN_VERSION_ATLEAST(bmain, 279, 0)) {
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      if (scene->r.im_format.exr_codec == R_IMF_EXR_CODEC_DWAB) {
        scene->r.im_format.exr_codec = R_IMF_EXR_CODEC_DWAA;
      }
    }

    /* Fix related to VGroup modifiers creating named defgroup CD layers! See T51520. */
    for (Mesh *me = bmain->meshes.first; me; me = me->id.next) {
      CustomData_set_layer_name(&me->vdata, CD_MDEFORMVERT, 0, "");
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 279, 3)) {
    if (!DNA_struct_elem_find(fd->filesdna, "SmokeDomainSettings", "float", "clipping")) {
      Object *ob;
      ModifierData *md;

      for (ob = bmain->objects.first; ob; ob = ob->id.next) {
        for (md = ob->modifiers.first; md; md = md->next) {
          if (md->type == eModifierType_Smoke) {
            SmokeModifierData *smd = (SmokeModifierData *)md;
            if (smd->domain) {
              smd->domain->clipping = 1e-3f;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 279, 4)) {
    /* Fix for invalid state of screen due to bug in older versions. */
    for (bScreen *sc = bmain->screens.first; sc; sc = sc->id.next) {
      for (ScrArea *sa = sc->areabase.first; sa; sa = sa->next) {
        if (sa->full && sc->state == SCREENNORMAL) {
          sa->full = NULL;
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "Brush", "float", "falloff_angle")) {
      for (Brush *br = bmain->brushes.first; br; br = br->id.next) {
        br->falloff_angle = DEG2RADF(80);
        br->flag &= ~(BRUSH_FLAG_UNUSED_1 | BRUSH_FLAG_UNUSED_6 | BRUSH_FLAG_UNUSED_7 |
                      BRUSH_FLAG_UNUSED_17 | BRUSH_FRONTFACE_FALLOFF);
      }

      for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
        ToolSettings *ts = scene->toolsettings;
        for (int i = 0; i < 2; i++) {
          VPaint *vp = i ? ts->vpaint : ts->wpaint;
          if (vp != NULL) {
            /* remove all other flags */
            vp->flag &= (VP_FLAG_VGROUP_RESTRICT);
          }
        }
      }
    }

    /* Simple deform modifier no longer assumes Z axis (X for bend type).
     * Must set previous defaults. */
    if (!DNA_struct_elem_find(fd->filesdna, "SimpleDeformModifierData", "char", "deform_axis")) {
      for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
        for (ModifierData *md = ob->modifiers.first; md; md = md->next) {
          if (md->type == eModifierType_SimpleDeform) {
            SimpleDeformModifierData *smd = (SimpleDeformModifierData *)md;
            smd->deform_axis = 2;
          }
        }
      }
    }

    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
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

    if (!DNA_struct_elem_find(
            fd->filesdna, "ParticleInstanceModifierData", "float", "particle_amount")) {
      for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
        for (ModifierData *md = ob->modifiers.first; md; md = md->next) {
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
  if (!MAIN_VERSION_ATLEAST(bmain, 279, 0)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        ntreeSetTypes(NULL, ntree);
        for (bNode *node = ntree->nodes.first; node; node = node->next) {
          if (node->type == CMP_NODE_HUE_SAT) {
            do_version_hue_sat_node(ntree, node);
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 279, 2)) {
    /* B-Bones (bbone_in/out -> bbone_easein/out) + Stepped FMod Frame Start/End fix */
    /* if (!DNA_struct_elem_find(fd->filesdna, "Bone", "float", "bbone_easein")) */
    BKE_fcurves_main_cb(bmain, do_version_bbone_easing_fcurve_fix, NULL);
  }
}
