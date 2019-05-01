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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2018, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>

#include "BLI_utildefines.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"

#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_modifier.h"
#include "BKE_library_query.h"
#include "BKE_scene.h"
#include "BKE_main.h"
#include "BKE_layer.h"

#include "MEM_guardedalloc.h"

#include "MOD_gpencil_util.h"
#include "MOD_gpencil_modifiertypes.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

static void initData(GpencilModifierData *md)
{
  MirrorGpencilModifierData *gpmd = (MirrorGpencilModifierData *)md;
  gpmd->pass_index = 0;
  gpmd->layername[0] = '\0';
  gpmd->object = NULL;
  gpmd->flag |= GP_MIRROR_AXIS_X;
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copyData_generic(md, target);
}

static void update_position(Object *ob, MirrorGpencilModifierData *mmd, bGPDstroke *gps, int axis)
{
  int i;
  bGPDspoint *pt;
  float factor[3] = {1.0f, 1.0f, 1.0f};
  factor[axis] = -1.0f;

  float clear[3] = {0.0f, 0.0f, 0.0f};
  clear[axis] = 1.0f;

  float ob_origin[3];
  float pt_origin[3];

  if (mmd->object) {
    float inv_mat[4][4];

    invert_m4_m4(inv_mat, mmd->object->obmat);
    mul_v3_m4v3(ob_origin, inv_mat, ob->obmat[3]);
  }
  else {
    copy_v3_v3(ob_origin, ob->obmat[3]);
  }

  /* only works with current axis */
  mul_v3_v3(ob_origin, clear);

  mul_v3_v3fl(pt_origin, ob_origin, -2.0f);

  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    mul_v3_v3(&pt->x, factor);
    if (mmd->object) {
      add_v3_v3(&pt->x, pt_origin);
    }
  }
}

/* Generic "generateStrokes" callback */
static void generateStrokes(GpencilModifierData *md,
                            Depsgraph *UNUSED(depsgraph),
                            Object *ob,
                            bGPDlayer *gpl,
                            bGPDframe *gpf)
{
  MirrorGpencilModifierData *mmd = (MirrorGpencilModifierData *)md;
  bGPDstroke *gps, *gps_new = NULL;
  int tot_strokes;
  int i;

  /* check each axis for mirroring */
  for (int xi = 0; xi < 3; ++xi) {
    if (mmd->flag & (GP_MIRROR_AXIS_X << xi)) {

      /* count strokes to avoid infinite loop after adding new strokes to tail of listbase */
      tot_strokes = BLI_listbase_count(&gpf->strokes);

      for (i = 0, gps = gpf->strokes.first; i < tot_strokes; i++, gps = gps->next) {
        if (is_stroke_affected_by_modifier(ob,
                                           mmd->layername,
                                           mmd->pass_index,
                                           mmd->layer_pass,
                                           1,
                                           gpl,
                                           gps,
                                           mmd->flag & GP_MIRROR_INVERT_LAYER,
                                           mmd->flag & GP_MIRROR_INVERT_PASS,
                                           mmd->flag & GP_MIRROR_INVERT_LAYERPASS)) {
          gps_new = BKE_gpencil_stroke_duplicate(gps);
          update_position(ob, mmd, gps_new, xi);
          BLI_addtail(&gpf->strokes, gps_new);
        }
      }
    }
  }
}

static void bakeModifier(Main *bmain, Depsgraph *depsgraph, GpencilModifierData *md, Object *ob)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  bGPdata *gpd = ob->data;
  int oldframe = (int)DEG_get_ctime(depsgraph);

  for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
    for (bGPDframe *gpf = gpl->frames.first; gpf; gpf = gpf->next) {
      /* apply mirror effects on this frame */
      CFRA = gpf->framenum;
      BKE_scene_graph_update_for_newframe(depsgraph, bmain);

      /* compute mirror effects on this frame */
      generateStrokes(md, depsgraph, ob, gpl, gpf);
    }
  }

  /* return frame state and DB to original state */
  CFRA = oldframe;
  BKE_scene_graph_update_for_newframe(depsgraph, bmain);
}

static bool isDisabled(GpencilModifierData *UNUSED(md), int UNUSED(userRenderParams))
{
  // MirrorGpencilModifierData *mmd = (MirrorGpencilModifierData *)md;

  return false;
}

static void updateDepsgraph(GpencilModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  MirrorGpencilModifierData *lmd = (MirrorGpencilModifierData *)md;
  if (lmd->object != NULL) {
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_GEOMETRY, "Mirror Modifier");
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Mirror Modifier");
  }
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Mirror Modifier");
}

static void foreachObjectLink(GpencilModifierData *md,
                              Object *ob,
                              ObjectWalkFunc walk,
                              void *userData)
{
  MirrorGpencilModifierData *mmd = (MirrorGpencilModifierData *)md;

  walk(userData, ob, &mmd->object, IDWALK_CB_NOP);
}

static int getDuplicationFactor(GpencilModifierData *md)
{
  MirrorGpencilModifierData *mmd = (MirrorGpencilModifierData *)md;
  int factor = 1;
  /* create a duplication for each axis */
  for (int xi = 0; xi < 3; ++xi) {
    if (mmd->flag & (GP_MIRROR_AXIS_X << xi)) {
      factor++;
    }
  }
  CLAMP_MIN(factor, 1);

  return factor;
}

GpencilModifierTypeInfo modifierType_Gpencil_Mirror = {
    /* name */ "Mirror",
    /* structName */ "MirrorGpencilModifierData",
    /* structSize */ sizeof(MirrorGpencilModifierData),
    /* type */ eGpencilModifierTypeType_Gpencil,
    /* flags */ eGpencilModifierTypeFlag_SupportsEditmode,

    /* copyData */ copyData,

    /* deformStroke */ NULL,
    /* generateStrokes */ generateStrokes,
    /* bakeModifier */ bakeModifier,
    /* remapTime */ NULL,

    /* initData */ initData,
    /* freeData */ NULL,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* getDuplicationFactor */ getDuplicationFactor,
};
