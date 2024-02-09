/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstdio>

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.hh"
#include "BKE_deform.hh"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"

#include "DEG_depsgraph.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

static void init_data(GpencilModifierData *md)
{
  TextureGpencilModifierData *gpmd = (TextureGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(TextureGpencilModifierData), modifier);
}

static void copy_data(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

/* change stroke uv texture values */
static void deform_stroke(GpencilModifierData *md,
                          Depsgraph * /*depsgraph*/,
                          Object *ob,
                          bGPDlayer *gpl,
                          bGPDframe * /*gpf*/,
                          bGPDstroke *gps)
{
  TextureGpencilModifierData *mmd = (TextureGpencilModifierData *)md;
  const int def_nr = BKE_object_defgroup_name_index(ob, mmd->vgname);
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      1,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_TEX_INVERT_LAYER,
                                      mmd->flag & GP_TEX_INVERT_PASS,
                                      mmd->flag & GP_TEX_INVERT_LAYERPASS,
                                      mmd->flag & GP_TEX_INVERT_MATERIAL))
  {
    return;
  }
  if (ELEM(mmd->mode, FILL, STROKE_AND_FILL)) {
    gps->uv_rotation += mmd->fill_rotation;
    gps->uv_translation[0] += mmd->fill_offset[0];
    gps->uv_translation[1] += mmd->fill_offset[1];
    gps->uv_scale *= mmd->fill_scale;
    BKE_gpencil_stroke_geometry_update(gpd, gps);
  }

  if (ELEM(mmd->mode, STROKE, STROKE_AND_FILL)) {
    float totlen = 1.0f;
    if (mmd->fit_method == GP_TEX_FIT_STROKE) {
      totlen = 0.0f;
      for (int i = 1; i < gps->totpoints; i++) {
        totlen += len_v3v3(&gps->points[i - 1].x, &gps->points[i].x);
      }
    }

    for (int i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      MDeformVert *dvert = gps->dvert != nullptr ? &gps->dvert[i] : nullptr;
      /* Verify point is part of vertex group. */
      float weight = get_modifier_point_weight(
          dvert, (mmd->flag & GP_TEX_INVERT_VGROUP) != 0, def_nr);
      if (weight < 0.0f) {
        continue;
      }

      pt->uv_fac /= totlen;
      pt->uv_fac *= mmd->uv_scale;
      pt->uv_fac += mmd->uv_offset;
      pt->uv_rot += mmd->alignment_rotation;
    }
  }
}

static void bake_modifier(Main * /*bmain*/,
                          Depsgraph *depsgraph,
                          GpencilModifierData *md,
                          Object *ob)
{
  generic_bake_deform_stroke(depsgraph, md, ob, false, deform_stroke);
}

static void foreach_ID_link(GpencilModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  TextureGpencilModifierData *mmd = (TextureGpencilModifierData *)md;

  walk(user_data, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  int mode = RNA_enum_get(ptr, "mode");

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "mode", UI_ITEM_NONE, nullptr, ICON_NONE);

  if (ELEM(mode, STROKE, STROKE_AND_FILL)) {
    col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "fit_method", UI_ITEM_NONE, IFACE_("Stroke Fit Method"), ICON_NONE);
    uiItemR(col, ptr, "uv_offset", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(col, ptr, "alignment_rotation", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(col, ptr, "uv_scale", UI_ITEM_NONE, IFACE_("Scale"), ICON_NONE);
  }

  if (mode == STROKE_AND_FILL) {
    uiItemS(layout);
  }

  if (ELEM(mode, FILL, STROKE_AND_FILL)) {
    col = uiLayoutColumn(layout, false);
    uiItemR(col, ptr, "fill_rotation", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(col, ptr, "fill_offset", UI_ITEM_NONE, IFACE_("Offset"), ICON_NONE);
    uiItemR(col, ptr, "fill_scale", UI_ITEM_NONE, IFACE_("Scale"), ICON_NONE);
  }

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext * /*C*/, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, true);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Texture, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", nullptr, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Texture = {
    /*name*/ N_("TextureMapping"),
    /*struct_name*/ "TextureGpencilModifierData",
    /*struct_size*/ sizeof(TextureGpencilModifierData),
    /*type*/ eGpencilModifierTypeType_Gpencil,
    /*flags*/ eGpencilModifierTypeFlag_SupportsEditmode,

    /*copy_data*/ copy_data,

    /*deform_stroke*/ deform_stroke,
    /*generate_strokes*/ nullptr,
    /*bake_modifier*/ bake_modifier,
    /*remap_time*/ nullptr,

    /*init_data*/ init_data,
    /*free_data*/ nullptr,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*panel_register*/ panel_register,
};
