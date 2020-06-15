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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2011 by Nicholas Bishop.
 */

/** \file
 * \ingroup modifiers
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_math_base.h"
#include "BLI_threads.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_mesh_remesh_voxel.h"
#include "BKE_mesh_runtime.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef WITH_MOD_REMESH
#  include "BLI_math_vector.h"

#  include "dualcon.h"
#endif

static void initData(ModifierData *md)
{
  RemeshModifierData *rmd = (RemeshModifierData *)md;

  rmd->scale = 0.9;
  rmd->depth = 4;
  rmd->hermite_num = 1;
  rmd->flag = MOD_REMESH_FLOOD_FILL;
  rmd->mode = MOD_REMESH_VOXEL;
  rmd->threshold = 1;
  rmd->voxel_size = 0.1f;
  rmd->adaptivity = 0.0f;
}

#ifdef WITH_MOD_REMESH

static void init_dualcon_mesh(DualConInput *input, Mesh *mesh)
{
  memset(input, 0, sizeof(DualConInput));

  input->co = (void *)mesh->mvert;
  input->co_stride = sizeof(MVert);
  input->totco = mesh->totvert;

  input->mloop = (void *)mesh->mloop;
  input->loop_stride = sizeof(MLoop);

  BKE_mesh_runtime_looptri_ensure(mesh);
  input->looptri = (void *)mesh->runtime.looptris.array;
  input->tri_stride = sizeof(MLoopTri);
  input->tottri = mesh->runtime.looptris.len;

  INIT_MINMAX(input->min, input->max);
  BKE_mesh_minmax(mesh, input->min, input->max);
}

/* simple structure to hold the output: a CDDM and two counters to
 * keep track of the current elements */
typedef struct {
  Mesh *mesh;
  int curvert, curface;
} DualConOutput;

/* allocate and initialize a DualConOutput */
static void *dualcon_alloc_output(int totvert, int totquad)
{
  DualConOutput *output;

  if (!(output = MEM_callocN(sizeof(DualConOutput), "DualConOutput"))) {
    return NULL;
  }

  output->mesh = BKE_mesh_new_nomain(totvert, 0, 0, 4 * totquad, totquad);
  return output;
}

static void dualcon_add_vert(void *output_v, const float co[3])
{
  DualConOutput *output = output_v;
  Mesh *mesh = output->mesh;

  BLI_assert(output->curvert < mesh->totvert);

  copy_v3_v3(mesh->mvert[output->curvert].co, co);
  output->curvert++;
}

static void dualcon_add_quad(void *output_v, const int vert_indices[4])
{
  DualConOutput *output = output_v;
  Mesh *mesh = output->mesh;
  MLoop *mloop;
  MPoly *cur_poly;
  int i;

  BLI_assert(output->curface < mesh->totpoly);

  mloop = mesh->mloop;
  cur_poly = &mesh->mpoly[output->curface];

  cur_poly->loopstart = output->curface * 4;
  cur_poly->totloop = 4;
  for (i = 0; i < 4; i++) {
    mloop[output->curface * 4 + i].v = vert_indices[i];
  }

  output->curface++;
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *UNUSED(ctx), Mesh *mesh)
{
  RemeshModifierData *rmd;
  DualConOutput *output;
  DualConInput input;
  Mesh *result;
  DualConFlags flags = 0;
  DualConMode mode = 0;

  rmd = (RemeshModifierData *)md;

  if (rmd->mode == MOD_REMESH_VOXEL) {
    /* OpenVDB modes. */
    if (rmd->voxel_size == 0.0f) {
      return NULL;
    }
    result = BKE_mesh_remesh_voxel_to_mesh_nomain(mesh, rmd->voxel_size, rmd->adaptivity, 0.0f);
  }
  else {
    /* Dualcon modes. */
    init_dualcon_mesh(&input, mesh);

    if (rmd->flag & MOD_REMESH_FLOOD_FILL) {
      flags |= DUALCON_FLOOD_FILL;
    }

    switch (rmd->mode) {
      case MOD_REMESH_CENTROID:
        mode = DUALCON_CENTROID;
        break;
      case MOD_REMESH_MASS_POINT:
        mode = DUALCON_MASS_POINT;
        break;
      case MOD_REMESH_SHARP_FEATURES:
        mode = DUALCON_SHARP_FEATURES;
        break;
      case MOD_REMESH_VOXEL:
        /* Should have been processed before as an OpenVDB operation. */
        BLI_assert(false);
        break;
    }
    /* TODO(jbakker): Dualcon crashes when run in parallel. Could be related to incorrect
     * input data or that the library isn't thread safe. This was identified when changing the task
     * isolations during T76553. */
    static ThreadMutex dualcon_mutex = BLI_MUTEX_INITIALIZER;
    BLI_mutex_lock(&dualcon_mutex);
    output = dualcon(&input,
                     dualcon_alloc_output,
                     dualcon_add_vert,
                     dualcon_add_quad,
                     flags,
                     mode,
                     rmd->threshold,
                     rmd->hermite_num,
                     rmd->scale,
                     rmd->depth);
    BLI_mutex_unlock(&dualcon_mutex);

    result = output->mesh;
    MEM_freeN(output);
  }

  if (rmd->flag & MOD_REMESH_SMOOTH_SHADING) {
    MPoly *mpoly = result->mpoly;
    int i, totpoly = result->totpoly;

    /* Apply smooth shading to output faces */
    for (i = 0; i < totpoly; i++) {
      mpoly[i].flag |= ME_SMOOTH;
    }
  }

  BKE_mesh_copy_settings(result, mesh);
  BKE_mesh_calc_edges(result, true, false);
  result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
  return result;
}

#else /* !WITH_MOD_REMESH */

static Mesh *modifyMesh(ModifierData *UNUSED(md),
                        const ModifierEvalContext *UNUSED(ctx),
                        Mesh *mesh)
{
  return mesh;
}

#endif /* !WITH_MOD_REMESH */

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
#ifdef WITH_MOD_REMESH
  uiLayout *row;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  int mode = RNA_enum_get(&ptr, "mode");

  uiItemR(layout, &ptr, "mode", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  if (mode == MOD_REMESH_VOXEL) {
    uiItemR(layout, &ptr, "voxel_size", 0, NULL, ICON_NONE);
    uiItemR(layout, &ptr, "adaptivity", 0, NULL, ICON_NONE);
  }
  else {
    uiItemR(layout, &ptr, "octree_depth", 0, NULL, ICON_NONE);
    uiItemR(layout, &ptr, "scale", 0, NULL, ICON_NONE);

    if (mode == MOD_REMESH_SHARP_FEATURES) {
      uiItemR(layout, &ptr, "sharpness", 0, NULL, ICON_NONE);
    }

    uiItemR(layout, &ptr, "use_remove_disconnected", 0, NULL, ICON_NONE);
    row = uiLayoutRow(layout, false);
    uiLayoutSetActive(row, RNA_boolean_get(&ptr, "use_remove_disconnected"));
    uiItemR(layout, &ptr, "threshold", 0, NULL, ICON_NONE);
  }
  uiItemR(layout, &ptr, "use_smooth_shade", 0, NULL, ICON_NONE);

  modifier_panel_end(layout, &ptr);

#else  /* WITH_MOD_REMESH */
  uiItemL(layout, IFACE_("Built without Remesh modifier"), ICON_NONE);
  UNUSED_VARS(C);
#endif /* WITH_MOD_REMESH */
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Remesh, panel_draw);
}

ModifierTypeInfo modifierType_Remesh = {
    /* name */ "Remesh",
    /* structName */ "RemeshModifierData",
    /* structSize */ sizeof(RemeshModifierData),
    /* type */ eModifierTypeType_Nonconstructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_SupportsEditmode,

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ NULL,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
    /* blendWrite */ NULL,
    /* blendRead */ NULL,
};
