/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_array.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_simd.hh"
#include "BLI_task.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_deform.hh"
#include "BKE_lib_query.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_modifier.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLO_read_write.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "MEM_guardedalloc.h"

#include "DEG_depsgraph.hh"

#include "MOD_ui_common.hh"
#include "MOD_util.hh"

static void init_data(ModifierData *md)
{
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(mmd, modifier));

  MEMCPY_STRUCT_AFTER(mmd, DNA_struct_default_get(MeshDeformModifierData), modifier);
}

static void free_data(ModifierData *md)
{
  using namespace blender;
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;

  implicit_sharing::free_shared_data(&mmd->bindinfluences, &mmd->bindinfluences_sharing_info);
  implicit_sharing::free_shared_data(&mmd->bindoffsets, &mmd->bindoffsets_sharing_info);
  implicit_sharing::free_shared_data(&mmd->bindcagecos, &mmd->bindcagecos_sharing_info);
  implicit_sharing::free_shared_data(&mmd->dyngrid, &mmd->dyngrid_sharing_info);
  implicit_sharing::free_shared_data(&mmd->dyninfluences, &mmd->dyninfluences_sharing_info);
  implicit_sharing::free_shared_data(&mmd->dynverts, &mmd->dynverts_sharing_info);
  if (mmd->bindweights) {
    MEM_freeN(mmd->bindweights); /* deprecated */
  }
  if (mmd->bindcos) {
    MEM_freeN(mmd->bindcos); /* deprecated */
  }
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  using namespace blender;
  const MeshDeformModifierData *mmd = (const MeshDeformModifierData *)md;
  MeshDeformModifierData *tmmd = (MeshDeformModifierData *)target;

  BKE_modifier_copydata_generic(md, target, flag);

  implicit_sharing::copy_shared_pointer(mmd->bindinfluences,
                                        mmd->bindinfluences_sharing_info,
                                        &tmmd->bindinfluences,
                                        &tmmd->bindinfluences_sharing_info);
  implicit_sharing::copy_shared_pointer(mmd->bindoffsets,
                                        mmd->bindoffsets_sharing_info,
                                        &tmmd->bindoffsets,
                                        &tmmd->bindoffsets_sharing_info);
  implicit_sharing::copy_shared_pointer(mmd->bindcagecos,
                                        mmd->bindcagecos_sharing_info,
                                        &tmmd->bindcagecos,
                                        &tmmd->bindcagecos_sharing_info);
  implicit_sharing::copy_shared_pointer(
      mmd->dyngrid, mmd->dyngrid_sharing_info, &tmmd->dyngrid, &tmmd->dyngrid_sharing_info);
  implicit_sharing::copy_shared_pointer(mmd->dyninfluences,
                                        mmd->dyninfluences_sharing_info,
                                        &tmmd->dyninfluences,
                                        &tmmd->dyninfluences_sharing_info);
  implicit_sharing::copy_shared_pointer(
      mmd->dynverts, mmd->dynverts_sharing_info, &tmmd->dynverts, &tmmd->dynverts_sharing_info);
  if (mmd->bindweights) {
    tmmd->bindweights = static_cast<float *>(MEM_dupallocN(mmd->bindweights)); /* deprecated */
  }
  if (mmd->bindcos) {
    tmmd->bindcos = static_cast<float *>(MEM_dupallocN(mmd->bindcos)); /* deprecated */
  }
}

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;

  /* Ask for vertex-groups if we need them. */
  if (mmd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
{
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the mesh is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */
  return !mmd->object || mmd->object->type != OB_MESH;
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;

  walk(user_data, ob, (ID **)&mmd->object, IDWALK_CB_NOP);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;
  if (mmd->object != nullptr) {
    DEG_add_object_relation(ctx->node, mmd->object, DEG_OB_COMP_TRANSFORM, "Mesh Deform Modifier");
    DEG_add_object_relation(ctx->node, mmd->object, DEG_OB_COMP_GEOMETRY, "Mesh Deform Modifier");
  }
  /* We need our own transformation as well. */
  DEG_add_depends_on_transform_relation(ctx->node, "Mesh Deform Modifier");
}

static float meshdeform_dynamic_bind(MeshDeformModifierData *mmd, float (*dco)[3], float vec[3])
{
  MDefCell *cell;
  MDefInfluence *inf;
  float gridvec[3], dvec[3], ivec[3], wx, wy, wz;
  float weight, cageweight, totweight, *cageco;
  int i, j, a, x, y, z, size;
#if BLI_HAVE_SSE2
  __m128 co = _mm_setzero_ps();
#else
  float co[3] = {0.0f, 0.0f, 0.0f};
#endif

  totweight = 0.0f;
  size = mmd->dyngridsize;

  for (i = 0; i < 3; i++) {
    gridvec[i] = (vec[i] - mmd->dyncellmin[i] - mmd->dyncellwidth * 0.5f) / mmd->dyncellwidth;
    ivec[i] = int(gridvec[i]);
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
    for (j = 0; j < cell->influences_num; j++, inf++) {
      cageco = dco[inf->vertex];
      cageweight = weight * inf->weight;
#if BLI_HAVE_SSE2
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

#if BLI_HAVE_SSE2
  copy_v3_v3(vec, (float *)&co);
#else
  copy_v3_v3(vec, co);
#endif

  return totweight;
}

struct MeshdeformUserdata {
  /*const*/ MeshDeformModifierData *mmd;
  const MDeformVert *dvert;
  /*const*/ float (*dco)[3];
  int defgrp_index;
  float (*vertexCos)[3];
  float (*cagemat)[4];
  float (*icagemat)[3];
};

static void meshdeform_vert_task(void *__restrict userdata,
                                 const int iter,
                                 const TaskParallelTLS *__restrict /*tls*/)
{
  MeshdeformUserdata *data = static_cast<MeshdeformUserdata *>(userdata);
  /*const*/ MeshDeformModifierData *mmd = data->mmd;
  const MDeformVert *dvert = data->dvert;
  const int defgrp_index = data->defgrp_index;
  const int *offsets = mmd->bindoffsets;
  const MDefInfluence *__restrict influences = mmd->bindinfluences;
  /*const*/ float (*__restrict dco)[3] = data->dco;
  float (*vertexCos)[3] = data->vertexCos;
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
    add_v3_v3(vertexCos[iter], co);
  }
}

static void meshdeformModifier_do(ModifierData *md,
                                  const ModifierEvalContext *ctx,
                                  Mesh *mesh,
                                  float (*vertexCos)[3],
                                  const int verts_num)
{
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;
  Object *ob = ctx->object;

  Mesh *cagemesh;
  const MDeformVert *dvert = nullptr;
  float imat[4][4], cagemat[4][4], iobmat[4][4], icagemat[3][3], cmat[4][4];
  const float (*bindcagecos)[3];
  int a, cage_verts_num, defgrp_index;
  MeshdeformUserdata data;

  static int recursive_bind_sentinel = 0;

  if (mmd->object == nullptr || (mmd->bindcagecos == nullptr && mmd->bindfunc == nullptr)) {
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
  cagemesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(ob_target);
  if (cagemesh == nullptr) {
    BKE_modifier_set_error(ctx->object, md, "Cannot get mesh from cage object");
    return;
  }

  /* compute matrices to go in and out of cage object space */
  invert_m4_m4(imat, ob_target->object_to_world().ptr());
  mul_m4_m4m4(cagemat, imat, ob->object_to_world().ptr());
  mul_m4_m4m4(cmat, mmd->bindmat, cagemat);
  invert_m4_m4(iobmat, cmat);
  copy_m3_m4(icagemat, iobmat);

  /* bind weights if needed */
  if (!mmd->bindcagecos) {
    /* progress bar redraw can make this recursive. */
    if (!DEG_is_active(ctx->depsgraph)) {
      BKE_modifier_set_error(ob, md, "Attempt to bind from inactive dependency graph");
      return;
    }
    if (!recursive_bind_sentinel) {
      recursive_bind_sentinel = 1;
      mmd->bindfunc(ob, mmd, cagemesh, (float *)vertexCos, verts_num, cagemat);
      recursive_bind_sentinel = 0;
    }

    return;
  }

  /* verify we have compatible weights */
  cage_verts_num = BKE_mesh_wrapper_vert_len(cagemesh);

  if (mmd->verts_num != verts_num) {
    BKE_modifier_set_error(ob, md, "Vertices changed from %d to %d", mmd->verts_num, verts_num);
    return;
  }
  if (mmd->cage_verts_num != cage_verts_num) {
    BKE_modifier_set_error(
        ob, md, "Cage vertices changed from %d to %d", mmd->cage_verts_num, cage_verts_num);
    return;
  }
  if (mmd->bindcagecos == nullptr) {
    BKE_modifier_set_error(ob, md, "Bind data missing");
    return;
  }

  /* We allocate 1 element extra to make it possible to
   * load the values to SSE registers, which are float4.
   */
  blender::Array<blender::float3> dco(cage_verts_num + 1);
  zero_v3(dco[cage_verts_num]);

  /* setup deformation data */
  BKE_mesh_wrapper_vert_coords_copy(cagemesh, dco.as_mutable_span().take_front(cage_verts_num));
  bindcagecos = (const float (*)[3])mmd->bindcagecos;

  for (a = 0; a < cage_verts_num; a++) {
    /* Get cage vertex in world-space with binding transform. */
    float co[3];
    mul_v3_m4v3(co, mmd->bindmat, dco[a]);
    /* compute difference with world space bind coord */
    sub_v3_v3v3(dco[a], co, bindcagecos[a]);
  }

  MOD_get_vgroup(ob, mesh, mmd->defgrp_name, &dvert, &defgrp_index);

  /* Initialize data to be pass to the for body function. */
  data.mmd = mmd;
  data.dvert = dvert;
  data.dco = reinterpret_cast<float (*)[3]>(dco.data());
  data.defgrp_index = defgrp_index;
  data.vertexCos = vertexCos;
  data.cagemat = cagemat;
  data.icagemat = icagemat;

  /* Do deformation. */
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.min_iter_per_thread = 16;
  BLI_task_parallel_range(0, verts_num, &data, meshdeform_vert_task, &settings);
}

static void deform_verts(ModifierData *md,
                         const ModifierEvalContext *ctx,
                         Mesh *mesh,
                         blender::MutableSpan<blender::float3> positions)
{
  /* if next modifier needs original vertices */
  MOD_previous_vcos_store(md, reinterpret_cast<float (*)[3]>(positions.data()));
  meshdeformModifier_do(
      md, ctx, mesh, reinterpret_cast<float (*)[3]>(positions.data()), positions.size());
}

#define MESHDEFORM_MIN_INFLUENCE 0.00001f

void BKE_modifier_mdef_compact_influences(ModifierData *md)
{
  using namespace blender;
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;
  float weight, totweight;
  int influences_num, verts_num, cage_verts_num, a, b;

  const float *weights = mmd->bindweights;
  if (!weights) {
    return;
  }

  verts_num = mmd->verts_num;
  cage_verts_num = mmd->cage_verts_num;

  /* count number of influences above threshold */
  for (b = 0; b < verts_num; b++) {
    for (a = 0; a < cage_verts_num; a++) {
      weight = weights[a + b * cage_verts_num];

      if (weight > MESHDEFORM_MIN_INFLUENCE) {
        mmd->influences_num++;
      }
    }
  }

  /* allocate bind influences */
  mmd->bindinfluences = MEM_calloc_arrayN<MDefInfluence>(mmd->influences_num, __func__);
  mmd->bindinfluences_sharing_info = implicit_sharing::info_for_mem_free(mmd->bindinfluences);
  mmd->bindoffsets = MEM_calloc_arrayN<int>(size_t(verts_num) + 1, __func__);
  mmd->bindoffsets_sharing_info = implicit_sharing::info_for_mem_free(mmd->bindoffsets);

  /* write influences */
  influences_num = 0;

  for (b = 0; b < verts_num; b++) {
    mmd->bindoffsets[b] = influences_num;
    totweight = 0.0f;

    /* sum total weight */
    for (a = 0; a < cage_verts_num; a++) {
      weight = weights[a + b * cage_verts_num];

      if (weight > MESHDEFORM_MIN_INFLUENCE) {
        totweight += weight;
      }
    }

    /* assign weights normalized */
    for (a = 0; a < cage_verts_num; a++) {
      weight = weights[a + b * cage_verts_num];

      if (weight > MESHDEFORM_MIN_INFLUENCE) {
        mmd->bindinfluences[influences_num].weight = weight / totweight;
        mmd->bindinfluences[influences_num].vertex = a;
        influences_num++;
      }
    }
  }

  mmd->bindoffsets[b] = influences_num;

  /* free */
  MEM_freeN(mmd->bindweights);
  mmd->bindweights = nullptr;
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  bool is_bound = RNA_boolean_get(ptr, "is_bound");

  layout->use_property_split_set(true);

  col = &layout->column(true);
  col->enabled_set(!is_bound);
  col->prop(ptr, "object", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", std::nullopt);

  col = &layout->column(false);
  col->enabled_set(!is_bound);
  col->prop(ptr, "precision", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(ptr, "use_dynamic_bind", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->op("OBJECT_OT_meshdeform_bind", is_bound ? IFACE_("Unbind") : IFACE_("Bind"), ICON_NONE);

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_MeshDeform, panel_draw);
}

static void blend_write(BlendWriter *writer, const ID *id_owner, const ModifierData *md)
{
  MeshDeformModifierData mmd = *(const MeshDeformModifierData *)md;
  const bool is_undo = BLO_write_is_undo(writer);

  if (ID_IS_OVERRIDE_LIBRARY(id_owner) && !is_undo) {
    BLI_assert(!ID_IS_LINKED(id_owner));
    const bool is_local = (md->flag & eModifierFlag_OverrideLibrary_Local) != 0;
    if (!is_local) {
      /* Modifier coming from linked data cannot be bound from an override, so we can remove all
       * binding data, can save a significant amount of memory. */
      mmd.influences_num = 0;
      mmd.bindinfluences = nullptr;
      mmd.bindinfluences_sharing_info = nullptr;
      mmd.verts_num = 0;
      mmd.bindoffsets = nullptr;
      mmd.bindoffsets_sharing_info = nullptr;
      mmd.cage_verts_num = 0;
      mmd.bindcagecos = nullptr;
      mmd.bindcagecos_sharing_info = nullptr;
      mmd.dyngridsize = 0;
      mmd.dyngrid = nullptr;
      mmd.dyngrid_sharing_info = nullptr;
      mmd.influences_num = 0;
      mmd.dyninfluences = nullptr;
      mmd.dyninfluences_sharing_info = nullptr;
      mmd.dynverts = nullptr;
      mmd.dynverts_sharing_info = nullptr;
    }
  }

  const int size = mmd.dyngridsize;

  BLO_write_shared(writer,
                   mmd.bindinfluences,
                   sizeof(MDefInfluence) * mmd.influences_num,
                   mmd.bindinfluences_sharing_info,
                   [&]() {
                     BLO_write_struct_array(
                         writer, MDefInfluence, mmd.influences_num, mmd.bindinfluences);
                   });

  /* NOTE: `bindoffset` is abusing `verts_num + 1` as its size, this becomes an incorrect value in
   * case `verts_num == 0`, since `bindoffset` is then nullptr, not a size 1 allocated array. */
  if (mmd.verts_num > 0) {
    BLO_write_shared(writer,
                     mmd.bindoffsets,
                     sizeof(int) * (mmd.verts_num + 1),
                     mmd.bindoffsets_sharing_info,
                     [&]() { BLO_write_int32_array(writer, mmd.verts_num + 1, mmd.bindoffsets); });
  }
  else {
    BLI_assert(mmd.bindoffsets == nullptr);
  }

  BLO_write_shared(writer,
                   mmd.bindcagecos,
                   sizeof(float[3]) * mmd.cage_verts_num,
                   mmd.bindcagecos_sharing_info,
                   [&]() { BLO_write_float3_array(writer, mmd.cage_verts_num, mmd.bindcagecos); });
  BLO_write_shared(
      writer, mmd.dyngrid, sizeof(MDefCell) * size * size * size, mmd.dyngrid_sharing_info, [&]() {
        BLO_write_struct_array(writer, MDefCell, size * size * size, mmd.dyngrid);
      });
  BLO_write_shared(writer,
                   mmd.dyninfluences,
                   sizeof(MDefInfluence) * mmd.influences_num,
                   mmd.dyninfluences_sharing_info,
                   [&]() {
                     BLO_write_struct_array(
                         writer, MDefInfluence, mmd.influences_num, mmd.dyninfluences);
                   });
  BLO_write_shared(writer,
                   mmd.dynverts,
                   sizeof(MDefInfluence) * mmd.verts_num,
                   mmd.dynverts_sharing_info,
                   [&]() { BLO_write_int32_array(writer, mmd.verts_num, mmd.dynverts); });

  BLO_write_struct_at_address(writer, MeshDeformModifierData, md, &mmd);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;
  const int size = mmd->dyngridsize;

  if (mmd->bindinfluences) {
    mmd->bindinfluences_sharing_info = BLO_read_shared(reader, &mmd->bindinfluences, [&]() {
      BLO_read_struct_array(reader, MDefInfluence, mmd->influences_num, &mmd->bindinfluences);
      return blender::implicit_sharing::info_for_mem_free(mmd->bindinfluences);
    });
  }

  /* NOTE: `bindoffset` is abusing `verts_num + 1` as its size, this becomes an incorrect value in
   * case `verts_num == 0`, since `bindoffset` is then nullptr, not a size 1 allocated array. */
  if (mmd->verts_num > 0) {
    if (mmd->bindoffsets) {
      mmd->bindoffsets_sharing_info = BLO_read_shared(reader, &mmd->bindoffsets, [&]() {
        BLO_read_int32_array(reader, mmd->verts_num + 1, &mmd->bindoffsets);
        return blender::implicit_sharing::info_for_mem_free(mmd->bindoffsets);
      });
    }
  }

  if (mmd->bindcagecos) {
    mmd->bindcagecos_sharing_info = BLO_read_shared(reader, &mmd->bindcagecos, [&]() {
      BLO_read_float3_array(reader, mmd->cage_verts_num, &mmd->bindcagecos);
      return blender::implicit_sharing::info_for_mem_free(mmd->bindcagecos);
    });
  }
  if (mmd->dyngrid) {
    mmd->dyngrid_sharing_info = BLO_read_shared(reader, &mmd->dyngrid, [&]() {
      BLO_read_struct_array(reader, MDefCell, size * size * size, &mmd->dyngrid);
      return blender::implicit_sharing::info_for_mem_free(mmd->dyngrid);
    });
  }
  if (mmd->dyninfluences) {
    mmd->dyninfluences_sharing_info = BLO_read_shared(reader, &mmd->dyninfluences, [&]() {
      BLO_read_struct_array(reader, MDefInfluence, mmd->influences_num, &mmd->dyninfluences);
      return blender::implicit_sharing::info_for_mem_free(mmd->dyninfluences);
    });
  }
  if (mmd->dynverts) {
    mmd->dynverts_sharing_info = BLO_read_shared(reader, &mmd->dynverts, [&]() {
      BLO_read_int32_array(reader, mmd->verts_num, &mmd->dynverts);
      return blender::implicit_sharing::info_for_mem_free(mmd->dynverts);
    });
  }

  /* Deprecated storage. */
  BLO_read_float_array(reader, mmd->verts_num, &mmd->bindweights);
  BLO_read_float3_array(reader, mmd->cage_verts_num, &mmd->bindcos);
}

ModifierTypeInfo modifierType_MeshDeform = {
    /*idname*/ "MeshDeform",
    /*name*/ N_("MeshDeform"),
    /*struct_name*/ "MeshDeformModifierData",
    /*struct_size*/ sizeof(MeshDeformModifierData),
    /*srna*/ &RNA_MeshDeformModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsCVs | eModifierTypeFlag_AcceptsVertexCosOnly |
        eModifierTypeFlag_SupportsEditmode,
    /*icon*/ ICON_MOD_MESHDEFORM,

    /*copy_data*/ copy_data,

    /*deform_verts*/ deform_verts,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ free_data,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ blend_write,
    /*blend_read*/ blend_read,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
