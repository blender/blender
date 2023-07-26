/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "MOD_modifiertypes.hh"

#include "UI_resources.h"

#include "RNA_prototypes.h"

/* We only need to define isDisabled; because it always returns 1,
 * no other functions will be called
 */

static bool isDisabled(const Scene * /*scene*/, ModifierData * /*md*/, bool /*useRenderParams*/)
{
  return true;
}

ModifierTypeInfo modifierType_None = {
    /*idname*/ "None",
    /*name*/ "None",
    /*structName*/ "ModifierData",
    /*structSize*/ sizeof(ModifierData),
    /*srna*/ &RNA_Modifier,
    /*type*/ eModifierTypeType_None,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs,
    /*icon*/ ICON_NONE,

    /*copyData*/ nullptr,

    /*deformVerts*/ nullptr,
    /*deformMatrices*/ nullptr,
    /*deformVertsEM*/ nullptr,
    /*deformMatricesEM*/ nullptr,
    /*modifyMesh*/ nullptr,
    /*modifyGeometrySet*/ nullptr,

    /*initData*/ nullptr,
    /*requiredDataMask*/ nullptr,
    /*freeData*/ nullptr,
    /*isDisabled*/ isDisabled,
    /*updateDepsgraph*/ nullptr,
    /*dependsOnTime*/ nullptr,
    /*dependsOnNormals*/ nullptr,
    /*foreachIDLink*/ nullptr,
    /*foreachTexLink*/ nullptr,
    /*freeRuntimeData*/ nullptr,
    /*panelRegister*/ nullptr,
    /*blendWrite*/ nullptr,
    /*blendRead*/ nullptr,
};
