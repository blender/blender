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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 */

#include <cstring>
#include <iostream>
#include <string>

#include "MEM_guardedalloc.h"

#include "BLI_float3.hh"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_simulation_types.h"

#include "BKE_customdata.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_pointcloud.h"
#include "BKE_screen.h"
#include "BKE_simulation.h"

#include "BLO_read_write.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

using blender::float3;

static void initData(ModifierData *md)
{
  SimulationModifierData *smd = (SimulationModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(smd, modifier));

  MEMCPY_STRUCT_AFTER(smd, DNA_struct_default_get(SimulationModifierData), modifier);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *UNUSED(ctx))
{
  SimulationModifierData *smd = reinterpret_cast<SimulationModifierData *>(md);
  UNUSED_VARS(smd);
}

static void foreachIDLink(ModifierData *md,
                          Object *UNUSED(ob),
                          IDWalkFunc UNUSED(walk),
                          void *UNUSED(userData))
{
  SimulationModifierData *smd = reinterpret_cast<SimulationModifierData *>(md);
  UNUSED_VARS(smd);
}

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(useRenderParams))
{
  SimulationModifierData *smd = reinterpret_cast<SimulationModifierData *>(md);
  UNUSED_VARS(smd);
  return false;
}

static PointCloud *modifyPointCloud(ModifierData *md,
                                    const ModifierEvalContext *UNUSED(ctx),
                                    PointCloud *pointcloud)
{
  SimulationModifierData *smd = reinterpret_cast<SimulationModifierData *>(md);
  UNUSED_VARS(smd);
  return pointcloud;
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiItemL(layout, "This modifier does nothing currently", ICON_INFO);

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Simulation, panel_draw);
}

static void blendWrite(BlendWriter *writer, const ModifierData *md)
{
  const SimulationModifierData *smd = reinterpret_cast<const SimulationModifierData *>(md);
  UNUSED_VARS(smd, writer);
}

static void blendRead(BlendDataReader *reader, ModifierData *md)
{
  SimulationModifierData *smd = reinterpret_cast<SimulationModifierData *>(md);
  UNUSED_VARS(smd, reader);
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
  const SimulationModifierData *smd = reinterpret_cast<const SimulationModifierData *>(md);
  SimulationModifierData *tsmd = reinterpret_cast<SimulationModifierData *>(target);
  UNUSED_VARS(smd, tsmd);

  BKE_modifier_copydata_generic(md, target, flag);
}

static void freeData(ModifierData *md)
{
  SimulationModifierData *smd = reinterpret_cast<SimulationModifierData *>(md);
  UNUSED_VARS(smd);
}

ModifierTypeInfo modifierType_Simulation = {
    /* name */ "Simulation",
    /* structName */ "SimulationModifierData",
    /* structSize */ sizeof(SimulationModifierData),
#ifdef WITH_GEOMETRY_NODES
    /* srna */ &RNA_SimulationModifier,
#else
    /* srna */ &RNA_Modifier,
#endif
    /* type */ eModifierTypeType_None,
    /* flags */ (ModifierTypeFlag)0,
    /* icon */ ICON_PHYSICS, /* TODO: Use correct icon. */

    /* copyData */ copyData,

    /* deformVerts */ nullptr,
    /* deformMatrices */ nullptr,
    /* deformVertsEM */ nullptr,
    /* deformMatricesEM */ nullptr,
    /* modifyMesh */ nullptr,
    /* modifyHair */ nullptr,
    /* modifyPointCloud */ modifyPointCloud,
    /* modifyVolume */ nullptr,

    /* initData */ initData,
    /* requiredDataMask */ nullptr,
    /* freeData */ freeData,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ nullptr,
    /* dependsOnNormals */ nullptr,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ nullptr,
    /* freeRuntimeData */ nullptr,
    /* panelRegister */ panelRegister,
    /* blendWrite */ blendWrite,
    /* blendRead */ blendRead,
};
