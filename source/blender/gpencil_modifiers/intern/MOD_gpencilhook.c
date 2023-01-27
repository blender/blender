/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation. */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>
#include <string.h>

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_action.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil_geom.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_gpencil_modifiertypes.h"
#include "MOD_gpencil_ui_common.h"
#include "MOD_gpencil_util.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

/* temp struct to hold data */
struct GPHookData_cb {
  struct CurveMapping *curfalloff;

  char falloff_type;
  float falloff;
  float falloff_sq;
  float fac_orig;

  uint use_falloff : 1;
  uint use_uniform : 1;

  float cent[3];

  float mat_uniform[3][3];
  float mat[4][4];
};

static void initData(GpencilModifierData *md)
{
  HookGpencilModifierData *gpmd = (HookGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(HookGpencilModifierData), modifier);

  gpmd->curfalloff = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
  BKE_curvemapping_init(gpmd->curfalloff);
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  HookGpencilModifierData *gmd = (HookGpencilModifierData *)md;
  HookGpencilModifierData *tgmd = (HookGpencilModifierData *)target;

  if (tgmd->curfalloff != NULL) {
    BKE_curvemapping_free(tgmd->curfalloff);
    tgmd->curfalloff = NULL;
  }

  BKE_gpencil_modifier_copydata_generic(md, target);

  tgmd->curfalloff = BKE_curvemapping_copy(gmd->curfalloff);
}

/* Calculate the factor of falloff. */
static float gpencil_hook_falloff(const struct GPHookData_cb *tData, const float len_sq)
{
  BLI_assert(tData->falloff_sq);
  if (len_sq > tData->falloff_sq) {
    return 0.0f;
  }
  if (len_sq > 0.0f) {
    float fac;

    if (tData->falloff_type == eGPHook_Falloff_Const) {
      fac = 1.0f;
      goto finally;
    }
    else if (tData->falloff_type == eGPHook_Falloff_InvSquare) {
      /* avoid sqrt below */
      fac = 1.0f - (len_sq / tData->falloff_sq);
      goto finally;
    }

    fac = 1.0f - (sqrtf(len_sq) / tData->falloff);

    switch (tData->falloff_type) {
      case eGPHook_Falloff_Curve:
        fac = BKE_curvemapping_evaluateF(tData->curfalloff, 0, fac);
        break;
      case eGPHook_Falloff_Sharp:
        fac = fac * fac;
        break;
      case eGPHook_Falloff_Smooth:
        fac = 3.0f * fac * fac - 2.0f * fac * fac * fac;
        break;
      case eGPHook_Falloff_Root:
        fac = sqrtf(fac);
        break;
      case eGPHook_Falloff_Linear:
        /* pass */
        break;
      case eGPHook_Falloff_Sphere:
        fac = sqrtf(2 * fac - fac * fac);
        break;
      default:
        break;
    }

  finally:
    return fac * tData->fac_orig;
  }
  else {
    return tData->fac_orig;
  }
}

/* apply point deformation */
static void gpencil_hook_co_apply(struct GPHookData_cb *tData, float weight, bGPDspoint *pt)
{
  float fac;

  if (tData->use_falloff) {
    float len_sq;

    if (tData->use_uniform) {
      float co_uniform[3];
      mul_v3_m3v3(co_uniform, tData->mat_uniform, &pt->x);
      len_sq = len_squared_v3v3(tData->cent, co_uniform);
    }
    else {
      len_sq = len_squared_v3v3(tData->cent, &pt->x);
    }

    fac = gpencil_hook_falloff(tData, len_sq);
  }
  else {
    fac = tData->fac_orig;
  }

  if (fac) {
    float co_tmp[3];
    mul_v3_m4v3(co_tmp, tData->mat, &pt->x);
    interp_v3_v3v3(&pt->x, &pt->x, co_tmp, fac * weight);
  }
}

/* deform stroke */
static void deformStroke(GpencilModifierData *md,
                         Depsgraph *UNUSED(depsgraph),
                         Object *ob,
                         bGPDlayer *gpl,
                         bGPDframe *UNUSED(gpf),
                         bGPDstroke *gps)
{
  HookGpencilModifierData *mmd = (HookGpencilModifierData *)md;
  if (!mmd->object) {
    return;
  }

  const int def_nr = BKE_object_defgroup_name_index(ob, mmd->vgname);

  bPoseChannel *pchan = BKE_pose_channel_find_name(mmd->object->pose, mmd->subtarget);
  float dmat[4][4];
  struct GPHookData_cb tData;

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      1,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_HOOK_INVERT_LAYER,
                                      mmd->flag & GP_HOOK_INVERT_PASS,
                                      mmd->flag & GP_HOOK_INVERT_LAYERPASS,
                                      mmd->flag & GP_HOOK_INVERT_MATERIAL)) {
    return;
  }
  bGPdata *gpd = ob->data;

  /* init struct */
  tData.curfalloff = mmd->curfalloff;
  tData.falloff_type = mmd->falloff_type;
  tData.falloff = (mmd->falloff_type == eHook_Falloff_None) ? 0.0f : mmd->falloff;
  tData.falloff_sq = square_f(tData.falloff);
  tData.fac_orig = mmd->force;
  tData.use_falloff = (tData.falloff_sq != 0.0f);
  tData.use_uniform = (mmd->flag & GP_HOOK_UNIFORM_SPACE) != 0;

  if (tData.use_uniform) {
    copy_m3_m4(tData.mat_uniform, mmd->parentinv);
    mul_v3_m3v3(tData.cent, tData.mat_uniform, mmd->cent);
  }
  else {
    unit_m3(tData.mat_uniform);
    copy_v3_v3(tData.cent, mmd->cent);
  }

  /* get world-space matrix of target, corrected for the space the verts are in */
  if (mmd->subtarget[0] && pchan) {
    /* bone target if there's a matching pose-channel */
    mul_m4_m4m4(dmat, mmd->object->object_to_world, pchan->pose_mat);
  }
  else {
    /* just object target */
    copy_m4_m4(dmat, mmd->object->object_to_world);
  }
  invert_m4_m4(ob->world_to_object, ob->object_to_world);
  mul_m4_series(tData.mat, ob->world_to_object, dmat, mmd->parentinv);

  /* loop points and apply deform */
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    MDeformVert *dvert = gps->dvert != NULL ? &gps->dvert[i] : NULL;

    /* verify vertex group */
    const float weight = get_modifier_point_weight(
        dvert, (mmd->flag & GP_HOOK_INVERT_VGROUP) != 0, def_nr);
    if (weight < 0.0f) {
      continue;
    }
    gpencil_hook_co_apply(&tData, weight, pt);
  }
  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);
}

/* FIXME: Ideally we be doing this on a copy of the main depsgraph
 * (i.e. one where we don't have to worry about restoring state)
 */
static void bakeModifier(Main *UNUSED(bmain),
                         Depsgraph *depsgraph,
                         GpencilModifierData *md,
                         Object *ob)
{
  HookGpencilModifierData *mmd = (HookGpencilModifierData *)md;

  if (mmd->object == NULL) {
    return;
  }

  generic_bake_deform_stroke(depsgraph, md, ob, true, deformStroke);
}

static void freeData(GpencilModifierData *md)
{
  HookGpencilModifierData *mmd = (HookGpencilModifierData *)md;

  if (mmd->curfalloff) {
    BKE_curvemapping_free(mmd->curfalloff);
  }
}

static bool isDisabled(GpencilModifierData *md, int UNUSED(userRenderParams))
{
  HookGpencilModifierData *mmd = (HookGpencilModifierData *)md;

  return !mmd->object;
}

static void updateDepsgraph(GpencilModifierData *md,
                            const ModifierUpdateDepsgraphContext *ctx,
                            const int UNUSED(mode))
{
  HookGpencilModifierData *lmd = (HookGpencilModifierData *)md;
  if (lmd->object != NULL) {
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_GEOMETRY, "Hook Modifier");
    DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Hook Modifier");
  }
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Hook Modifier");
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  HookGpencilModifierData *mmd = (HookGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);
  walk(userData, ob, (ID **)&mmd->object, IDWALK_CB_NOP);
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *sub, *row, *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, &ob_ptr);

  PointerRNA hook_object_ptr = RNA_pointer_get(ptr, "object");
  bool has_vertex_group = RNA_string_length(ptr, "vertex_group") != 0;

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "object", 0, NULL, ICON_NONE);
  if (!RNA_pointer_is_null(&hook_object_ptr) &&
      RNA_enum_get(&hook_object_ptr, "type") == OB_ARMATURE) {
    PointerRNA hook_object_data_ptr = RNA_pointer_get(&hook_object_ptr, "data");
    uiItemPointerR(
        col, ptr, "subtarget", &hook_object_data_ptr, "bones", IFACE_("Bone"), ICON_NONE);
  }

  row = uiLayoutRow(layout, true);
  uiItemPointerR(row, ptr, "vertex_group", &ob_ptr, "vertex_groups", NULL, ICON_NONE);
  sub = uiLayoutRow(row, true);
  uiLayoutSetActive(sub, has_vertex_group);
  uiLayoutSetPropSep(sub, false);
  uiItemR(sub, ptr, "invert_vertex", 0, "", ICON_ARROW_LEFTRIGHT);

  uiItemR(layout, ptr, "strength", UI_ITEM_R_SLIDER, NULL, ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
}

static void falloff_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  bool use_falloff = RNA_enum_get(ptr, "falloff_type") != eWarp_Falloff_None;

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "falloff_type", 0, IFACE_("Type"), ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiLayoutSetActive(row, use_falloff);
  uiItemR(row, ptr, "falloff_radius", 0, NULL, ICON_NONE);

  uiItemR(layout, ptr, "use_falloff_uniform", 0, NULL, ICON_NONE);

  if (RNA_enum_get(ptr, "falloff_type") == eWarp_Falloff_Curve) {
    uiTemplateCurveMapping(layout, ptr, "falloff_curve", 0, false, false, false, false);
  }
}

static void mask_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, false);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Hook, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "falloff", "Falloff", NULL, falloff_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", NULL, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Hook = {
    /*name*/ N_("Hook"),
    /*structName*/ "HookGpencilModifierData",
    /*structSize*/ sizeof(HookGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ eGpencilModifierTypeFlag_SupportsEditmode,

    /*copyData*/ copyData,

    /*deformStroke*/ deformStroke,
    /*generateStrokes*/ NULL,
    /*bakeModifier*/ bakeModifier,
    /*remapTime*/ NULL,

    /*initData*/ initData,
    /*freeData*/ freeData,
    /*isDisabled*/ isDisabled,
    /*updateDepsgraph*/ updateDepsgraph,
    /*dependsOnTime*/ NULL,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ NULL,
    /*panelRegister*/ panelRegister,
};
