/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup modifiers
 */

#include <stdio.h>

#include "BLI_utildefines.h"

#include "MOD_modifiertypes.h"

#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

/* We only need to define isDisabled; because it always returns 1,
 * no other functions will be called
 */

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *UNUSED(md),
                       bool UNUSED(userRenderParams))
{
  return true;
}

ModifierTypeInfo modifierType_None = {
    /* name */ "None",
    /* structName */ "ModifierData",
    /* structSize */ sizeof(ModifierData),
    /* srna */ &RNA_Modifier,
    /* type */ eModifierTypeType_None,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs,
    /* icon */ ICON_NONE,

    /* copyData */ NULL,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ NULL,
    /* modifyGeometrySet */ NULL,

    /* initData */ NULL,
    /* requiredDataMask */ NULL,
    /* freeData */ NULL,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ NULL,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};
