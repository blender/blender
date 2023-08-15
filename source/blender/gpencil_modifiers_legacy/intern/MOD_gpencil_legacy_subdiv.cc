/* SPDX-FileCopyrightText: 2017 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstdio>
#include <cstring> /* For #MEMCPY_STRUCT_AFTER. */

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lib_query.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_ui_common.h"
#include "MOD_gpencil_legacy_util.h"

static void init_data(GpencilModifierData *md)
{
  SubdivGpencilModifierData *gpmd = (SubdivGpencilModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(gpmd, modifier));

  MEMCPY_STRUCT_AFTER(gpmd, DNA_struct_default_get(SubdivGpencilModifierData), modifier);
}

static void copy_data(const GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_generic(md, target);
}

/* subdivide stroke to get more control points */
static void deform_stroke(GpencilModifierData *md,
                          Depsgraph * /*depsgraph*/,
                          Object *ob,
                          bGPDlayer *gpl,
                          bGPDframe * /*gpf*/,
                          bGPDstroke *gps)
{
  SubdivGpencilModifierData *mmd = (SubdivGpencilModifierData *)md;
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);

  if (!is_stroke_affected_by_modifier(ob,
                                      mmd->layername,
                                      mmd->material,
                                      mmd->pass_index,
                                      mmd->layer_pass,
                                      2,
                                      gpl,
                                      gps,
                                      mmd->flag & GP_SUBDIV_INVERT_LAYER,
                                      mmd->flag & GP_SUBDIV_INVERT_PASS,
                                      mmd->flag & GP_SUBDIV_INVERT_LAYERPASS,
                                      mmd->flag & GP_SUBDIV_INVERT_MATERIAL))
  {
    return;
  }

  /* For strokes with less than 3 points, only the Simple Subdivision makes sense. */
  short type = gps->totpoints < 3 ? short(GP_SUBDIV_SIMPLE) : mmd->type;

  BKE_gpencil_stroke_subdivide(gpd, gps, mmd->level, type);
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
  SubdivGpencilModifierData *mmd = (SubdivGpencilModifierData *)md;

  walk(user_data, ob, (ID **)&mmd->material, IDWALK_CB_USER);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = gpencil_modifier_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "subdivision_type", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "level", UI_ITEM_NONE, IFACE_("Subdivisions"), ICON_NONE);

  gpencil_modifier_panel_end(layout, ptr);
}

static void mask_panel_draw(const bContext * /*C*/, Panel *panel)
{
  gpencil_modifier_masking_panel_draw(panel, true, false);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = gpencil_modifier_panel_register(
      region_type, eGpencilModifierType_Subdiv, panel_draw);
  gpencil_modifier_subpanel_register(
      region_type, "mask", "Influence", nullptr, mask_panel_draw, panel_type);
}

GpencilModifierTypeInfo modifierType_Gpencil_Subdiv = {
    /*name*/ N_("Subdivide"),
    /*struct_name*/ "SubdivGpencilModifierData",
    /*struct_size*/ sizeof(SubdivGpencilModifierData),
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
