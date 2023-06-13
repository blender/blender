/* SPDX-FileCopyrightText: 2021 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

static void initData(GpencilModifierData *md)
{
  WeightAngleGpencilModifierData *gpmd = (WeightAngleGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(WeightAngleGpencilModifierData), modifier);
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

/* change stroke thickness */
static void deformStroke(GpencilModifierData *md,
                         Depsgraph *UNUSED(depsgraph),
                         Object *ob,
                         bGPDlayer *gpl,
                         bGPDframe *UNUSED(gpf),
                         bGPDstroke *gps)
{
  WeightAngleGpencilModifierData *mmd = (WeightAngleGpencilModifierData *)md;
  const int def_nr = BKE_object_defgroup_name_index(ob, mmd->vgname);

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
                                      mmd->flag & GP_WEIGHT_INVERT_MATERIAL))
  {
    return;
  }

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
    mul_mat3_m4_v3(ob->object_to_world, vec_ref);
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

    /* Special case for single points. */
    if (gps->totpoints == 1) {
      weight_pt = 1.0f;
      break;
    }

    bGPDspoint *pt1 = (i > 0) ? &gps->points[i] : &gps->points[i + 1];
    bGPDspoint *pt2 = (i > 0) ? &gps->points[i - 1] : &gps->points[i];
    float fpt1[3], fpt2[3];
    mul_v3_m4v3(fpt1, ob->object_to_world, &pt1->x);
    mul_v3_m4v3(fpt2, ob->object_to_world, &pt2->x);

    float vec[3];
    sub_v3_v3v3(vec, fpt1, fpt2);
    float angle = angle_on_axis_v3v3_v3(vec_ref, vec, axis);
    /* Use sin to get a value between 0 and 1. */
    weight_pt = 1.0f - sin(angle);

    /* Invert weight if required. */
    if (mmd->flag & GP_WEIGHT_INVERT_OUTPUT) {
      weight_pt = 1.0f - weight_pt;
    }
    /* Assign weight. */
    dvert = gps->dvert != NULL ? &gps->dvert[i] : NULL;
    if (dvert != NULL) {
      MDeformWeight *dw = BKE_defvert_ensure_index(dvert, target_def_nr);
      if (dw) {
        dw->weight = (mmd->flag & GP_WEIGHT_MULTIPLY_DATA) ? dw->weight * weight_pt : weight_pt;
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
  generic_bake_deform_stroke(depsgraph, md, ob, false, deformStroke);
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  WeightAngleGpencilModifierData *mmd = (WeightAngleGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static bool isDisabled(GpencilModifierData *md, int UNUSED(userRenderParams))
{
  WeightAngleGpencilModifierData *mmd = (WeightAngleGpencilModifierData *)md;

  return (mmd->target_vgname[0] == '\0');
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *row, *sub;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);
  row = uiLayoutRow(layout, true);
  uiItemPointerR(row, ptr, "target_vertex_group", &ob_ptr, "vertex_groups", NULL, ICON_NONE);
  sub = uiLayoutRow(row, true);
  bool has_output = RNA_string_length(ptr, "target_vertex_group") != 0;
  uiLayoutSetPropDecorate(sub, false);
  uiLayoutSetActive(sub, has_output);
  uiItemR(sub, ptr, "use_invert_output", 0, "", ICON_ARROW_LEFTRIGHT);

  uiItemR(layout, ptr, "angle", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "axis", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "space", 0, NULL, ICON_NONE);

  uiItemR(layout, ptr, "minimum_weight", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "use_multiply", 0, NULL, ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, true);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_WeightAngle, panel_draw);

  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", NULL, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_WeightAngle = {
    /*name*/ N_("Vertex Weight Angle"),
    /*structName*/ "WeightAngleGpencilModifierData",
    /*structSize*/ sizeof(WeightAngleGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ 0,

    /*copyData*/ copyData,

    /*deformStroke*/ deformStroke,
    /*generateStrokes*/ NULL,
    /*bakeModifier*/ bakeModifier,
    /*remapTime*/ NULL,

    /*initData*/ initData,
    /*freeData*/ NULL,
    /*isDisabled*/ isDisabled,
    /*updateDepsgraph*/ NULL,
    /*dependsOnTime*/ NULL,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ NULL,
    /*panelRegister*/ panelRegister,
};
