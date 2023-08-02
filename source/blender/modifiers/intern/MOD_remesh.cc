/* SPDX-FileCopyrightText: 2011 by Nicholas Bishop.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
#include "BKE_mesh_remesh_voxel.hh"
#include "BKE_mesh_runtime.hh"
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

static void init_data(ModifierData *md)
{
  RemeshModifierData *rmd = (RemeshModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(rmd, modifier));

  MEMCPY_STRUCT_AFTER(rmd, DNA_struct_default_get(RemeshModifierData), modifier);
}

#ifdef WITH_MOD_REMESH

static void init_dualcon_mesh(DualConInput *input, Mesh *mesh)
{
  memset(input, 0, sizeof(DualConInput));

  input->co = (DualConCo)mesh->vert_positions().data();
  input->co_stride = sizeof(blender::float3);
  input->totco = mesh->totvert;

  input->mloop = (DualConLoop)mesh->corner_verts().data();
  input->loop_stride = sizeof(int);

  input->looptri = (DualConTri)mesh->looptris().data();
  input->tri_stride = sizeof(MLoopTri);
  input->tottri = BKE_mesh_runtime_looptri_len(mesh);

  const blender::Bounds<blender::float3> bounds = *mesh->bounds_min_max();
  copy_v3_v3(input->min, bounds.min);
  copy_v3_v3(input->max, bounds.max);
}

/* simple structure to hold the output: a CDDM and two counters to
 * keep track of the current elements */
struct DualConOutput {
  Mesh *mesh;
  blender::float3 *vert_positions;
  int *face_offsets;
  int *corner_verts;
  int curvert, curface;
};

/* allocate and initialize a DualConOutput */
static void *dualcon_alloc_output(int totvert, int totquad)
{
  DualConOutput *output;

  if (!(output = MEM_cnew<DualConOutput>(__func__))) {
    return nullptr;
  }

  output->mesh = BKE_mesh_new_nomain(totvert, 0, totquad, 4 * totquad);
  output->vert_positions = output->mesh->vert_positions_for_write().data();
  output->face_offsets = output->mesh->face_offsets_for_write().data();
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

  BLI_assert(output->curface < mesh->faces_num);
  UNUSED_VARS_NDEBUG(mesh);

  output->face_offsets[output->curface] = output->curface * 4;
  for (i = 0; i < 4; i++) {
    output->corner_verts[output->curface * 4 + i] = vert_indices[i];
  }

  output->curface++;
}

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext * /*ctx*/, Mesh *mesh)
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
    result = output->mesh;
    MEM_freeN(output);
  }

  BKE_mesh_smooth_flag_set(result, rmd->flag & MOD_REMESH_SMOOTH_SHADING);

  BKE_mesh_copy_parameters_for_eval(result, mesh);
  BKE_mesh_calc_edges(result, true, false);
  return result;
}

#else /* !WITH_MOD_REMESH */

static Mesh *modify_mesh(ModifierData * /*md*/, const ModifierEvalContext * /*ctx*/, Mesh *mesh)
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
    uiItemR(col, ptr, "voxel_size", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(col, ptr, "adaptivity", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  else {
    uiItemR(col, ptr, "octree_depth", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(col, ptr, "scale", UI_ITEM_NONE, nullptr, ICON_NONE);

    if (mode == MOD_REMESH_SHARP_FEATURES) {
      uiItemR(col, ptr, "sharpness", UI_ITEM_NONE, nullptr, ICON_NONE);
    }

    uiItemR(layout, ptr, "use_remove_disconnected", UI_ITEM_NONE, nullptr, ICON_NONE);
    row = uiLayoutRow(layout, false);
    uiLayoutSetActive(row, RNA_boolean_get(ptr, "use_remove_disconnected"));
    uiItemR(layout, ptr, "threshold", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  uiItemR(layout, ptr, "use_smooth_shade", UI_ITEM_NONE, nullptr, ICON_NONE);

  modifier_panel_end(layout, ptr);

#else  /* WITH_MOD_REMESH */
  uiItemL(layout, TIP_("Built without Remesh modifier"), ICON_NONE);
#endif /* WITH_MOD_REMESH */
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Remesh, panel_draw);
}

ModifierTypeInfo modifierType_Remesh = {
    /*idname*/ "Remesh",
    /*name*/ N_("Remesh"),
    /*struct_name*/ "RemeshModifierData",
    /*struct_size*/ sizeof(RemeshModifierData),
    /*srna*/ &RNA_RemeshModifier,
    /*type*/ eModifierTypeType_Nonconstructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_REMESH,

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ nullptr,
    /*free_data*/ nullptr,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ nullptr,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
};
