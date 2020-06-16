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
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_math.h"
#include "BLI_task.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BLO_read_write.h"

#include "RNA_access.h"

#include "MEM_guardedalloc.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_ui_common.h"
#include "MOD_util.h"

#ifdef __SSE2__
#  include <emmintrin.h>
#endif

static void initData(ModifierData *md)
{
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;

  mmd->gridsize = 5;
}

static void freeData(ModifierData *md)
{
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;

  if (mmd->bindinfluences) {
    MEM_freeN(mmd->bindinfluences);
  }
  if (mmd->bindoffsets) {
    MEM_freeN(mmd->bindoffsets);
  }
  if (mmd->bindcagecos) {
    MEM_freeN(mmd->bindcagecos);
  }
  if (mmd->dyngrid) {
    MEM_freeN(mmd->dyngrid);
  }
  if (mmd->dyninfluences) {
    MEM_freeN(mmd->dyninfluences);
  }
  if (mmd->dynverts) {
    MEM_freeN(mmd->dynverts);
  }
  if (mmd->bindweights) {
    MEM_freeN(mmd->bindweights); /* deprecated */
  }
  if (mmd->bindcos) {
    MEM_freeN(mmd->bindcos); /* deprecated */
  }
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
  const MeshDeformModifierData *mmd = (const MeshDeformModifierData *)md;
  MeshDeformModifierData *tmmd = (MeshDeformModifierData *)target;

  BKE_modifier_copydata_generic(md, target, flag);

  if (mmd->bindinfluences) {
    tmmd->bindinfluences = MEM_dupallocN(mmd->bindinfluences);
  }
  if (mmd->bindoffsets) {
    tmmd->bindoffsets = MEM_dupallocN(mmd->bindoffsets);
  }
  if (mmd->bindcagecos) {
    tmmd->bindcagecos = MEM_dupallocN(mmd->bindcagecos);
  }
  if (mmd->dyngrid) {
    tmmd->dyngrid = MEM_dupallocN(mmd->dyngrid);
  }
  if (mmd->dyninfluences) {
    tmmd->dyninfluences = MEM_dupallocN(mmd->dyninfluences);
  }
  if (mmd->dynverts) {
    tmmd->dynverts = MEM_dupallocN(mmd->dynverts);
  }
  if (mmd->bindweights) {
    tmmd->bindweights = MEM_dupallocN(mmd->bindweights); /* deprecated */
  }
  if (mmd->bindcos) {
    tmmd->bindcos = MEM_dupallocN(mmd->bindcos); /* deprecated */
  }
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;

  /* ask for vertexgroups if we need them */
  if (mmd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(useRenderParams))
{
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the mesh is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */
  return !mmd->object || mmd->object->type != OB_MESH;
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;

  walk(userData, ob, &mmd->object, IDWALK_CB_NOP);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;
  if (mmd->object != NULL) {
    DEG_add_object_relation(ctx->node, mmd->object, DEG_OB_COMP_TRANSFORM, "Mesh Deform Modifier");
    DEG_add_object_relation(ctx->node, mmd->object, DEG_OB_COMP_GEOMETRY, "Mesh Deform Modifier");
  }
  /* We need own transformation as well. */
  DEG_add_modifier_to_transform_relation(ctx->node, "Mesh Deform Modifier");
}

static float meshdeform_dynamic_bind(MeshDeformModifierData *mmd, float (*dco)[3], float vec[3])
{
  MDefCell *cell;
  MDefInfluence *inf;
  float gridvec[3], dvec[3], ivec[3], wx, wy, wz;
  float weight, cageweight, totweight, *cageco;
  int i, j, a, x, y, z, size;
#ifdef __SSE2__
  __m128 co = _mm_setzero_ps();
#else
  float co[3] = {0.0f, 0.0f, 0.0f};
#endif

  totweight = 0.0f;
  size = mmd->dyngridsize;

  for (i = 0; i < 3; i++) {
    gridvec[i] = (vec[i] - mmd->dyncellmin[i] - mmd->dyncellwidth * 0.5f) / mmd->dyncellwidth;
    ivec[i] = (int)gridvec[i];
    dvec[i] = gridvec[i] - ivec[i];
  }

  for (i = 0; i < 8; i++) {
    if (i & 1) {
      x = ivec[0] + 1;
      wx = dvec[0];
    }
    else {
      x = ivec[0];
      wx = 1.0f - dvec[0];
    }

    if (i & 2) {
      y = ivec[1] + 1;
      wy = dvec[1];
    }
    else {
      y = ivec[1];
      wy = 1.0f - dvec[1];
    }

    if (i & 4) {
      z = ivec[2] + 1;
      wz = dvec[2];
    }
    else {
      z = ivec[2];
      wz = 1.0f - dvec[2];
    }

    CLAMP(x, 0, size - 1);
    CLAMP(y, 0, size - 1);
    CLAMP(z, 0, size - 1);

    a = x + y * size + z * size * size;
    weight = wx * wy * wz;

    cell = &mmd->dyngrid[a];
    inf = mmd->dyninfluences + cell->offset;
    for (j = 0; j < cell->totinfluence; j++, inf++) {
      cageco = dco[inf->vertex];
      cageweight = weight * inf->weight;
#ifdef __SSE2__
      {
        __m128 cageweight_r = _mm_set1_ps(cageweight);
        /* This will load one extra element, this is ok because
         * we ignore that part of register anyway.
         */
        __m128 cageco_r = _mm_loadu_ps(cageco);
        co = _mm_add_ps(co, _mm_mul_ps(cageco_r, cageweight_r));
      }
#else
      co[0] += cageweight * cageco[0];
      co[1] += cageweight * cageco[1];
      co[2] += cageweight * cageco[2];
#endif
      totweight += cageweight;
    }
  }

#ifdef __SSE2__
  copy_v3_v3(vec, (float *)&co);
#else
  copy_v3_v3(vec, co);
#endif

  return totweight;
}

typedef struct MeshdeformUserdata {
  /*const*/ MeshDeformModifierData *mmd;
  const MDeformVert *dvert;
  /*const*/ float (*dco)[3];
  int defgrp_index;
  float (*vertexCos)[3];
  float (*cagemat)[4];
  float (*icagemat)[3];
} MeshdeformUserdata;

static void meshdeform_vert_task(void *__restrict userdata,
                                 const int iter,
                                 const TaskParallelTLS *__restrict UNUSED(tls))
{
  MeshdeformUserdata *data = userdata;
  /*const*/ MeshDeformModifierData *mmd = data->mmd;
  const MDeformVert *dvert = data->dvert;
  const int defgrp_index = data->defgrp_index;
  const int *offsets = mmd->bindoffsets;
  const MDefInfluence *__restrict influences = mmd->bindinfluences;
  /*const*/ float(*__restrict dco)[3] = data->dco;
  float(*vertexCos)[3] = data->vertexCos;
  float co[3];
  float weight, totweight, fac = 1.0f;

  if (mmd->flag & MOD_MDEF_DYNAMIC_BIND) {
    if (!mmd->dynverts[iter]) {
      return;
    }
  }

  if (dvert) {
    fac = BKE_defvert_find_weight(&dvert[iter], defgrp_index);

    if (mmd->flag & MOD_MDEF_INVERT_VGROUP) {
      fac = 1.0f - fac;
    }

    if (fac <= 0.0f) {
      return;
    }
  }

  if (mmd->flag & MOD_MDEF_DYNAMIC_BIND) {
    /* transform coordinate into cage's local space */
    mul_v3_m4v3(co, data->cagemat, vertexCos[iter]);
    totweight = meshdeform_dynamic_bind(mmd, dco, co);
  }
  else {
    totweight = 0.0f;
    zero_v3(co);
    int start = offsets[iter];
    int end = offsets[iter + 1];

    for (int a = start; a < end; a++) {
      weight = influences[a].weight;
      madd_v3_v3fl(co, dco[influences[a].vertex], weight);
      totweight += weight;
    }
  }

  if (totweight > 0.0f) {
    mul_v3_fl(co, fac / totweight);
    mul_m3_v3(data->icagemat, co);
    if (G.debug_value != 527) {
      add_v3_v3(vertexCos[iter], co);
    }
    else {
      copy_v3_v3(vertexCos[iter], co);
    }
  }
}

static void meshdeformModifier_do(ModifierData *md,
                                  const ModifierEvalContext *ctx,
                                  Mesh *mesh,
                                  float (*vertexCos)[3],
                                  int numVerts)
{
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;
  Object *ob = ctx->object;

  Mesh *cagemesh;
  MDeformVert *dvert = NULL;
  float imat[4][4], cagemat[4][4], iobmat[4][4], icagemat[3][3], cmat[4][4];
  float co[3], (*dco)[3] = NULL, (*bindcagecos)[3];
  int a, totvert, totcagevert, defgrp_index;
  float(*cagecos)[3] = NULL;
  MeshdeformUserdata data;

  static int recursive_bind_sentinel = 0;

  if (mmd->object == NULL || (mmd->bindcagecos == NULL && mmd->bindfunc == NULL)) {
    return;
  }

  /* Get cage mesh.
   *
   * Only do this is the target object is in edit mode by itself, meaning
   * we don't allow linked edit meshes here.
   * This is because editbmesh_get_mesh_cage_and_final() might easily
   * conflict with the thread which evaluates object which is in the edit
   * mode for this mesh.
   *
   * We'll support this case once granular dependency graph is landed.
   */
  Object *ob_target = mmd->object;
  cagemesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(ob_target, false);
  if (cagemesh == NULL) {
    BKE_modifier_set_error(md, "Cannot get mesh from cage object");
    return;
  }

  /* compute matrices to go in and out of cage object space */
  invert_m4_m4(imat, ob_target->obmat);
  mul_m4_m4m4(cagemat, imat, ob->obmat);
  mul_m4_m4m4(cmat, mmd->bindmat, cagemat);
  invert_m4_m4(iobmat, cmat);
  copy_m3_m4(icagemat, iobmat);

  /* bind weights if needed */
  if (!mmd->bindcagecos) {
    /* progress bar redraw can make this recursive .. */
    if (!DEG_is_active(ctx->depsgraph)) {
      BKE_modifier_set_error(md, "Attempt to bind from inactive dependency graph");
      goto finally;
    }
    if (!recursive_bind_sentinel) {
      recursive_bind_sentinel = 1;
      mmd->bindfunc(mmd, cagemesh, (float *)vertexCos, numVerts, cagemat);
      recursive_bind_sentinel = 0;
    }

    goto finally;
  }

  /* verify we have compatible weights */
  totvert = numVerts;
  totcagevert = cagemesh->totvert;

  if (mmd->totvert != totvert) {
    BKE_modifier_set_error(md, "Verts changed from %d to %d", mmd->totvert, totvert);
    goto finally;
  }
  else if (mmd->totcagevert != totcagevert) {
    BKE_modifier_set_error(md, "Cage verts changed from %d to %d", mmd->totcagevert, totcagevert);
    goto finally;
  }
  else if (mmd->bindcagecos == NULL) {
    BKE_modifier_set_error(md, "Bind data missing");
    goto finally;
  }

  /* setup deformation data */
  cagecos = BKE_mesh_vert_coords_alloc(cagemesh, NULL);
  bindcagecos = (float(*)[3])mmd->bindcagecos;

  /* We allocate 1 element extra to make it possible to
   * load the values to SSE registers, which are float4.
   */
  dco = MEM_calloc_arrayN((totcagevert + 1), sizeof(*dco), "MDefDco");
  zero_v3(dco[totcagevert]);
  for (a = 0; a < totcagevert; a++) {
    /* get cage vertex in world space with binding transform */
    copy_v3_v3(co, cagecos[a]);

    if (G.debug_value != 527) {
      mul_m4_v3(mmd->bindmat, co);
      /* compute difference with world space bind coord */
      sub_v3_v3v3(dco[a], co, bindcagecos[a]);
    }
    else {
      copy_v3_v3(dco[a], co);
    }
  }

  MOD_get_vgroup(ob, mesh, mmd->defgrp_name, &dvert, &defgrp_index);

  /* Initialize data to be pass to the for body function. */
  data.mmd = mmd;
  data.dvert = dvert;
  data.dco = dco;
  data.defgrp_index = defgrp_index;
  data.vertexCos = vertexCos;
  data.cagemat = cagemat;
  data.icagemat = icagemat;

  /* Do deformation. */
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.min_iter_per_thread = 16;
  BLI_task_parallel_range(0, totvert, &data, meshdeform_vert_task, &settings);

finally:
  MEM_SAFE_FREE(dco);
  MEM_SAFE_FREE(cagecos);
}

static void deformVerts(ModifierData *md,
                        const ModifierEvalContext *ctx,
                        Mesh *mesh,
                        float (*vertexCos)[3],
                        int numVerts)
{
  Mesh *mesh_src = MOD_deform_mesh_eval_get(ctx->object, NULL, mesh, NULL, numVerts, false, false);

  MOD_previous_vcos_store(md, vertexCos); /* if next modifier needs original vertices */

  meshdeformModifier_do(md, ctx, mesh_src, vertexCos, numVerts);

  if (!ELEM(mesh_src, NULL, mesh)) {
    BKE_id_free(NULL, mesh_src);
  }
}

static void deformVertsEM(ModifierData *md,
                          const ModifierEvalContext *ctx,
                          struct BMEditMesh *editData,
                          Mesh *mesh,
                          float (*vertexCos)[3],
                          int numVerts)
{
  Mesh *mesh_src = MOD_deform_mesh_eval_get(
      ctx->object, editData, mesh, NULL, numVerts, false, false);

  /* TODO(Campbell): use edit-mode data only (remove this line). */
  if (mesh_src != NULL) {
    BKE_mesh_wrapper_ensure_mdata(mesh_src);
  }

  meshdeformModifier_do(md, ctx, mesh_src, vertexCos, numVerts);

  if (!ELEM(mesh_src, NULL, mesh)) {
    BKE_id_free(NULL, mesh_src);
  }
}

#define MESHDEFORM_MIN_INFLUENCE 0.00001f

void BKE_modifier_mdef_compact_influences(ModifierData *md)
{
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;
  float weight, *weights, totweight;
  int totinfluence, totvert, totcagevert, a, b;

  weights = mmd->bindweights;
  if (!weights) {
    return;
  }

  totvert = mmd->totvert;
  totcagevert = mmd->totcagevert;

  /* count number of influences above threshold */
  for (b = 0; b < totvert; b++) {
    for (a = 0; a < totcagevert; a++) {
      weight = weights[a + b * totcagevert];

      if (weight > MESHDEFORM_MIN_INFLUENCE) {
        mmd->totinfluence++;
      }
    }
  }

  /* allocate bind influences */
  mmd->bindinfluences = MEM_calloc_arrayN(
      mmd->totinfluence, sizeof(MDefInfluence), "MDefBindInfluence");
  mmd->bindoffsets = MEM_calloc_arrayN((totvert + 1), sizeof(int), "MDefBindOffset");

  /* write influences */
  totinfluence = 0;

  for (b = 0; b < totvert; b++) {
    mmd->bindoffsets[b] = totinfluence;
    totweight = 0.0f;

    /* sum total weight */
    for (a = 0; a < totcagevert; a++) {
      weight = weights[a + b * totcagevert];

      if (weight > MESHDEFORM_MIN_INFLUENCE) {
        totweight += weight;
      }
    }

    /* assign weights normalized */
    for (a = 0; a < totcagevert; a++) {
      weight = weights[a + b * totcagevert];

      if (weight > MESHDEFORM_MIN_INFLUENCE) {
        mmd->bindinfluences[totinfluence].weight = weight / totweight;
        mmd->bindinfluences[totinfluence].vertex = a;
        totinfluence++;
      }
    }
  }

  mmd->bindoffsets[b] = totinfluence;

  /* free */
  MEM_freeN(mmd->bindweights);
  mmd->bindweights = NULL;
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  bool is_bound = RNA_boolean_get(&ptr, "is_bound");

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, true);
  uiLayoutSetEnabled(col, !is_bound);
  uiItemR(col, &ptr, "object", 0, NULL, ICON_NONE);

  modifier_vgroup_ui(layout, &ptr, &ob_ptr, "vertex_group", "invert_vertex_group", NULL);

  col = uiLayoutColumn(layout, false);
  uiLayoutSetEnabled(col, !is_bound);
  uiItemR(col, &ptr, "precision", 0, NULL, ICON_NONE);
  uiItemR(col, &ptr, "use_dynamic_bind", 0, NULL, ICON_NONE);

  uiItemO(layout,
          is_bound ? IFACE_("Unbind") : IFACE_("Bind"),
          ICON_NONE,
          "OBJECT_OT_meshdeform_bind");

  modifier_panel_end(layout, &ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_MeshDeform, panel_draw);
}

static void blendWrite(BlendWriter *writer, const ModifierData *md)
{
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;
  int size = mmd->dyngridsize;

  BLO_write_struct_array(writer, MDefInfluence, mmd->totinfluence, mmd->bindinfluences);
  BLO_write_int32_array(writer, mmd->totvert + 1, mmd->bindoffsets);
  BLO_write_float3_array(writer, mmd->totcagevert, mmd->bindcagecos);
  BLO_write_struct_array(writer, MDefCell, size * size * size, mmd->dyngrid);
  BLO_write_struct_array(writer, MDefInfluence, mmd->totinfluence, mmd->dyninfluences);
  BLO_write_int32_array(writer, mmd->totvert, mmd->dynverts);
}

static void blendRead(BlendDataReader *reader, ModifierData *md)
{
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;

  BLO_read_data_address(reader, &mmd->bindinfluences);
  BLO_read_int32_array(reader, mmd->totvert + 1, &mmd->bindoffsets);
  BLO_read_float3_array(reader, mmd->totcagevert, &mmd->bindcagecos);
  BLO_read_data_address(reader, &mmd->dyngrid);
  BLO_read_data_address(reader, &mmd->dyninfluences);
  BLO_read_int32_array(reader, mmd->totvert, &mmd->dynverts);

  /* Deprecated storage. */
  BLO_read_float_array(reader, mmd->totvert, &mmd->bindweights);
  BLO_read_float3_array(reader, mmd->totcagevert, &mmd->bindcos);
}

ModifierTypeInfo modifierType_MeshDeform = {
    /* name */ "MeshDeform",
    /* structName */ "MeshDeformModifierData",
    /* structSize */ sizeof(MeshDeformModifierData),
    /* type */ eModifierTypeType_OnlyDeform,
    /* flags */ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_SupportsEditmode,

    /* copyData */ copyData,

    /* deformVerts */ deformVerts,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ deformVertsEM,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ NULL,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ freeData,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
    /* blendWrite */ blendWrite,
    /* blendRead */ blendRead,
};
