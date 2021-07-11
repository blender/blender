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
 *
 * The Original Code is Copyright (C) 2021, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_lib_query.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_gpencil_modifiertypes.h"
#include "MOD_gpencil_ui_common.h"
#include "MOD_gpencil_util.h"

static void initData(GpencilModifierData *md)
{
  WeightGpencilModifierData *gpmd = (WeightGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(WeightGpencilModifierData), modifier);
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

/* Calc distance between point and target object. */
static float calc_point_weight_by_distance(Object *ob,
                                           WeightGpencilModifierData *mmd,
                                           const float dist_max,
                                           const float dist_min,
                                           bGPDspoint *pt)
{
  float weight;
  float gvert[3];
  mul_v3_m4v3(gvert, ob->obmat, &pt->x);
  float dist = len_v3v3(mmd->object->obmat[3], gvert);

  if (dist > dist_max) {
    weight = 0.0f;
  }
  else if (dist <= dist_max && dist > dist_min) {
    weight = (dist_max - dist) / max_ff((dist_max - dist_min), 0.0001f);
  }
  else {
    weight = 1.0f;
  }

  return weight;
}

/* change stroke thickness */
static void deformStroke(GpencilModifierData *md,
                         Depsgraph *UNUSED(depsgraph),
                         Object *ob,
                         bGPDlayer *gpl,
                         bGPDframe *UNUSED(gpf),
                         bGPDstroke *gps)
{
  WeightGpencilModifierData *mmd = (WeightGpencilModifierData *)md;
  const int def_nr = BKE_object_defgroup_name_index(ob, mmd->vgname);
  const eWeightGpencilModifierMode mode = mmd->mode;

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      1,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_WEIGHT_INVERT_LAYER,
                                      mmd->flag & GP_WEIGHT_INVERT_PASS,
                                      mmd->flag & GP_WEIGHT_INVERT_LAYERPASS,
                                      mmd->flag & GP_WEIGHT_INVERT_MATERIAL)) {
    return;
  }

  const float dist_max = MAX2(mmd->dist_start, mmd->dist_end);
  const float dist_min = MIN2(mmd->dist_start, mmd->dist_end);
  const int target_def_nr = BKE_object_defgroup_name_index(ob, mmd->target_vgname);

  if (target_def_nr == -1) {
    return;
  }

  /* Use default Z up. */
  float vec_axis[3] = {0.0f, 0.0f, 1.0f};
  float axis[3] = {0.0f, 0.0f, 0.0f};
  axis[mmd->axis] = 1.0f;
  float vec_ref[3];
  /* Apply modifier rotation (sub 90 degrees for Y axis due Z-Up vector). */
  float rot_angle = mmd->angle - ((mmd->axis == 1) ? M_PI_2 : 0.0f);
  rotate_normalized_v3_v3v3fl(vec_ref, vec_axis, axis, rot_angle);

  /* Apply the rotation of the object. */
  if (mmd->space == GP_SPACE_LOCAL) {
    mul_mat3_m4_v3(ob->obmat, vec_ref);
  }

  /* Ensure there is a vertex group. */
  BKE_gpencil_dvert_ensure(gps);

  float weight_pt = 1.0f;
  for (int i = 0; i < gps->totpoints; i++) {
    MDeformVert *dvert = gps->dvert != NULL ? &gps->dvert[i] : NULL;
    /* Verify point is part of vertex group. */
    float weight = get_modifier_point_weight(
        dvert, (mmd->flag & GP_WEIGHT_INVERT_VGROUP) != 0, def_nr);
    if (weight < 0.0f) {
      continue;
    }

    switch (mode) {
      case GP_WEIGHT_MODE_DISTANCE: {
        if (mmd->object) {
          bGPDspoint *pt = &gps->points[i];
          weight_pt = calc_point_weight_by_distance(ob, mmd, dist_max, dist_min, pt);
        }
        break;
      }
      case GP_WEIGHT_MODE_ANGLE: {
        /* Special case for single points. */
        if (gps->totpoints == 1) {
          weight_pt = 1.0f;
          break;
        }

        bGPDspoint *pt1 = (i > 0) ? &gps->points[i] : &gps->points[i + 1];
        bGPDspoint *pt2 = (i > 0) ? &gps->points[i - 1] : &gps->points[i];
        float fpt1[3], fpt2[3];
        mul_v3_m4v3(fpt1, ob->obmat, &pt1->x);
        mul_v3_m4v3(fpt2, ob->obmat, &pt2->x);

        float vec[3];
        sub_v3_v3v3(vec, fpt1, fpt2);
        float angle = angle_on_axis_v3v3_v3(vec_ref, vec, axis);
        /* Use sin to get a value between 0 and 1. */
        weight_pt = 1.0f - sin(angle);
        break;
      }
      default:
        break;
    }

    /* Invert weight if required. */
    if (mmd->flag & GP_WEIGHT_INVERT_OUTPUT) {
      weight_pt = 1.0f - weight_pt;
    }
    /* Assign weight. */
    dvert = gps->dvert != NULL ? &gps->dvert[i] : NULL;
    if (dvert != NULL) {
      MDeformWeight *dw = BKE_defvert_ensure_index(dvert, target_def_nr);
      if (dw) {
        dw->weight = (mmd->flag & GP_WEIGHT_BLEND_DATA) ? dw->weight * weight_pt : weight_pt;
        CLAMP(dw->weight, mmd->min_weight, 1.0f);
      }
    }
  }
}

static void bakeModifier(struct Main *UNUSED(bmain),
                         Depsgraph *depsgraph,
                         GpencilModifierData *md,
                         Object *ob)
{
  bGPdata *gpd = ob->data;

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        deformStroke(md, depsgraph, ob, gpl, gpf, gps);
      }
    }
  }
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  WeightGpencilModifierData *mmd = (WeightGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);
  walk(userData, ob, (ID **)&mmd->object, IDWALK_CB_NOP);
}

static void updateDepsgraph(GpencilModifierData *md,
                            const ModifierUpdateDepsgraphContext *ctx,
                            const int UNUSED(mode))
{
  WeightGpencilModifierData *mmd = (WeightGpencilModifierData *)md;
  if (mmd->object != NULL) {
    DEG_add_object_relation(
        ctx->node, mmd->object, DEG_OB_COMP_TRANSFORM, "GPencil Weight Modifier");
  }
  DEG_add_object_relation(
      ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "GPencil Weight Modifier");
}

static bool isDisabled(GpencilModifierData *md, int UNUSED(userRenderParams))
{
  WeightGpencilModifierData *mmd = (WeightGpencilModifierData *)md;

  return !(mmd->target_vgname && mmd->target_vgname[0] != '\0');
}

static void distance_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "object", 0, NULL, ICON_CUBE);
  uiLayout *sub = uiLayoutColumn(layout, true);
  uiItemR(sub, ptr, "distance_start", 0, NULL, ICON_NONE);
  uiItemR(sub, ptr, "distance_end", 0, "End", ICON_NONE);
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);
  uiItemR(layout, ptr, "mode", 0, NULL, ICON_NONE);

  const eWeightGpencilModifierMode mode = RNA_enum_get(ptr, "mode");

  uiItemPointerR(layout, ptr, "target_vertex_group", &ob_ptr, "vertex_groups", NULL, ICON_NONE);

  uiItemR(layout, ptr, "minimum_weight", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "use_invert_output", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "use_blend", 0, NULL, ICON_NONE);

  switch (mode) {
    case GP_WEIGHT_MODE_DISTANCE:
      distance_panel_draw(C, panel);
      break;
    case GP_WEIGHT_MODE_ANGLE:
      uiItemR(layout, ptr, "angle", 0, NULL, ICON_NONE);
      uiItemR(layout, ptr, "axis", 0, NULL, ICON_NONE);
      uiItemR(layout, ptr, "space", 0, NULL, ICON_NONE);
      break;
    default:
      break;
  }

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, true);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Weight, panel_draw);

  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", NULL, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Weight = {
    /* name */ "Vertex Weight",
    /* structName */ "WeightGpencilModifierData",
    /* structSize */ sizeof(WeightGpencilModifierData),
    /* type */ eGpencilModifierTypeType_Gpencil,
    /* flags */ 0,

    /* copyData */ copyData,

    /* deformStroke */ deformStroke,
    /* generateStrokes */ NULL,
    /* bakeModifier */ bakeModifier,
    /* remapTime */ NULL,

    /* initData */ initData,
    /* freeData */ NULL,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* panelRegister */ panelRegister,
};
