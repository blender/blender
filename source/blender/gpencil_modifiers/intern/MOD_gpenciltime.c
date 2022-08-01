/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2018 Blender Foundation. */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_gpencil_modifiertypes.h"
#include "MOD_gpencil_ui_common.h"

static void initData(GpencilModifierData *md)
{
  TimeGpencilModifierData *gpmd = (TimeGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(TimeGpencilModifierData), modifier);
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

static int remapTime(struct GpencilModifierData *md,
                     struct Depsgraph *UNUSED(depsgraph),
                     struct Scene *scene,
                     struct Object *UNUSED(ob),
                     struct bGPDlayer *gpl,
                     int cfra)
{
  TimeGpencilModifierData *mmd = (TimeGpencilModifierData *)md;
  const bool custom = mmd->flag & GP_TIME_CUSTOM_RANGE;
  const bool invgpl = mmd->flag & GP_TIME_INVERT_LAYER;
  const bool invpass = mmd->flag & GP_TIME_INVERT_LAYERPASS;
  int sfra = custom ? mmd->sfra : scene->r.sfra;
  int efra = custom ? mmd->efra : scene->r.efra;
  int offset = mmd->offset;
  int nfra = 0;
  CLAMP_MIN(sfra, 0);
  CLAMP_MIN(efra, 0);

  if (offset < 0) {
    offset = abs(efra - sfra + offset + 1);
  }
  /* Avoid inverse ranges. */
  if (efra <= sfra) {
    return cfra;
  }

  /* omit if filter by layer */
  if (mmd->layername[0] != '\0') {
    if (invgpl == false) {
      if (!STREQ(mmd->layername, gpl->info)) {
        return cfra;
      }
    }
    else {
      if (STREQ(mmd->layername, gpl->info)) {
        return cfra;
      }
    }
  }
  /* verify pass */
  if (mmd->layer_pass > 0) {
    if (invpass == false) {
      if (gpl->pass_index != mmd->layer_pass) {
        return cfra;
      }
    }
    else {
      if (gpl->pass_index == mmd->layer_pass) {
        return cfra;
      }
    }
  }

  /* apply frame scale */
  cfra *= mmd->frame_scale;

  /* if fix mode, return predefined frame number */
  if (mmd->mode == GP_TIME_MODE_FIX) {
    return offset;
  }

  if (mmd->mode == GP_TIME_MODE_NORMAL) {
    if ((mmd->flag & GP_TIME_KEEP_LOOP) == 0) {
      nfra = cfra + sfra + offset - 1 < efra ? cfra + sfra + offset - 1 : efra;
    }
    else {
      nfra = (offset + cfra - 1) % (efra - sfra + 1) + sfra;
    }
  }
  if (mmd->mode == GP_TIME_MODE_REVERSE) {
    if ((mmd->flag & GP_TIME_KEEP_LOOP) == 0) {
      nfra = efra - cfra - offset > sfra ? efra - cfra - offset + 1 : sfra;
    }
    else {
      nfra = (efra + 1 - (cfra + offset - 1) % (efra - sfra + 1)) - 1;
    }
  }

  if (mmd->mode == GP_TIME_MODE_PINGPONG) {
    if ((mmd->flag & GP_TIME_KEEP_LOOP) == 0) {
      if (((int)(cfra + offset - 1) / (efra - sfra)) % (2)) {
        nfra = efra - (cfra + offset - 1) % (efra - sfra);
      }
      else {
        nfra = sfra + (cfra + offset - 1) % (efra - sfra);
      }
      if (cfra > (efra - sfra) * 2) {
        nfra = sfra + offset;
      }
    }
    else {

      if (((int)(cfra + offset - 1) / (efra - sfra)) % (2)) {
        nfra = efra - (cfra + offset - 1) % (efra - sfra);
      }
      else {
        nfra = sfra + (cfra + offset - 1) % (efra - sfra);
      }
    }
  }

  return nfra;
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *row, *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  int mode = RNA_enum_get(ptr, "mode");

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "mode", 0, NULL, ICON_NONE);

  col = uiLayoutColumn(layout, false);

  const char *text = (mode == GP_TIME_MODE_FIX) ? IFACE_("Frame") : IFACE_("Frame Offset");
  uiItemR(col, ptr, "offset", 0, text, ICON_NONE);

  row = uiLayoutRow(col, false);
  uiLayoutSetActive(row, mode != GP_TIME_MODE_FIX);
  uiItemR(row, ptr, "frame_scale", 0, IFACE_("Scale"), ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiLayoutSetActive(row, mode != GP_TIME_MODE_FIX);
  uiItemR(row, ptr, "use_keep_loop", 0, NULL, ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
}

static void custom_range_header_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  int mode = RNA_enum_get(ptr, "mode");

  uiLayoutSetActive(layout, mode != GP_TIME_MODE_FIX);

  uiItemR(layout, ptr, "use_custom_frame_range", 0, NULL, ICON_NONE);
}

static void custom_range_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, NULL);

  int mode = RNA_enum_get(ptr, "mode");

  uiLayoutSetPropSep(layout, true);

  uiLayoutSetActive(
      layout, (mode != GP_TIME_MODE_FIX) && (RNA_boolean_get(ptr, "use_custom_frame_range")));

  col = uiLayoutColumn(layout, true);
  uiItemR(col, ptr, "frame_start", 0, IFACE_("Frame Start"), ICON_NONE);
  uiItemR(col, ptr, "frame_end", 0, IFACE_("End"), ICON_NONE);
}

static void mask_panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, false, false);
}

static void panelRegister(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Time, panel_draw);
  gpencil_modifier_subpanel_register(region_type,
                                     "custom_range",
                                     "",
                                     custom_range_header_draw,
                                     custom_range_panel_draw,
                                     panel_type);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", NULL, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Time = {
    /* name */ N_("TimeOffset"),
    /* structName */ "TimeGpencilModifierData",
    /* structSize */ sizeof(TimeGpencilModifierData),
    /* type */ eGpencilModifierTypeType_Gpencil,
    /* flags */ eGpencilModifierTypeFlag_NoApply,

    /* copyData */ copyData,

    /* deformStroke */ NULL,
    /* generateStrokes */ NULL,
    /* bakeModifier */ NULL,
    /* remapTime */ remapTime,

    /* initData */ initData,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* panelRegister */ panelRegister,
};
