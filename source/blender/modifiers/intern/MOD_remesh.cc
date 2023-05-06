/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 by Nicholas Bishop. */

/** \file
 * \ingroup modifiers
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_base.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_remesh_voxel.h"
#include "BKE_mesh_runtime.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#include <cstdlib>
#include <cstring>

#ifdef WITH_MOD_REMESH
#  include "BLI_math_vector.h"

#  include "dualcon.h"
#endif

static void initData(ModifierData *md)
{
  RemeshModifierData *rmd = (RemeshModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(rmd, modifier));

  MEMCPY_STRUCT_AFTER(rmd, DNA_struct_default_get(RemeshModifierData), modifier);
}

#ifdef WITH_MOD_REMESH

static void init_dualcon_mesh(DualConInput *input, Mesh *mesh)
{
  memset(input, 0, sizeof(DualConInput));

  input->co = (DualConCo)BKE_mesh_vert_positions(mesh);
  input->co_stride = sizeof(float[3]);
  input->totco = mesh->totvert;

  input->mloop = (DualConLoop)mesh->corner_verts().data();
  input->loop_stride = sizeof(int);

  input->looptri = (DualConTri)mesh->looptris().data();
  input->tri_stride = sizeof(MLoopTri);
  input->tottri = BKE_mesh_runtime_looptri_len(mesh);

  INIT_MINMAX(input->min, input->max);
  BKE_mesh_minmax(mesh, input->min, input->max);
}

/* simple structure to hold the output: a CDDM and two counters to
 * keep track of the current elements */
typedef struct {
  Mesh *mesh;
  float (*vert_positions)[3];
  int *poly_offsets;
  int *corner_verts;
  int curvert, curface;
} DualConOutput;

/* allocate and initialize a DualConOutput */
static void *dualcon_alloc_output(int totvert, int totquad)
{
  DualConOutput *output;

  if (!(output = MEM_cnew<DualConOutput>(__func__))) {
    return nullptr;
  }

  output->mesh = BKE_mesh_new_nomain(totvert, 0, totquad, 4 * totquad);
  output->vert_positions = BKE_mesh_vert_positions_for_write(output->mesh);
  output->poly_offsets = output->mesh->poly_offsets_for_write().data();
  output->corner_verts = output->mesh->corner_verts_for_write().data();

  return output;
}

static void dualcon_add_vert(void *output_v, const float co[3])
{
  DualConOutput *output = static_cast<DualConOutput *>(output_v);

  BLI_assert(output->curvert < output->mesh->totvert);

  copy_v3_v3(output->vert_positions[output->curvert], co);
  output->curvert++;
}

static void dualcon_add_quad(void *output_v, const int vert_indices[4])
{
  DualConOutput *output = static_cast<DualConOutput *>(output_v);
  Mesh *mesh = output->mesh;
  int i;

  BLI_assert(output->curface < mesh->totpoly);
  UNUSED_VARS_NDEBUG(mesh);

  output->poly_offsets[output->curface] = output->curface * 4;
  for (i = 0; i < 4; i++) {
    output->corner_verts[output->curface * 4 + i] = vert_indices[i];
  }

  output->curface++;
}

static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext * /*ctx*/, Mesh *mesh)
{
  RemeshModifierData *rmd;
  DualConOutput *output;
  DualConInput input;
  Mesh *result;
  DualConFlags flags = DualConFlags(0);
  DualConMode mode = DualConMode(0);

  rmd = (RemeshModifierData *)md;

  if (rmd->mode == MOD_REMESH_VOXEL) {
    /* OpenVDB modes. */
    if (rmd->voxel_size == 0.0f) {
      return nullptr;
    }
    result = BKE_mesh_remesh_voxel(mesh, rmd->voxel_size, rmd->adaptivity, 0.0f);
    if (result == nullptr) {
      return nullptr;
    }
  }
  else {
    /* Dualcon modes. */
    init_dualcon_mesh(&input, mesh);

    if (rmd->flag & MOD_REMESH_FLOOD_FILL) {
      flags = DualConFlags(flags | DUALCON_FLOOD_FILL);
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
     * input data or that the library isn't thread safe.
     * This was identified when changing the task isolation's during #76553. */
    static ThreadMutex dualcon_mutex = BLI_MUTEX_INITIALIZER;
    BLI_mutex_lock(&dualcon_mutex);
    output = static_cast<DualConOutput *>(dualcon(&input,
                                                  dualcon_alloc_output,
                                                  dualcon_add_vert,
                                                  dualcon_add_quad,
                                                  flags,
                                                  mode,
                                                  rmd->threshold,
                                                  rmd->hermite_num,
                                                  rmd->scale,
                                                  rmd->depth));
    BLI_mutex_unlock(&dualcon_mutex);
    output->mesh->poly_offsets_for_write().last() = output->mesh->totloop;
    result = output->mesh;
    MEM_freeN(output);
  }

  BKE_mesh_smooth_flag_set(result, rmd->flag & MOD_REMESH_SMOOTH_SHADING);

  BKE_mesh_copy_parameters_for_eval(result, mesh);
  BKE_mesh_calc_edges(result, true, false);
  return result;
}

#else /* !WITH_MOD_REMESH */

static Mesh *modifyMesh(ModifierData * /*md*/, const ModifierEvalContext * /*ctx*/, Mesh *mesh)
{
  return mesh;
}

#endif /* !WITH_MOD_REMESH */

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
#ifdef WITH_MOD_REMESH
  uiLayout *row, *col;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  int mode = RNA_enum_get(ptr, "mode");

  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  if (mode == MOD_REMESH_VOXEL) {
    uiItemR(col, ptr, "voxel_size", 0, nullptr, ICON_NONE);
    uiItemR(col, ptr, "adaptivity", 0, nullptr, ICON_NONE);
  }
  else {
    uiItemR(col, ptr, "octree_depth", 0, nullptr, ICON_NONE);
    uiItemR(col, ptr, "scale", 0, nullptr, ICON_NONE);

    if (mode == MOD_REMESH_SHARP_FEATURES) {
      uiItemR(col, ptr, "sharpness", 0, nullptr, ICON_NONE);
    }

    uiItemR(layout, ptr, "use_remove_disconnected", 0, nullptr, ICON_NONE);
    row = uiLayoutRow(layout, false);
    uiLayoutSetActive(row, RNA_boolean_get(ptr, "use_remove_disconnected"));
    uiItemR(layout, ptr, "threshold", 0, nullptr, ICON_NONE);
  }
  uiItemR(layout, ptr, "use_smooth_shade", 0, nullptr, ICON_NONE);

  modifier_panel_end(layout, ptr);

#else  /* WITH_MOD_REMESH */
  uiItemL(layout, TIP_("Built without Remesh modifier"), ICON_NONE);
#endif /* WITH_MOD_REMESH */
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Remesh, panel_draw);
}

ModifierTypeInfo modifierType_Remesh = {
    /*name*/ N_("Remesh"),
    /*structName*/ "RemeshModifierData",
    /*structSize*/ sizeof(RemeshModifierData),
    /*srna*/ &RNA_RemeshModifier,
    /*type*/ eModifierTypeType_Nonconstructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_REMESH,

    /*copyData*/ BKE_modifier_copydata_generic,

    /*deformVerts*/ nullptr,
    /*deformMatrices*/ nullptr,
    /*deformVertsEM*/ nullptr,
    /*deformMatricesEM*/ nullptr,
    /*modifyMesh*/ modifyMesh,
    /*modifyGeometrySet*/ nullptr,

    /*initData*/ initData,
    /*requiredDataMask*/ nullptr,
    /*freeData*/ nullptr,
    /*isDisabled*/ nullptr,
    /*updateDepsgraph*/ nullptr,
    /*dependsOnTime*/ nullptr,
    /*dependsOnNormals*/ nullptr,
    /*foreachIDLink*/ nullptr,
    /*foreachTexLink*/ nullptr,
    /*freeRuntimeData*/ nullptr,
    /*panelRegister*/ panelRegister,
    /*blendWrite*/ nullptr,
    /*blendRead*/ nullptr,
};
