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

#include "BLI_alloca.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_brush_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_fluid_types.h"
#include "DNA_genfile.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_hair_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_screen_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_space_types.h"
#include "DNA_tracking_types.h"
#include "DNA_workspace_types.h"

#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_collection.h"
#include "BKE_colortools.h"
#include "BKE_cryptomatte.h"
#include "BKE_fcurve.h"
#include "BKE_gpencil.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"
#include "BKE_node.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "SEQ_proxy.h"
#include "SEQ_render.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"
#include "SEQ_transform.h"

#include "BLO_readfile.h"
#include "readfile.h"

/* Make preferences read-only, use versioning_userdef.c. */
#define U (*((const UserDef *)&U))

static eSpaceSeq_Proxy_RenderSize get_sequencer_render_size(Main *bmain)
{
  eSpaceSeq_Proxy_RenderSize render_size = 100;

  for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        switch (sl->spacetype) {
          case SPACE_SEQ: {
            SpaceSeq *sseq = (SpaceSeq *)sl;
            if (sseq->mainb == SEQ_DRAW_IMG_IMBUF) {
              render_size = sseq->render_size;
              break;
            }
          }
        }
      }
    }
  }

  return render_size;
}

/* image_size is width or height depending what RNA property is converted - X or Y. */
static void seq_convert_transform_animation(const Scene *scene,
                                            const char *path,
                                            const int image_size)
{
  if (scene->adt == NULL || scene->adt->action == NULL) {
    return;
  }

  FCurve *fcu = BKE_fcurve_find(&scene->adt->action->curves, path, 0);
  if (fcu != NULL && !BKE_fcurve_is_empty(fcu)) {
    BezTriple *bezt = fcu->bezt;
    for (int i = 0; i < fcu->totvert; i++, bezt++) {
      /* Same math as with old_image_center_*, but simplified. */
      bezt->vec[0][1] = image_size / 2 + bezt->vec[0][1] - scene->r.xsch / 2;
      bezt->vec[1][1] = image_size / 2 + bezt->vec[1][1] - scene->r.xsch / 2;
      bezt->vec[2][1] = image_size / 2 + bezt->vec[2][1] - scene->r.xsch / 2;
    }
  }
}

static void seq_convert_transform_crop(const Scene *scene,
                                       Sequence *seq,
                                       const eSpaceSeq_Proxy_RenderSize render_size)
{
  if (seq->strip->transform == NULL) {
    seq->strip->transform = MEM_callocN(sizeof(struct StripTransform), "StripTransform");
  }
  if (seq->strip->crop == NULL) {
    seq->strip->crop = MEM_callocN(sizeof(struct StripCrop), "StripCrop");
  }

  StripCrop *c = seq->strip->crop;
  StripTransform *t = seq->strip->transform;
  int old_image_center_x = scene->r.xsch / 2;
  int old_image_center_y = scene->r.ysch / 2;
  int image_size_x = scene->r.xsch;
  int image_size_y = scene->r.ysch;

  /* Hardcoded legacy bit-flags which has been removed. */
  const uint32_t use_transform_flag = (1 << 16);
  const uint32_t use_crop_flag = (1 << 17);

  const StripElem *s_elem = SEQ_render_give_stripelem(seq, seq->start);
  if (s_elem != NULL) {
    image_size_x = s_elem->orig_width;
    image_size_y = s_elem->orig_height;

    if (SEQ_can_use_proxy(seq, SEQ_rendersize_to_proxysize(render_size))) {
      image_size_x /= SEQ_rendersize_to_scale_factor(render_size);
      image_size_y /= SEQ_rendersize_to_scale_factor(render_size);
    }
  }

  /* Default scale. */
  if (t->scale_x == 0.0f && t->scale_y == 0.0f) {
    t->scale_x = 1.0f;
    t->scale_y = 1.0f;
  }

  /* Clear crop if it was unused. This must happen before converting values. */
  if ((seq->flag & use_crop_flag) == 0) {
    c->bottom = c->top = c->left = c->right = 0;
  }

  if ((seq->flag & use_transform_flag) == 0) {
    t->xofs = t->yofs = 0;

    /* Reverse scale to fit for strips not using offset. */
    float project_aspect = (float)scene->r.xsch / (float)scene->r.ysch;
    float image_aspect = (float)image_size_x / (float)image_size_y;
    if (project_aspect > image_aspect) {
      t->scale_x = project_aspect / image_aspect;
    }
    else {
      t->scale_y = image_aspect / project_aspect;
    }
  }

  if ((seq->flag & use_crop_flag) != 0 && (seq->flag & use_transform_flag) == 0) {
    /* Calculate image offset. */
    float s_x = scene->r.xsch / image_size_x;
    float s_y = scene->r.ysch / image_size_y;
    old_image_center_x += c->right * s_x - c->left * s_x;
    old_image_center_y += c->top * s_y - c->bottom * s_y;

    /* Convert crop to scale. */
    int cropped_image_size_x = image_size_x - c->right - c->left;
    int cropped_image_size_y = image_size_y - c->top - c->bottom;
    c->bottom = c->top = c->left = c->right = 0;
    t->scale_x *= (float)image_size_x / (float)cropped_image_size_x;
    t->scale_y *= (float)image_size_y / (float)cropped_image_size_y;
  }

  if ((seq->flag & use_transform_flag) != 0) {
    /* Convert image offset. */
    old_image_center_x = image_size_x / 2 - c->left + t->xofs;
    old_image_center_y = image_size_y / 2 - c->bottom + t->yofs;

    /* Preserve original image size. */
    t->scale_x = t->scale_y = MAX2((float)image_size_x / (float)scene->r.xsch,
                                   (float)image_size_y / (float)scene->r.ysch);

    /* Convert crop. */
    if ((seq->flag & use_crop_flag) != 0) {
      c->top /= t->scale_x;
      c->bottom /= t->scale_x;
      c->left /= t->scale_x;
      c->right /= t->scale_x;
    }
  }

  t->xofs = old_image_center_x - scene->r.xsch / 2;
  t->yofs = old_image_center_y - scene->r.ysch / 2;

  /* Convert offset animation, but only if crop is not used. */
  if ((seq->flag & use_transform_flag) != 0 && (seq->flag & use_crop_flag) == 0) {
    char name_esc[(sizeof(seq->name) - 2) * 2], *path;
    BLI_str_escape(name_esc, seq->name + 2, sizeof(name_esc));

    path = BLI_sprintfN("sequence_editor.sequences_all[\"%s\"].transform.offset_x", name_esc);
    seq_convert_transform_animation(scene, path, image_size_x);
    MEM_freeN(path);
    path = BLI_sprintfN("sequence_editor.sequences_all[\"%s\"].transform.offset_y", name_esc);
    seq_convert_transform_animation(scene, path, image_size_y);
    MEM_freeN(path);
  }

  seq->flag &= ~use_transform_flag;
  seq->flag &= ~use_crop_flag;
}

static void seq_convert_transform_crop_lb(const Scene *scene,
                                          const ListBase *lb,
                                          const eSpaceSeq_Proxy_RenderSize render_size)
{

  LISTBASE_FOREACH (Sequence *, seq, lb) {
    if (seq->type != SEQ_TYPE_SOUND_RAM) {
      seq_convert_transform_crop(scene, seq, render_size);
    }
    if (seq->type == SEQ_TYPE_META) {
      seq_convert_transform_crop_lb(scene, &seq->seqbase, render_size);
    }
  }
}

static void seq_convert_transform_animation_2(const Scene *scene,
                                              const char *path,
                                              const float scale_to_fit_factor)
{
  if (scene->adt == NULL || scene->adt->action == NULL) {
    return;
  }

  FCurve *fcu = BKE_fcurve_find(&scene->adt->action->curves, path, 0);
  if (fcu != NULL && !BKE_fcurve_is_empty(fcu)) {
    BezTriple *bezt = fcu->bezt;
    for (int i = 0; i < fcu->totvert; i++, bezt++) {
      /* Same math as with old_image_center_*, but simplified. */
      bezt->vec[0][1] *= scale_to_fit_factor;
      bezt->vec[1][1] *= scale_to_fit_factor;
      bezt->vec[2][1] *= scale_to_fit_factor;
    }
  }
}

static void seq_convert_transform_crop_2(const Scene *scene,
                                         Sequence *seq,
                                         const eSpaceSeq_Proxy_RenderSize render_size)
{
  const StripElem *s_elem = SEQ_render_give_stripelem(seq, seq->start);
  if (s_elem == NULL) {
    return;
  }

  StripCrop *c = seq->strip->crop;
  StripTransform *t = seq->strip->transform;
  int image_size_x = s_elem->orig_width;
  int image_size_y = s_elem->orig_height;

  if (SEQ_can_use_proxy(seq, SEQ_rendersize_to_proxysize(render_size))) {
    image_size_x /= SEQ_rendersize_to_scale_factor(render_size);
    image_size_y /= SEQ_rendersize_to_scale_factor(render_size);
  }

  /* Calculate scale factor, so image fits in preview area with original aspect ratio. */
  const float scale_to_fit_factor = MIN2((float)scene->r.xsch / (float)image_size_x,
                                         (float)scene->r.ysch / (float)image_size_y);
  t->scale_x *= scale_to_fit_factor;
  t->scale_y *= scale_to_fit_factor;
  c->top /= scale_to_fit_factor;
  c->bottom /= scale_to_fit_factor;
  c->left /= scale_to_fit_factor;
  c->right /= scale_to_fit_factor;

  char name_esc[(sizeof(seq->name) - 2) * 2], *path;
  BLI_str_escape(name_esc, seq->name + 2, sizeof(name_esc));
  path = BLI_sprintfN("sequence_editor.sequences_all[\"%s\"].transform.scale_x", name_esc);
  seq_convert_transform_animation_2(scene, path, scale_to_fit_factor);
  MEM_freeN(path);
  path = BLI_sprintfN("sequence_editor.sequences_all[\"%s\"].transform.scale_y", name_esc);
  seq_convert_transform_animation_2(scene, path, scale_to_fit_factor);
  MEM_freeN(path);
  path = BLI_sprintfN("sequence_editor.sequences_all[\"%s\"].crop.min_x", name_esc);
  seq_convert_transform_animation_2(scene, path, 1 / scale_to_fit_factor);
  MEM_freeN(path);
  path = BLI_sprintfN("sequence_editor.sequences_all[\"%s\"].crop.max_x", name_esc);
  seq_convert_transform_animation_2(scene, path, 1 / scale_to_fit_factor);
  MEM_freeN(path);
  path = BLI_sprintfN("sequence_editor.sequences_all[\"%s\"].crop.min_y", name_esc);
  seq_convert_transform_animation_2(scene, path, 1 / scale_to_fit_factor);
  MEM_freeN(path);
  path = BLI_sprintfN("sequence_editor.sequences_all[\"%s\"].crop.max_x", name_esc);
  seq_convert_transform_animation_2(scene, path, 1 / scale_to_fit_factor);
  MEM_freeN(path);
}

static void seq_convert_transform_crop_lb_2(const Scene *scene,
                                            const ListBase *lb,
                                            const eSpaceSeq_Proxy_RenderSize render_size)
{

  LISTBASE_FOREACH (Sequence *, seq, lb) {
    if (seq->type != SEQ_TYPE_SOUND_RAM) {
      seq_convert_transform_crop_2(scene, seq, render_size);
    }
    if (seq->type == SEQ_TYPE_META) {
      seq_convert_transform_crop_lb_2(scene, &seq->seqbase, render_size);
    }
  }
}

static void seq_update_meta_disp_range(Editing *ed)
{
  if (ed == NULL) {
    return;
  }

  LISTBASE_FOREACH_BACKWARD (MetaStack *, ms, &ed->metastack) {
    /* Update ms->disp_range from meta. */
    if (ms->disp_range[0] == ms->disp_range[1]) {
      copy_v2_v2_int(ms->disp_range, &ms->parseq->startdisp);
    }

    /* Update meta strip endpoints. */
    SEQ_transform_set_left_handle_frame(ms->parseq, ms->disp_range[0]);
    SEQ_transform_set_right_handle_frame(ms->parseq, ms->disp_range[1]);
    SEQ_transform_fix_single_image_seq_offsets(ms->parseq);

    /* Recalculate effects using meta strip. */
    LISTBASE_FOREACH (Sequence *, seq, ms->oldbasep) {
      if (seq->seq2) {
        seq->start = seq->startdisp = max_ii(seq->seq1->startdisp, seq->seq2->startdisp);
        seq->enddisp = min_ii(seq->seq1->enddisp, seq->seq2->enddisp);
      }
    }

    /* Ensure that active seqbase points to active meta strip seqbase. */
    MetaStack *active_ms = SEQ_meta_stack_active_get(ed);
    SEQ_seqbase_active_set(ed, &active_ms->parseq->seqbase);
  }
}

void do_versions_after_linking_290(Main *bmain, ReportList *UNUSED(reports))
{
  if (!MAIN_VERSION_ATLEAST(bmain, 290, 1)) {
    /* Patch old grease pencil modifiers material filter. */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
        switch (md->type) {
          case eGpencilModifierType_Array: {
            ArrayGpencilModifierData *gpmd = (ArrayGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Color: {
            ColorGpencilModifierData *gpmd = (ColorGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Hook: {
            HookGpencilModifierData *gpmd = (HookGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Lattice: {
            LatticeGpencilModifierData *gpmd = (LatticeGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Mirror: {
            MirrorGpencilModifierData *gpmd = (MirrorGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Multiply: {
            MultiplyGpencilModifierData *gpmd = (MultiplyGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Noise: {
            NoiseGpencilModifierData *gpmd = (NoiseGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Offset: {
            OffsetGpencilModifierData *gpmd = (OffsetGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Opacity: {
            OpacityGpencilModifierData *gpmd = (OpacityGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Simplify: {
            SimplifyGpencilModifierData *gpmd = (SimplifyGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Smooth: {
            SmoothGpencilModifierData *gpmd = (SmoothGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Subdiv: {
            SubdivGpencilModifierData *gpmd = (SubdivGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Texture: {
            TextureGpencilModifierData *gpmd = (TextureGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Thick: {
            ThickGpencilModifierData *gpmd = (ThickGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          default:
            break;
        }
      }
    }

    /* Patch first frame for old files. */
    Scene *scene = bmain->scenes.first;
    if (scene != NULL) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        if (ob->type != OB_GPENCIL) {
          continue;
        }
        bGPdata *gpd = ob->data;
        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          bGPDframe *gpf = gpl->frames.first;
          if (gpf && gpf->framenum > scene->r.sfra) {
            bGPDframe *gpf_dup = BKE_gpencil_frame_duplicate(gpf, true);
            gpf_dup->framenum = scene->r.sfra;
            BLI_addhead(&gpl->frames, gpf_dup);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 291, 1)) {
    LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
      if (BKE_collection_cycles_fix(bmain, collection)) {
        printf(
            "WARNING: Cycle detected in collection '%s', fixed as best as possible.\n"
            "You may have to reconstruct your View Layers...\n",
            collection->id.name);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 291, 8)) {
    /**
     * Make sure Emission Alpha fcurve and drivers is properly mapped after the Emission Strength
     * got introduced.
     */

    /**
     * Effectively we are replacing the (animation of) node socket input 18 with 19.
     * Emission Strength is the new socket input 18, pushing Emission Alpha to input 19.
     *
     * To play safe we move all the inputs beyond 18 to their rightful new place.
     * In case users are doing unexpected things with not-really supported keyframeable channels.
     *
     * The for loop for the input ids is at the top level otherwise we lose the animation
     * keyframe data.
     */
    for (int input_id = 21; input_id >= 18; input_id--) {
      FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
        if (ntree->type == NTREE_SHADER) {
          LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
            if (node->type != SH_NODE_BSDF_PRINCIPLED) {
              continue;
            }

            const size_t node_name_length = strlen(node->name);
            const size_t node_name_escaped_max_length = (node_name_length * 2);
            char *node_name_escaped = MEM_mallocN(node_name_escaped_max_length + 1,
                                                  "escaped name");
            BLI_str_escape(node_name_escaped, node->name, node_name_escaped_max_length);
            char *rna_path_prefix = BLI_sprintfN("nodes[\"%s\"].inputs", node_name_escaped);

            BKE_animdata_fix_paths_rename_all_ex(
                bmain, id, rna_path_prefix, NULL, NULL, input_id, input_id + 1, false);
            MEM_freeN(rna_path_prefix);
            MEM_freeN(node_name_escaped);
          }
        }
      }
      FOREACH_NODETREE_END;
    }
  }

  /* Convert all Multires displacement to Catmull-Clark subdivision limit surface. */
  if (!MAIN_VERSION_ATLEAST(bmain, 292, 1)) {
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      ModifierData *md;
      for (md = ob->modifiers.first; md; md = md->next) {
        if (md->type == eModifierType_Multires) {
          MultiresModifierData *mmd = (MultiresModifierData *)md;
          if (mmd->simple) {
            multires_do_versions_simple_to_catmull_clark(ob, mmd);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 292, 2)) {

    eSpaceSeq_Proxy_RenderSize render_size = get_sequencer_render_size(bmain);

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed != NULL) {
        seq_convert_transform_crop_lb(scene, &scene->ed->seqbase, render_size);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 292, 8)) {
    /* Systematically rebuild posebones to ensure consistent ordering matching the one of bones in
     * Armature obdata. */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      if (ob->type == OB_ARMATURE) {
        BKE_pose_rebuild(bmain, ob, ob->data, true);
      }
    }

    /* Wet Paint Radius Factor */
    for (Brush *br = bmain->brushes.first; br; br = br->id.next) {
      if (br->ob_mode & OB_MODE_SCULPT && br->wet_paint_radius_factor == 0.0f) {
        br->wet_paint_radius_factor = 1.0f;
      }
    }

    eSpaceSeq_Proxy_RenderSize render_size = get_sequencer_render_size(bmain);
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed != NULL) {
        seq_convert_transform_crop_lb_2(scene, &scene->ed->seqbase, render_size);
      }
    }
  }

  /**
   * Versioning code until next subversion bump goes here.
   *
   * \note Be sure to check when bumping the version:
   * - #blo_do_versions_290 in this file.
   * - "versioning_userdef.c", #blo_do_versions_userdef
   * - "versioning_userdef.c", #do_versions_theme
   *
   * \note Keep this message at the bottom of the function.
   */
  {
    /* Keep this block, even when empty. */

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      seq_update_meta_disp_range(SEQ_editing_get(scene, false));
    }
  }
}

static void panels_remove_x_closed_flag_recursive(Panel *panel)
{
  const bool was_closed_x = panel->flag & PNL_UNUSED_1;
  const bool was_closed_y = panel->flag & PNL_CLOSED; /* That value was the Y closed flag. */

  SET_FLAG_FROM_TEST(panel->flag, was_closed_x || was_closed_y, PNL_CLOSED);

  /* Clear the old PNL_CLOSEDX flag. */
  panel->flag &= ~PNL_UNUSED_1;

  LISTBASE_FOREACH (Panel *, child_panel, &panel->children) {
    panels_remove_x_closed_flag_recursive(child_panel);
  }
}

static void do_versions_point_attributes(CustomData *pdata)
{
  /* Change to generic named float/float3 attributes. */
  const int CD_LOCATION = 43;
  const int CD_RADIUS = 44;

  for (int i = 0; i < pdata->totlayer; i++) {
    CustomDataLayer *layer = &pdata->layers[i];
    if (layer->type == CD_LOCATION) {
      STRNCPY(layer->name, "Position");
      layer->type = CD_PROP_FLOAT3;
    }
    else if (layer->type == CD_RADIUS) {
      STRNCPY(layer->name, "Radius");
      layer->type = CD_PROP_FLOAT;
    }
  }
}

static void do_versions_point_attribute_names(CustomData *pdata)
{
  /* Change from capital initial letter to lower case (T82693). */
  for (int i = 0; i < pdata->totlayer; i++) {
    CustomDataLayer *layer = &pdata->layers[i];
    if (layer->type == CD_PROP_FLOAT3 && STREQ(layer->name, "Position")) {
      STRNCPY(layer->name, "position");
    }
    else if (layer->type == CD_PROP_FLOAT && STREQ(layer->name, "Radius")) {
      STRNCPY(layer->name, "radius");
    }
  }
}

/* Move FCurve handles towards the control point in such a way that the curve itself doesn't
 * change. Since 2.91 FCurves are computed slightly differently, which requires this update to keep
 * the same animation result. Previous versions scaled down overlapping handles during evaluation.
 * This function applies the old correction to the actual animation data instead. */
static void do_versions_291_fcurve_handles_limit(FCurve *fcu)
{
  uint i = 1;
  for (BezTriple *bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
    /* Only adjust bezier key-frames. */
    if (bezt->ipo != BEZT_IPO_BEZ) {
      continue;
    }

    BezTriple *nextbezt = bezt + 1;
    const float v1[2] = {bezt->vec[1][0], bezt->vec[1][1]};
    const float v2[2] = {bezt->vec[2][0], bezt->vec[2][1]};
    const float v3[2] = {nextbezt->vec[0][0], nextbezt->vec[0][1]};
    const float v4[2] = {nextbezt->vec[1][0], nextbezt->vec[1][1]};

    /* If the handles have no length, no need to do any corrections. */
    if (v1[0] == v2[0] && v3[0] == v4[0]) {
      continue;
    }

    /* Calculate handle deltas. */
    float delta1[2], delta2[2];
    sub_v2_v2v2(delta1, v1, v2);
    sub_v2_v2v2(delta2, v4, v3);

    const float len1 = fabsf(delta1[0]); /* Length of handle of first key. */
    const float len2 = fabsf(delta2[0]); /* Length of handle of second key. */

    /* Overlapping handles used to be internally scaled down in previous versions.
     * We bake the handles onto these previously virtual values. */
    const float time_delta = v4[0] - v1[0];
    const float total_len = len1 + len2;
    if (total_len <= time_delta) {
      continue;
    }

    const float factor = time_delta / total_len;
    /* Current keyframe's right handle: */
    madd_v2_v2v2fl(bezt->vec[2], v1, delta1, -factor); /* vec[2] = v1 - factor * delta1 */
    /* Next keyframe's left handle: */
    madd_v2_v2v2fl(nextbezt->vec[0], v4, delta2, -factor); /* vec[0] = v4 - factor * delta2 */
  }
}

static void do_versions_strip_cache_settings_recursive(const ListBase *seqbase)
{
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    seq->cache_flag = 0;
    if (seq->type == SEQ_TYPE_META) {
      do_versions_strip_cache_settings_recursive(&seq->seqbase);
    }
  }
}

static void version_node_socket_name(bNodeTree *ntree,
                                     const int node_type,
                                     const char *old_name,
                                     const char *new_name)
{
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type == node_type) {
      LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
        if (STREQ(socket->name, old_name)) {
          strcpy(socket->name, new_name);
        }
        if (STREQ(socket->identifier, old_name)) {
          strcpy(socket->identifier, new_name);
        }
      }
      LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
        if (STREQ(socket->name, old_name)) {
          strcpy(socket->name, new_name);
        }
        if (STREQ(socket->identifier, old_name)) {
          strcpy(socket->identifier, new_name);
        }
      }
    }
  }
}

static void version_node_join_geometry_for_multi_input_socket(bNodeTree *ntree)
{
  LISTBASE_FOREACH_MUTABLE (bNodeLink *, link, &ntree->links) {
    if (link->tonode->type == GEO_NODE_JOIN_GEOMETRY && !(link->tosock->flag & SOCK_MULTI_INPUT)) {
      link->tosock = link->tonode->inputs.first;
    }
  }
  LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
    if (node->type == GEO_NODE_JOIN_GEOMETRY) {
      bNodeSocket *socket = node->inputs.first;
      socket->flag |= SOCK_MULTI_INPUT;
      socket->limit = 4095;
      nodeRemoveSocket(ntree, node, socket->next);
    }
  }
}

/* NOLINTNEXTLINE: readability-function-size */
void blo_do_versions_290(FileData *fd, Library *UNUSED(lib), Main *bmain)
{
  UNUSED_VARS(fd);

  if (MAIN_VERSION_ATLEAST(bmain, 290, 2) && MAIN_VERSION_OLDER(bmain, 291, 1)) {
    /* In this range, the extrude manifold could generate meshes with degenerated face. */
    LISTBASE_FOREACH (Mesh *, me, &bmain->meshes) {
      for (MPoly *mp = me->mpoly, *mp_end = mp + me->totpoly; mp < mp_end; mp++) {
        if (mp->totloop == 2) {
          bool changed;
          BKE_mesh_validate_arrays(me,
                                   me->mvert,
                                   me->totvert,
                                   me->medge,
                                   me->totedge,
                                   me->mface,
                                   me->totface,
                                   me->mloop,
                                   me->totloop,
                                   me->mpoly,
                                   me->totpoly,
                                   me->dvert,
                                   false,
                                   true,
                                   &changed);
          break;
        }
      }
    }
  }

  /** Repair files from duplicate brushes added to blend files, see: T76738. */
  if (!MAIN_VERSION_ATLEAST(bmain, 290, 2)) {
    {
      short id_codes[] = {ID_BR, ID_PAL};
      for (int i = 0; i < ARRAY_SIZE(id_codes); i++) {
        ListBase *lb = which_libbase(bmain, id_codes[i]);
        BKE_main_id_repair_duplicate_names_listbase(lb);
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "SpaceImage", "float", "uv_opacity")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_IMAGE) {
              SpaceImage *sima = (SpaceImage *)sl;
              sima->uv_opacity = 1.0f;
            }
          }
        }
      }
    }

    /* Init Grease Pencil new random curves. */
    if (!DNA_struct_elem_find(fd->filesdna, "BrushGpencilSettings", "float", "random_hue")) {
      LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
        if ((brush->gpencil_settings) && (brush->gpencil_settings->curve_rand_pressure == NULL)) {
          brush->gpencil_settings->curve_rand_pressure = BKE_curvemapping_add(
              1, 0.0f, 0.0f, 1.0f, 1.0f);
          brush->gpencil_settings->curve_rand_strength = BKE_curvemapping_add(
              1, 0.0f, 0.0f, 1.0f, 1.0f);
          brush->gpencil_settings->curve_rand_uv = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
          brush->gpencil_settings->curve_rand_hue = BKE_curvemapping_add(
              1, 0.0f, 0.0f, 1.0f, 1.0f);
          brush->gpencil_settings->curve_rand_saturation = BKE_curvemapping_add(
              1, 0.0f, 0.0f, 1.0f, 1.0f);
          brush->gpencil_settings->curve_rand_value = BKE_curvemapping_add(
              1, 0.0f, 0.0f, 1.0f, 1.0f);
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 290, 4)) {
    /* Clear old deprecated bit-flag from edit weights modifiers, we now use it for something else.
     */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_WeightVGEdit) {
          ((WeightVGEditModifierData *)md)->edit_flags &= ~MOD_WVG_EDIT_WEIGHTS_NORMALIZE;
        }
      }
    }

    /* Initialize parameters of the new Nishita sky model. */
    if (!DNA_struct_elem_find(fd->filesdna, "NodeTexSky", "float", "sun_size")) {
      FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
        if (ntree->type == NTREE_SHADER) {
          LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
            if (node->type == SH_NODE_TEX_SKY && node->storage) {
              NodeTexSky *tex = (NodeTexSky *)node->storage;
              tex->sun_disc = true;
              tex->sun_size = DEG2RADF(0.545);
              tex->sun_elevation = M_PI_2;
              tex->sun_rotation = 0.0f;
              tex->altitude = 0.0f;
              tex->air_density = 1.0f;
              tex->dust_density = 1.0f;
              tex->ozone_density = 1.0f;
            }
          }
        }
      }
      FOREACH_NODETREE_END;
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 290, 6)) {
    /* Transition to saving expansion for all of a modifier's sub-panels. */
    if (!DNA_struct_elem_find(fd->filesdna, "ModifierData", "short", "ui_expand_flag")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
          if (md->mode & eModifierMode_Expanded_DEPRECATED) {
            md->ui_expand_flag = 1;
          }
          else {
            md->ui_expand_flag = 0;
          }
        }
      }
    }

    /* EEVEE Motion blur new parameters. */
    if (!DNA_struct_elem_find(fd->filesdna, "SceneEEVEE", "float", "motion_blur_depth_scale")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->eevee.motion_blur_depth_scale = 100.0f;
        scene->eevee.motion_blur_max = 32;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "SceneEEVEE", "int", "motion_blur_steps")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->eevee.motion_blur_steps = 1;
      }
    }

    /* Transition to saving expansion for all of a constraint's sub-panels. */
    if (!DNA_struct_elem_find(fd->filesdna, "bConstraint", "short", "ui_expand_flag")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (bConstraint *, con, &object->constraints) {
          if (con->flag & CONSTRAINT_EXPAND_DEPRECATED) {
            con->ui_expand_flag = 1;
          }
          else {
            con->ui_expand_flag = 0;
          }
        }
      }
    }

    /* Transition to saving expansion for all of grease pencil modifier's sub-panels. */
    if (!DNA_struct_elem_find(fd->filesdna, "GpencilModifierData", "short", "ui_expand_flag")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (GpencilModifierData *, md, &object->greasepencil_modifiers) {
          if (md->mode & eGpencilModifierMode_Expanded_DEPRECATED) {
            md->ui_expand_flag = 1;
          }
          else {
            md->ui_expand_flag = 0;
          }
        }
      }
    }

    /* Transition to saving expansion for all of an effect's sub-panels. */
    if (!DNA_struct_elem_find(fd->filesdna, "ShaderFxData", "short", "ui_expand_flag")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (ShaderFxData *, fx, &object->shader_fx) {
          if (fx->mode & eShaderFxMode_Expanded_DEPRECATED) {
            fx->ui_expand_flag = 1;
          }
          else {
            fx->ui_expand_flag = 0;
          }
        }
      }
    }

    /* Refactor bevel profile type to use an enum. */
    if (!DNA_struct_elem_find(fd->filesdna, "BevelModifierData", "short", "profile_type")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
          if (md->type == eModifierType_Bevel) {
            BevelModifierData *bmd = (BevelModifierData *)md;
            bool use_custom_profile = bmd->flags & MOD_BEVEL_CUSTOM_PROFILE_DEPRECATED;
            bmd->profile_type = use_custom_profile ? MOD_BEVEL_PROFILE_CUSTOM :
                                                     MOD_BEVEL_PROFILE_SUPERELLIPSE;
          }
        }
      }
    }

    /* Change ocean modifier values from [0, 10] to [0, 1] ranges. */
    for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
      LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
        if (md->type == eModifierType_Ocean) {
          OceanModifierData *omd = (OceanModifierData *)md;
          omd->wave_alignment *= 0.1f;
          omd->sharpen_peak_jonswap *= 0.1f;
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 291, 1)) {

    /* Initialize additional parameter of the Nishita sky model and change altitude unit. */
    if (!DNA_struct_elem_find(fd->filesdna, "NodeTexSky", "float", "sun_intensity")) {
      FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
        if (ntree->type == NTREE_SHADER) {
          LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
            if (node->type == SH_NODE_TEX_SKY && node->storage) {
              NodeTexSky *tex = (NodeTexSky *)node->storage;
              tex->sun_intensity = 1.0f;
              tex->altitude *= 0.001f;
            }
          }
        }
      }
      FOREACH_NODETREE_END;
    }

    /* Refactor bevel affect type to use an enum. */
    if (!DNA_struct_elem_find(fd->filesdna, "BevelModifierData", "char", "affect_type")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
          if (md->type == eModifierType_Bevel) {
            BevelModifierData *bmd = (BevelModifierData *)md;
            const bool use_vertex_bevel = bmd->flags & MOD_BEVEL_VERT_DEPRECATED;
            bmd->affect_type = use_vertex_bevel ? MOD_BEVEL_AFFECT_VERTICES :
                                                  MOD_BEVEL_AFFECT_EDGES;
          }
        }
      }
    }

    /* Initialize additional velocity parameter for #CacheFile's. */
    if (!DNA_struct_elem_find(
            fd->filesdna, "MeshSeqCacheModifierData", "float", "velocity_scale")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
          if (md->type == eModifierType_MeshSequenceCache) {
            MeshSeqCacheModifierData *mcmd = (MeshSeqCacheModifierData *)md;
            mcmd->velocity_scale = 1.0f;
            mcmd->vertex_velocities = NULL;
            mcmd->num_vertices = 0;
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "CacheFile", "char", "velocity_unit")) {
      for (CacheFile *cache_file = bmain->cachefiles.first; cache_file != NULL;
           cache_file = cache_file->id.next) {
        BLI_strncpy(cache_file->velocity_name, ".velocities", sizeof(cache_file->velocity_name));
        cache_file->velocity_unit = CACHEFILE_VELOCITY_UNIT_SECOND;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "OceanModifierData", "int", "viewport_resolution")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
          if (md->type == eModifierType_Ocean) {
            OceanModifierData *omd = (OceanModifierData *)md;
            omd->viewport_resolution = omd->resolution;
          }
        }
      }
    }

    /* Remove panel X axis collapsing, a remnant of horizontal panel alignment. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
          LISTBASE_FOREACH (Panel *, panel, &region->panels) {
            panels_remove_x_closed_flag_recursive(panel);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 291, 2)) {
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      RigidBodyWorld *rbw = scene->rigidbody_world;

      if (rbw == NULL) {
        continue;
      }

      /* The substep method changed from "per second" to "per frame".
       * To get the new value simply divide the old bullet sim fps with the scene fps.
       */
      rbw->substeps_per_frame /= FPS;

      if (rbw->substeps_per_frame <= 0) {
        rbw->substeps_per_frame = 1;
      }
    }

    /* Hair and PointCloud attributes. */
    for (Hair *hair = bmain->hairs.first; hair != NULL; hair = hair->id.next) {
      do_versions_point_attributes(&hair->pdata);
    }
    for (PointCloud *pointcloud = bmain->pointclouds.first; pointcloud != NULL;
         pointcloud = pointcloud->id.next) {
      do_versions_point_attributes(&pointcloud->pdata);
    }

    /* Show outliner mode column by default. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
          if (space->spacetype == SPACE_OUTLINER) {
            SpaceOutliner *space_outliner = (SpaceOutliner *)space;

            space_outliner->flag |= SO_MODE_COLUMN;
          }
        }
      }
    }

    /* Solver and Collections for Boolean. */
    for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
      LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
        if (md->type == eModifierType_Boolean) {
          BooleanModifierData *bmd = (BooleanModifierData *)md;
          bmd->solver = eBooleanModifierSolver_Fast;
          bmd->flag = eBooleanModifierFlag_Object;
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 291, 4) && MAIN_VERSION_ATLEAST(bmain, 291, 1)) {
    /* Due to a48d78ce07f4f, CustomData.totlayer and CustomData.maxlayer has been written
     * incorrectly. Fortunately, the size of the layers array has been written to the .blend file
     * as well, so we can reconstruct totlayer and maxlayer from that. */
    LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
      mesh->vdata.totlayer = mesh->vdata.maxlayer = MEM_allocN_len(mesh->vdata.layers) /
                                                    sizeof(CustomDataLayer);
      mesh->edata.totlayer = mesh->edata.maxlayer = MEM_allocN_len(mesh->edata.layers) /
                                                    sizeof(CustomDataLayer);
      /* We can be sure that mesh->fdata is empty for files written by 2.90. */
      mesh->ldata.totlayer = mesh->ldata.maxlayer = MEM_allocN_len(mesh->ldata.layers) /
                                                    sizeof(CustomDataLayer);
      mesh->pdata.totlayer = mesh->pdata.maxlayer = MEM_allocN_len(mesh->pdata.layers) /
                                                    sizeof(CustomDataLayer);
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 291, 5)) {
    /* Fix fcurves to allow for new bezier handles behavior (T75881 and D8752). */
    for (bAction *act = bmain->actions.first; act; act = act->id.next) {
      for (FCurve *fcu = act->curves.first; fcu; fcu = fcu->next) {
        /* Only need to fix Bezier curves with at least 2 key-frames. */
        if (fcu->totvert < 2 || fcu->bezt == NULL) {
          continue;
        }
        do_versions_291_fcurve_handles_limit(fcu);
      }
    }

    LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
      collection->color_tag = COLLECTION_COLOR_NONE;
    }
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      /* Old files do not have a master collection, but it will be created by
       * `BKE_collection_master_add()`. */
      if (scene->master_collection) {
        scene->master_collection->color_tag = COLLECTION_COLOR_NONE;
      }
    }

    /* Add custom profile and bevel mode to curve bevels. */
    if (!DNA_struct_elem_find(fd->filesdna, "Curve", "char", "bevel_mode")) {
      LISTBASE_FOREACH (Curve *, curve, &bmain->curves) {
        if (curve->bevobj != NULL) {
          curve->bevel_mode = CU_BEV_MODE_OBJECT;
        }
        else {
          curve->bevel_mode = CU_BEV_MODE_ROUND;
        }
      }
    }

    /* Ensure that new viewport display fields are initialized correctly. */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_Fluid) {
          FluidModifierData *fmd = (FluidModifierData *)md;
          if (fmd->domain != NULL) {
            if (!fmd->domain->coba_field && fmd->domain->type == FLUID_DOMAIN_TYPE_LIQUID) {
              fmd->domain->coba_field = FLUID_DOMAIN_FIELD_PHI;
            }
            fmd->domain->grid_scale = 1.0;
            fmd->domain->gridlines_upper_bound = 1.0;
            fmd->domain->vector_scale_with_magnitude = true;
            const float grid_lines[4] = {1.0, 0.0, 0.0, 1.0};
            copy_v4_v4(fmd->domain->gridlines_range_color, grid_lines);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 291, 6)) {
    /* Darken Inactive Overlay. */
    if (!DNA_struct_elem_find(fd->filesdna, "View3DOverlay", "float", "fade_alpha")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->overlay.fade_alpha = 0.40f;
              v3d->overlay.flag |= V3D_OVERLAY_FADE_INACTIVE;
            }
          }
        }
      }
    }

    /* Unify symmetry as a mesh property. */
    if (!DNA_struct_elem_find(fd->filesdna, "Mesh", "char", "symmetry")) {
      LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
        /* The previous flags used to store mesh symmetry in edit-mode match the new ones that are
         * used in #Mesh.symmetry. */
        mesh->symmetry = mesh->editflag & (ME_SYMMETRY_X | ME_SYMMETRY_Y | ME_SYMMETRY_Z);
      }
    }

    /* Alembic importer: allow vertex interpolation by default. */
    for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
      LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
        if (md->type != eModifierType_MeshSequenceCache) {
          continue;
        }

        MeshSeqCacheModifierData *data = (MeshSeqCacheModifierData *)md;
        data->read_flag |= MOD_MESHSEQ_INTERPOLATE_VERTICES;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 291, 7)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->r.simplify_volumes = 1.0f;
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 291, 8)) {
    if (!DNA_struct_elem_find(fd->filesdna, "WorkSpaceDataRelation", "int", "parentid")) {
      LISTBASE_FOREACH (WorkSpace *, workspace, &bmain->workspaces) {
        LISTBASE_FOREACH_MUTABLE (
            WorkSpaceDataRelation *, relation, &workspace->hook_layout_relations) {
          relation->parent = blo_read_get_new_globaldata_address(fd, relation->parent);
          BLI_assert(relation->parentid == 0);
          if (relation->parent != NULL) {
            LISTBASE_FOREACH (wmWindowManager *, wm, &bmain->wm) {
              wmWindow *win = BLI_findptr(
                  &wm->windows, relation->parent, offsetof(wmWindow, workspace_hook));
              if (win != NULL) {
                relation->parentid = win->winid;
                break;
              }
            }
            if (relation->parentid == 0) {
              BLI_assert(
                  !"Found a valid parent for workspace data relation, but no valid parent id.");
            }
          }
          if (relation->parentid == 0) {
            BLI_freelinkN(&workspace->hook_layout_relations, relation);
          }
        }
      }
    }

    /* UV/Image show overlay option. */
    if (!DNA_struct_find(fd->filesdna, "SpaceImageOverlay")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
            if (space->spacetype == SPACE_IMAGE) {
              SpaceImage *sima = (SpaceImage *)space;
              sima->overlay.flag = SI_OVERLAY_SHOW_OVERLAYS;
            }
          }
        }
      }
    }

    /* Ensure that particle systems generated by fluid modifier have correct phystype. */
    LISTBASE_FOREACH (ParticleSettings *, part, &bmain->particles) {
      if (ELEM(
              part->type, PART_FLUID_FLIP, PART_FLUID_SPRAY, PART_FLUID_BUBBLE, PART_FLUID_FOAM)) {
        part->phystype = PART_PHYS_NO;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 291, 9)) {
    /* Remove options of legacy UV/Image editor */
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          switch (sl->spacetype) {
            case SPACE_IMAGE: {
              SpaceImage *sima = (SpaceImage *)sl;
              sima->flag &= ~(SI_FLAG_UNUSED_20);
              break;
            }
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "FluidModifierData", "float", "fractions_distance")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_Fluid) {
            FluidModifierData *fmd = (FluidModifierData *)md;
            if (fmd->domain) {
              fmd->domain->fractions_distance = 0.5;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 292, 1)) {
    {
      const int LEGACY_REFINE_RADIAL_DISTORTION_K1 = (1 << 2);

      LISTBASE_FOREACH (MovieClip *, clip, &bmain->movieclips) {
        MovieTracking *tracking = &clip->tracking;
        MovieTrackingSettings *settings = &tracking->settings;
        int new_refine_camera_intrinsics = 0;

        if (settings->refine_camera_intrinsics & REFINE_FOCAL_LENGTH) {
          new_refine_camera_intrinsics |= REFINE_FOCAL_LENGTH;
        }

        if (settings->refine_camera_intrinsics & REFINE_PRINCIPAL_POINT) {
          new_refine_camera_intrinsics |= REFINE_PRINCIPAL_POINT;
        }

        /* The end goal is to enable radial distortion refinement if either K1 or K2 were set for
         * refinement. It is enough to only check for L1 it was not possible to refine K2 without
         * K1. */
        if (settings->refine_camera_intrinsics & LEGACY_REFINE_RADIAL_DISTORTION_K1) {
          new_refine_camera_intrinsics |= REFINE_RADIAL_DISTORTION;
        }

        settings->refine_camera_intrinsics = new_refine_camera_intrinsics;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 292, 5)) {
    /* Initialize the opacity of the overlay wireframe */
    if (!DNA_struct_elem_find(fd->filesdna, "View3DOverlay", "float", "wireframe_opacity")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->overlay.wireframe_opacity = 1.0f;
            }
          }
        }
      }
    }

    /* Replace object hidden filter with inverted object visible filter.  */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
          if (space->spacetype == SPACE_OUTLINER) {
            SpaceOutliner *space_outliner = (SpaceOutliner *)space;
            if (space_outliner->filter_state == SO_FILTER_OB_HIDDEN) {
              space_outliner->filter_state = SO_FILTER_OB_VISIBLE;
              space_outliner->filter |= SO_FILTER_OB_STATE_INVERSE;
            }
          }
        }
      }
    }

    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_WeightVGProximity) {
          WeightVGProximityModifierData *wmd = (WeightVGProximityModifierData *)md;
          if (wmd->cmap_curve == NULL) {
            wmd->cmap_curve = BKE_curvemapping_add(1, 0.0, 0.0, 1.0, 1.0);
            BKE_curvemapping_init(wmd->cmap_curve);
          }
        }
      }
    }

    /* Hair and PointCloud attributes names. */
    LISTBASE_FOREACH (Hair *, hair, &bmain->hairs) {
      do_versions_point_attribute_names(&hair->pdata);
    }
    LISTBASE_FOREACH (PointCloud *, pointcloud, &bmain->pointclouds) {
      do_versions_point_attribute_names(&pointcloud->pdata);
    }

    /* Cryptomatte render pass */
    if (!DNA_struct_elem_find(fd->filesdna, "ViewLayer", "short", "cryptomatte_levels")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
          view_layer->cryptomatte_levels = 6;
          view_layer->cryptomatte_flag = VIEW_LAYER_CRYPTOMATTE_ACCURATE;
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 292, 7)) {
    /* Make all IDProperties used as interface of geometry node trees overridable. */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_Nodes) {
          NodesModifierData *nmd = (NodesModifierData *)md;
          IDProperty *nmd_properties = nmd->settings.properties;

          BLI_assert(nmd_properties->type == IDP_GROUP);
          LISTBASE_FOREACH (IDProperty *, nmd_socket_idprop, &nmd_properties->data.group) {
            nmd_socket_idprop->flag |= IDP_FLAG_OVERRIDABLE_LIBRARY;
          }
        }
      }
    }

    /* EEVEE/Cycles Volumes consistency */
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      /* Remove Volume Transmittance render pass from each view layer. */
      LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
        view_layer->eevee.render_passes &= ~EEVEE_RENDER_PASS_UNUSED_8;
      }

      /* Rename Renderlayer Socket `VolumeScatterCol` to `VolumeDir` */
      if (scene->nodetree) {
        LISTBASE_FOREACH (bNode *, node, &scene->nodetree->nodes) {
          if (node->type == CMP_NODE_R_LAYERS) {
            LISTBASE_FOREACH (bNodeSocket *, output_socket, &node->outputs) {
              const char *volume_scatter = "VolumeScatterCol";
              if (STREQLEN(output_socket->name, volume_scatter, MAX_NAME)) {
                BLI_strncpy(output_socket->name, RE_PASSNAME_VOLUME_LIGHT, MAX_NAME);
              }
            }
          }
        }
      }
    }

    /* Convert `NodeCryptomatte->storage->matte_id` to `NodeCryptomatte->storage->entries` */
    if (!DNA_struct_find(fd->filesdna, "CryptomatteEntry")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        if (scene->nodetree) {
          LISTBASE_FOREACH (bNode *, node, &scene->nodetree->nodes) {
            if (node->type == CMP_NODE_CRYPTOMATTE) {
              NodeCryptomatte *storage = (NodeCryptomatte *)node->storage;
              char *matte_id = storage->matte_id;
              if (matte_id == NULL || strlen(storage->matte_id) == 0) {
                continue;
              }
              BKE_cryptomatte_matte_id_to_entries(storage, storage->matte_id);
            }
          }
        }
      }
    }

    /* Overlay elements in the sequencer. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
          if (sl->spacetype == SPACE_SEQ) {
            SpaceSeq *sseq = (SpaceSeq *)sl;
            sseq->flag |= (SEQ_SHOW_STRIP_OVERLAY | SEQ_SHOW_STRIP_NAME | SEQ_SHOW_STRIP_SOURCE |
                           SEQ_SHOW_STRIP_DURATION);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 292, 8)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (STREQ(node->idname, "GeometryNodeRandomAttribute")) {
          STRNCPY(node->idname, "GeometryNodeAttributeRandomize");
        }
      }
    }

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      if (scene->ed != NULL) {
        scene->toolsettings->sequencer_tool_settings = SEQ_tool_settings_init();
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 292, 9)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_GEOMETRY) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type == GEO_NODE_ATTRIBUTE_MATH && node->storage == NULL) {
            const int old_use_attibute_a = (1 << 0);
            const int old_use_attibute_b = (1 << 1);
            NodeAttributeMath *data = MEM_callocN(sizeof(NodeAttributeMath), "NodeAttributeMath");
            data->operation = NODE_MATH_ADD;
            data->input_type_a = (node->custom2 & old_use_attibute_a) ?
                                     GEO_NODE_ATTRIBUTE_INPUT_ATTRIBUTE :
                                     GEO_NODE_ATTRIBUTE_INPUT_FLOAT;
            data->input_type_b = (node->custom2 & old_use_attibute_b) ?
                                     GEO_NODE_ATTRIBUTE_INPUT_ATTRIBUTE :
                                     GEO_NODE_ATTRIBUTE_INPUT_FLOAT;
            node->storage = data;
          }
        }
      }
    }
    FOREACH_NODETREE_END;

    /* Default properties editors to auto outliner sync. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
          if (space->spacetype == SPACE_PROPERTIES) {
            SpaceProperties *space_properties = (SpaceProperties *)space;
            space_properties->outliner_sync = PROPERTIES_SYNC_AUTO;
          }
        }
      }
    }

    /* Ensure that new viscosity strength field is initialized correctly. */
    if (!DNA_struct_elem_find(fd->filesdna, "FluidModifierData", "float", "viscosity_value")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
          if (md->type == eModifierType_Fluid) {
            FluidModifierData *fmd = (FluidModifierData *)md;
            if (fmd->domain != NULL) {
              fmd->domain->viscosity_value = 0.05;
            }
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 292, 10)) {
    if (!DNA_struct_find(fd->filesdna, "NodeSetAlpha")) {
      FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
        if (ntree->type != NTREE_COMPOSIT) {
          continue;
        }
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type != CMP_NODE_SETALPHA) {
            continue;
          }
          NodeSetAlpha *storage = MEM_callocN(sizeof(NodeSetAlpha), "NodeSetAlpha");
          storage->mode = CMP_NODE_SETALPHA_MODE_REPLACE_ALPHA;
          node->storage = storage;
        }
      }
      FOREACH_NODETREE_END;
    }

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      Editing *ed = SEQ_editing_get(scene, false);
      if (ed == NULL) {
        continue;
      }
      ed->cache_flag = (SEQ_CACHE_STORE_RAW | SEQ_CACHE_STORE_FINAL_OUT);
      do_versions_strip_cache_settings_recursive(&ed->seqbase);
    }
  }

  /* Enable "Save as Render" option for file output node by default (apply view transform to image
   * on save) */
  if (!MAIN_VERSION_ATLEAST(bmain, 292, 11)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_COMPOSIT) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type == CMP_NODE_OUTPUT_FILE) {
            LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
              NodeImageMultiFileSocket *simf = sock->storage;
              simf->save_as_render = true;
            }
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 293, 1)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_GEOMETRY) {
        version_node_socket_name(ntree, GEO_NODE_BOOLEAN, "Geometry A", "Geometry 1");
        version_node_socket_name(ntree, GEO_NODE_BOOLEAN, "Geometry B", "Geometry 2");
      }
    }
    FOREACH_NODETREE_END;

    /* Init grease pencil default curve resolution. */
    if (!DNA_struct_elem_find(fd->filesdna, "bGPdata", "int", "curve_edit_resolution")) {
      LISTBASE_FOREACH (bGPdata *, gpd, &bmain->gpencils) {
        gpd->curve_edit_resolution = GP_DEFAULT_CURVE_RESOLUTION;
        gpd->flag |= GP_DATA_CURVE_ADAPTIVE_RESOLUTION;
      }
    }
    /* Init grease pencil curve editing error threshold. */
    if (!DNA_struct_elem_find(fd->filesdna, "bGPdata", "float", "curve_edit_threshold")) {
      LISTBASE_FOREACH (bGPdata *, gpd, &bmain->gpencils) {
        gpd->curve_edit_threshold = GP_DEFAULT_CURVE_ERROR;
        gpd->curve_edit_corner_angle = GP_DEFAULT_CURVE_EDIT_CORNER_ANGLE;
      }
    }
  }

  if ((!MAIN_VERSION_ATLEAST(bmain, 292, 14)) ||
      ((bmain->versionfile == 293) && (!MAIN_VERSION_ATLEAST(bmain, 293, 1)))) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type == GEO_NODE_OBJECT_INFO && node->storage == NULL) {
          NodeGeometryObjectInfo *data = (NodeGeometryObjectInfo *)MEM_callocN(
              sizeof(NodeGeometryObjectInfo), __func__);
          data->transform_space = GEO_NODE_TRANSFORM_SPACE_RELATIVE;
          node->storage = data;
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 293, 1)) {
    /* Grease pencil layer transform matrix. */
    if (!DNA_struct_elem_find(fd->filesdna, "bGPDlayer", "float", "location[0]")) {
      LISTBASE_FOREACH (bGPdata *, gpd, &bmain->gpencils) {
        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          zero_v3(gpl->location);
          zero_v3(gpl->rotation);
          copy_v3_fl(gpl->scale, 1.0f);
          loc_eul_size_to_mat4(gpl->layer_mat, gpl->location, gpl->rotation, gpl->scale);
          invert_m4_m4(gpl->layer_invmat, gpl->layer_mat);
        }
      }
    }
    /* Fix Fill factor for grease pencil fill brushes. */
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if ((brush->gpencil_settings) && (brush->gpencil_settings->fill_factor == 0.0f)) {
        brush->gpencil_settings->fill_factor = 1.0f;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 293, 3)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type == GEO_NODE_POINT_INSTANCE && node->storage == NULL) {
          NodeGeometryPointInstance *data = (NodeGeometryPointInstance *)MEM_callocN(
              sizeof(NodeGeometryPointInstance), __func__);
          data->instance_type = node->custom1;
          data->flag = (node->custom2 ? 0 : GEO_NODE_POINT_INSTANCE_WHOLE_COLLECTION);
          node->storage = data;
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 293, 4)) {
    /* Add support for all operations to the "Attribute Math" node. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_GEOMETRY) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type == GEO_NODE_ATTRIBUTE_MATH) {
            NodeAttributeMath *data = (NodeAttributeMath *)node->storage;
            data->input_type_c = GEO_NODE_ATTRIBUTE_INPUT_ATTRIBUTE;
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 293, 5)) {
    /* Change Nishita sky model Altitude unit. */
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_SHADER) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (node->type == SH_NODE_TEX_SKY && node->storage) {
            NodeTexSky *tex = (NodeTexSky *)node->storage;
            tex->altitude *= 1000.0f;
          }
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 293, 6)) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
          /* UV/Image Max resolution images in image editor. */
          if (space->spacetype == SPACE_IMAGE) {
            SpaceImage *sima = (SpaceImage *)space;
            sima->iuser.flag |= IMA_SHOW_MAX_RESOLUTION;
          }
          /* Enable Outliner render visibility column. */
          else if (space->spacetype == SPACE_OUTLINER) {
            SpaceOutliner *space_outliner = (SpaceOutliner *)space;
            space_outliner->show_restrict_flags |= SO_RESTRICT_RENDER;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 293, 7)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_GEOMETRY) {
        version_node_join_geometry_for_multi_input_socket(ntree);
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 293, 8)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type != NTREE_GEOMETRY) {
        continue;
      }
      LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
        if (node->type == GEO_NODE_ATTRIBUTE_RANDOMIZE && node->storage == NULL) {
          NodeAttributeRandomize *data = (NodeAttributeRandomize *)MEM_callocN(
              sizeof(NodeAttributeRandomize), __func__);
          data->data_type = node->custom1;
          data->operation = GEO_NODE_ATTRIBUTE_RANDOMIZE_REPLACE_CREATE;
          node->storage = data;
        }
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 293, 9)) {
    if (!DNA_struct_elem_find(fd->filesdna, "SceneEEVEE", "float", "bokeh_overblur")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->eevee.bokeh_neighbor_max = 10.0f;
        scene->eevee.bokeh_denoise_fac = 0.75f;
        scene->eevee.bokeh_overblur = 5.0f;
      }
    }

    /* Add subpanels for FModifiers, which requires a field to store expansion. */
    if (!DNA_struct_elem_find(fd->filesdna, "FModifier", "short", "ui_expand_flag")) {
      LISTBASE_FOREACH (bAction *, act, &bmain->actions) {
        LISTBASE_FOREACH (FCurve *, fcu, &act->curves) {
          LISTBASE_FOREACH (FModifier *, fcm, &fcu->modifiers) {
            SET_FLAG_FROM_TEST(fcm->ui_expand_flag,
                               fcm->flag & FMODIFIER_FLAG_EXPANDED,
                               UI_PANEL_DATA_EXPAND_ROOT);
          }
        }
      }
    }

    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_GEOMETRY) {
        version_node_socket_name(ntree, GEO_NODE_ATTRIBUTE_PROXIMITY, "Result", "Distance");
      }
    }
    FOREACH_NODETREE_END;
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 293, 10)) {
    FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
      if (ntree->type == NTREE_GEOMETRY) {
        version_node_socket_name(ntree, GEO_NODE_ATTRIBUTE_PROXIMITY, "Location", "Position");
      }
    }
    FOREACH_NODETREE_END;

    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      /* Fix old scene with too many samples that were not being used.
       * Now they are properly used and might produce a huge slowdown.
       * So we clamp to what the old max actual was. */
      if (scene->eevee.volumetric_shadow_samples > 32) {
        scene->eevee.volumetric_shadow_samples = 32;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 293, 11)) {
    LISTBASE_FOREACH (bNodeTree *, ntree, &bmain->nodetrees) {
      if (ntree->type == NTREE_GEOMETRY) {
        LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
          if (STREQ(node->idname, "GeometryNodeSubdivisionSurfaceSimple")) {
            STRNCPY(node->idname, "GeometryNodeSubdivide");
          }
          if (STREQ(node->idname, "GeometryNodeSubdivisionSurface")) {
            STRNCPY(node->idname, "GeometryNodeSubdivideSmooth");
          }
        }
      }
    }
  }

  /**
   * Versioning code until next subversion bump goes here.
   *
   * \note Be sure to check when bumping the version:
   * - "versioning_userdef.c", #blo_do_versions_userdef
   * - "versioning_userdef.c", #do_versions_theme
   *
   * \note Keep this message at the bottom of the function.
   */
  {
    /* Keep this block, even when empty. */
  }
}
