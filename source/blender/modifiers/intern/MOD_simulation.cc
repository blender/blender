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

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  SimulationModifierData *smd = reinterpret_cast<SimulationModifierData *>(md);
  if (smd->simulation) {
    DEG_add_simulation_relation(ctx->node, smd->simulation, "Accessed Simulation");
  }
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  SimulationModifierData *smd = reinterpret_cast<SimulationModifierData *>(md);
  walk(userData, ob, (ID **)&smd->simulation, IDWALK_CB_USER);
}

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(useRenderParams))
{
  SimulationModifierData *smd = reinterpret_cast<SimulationModifierData *>(md);
  return smd->simulation == nullptr;
}

static const ParticleSimulationState *find_particle_state(SimulationModifierData *smd)
{
  return reinterpret_cast<const ParticleSimulationState *>(
      BKE_simulation_state_try_find_by_name_and_type(
          smd->simulation, smd->data_path, SIM_TYPE_NAME_PARTICLE_SIMULATION));
}

static PointCloud *modifyPointCloud(ModifierData *md,
                                    const ModifierEvalContext *UNUSED(ctx),
                                    PointCloud *input_pointcloud)
{
  SimulationModifierData *smd = reinterpret_cast<SimulationModifierData *>(md);
  const ParticleSimulationState *state = find_particle_state(smd);
  if (state == nullptr) {
    return input_pointcloud;
  }

  PointCloud *pointcloud = BKE_pointcloud_new_for_eval(input_pointcloud, state->tot_particles);
  if (state->tot_particles == 0) {
    return pointcloud;
  }

  const float3 *positions = static_cast<const float3 *>(
      CustomData_get_layer_named(&state->attributes, CD_PROP_FLOAT3, "Position"));
  const float *radii = static_cast<const float *>(
      CustomData_get_layer_named(&state->attributes, CD_PROP_FLOAT, "Radius"));
  memcpy(pointcloud->co, positions, sizeof(float3) * state->tot_particles);

  for (int i = 0; i < state->tot_particles; i++) {
    pointcloud->radius[i] = radii[i];
  }

  return pointcloud;
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiItemR(layout, &ptr, "simulation", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "data_path", 0, NULL, ICON_NONE);

  modifier_panel_end(layout, &ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Simulation, panel_draw);
}

static void blendWrite(BlendWriter *writer, const ModifierData *md)
{
  const SimulationModifierData *smd = reinterpret_cast<const SimulationModifierData *>(md);
  BLO_write_string(writer, smd->data_path);
}

static void blendRead(BlendDataReader *reader, ModifierData *md)
{
  SimulationModifierData *smd = reinterpret_cast<SimulationModifierData *>(md);
  BLO_read_data_address(reader, &smd->data_path);
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
  const SimulationModifierData *smd = reinterpret_cast<const SimulationModifierData *>(md);
  SimulationModifierData *tsmd = reinterpret_cast<SimulationModifierData *>(target);

  BKE_modifier_copydata_generic(md, target, flag);
  if (smd->data_path != nullptr) {
    tsmd->data_path = BLI_strdup(smd->data_path);
  }
}

static void freeData(ModifierData *md)
{
  SimulationModifierData *smd = reinterpret_cast<SimulationModifierData *>(md);
  if (smd->data_path) {
    MEM_freeN(smd->data_path);
  }
}

ModifierTypeInfo modifierType_Simulation = {
    /* name */ "Simulation",
    /* structName */ "SimulationModifierData",
    /* structSize */ sizeof(SimulationModifierData),
    /* type */ eModifierTypeType_None,
    /* flags */ (ModifierTypeFlag)0,

    /* copyData */ copyData,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ NULL,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ modifyPointCloud,
    /* modifyVolume */ NULL,

    /* initData */ NULL,
    /* requiredDataMask */ NULL,
    /* freeData */ freeData,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
    /* blendWrite */ blendWrite,
    /* blendRead */ blendRead,
};
