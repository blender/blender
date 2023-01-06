/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation. */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>

#include "BLI_hash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "DNA_defaults.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "RNA_access.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil_geom.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_lib_query.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "MOD_gpencil_modifiertypes.h"
#include "MOD_gpencil_ui_common.h"
#include "MOD_gpencil_util.h"

static void initData(GpencilModifierData *md)
{
  OffsetGpencilModifierData *gpmd = (OffsetGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(OffsetGpencilModifierData), modifier);
  /* Open the first subpanel too, because it's activated by default. */
  md->ui_expand_flag = UI_PANEL_DATA_EXPAND_ROOT | UI_SUBPANEL_DATA_EXPAND_1;
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

/* change stroke offsetness */
static void deformStroke(GpencilModifierData *md,
                         Depsgraph *UNUSED(depsgraph),
                         Object *ob,
                         bGPDlayer *gpl,
                         bGPDframe *gpf,
                         bGPDstroke *gps)
{
  OffsetGpencilModifierData *mmd = (OffsetGpencilModifierData *)md;
  const int def_nr = BKE_object_defgroup_name_index(ob, mmd->vgname);

  float mat[4][4];
  float loc[3], rot[3], scale[3];

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      1,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_OFFSET_INVERT_LAYER,
                                      mmd->flag & GP_OFFSET_INVERT_PASS,
                                      mmd->flag & GP_OFFSET_INVERT_LAYERPASS,
                                      mmd->flag & GP_OFFSET_INVERT_MATERIAL)) {
    return;
  }

  const bool is_randomized = !(is_zero_v3(mmd->rnd_offset) && is_zero_v3(mmd->rnd_rot) &&
                               is_zero_v3(mmd->rnd_scale));
  const bool is_general = !(is_zero_v3(mmd->loc) && is_zero_v3(mmd->rot) &&
                            is_zero_v3(mmd->scale));

  int seed = mmd->seed;
  /* Make sure different modifiers get different seeds. */
  seed += BLI_hash_string(ob->id.name + 2);
  seed += BLI_hash_string(md->name);

  float rand[3][3];
  float rand_offset = BLI_hash_int_01(seed);
  bGPdata *gpd = ob->data;

  if (is_randomized && mmd->mode == GP_OFFSET_RANDOM) {
    /* Get stroke index for random offset. */
    int rnd_index = BLI_findindex(&gpf->strokes, gps);
    for (int j = 0; j < 3; j++) {
      const uint primes[3] = {2, 3, 7};
      double offset[3] = {0.0f, 0.0f, 0.0f};
      double r[3];
      /* To ensure a nice distribution, we use halton sequence and offset using the seed. */
      BLI_halton_3d(primes, offset, rnd_index, r);

      if ((mmd->flag & GP_OFFSET_UNIFORM_RANDOM_SCALE) && j == 2) {
        float rand_value;
        rand_value = fmodf(r[0] * 2.0f - 1.0f + rand_offset, 1.0f);
        rand_value = fmodf(sin(rand_value * 12.9898f + j * 78.233f) * 43758.5453f, 1.0f);
        copy_v3_fl(rand[j], rand_value);
      }
      else {
        for (int i = 0; i < 3; i++) {
          rand[j][i] = fmodf(r[i] * 2.0f - 1.0f + rand_offset, 1.0f);
          rand[j][i] = fmodf(sin(rand[j][i] * 12.9898f + j * 78.233f) * 43758.5453f, 1.0f);
        }
      }
    }
  }
  else {
    if (is_randomized) {
      const int step = max_ii(mmd->stroke_step, 1);
      const int start_offset = mmd->stroke_start_offset;
      int offset_index;
      int offset_size;
      float offset_factor;
      switch (mmd->mode) {
        case GP_OFFSET_STROKE:
          offset_size = max_ii(BLI_listbase_count(&gpf->strokes), 1);
          offset_index = max_ii(BLI_findindex(&gpf->strokes, gps), 0);
          break;
        case GP_OFFSET_MATERIAL:
          offset_size = max_ii(gpd->totcol, 1);
          offset_index = max_ii(gps->mat_nr, 0);
          break;
        case GP_OFFSET_LAYER:
          offset_size = max_ii(BLI_listbase_count(&gpd->layers), 1);
          offset_index = max_ii(BLI_findindex(&gpd->layers, gpl), 0);
          break;
      }

      offset_factor = ((offset_size - (offset_index / step + start_offset % offset_size) %
                                          offset_size * step % offset_size) -
                       1) /
                      (float)offset_size;
      for (int j = 0; j < 3; j++) {
        for (int i = 0; i < 3; i++) {
          rand[j][i] = offset_factor;
        }
      }
    }
  }
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt = &gps->points[i];
    MDeformVert *dvert = gps->dvert != NULL ? &gps->dvert[i] : NULL;

    /* Verify vertex group. */
    const float weight = get_modifier_point_weight(
        dvert, (mmd->flag & GP_OFFSET_INVERT_VGROUP) != 0, def_nr);
    if (weight < 0.0f) {
      continue;
    }

    /* Calculate Random matrix. */
    if (is_randomized) {
      float mat_rnd[4][4];
      float rnd_loc[3], rnd_rot[3], rnd_scale_weight[3];
      float rnd_scale[3] = {1.0f, 1.0f, 1.0f};

      mul_v3_v3fl(rnd_loc, rand[0], weight);
      mul_v3_v3fl(rnd_rot, rand[1], weight);
      mul_v3_v3fl(rnd_scale_weight, rand[2], weight);

      mul_v3_v3v3(rnd_loc, mmd->rnd_offset, rnd_loc);
      mul_v3_v3v3(rnd_rot, mmd->rnd_rot, rnd_rot);
      madd_v3_v3v3(rnd_scale, mmd->rnd_scale, rnd_scale_weight);

      loc_eul_size_to_mat4(mat_rnd, rnd_loc, rnd_rot, rnd_scale);
      /* Apply randomness matrix. */
      mul_m4_v3(mat_rnd, &pt->x);
    }

    /* Calculate matrix. */
    if (is_general) {
      mul_v3_v3fl(loc, mmd->loc, weight);
      mul_v3_v3fl(rot, mmd->rot, weight);
      mul_v3_v3fl(scale, mmd->scale, weight);
      add_v3_fl(scale, 1.0f);
      loc_eul_size_to_mat4(mat, loc, rot, scale);

      /* Apply scale to thickness. */
      float unit_scale = (fabsf(scale[0]) + fabsf(scale[1]) + fabsf(scale[2])) / 3.0f;
      pt->pressure *= unit_scale;

      mul_m4_v3(mat, &pt->x);
    }
  }
  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gpd, gps);
}

static void bakeModifier(struct Main *UNUSED(bmain),
                         Depsgraph *depsgraph,
                         GpencilModifierData *md,
                         Object *ob)
{
  generic_bake_deform_stroke(depsgraph, md, ob, false, deformStroke);
}

static void updateDepsgraph(GpencilModifierData *UNUSED(md),
                            const ModifierUpdateDepsgraphContext *ctx,
                            const int UNUSED(mode))
{
  DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Offset Modifier");
}

static void foreachIDLink(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  OffsetGpencilModifierData *mmd = (OffsetGpencilModifierData *)md;

  walk(userData, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);
  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "location", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "rotation", 0, NULL, ICON_NONE);
  uiItemR(layout, ptr, "scale", 0, NULL, ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
  uiLayoutSetActive(layout, true);
}

static void empty_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);
  uiLayoutSetPropSep(layout, true);

  gpencil_modifier_panel_end(layout, ptr);
}

static void random_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);
  int mode = RNA_enum_get(ptr, "mode");
  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "mode", 0, NULL, ICON_NONE);

  uiItemR(layout, ptr, "random_offset", 0, IFACE_("Offset"), ICON_NONE);
  uiItemR(layout, ptr, "random_rotation", 0, IFACE_("Rotation"), ICON_NONE);
  uiItemR(layout, ptr, "random_scale", 0, IFACE_("Scale"), ICON_NONE);
  switch (mode) {
    case GP_OFFSET_RANDOM:
      uiItemR(layout, ptr, "use_uniform_random_scale", 0, NULL, ICON_NONE);
      uiItemR(layout, ptr, "seed", 0, NULL, ICON_NONE);
      break;
    case GP_OFFSET_STROKE:
      uiItemR(layout, ptr, "stroke_step", 0, IFACE_("Stroke Step"), ICON_NONE);
      uiItemR(layout, ptr, "stroke_start_offset", 0, IFACE_("Stroke Offset"), ICON_NONE);
      break;
    case GP_OFFSET_MATERIAL:
      uiItemR(layout, ptr, "stroke_step", 0, IFACE_("Material Step"), ICON_NONE);
      uiItemR(layout, ptr, "stroke_start_offset", 0, IFACE_("Material Offset"), ICON_NONE);
      break;
    case GP_OFFSET_LAYER:
      uiItemR(layout, ptr, "stroke_step", 0, IFACE_("Layer Step"), ICON_NONE);
      uiItemR(layout, ptr, "stroke_start_offset", 0, IFACE_("Layer Offset"), ICON_NONE);
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
      region_type, eGpencilModifierType_Offset, empty_panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "general", "General", NULL, panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "randomize", "Advanced", NULL, random_panel_draw, panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", NULL, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Offset = {
    /* name */ N_("Offset"),
    /* structName */ "OffsetGpencilModifierData",
    /* structSize */ sizeof(OffsetGpencilModifierData),
    /* type */ eGpencilModifierTypeType_Gpencil,
    /* flags */ eGpencilModifierTypeFlag_SupportsEditmode,

    /* copyData */ copyData,

    /* deformStroke */ deformStroke,
    /* generateStrokes */ NULL,
    /* bakeModifier */ bakeModifier,
    /* remapTime */ NULL,

    /* initData */ initData,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* panelRegister */ panelRegister,
};
