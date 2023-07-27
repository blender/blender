/* SPDX-FileCopyrightText: 2017 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shader_fx
 */

#include <cstdio>

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "DNA_screen_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "FX_shader_types.h"
#include "FX_ui_common.h"

static void init_data(ShaderFxData *fx)
{
  BlurShaderFxData *gpfx = (BlurShaderFxData *)fx;
  copy_v2_fl(gpfx->radius, 50.0f);
  gpfx->samples = 8;
  gpfx->rotation = 0.0f;
}

static void copy_data(const ShaderFxData *md, ShaderFxData *target)
{
  BKE_shaderfx_copydata_generic(md, target);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = shaderfx_panel_get_property_pointers(panel, nullptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "samples", 0, nullptr, ICON_NONE);

  uiItemR(layout, ptr, "use_dof_mode", 0, IFACE_("Use Depth of Field"), ICON_NONE);
  col = uiLayoutColumn(layout, false);
  uiLayoutSetActive(col, !RNA_boolean_get(ptr, "use_dof_mode"));
  uiItemR(col, ptr, "size", 0, nullptr, ICON_NONE);
  uiItemR(col, ptr, "rotation", 0, nullptr, ICON_NONE);

  shaderfx_panel_end(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  shaderfx_panel_register(region_type, eShaderFxType_Blur, panel_draw);
}

ShaderFxTypeInfo shaderfx_Type_Blur = {
    /*name*/ N_("Blur"),
    /*struct_name*/ "BlurShaderFxData",
    /*struct_size*/ sizeof(BlurShaderFxData),
    /*type*/ eShaderFxType_GpencilType,
    /*flags*/ ShaderFxTypeFlag(0),

    /*copy_data*/ copy_data,

    /*init_data*/ init_data,
    /*free_data*/ nullptr,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*foreach_ID_link*/ nullptr,
    /*panel_register*/ panel_register,
};
