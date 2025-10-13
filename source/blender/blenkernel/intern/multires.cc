/* SPDX-FileCopyrightText: 2007 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

/* for reading old multires */
#define DNA_DEPRECATED_ALLOW

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_bitmap.h"
#include "BLI_index_mask.hh"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"

#include "BKE_ccg.hh"
#include "BKE_editmesh.hh"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.hh"
#include "BKE_mesh_types.hh"
#include "BKE_modifier.hh"
#include "BKE_multires.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_scene.hh"
#include "BKE_subdiv_ccg.hh"

#include "BKE_object.hh"

#include "DEG_depsgraph_query.hh"

#include <cmath>
#include <cstring>

/* MULTIRES MODIFIER */
static const int multires_grid_tot[] = {
    0, 4, 9, 25, 81, 289, 1089, 4225, 16641, 66049, 263169, 1050625, 4198401, 16785409};
static const int multires_side_tot[] = {
    0, 2, 3, 5, 9, 17, 33, 65, 129, 257, 513, 1025, 2049, 4097};

/** Custom-data. */

void multires_customdata_delete(Mesh *mesh)
{
  if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
    /* CustomData_external_remove is used here only to mark layer
     * as non-external for further freeing, so zero element count
     * looks safer than `em->bm->totface`. */
    CustomData_external_remove(&em->bm->ldata, &mesh->id, CD_MDISPS, 0);

    if (CustomData_has_layer(&em->bm->ldata, CD_MDISPS)) {
      BM_data_layer_free(em->bm, &em->bm->ldata, CD_MDISPS);
    }

    if (CustomData_has_layer(&em->bm->ldata, CD_GRID_PAINT_MASK)) {
      BM_data_layer_free(em->bm, &em->bm->ldata, CD_GRID_PAINT_MASK);
    }
  }
  else {
    CustomData_external_remove(&mesh->corner_data, &mesh->id, CD_MDISPS, mesh->corners_num);
    CustomData_free_layer_active(&mesh->corner_data, CD_MDISPS);

    CustomData_free_layer_active(&mesh->corner_data, CD_GRID_PAINT_MASK);
  }
}

static BLI_bitmap *multires_mdisps_downsample_hidden(const BLI_bitmap *old_hidden,
                                                     const int old_level,
                                                     const int new_level)
{
  const int new_gridsize = CCG_grid_size(new_level);
  const int old_gridsize = CCG_grid_size(old_level);

  BLI_assert(new_level <= old_level);
  const int factor = CCG_grid_factor(new_level, old_level);
  BLI_bitmap *new_hidden = BLI_BITMAP_NEW(square_i(new_gridsize), "downsample hidden");

  for (int y = 0; y < new_gridsize; y++) {
    for (int x = 0; x < new_gridsize; x++) {
      const int old_value = BLI_BITMAP_TEST(old_hidden, factor * y * old_gridsize + x * factor);

      BLI_BITMAP_SET(new_hidden, y * new_gridsize + x, old_value);
    }
  }

  return new_hidden;
}

Mesh *BKE_multires_create_mesh(Depsgraph *depsgraph, Object *object, MultiresModifierData *mmd)
{
  Object *object_eval = DEG_get_evaluated(depsgraph, object);
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Mesh *deformed_mesh = blender::bke::mesh_get_eval_deform(
      depsgraph, scene_eval, object_eval, &CD_MASK_BAREMESH);
  ModifierEvalContext modifier_ctx{};
  modifier_ctx.depsgraph = depsgraph;
  modifier_ctx.object = object_eval;
  modifier_ctx.flag = MOD_APPLY_USECACHE | MOD_APPLY_IGNORE_SIMPLIFY;

  const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(mmd->modifier.type));
  Mesh *result = mti->modify_mesh(&mmd->modifier, &modifier_ctx, deformed_mesh);

  if (result == deformed_mesh) {
    result = BKE_mesh_copy_for_eval(*deformed_mesh);
  }
  return result;
}

blender::Array<blender::float3> BKE_multires_create_deformed_base_mesh_vert_coords(
    Depsgraph *depsgraph, Object *object, MultiresModifierData *mmd)
{
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Object *object_eval = DEG_get_evaluated(depsgraph, object);

  Object object_for_eval = blender::dna::shallow_copy(*object_eval);
  blender::bke::ObjectRuntime runtime = *object_eval->runtime;
  object_for_eval.runtime = &runtime;

  object_for_eval.data = object->data;
  object_for_eval.sculpt = nullptr;

  const bool use_render = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  ModifierEvalContext mesh_eval_context = {depsgraph, &object_for_eval, ModifierApplyFlag(0)};
  if (use_render) {
    mesh_eval_context.flag |= MOD_APPLY_RENDER;
  }
  const int required_mode = use_render ? eModifierMode_Render : eModifierMode_Realtime;

  VirtualModifierData virtual_modifier_data;
  ModifierData *first_md = BKE_modifiers_get_virtual_modifierlist(&object_for_eval,
                                                                  &virtual_modifier_data);

  Mesh *base_mesh = static_cast<Mesh *>(object->data);

  blender::Array<blender::float3> deformed_verts(base_mesh->vert_positions());

  for (ModifierData *md = first_md; md != nullptr; md = md->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));
    if (md == &mmd->modifier) {
      break;
    }
    if (!BKE_modifier_is_enabled(scene_eval, md, required_mode)) {
      continue;
    }
    if (mti->type != ModifierTypeType::OnlyDeform) {
      break;
    }
    BKE_modifier_deform_verts(md, &mesh_eval_context, base_mesh, deformed_verts);
  }

  return deformed_verts;
}

MultiresModifierData *find_multires_modifier_before(Scene *scene, ModifierData *lastmd)
{

  for (ModifierData *md = lastmd; md; md = md->prev) {
    if (md->type == eModifierType_Multires) {
      if (BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
        return reinterpret_cast<MultiresModifierData *>(md);
      }
    }
  }

  return nullptr;
}

MultiresModifierData *get_multires_modifier(Scene *scene, Object *ob, const bool use_first)
{
  MultiresModifierData *mmd = nullptr, *firstmmd = nullptr;

  /* find first active multires modifier */
  LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
    if (md->type == eModifierType_Multires) {
      if (!firstmmd) {
        firstmmd = reinterpret_cast<MultiresModifierData *>(md);
      }

      if (BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
        mmd = reinterpret_cast<MultiresModifierData *>(md);
        break;
      }
    }
  }

  if (!mmd && use_first) {
    /* active multires have not been found
     * try to use first one */
    return firstmmd;
  }

  return mmd;
}

int multires_get_level(const Scene *scene,
                       const Object *ob,
                       const MultiresModifierData *mmd,
                       const bool render,
                       const bool ignore_simplify)
{
  if (render) {
    return (scene != nullptr) ? get_render_subsurf_level(&scene->r, mmd->renderlvl, true) :
                                mmd->renderlvl;
  }
  if (ob->mode == OB_MODE_SCULPT) {
    return mmd->sculptlvl;
  }
  if (ignore_simplify) {
    return mmd->lvl;
  }

  return (scene != nullptr) ? get_render_subsurf_level(&scene->r, mmd->lvl, false) : mmd->lvl;
}

void multires_set_tot_level(Object *ob, MultiresModifierData *mmd, const int lvl)
{
  mmd->totlvl = lvl;

  if (ob->mode != OB_MODE_SCULPT) {
    mmd->lvl = std::clamp<char>(std::max<char>(mmd->lvl, lvl), 0, mmd->totlvl);
  }

  mmd->sculptlvl = std::clamp<char>(std::max<char>(mmd->sculptlvl, lvl), 0, mmd->totlvl);
  mmd->renderlvl = std::clamp<char>(std::max<char>(mmd->renderlvl, lvl), 0, mmd->totlvl);
}

static void multires_ccg_mark_as_modified(SubdivCCG *subdiv_ccg, const MultiresModifiedFlags flags)
{
  if (flags & MULTIRES_COORDS_MODIFIED) {
    subdiv_ccg->dirty.coords = true;
  }
  if (flags & MULTIRES_HIDDEN_MODIFIED) {
    subdiv_ccg->dirty.hidden = true;
  }
}

void multires_mark_as_modified(Depsgraph *depsgraph,
                               Object *object,
                               const MultiresModifiedFlags flags)
{
  if (object == nullptr) {
    return;
  }
  /* NOTE: CCG live inside of evaluated object.
   *
   * While this is a bit weird to tag the only one, this is how other areas were built
   * historically: they are tagging multires for update and then rely on object re-evaluation to
   * do an actual update.
   *
   * In a longer term maybe special dependency graph tag can help sanitizing this a bit. */
  Object *object_eval = DEG_get_evaluated(depsgraph, object);
  Mesh *mesh = static_cast<Mesh *>(object_eval->data);
  SubdivCCG *subdiv_ccg = mesh->runtime->subdiv_ccg.get();
  if (subdiv_ccg == nullptr) {
    return;
  }
  multires_ccg_mark_as_modified(subdiv_ccg, flags);
}

void multires_flush_sculpt_updates(Object *object)
{
  if (object == nullptr || object->sculpt == nullptr) {
    return;
  }
  const blender::bke::pbvh::Tree *pbvh = blender::bke::object::pbvh_get(*object);
  if (!pbvh) {
    return;
  }

  SculptSession *sculpt_session = object->sculpt;
  if (pbvh->type() != blender::bke::pbvh::Type::Grids || !sculpt_session->multires.active ||
      sculpt_session->multires.modifier == nullptr)
  {
    return;
  }

  SubdivCCG *subdiv_ccg = sculpt_session->subdiv_ccg;
  if (subdiv_ccg == nullptr) {
    return;
  }

  if (!subdiv_ccg->dirty.coords && !subdiv_ccg->dirty.hidden) {
    return;
  }

  Mesh *mesh = static_cast<Mesh *>(object->data);

  /* Check that the multires modifier still exists.
   * Fixes crash when deleting multires modifier
   * from within sculpt mode.
   */
  MultiresModifierData *mmd = nullptr;
  VirtualModifierData virtual_modifier_data;

  for (ModifierData *md = BKE_modifiers_get_virtual_modifierlist(object, &virtual_modifier_data);
       md;
       md = md->next)
  {
    if (md->type == eModifierType_Multires) {
      if (BKE_modifier_is_enabled(nullptr, md, eModifierMode_Realtime)) {
        mmd = reinterpret_cast<MultiresModifierData *>(md);
      }
    }
  }

  if (!mmd) {
    return;
  }

  multiresModifier_reshapeFromCCG(
      sculpt_session->multires.modifier->totlvl, mesh, sculpt_session->subdiv_ccg);

  subdiv_ccg->dirty.coords = false;
  subdiv_ccg->dirty.hidden = false;
}

void multires_force_sculpt_rebuild(Object *object)
{
  using namespace blender;
  multires_flush_sculpt_updates(object);

  if (object == nullptr || object->sculpt == nullptr) {
    return;
  }

  BKE_sculptsession_free_pbvh(*object);
}

void multires_force_external_reload(Object *object)
{
  Mesh *mesh = BKE_mesh_from_object(object);

  CustomData_external_reload(&mesh->corner_data, &mesh->id, CD_MASK_MDISPS, mesh->corners_num);
  multires_force_sculpt_rebuild(object);
}

/* reset the multires levels to match the number of mdisps */
static int get_levels_from_disps(Object *ob)
{
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  const blender::OffsetIndices faces = mesh->faces();
  int totlvl = 0;

  const MDisps *mdisp = static_cast<const MDisps *>(
      CustomData_get_layer(&mesh->corner_data, CD_MDISPS));

  for (const int i : faces.index_range()) {
    for (const int corner : faces[i]) {
      const MDisps *md = &mdisp[corner];
      if (md->totdisp == 0) {
        continue;
      }

      while (true) {
        const int side = (1 << (totlvl - 1)) + 1;
        const int lvl_totdisp = side * side;
        if (md->totdisp == lvl_totdisp) {
          break;
        }
        if (md->totdisp < lvl_totdisp) {
          totlvl--;
        }
        else {
          totlvl++;
        }
      }

      break;
    }
  }

  return totlvl;
}

void multiresModifier_set_levels_from_disps(MultiresModifierData *mmd, Object *ob)
{
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  const MDisps *mdisp;

  if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
    mdisp = static_cast<const MDisps *>(CustomData_get_layer(&em->bm->ldata, CD_MDISPS));
  }
  else {
    mdisp = static_cast<const MDisps *>(CustomData_get_layer(&mesh->corner_data, CD_MDISPS));
  }

  if (mdisp) {
    mmd->totlvl = get_levels_from_disps(ob);
    mmd->lvl = std::min(mmd->lvl, mmd->totlvl);
    mmd->sculptlvl = std::min(mmd->sculptlvl, mmd->totlvl);
    mmd->renderlvl = std::min(mmd->renderlvl, mmd->totlvl);
  }
}

static void multires_set_tot_mdisps(Mesh *mesh, const int lvl)
{
  MDisps *mdisps = static_cast<MDisps *>(
      CustomData_get_layer_for_write(&mesh->corner_data, CD_MDISPS, mesh->corners_num));

  if (mdisps) {
    for (int i = 0; i < mesh->corners_num; i++, mdisps++) {
      mdisps->totdisp = multires_grid_tot[lvl];
      mdisps->level = lvl;
    }
  }
}

static void multires_copy_grid(float (*gridA)[3],
                               float (*gridB)[3],
                               const int sizeA,
                               const int sizeB)
{
  int x, y, j, skip;

  if (sizeA > sizeB) {
    skip = (sizeA - 1) / (sizeB - 1);

    for (j = 0, y = 0; y < sizeB; y++) {
      for (x = 0; x < sizeB; x++, j++) {
        copy_v3_v3(gridA[y * skip * sizeA + x * skip], gridB[j]);
      }
    }
  }
  else {
    skip = (sizeB - 1) / (sizeA - 1);

    for (j = 0, y = 0; y < sizeA; y++) {
      for (x = 0; x < sizeA; x++, j++) {
        copy_v3_v3(gridA[j], gridB[y * skip * sizeB + x * skip]);
      }
    }
  }
}

/* Reallocate gpm->data at a lower resolution and copy values over
 * from the original high-resolution data */
static void multires_grid_paint_mask_downsample(GridPaintMask *gpm, const int level)
{
  if (level < gpm->level) {
    const int gridsize = CCG_grid_size(level);
    float *data = MEM_calloc_arrayN<float>(size_t(square_i(gridsize)), __func__);

    for (int y = 0; y < gridsize; y++) {
      for (int x = 0; x < gridsize; x++) {
        data[y * gridsize + x] = paint_grid_paint_mask(gpm, level, x, y);
      }
    }

    MEM_freeN(gpm->data);
    gpm->data = data;
    gpm->level = level;
  }
}

static void multires_del_higher(MultiresModifierData *mmd, Object *ob, const int lvl)
{
  Mesh *mesh = (Mesh *)ob->data;
  const blender::OffsetIndices faces = mesh->faces();
  const int levels = mmd->totlvl - lvl;
  MDisps *mdisps;
  GridPaintMask *gpm;

  multires_set_tot_mdisps(mesh, mmd->totlvl);
  multiresModifier_ensure_external_read(mesh, mmd);
  mdisps = static_cast<MDisps *>(
      CustomData_get_layer_for_write(&mesh->corner_data, CD_MDISPS, mesh->corners_num));
  gpm = static_cast<GridPaintMask *>(
      CustomData_get_layer_for_write(&mesh->corner_data, CD_GRID_PAINT_MASK, mesh->corners_num));

  multires_force_sculpt_rebuild(ob);

  if (mdisps && levels > 0) {
    if (lvl > 0) {
      const int nsize = multires_side_tot[lvl];
      const int hsize = multires_side_tot[mmd->totlvl];

      for (const int i : faces.index_range()) {
        for (const int corner : faces[i]) {
          MDisps *mdisp = &mdisps[corner];
          const int totdisp = multires_grid_tot[lvl];

          float (*disps)[3] = MEM_calloc_arrayN<float[3]>(totdisp, "multires disps");

          if (mdisp->disps != nullptr) {
            float (*ndisps)[3] = disps;
            float (*hdisps)[3] = mdisp->disps;

            multires_copy_grid(ndisps, hdisps, nsize, hsize);
            if (mdisp->hidden) {
              BLI_bitmap *gh = multires_mdisps_downsample_hidden(mdisp->hidden, mdisp->level, lvl);
              MEM_freeN(mdisp->hidden);
              mdisp->hidden = gh;
            }

            MEM_freeN(mdisp->disps);
          }

          mdisp->disps = disps;
          mdisp->totdisp = totdisp;
          mdisp->level = lvl;

          if (gpm) {
            multires_grid_paint_mask_downsample(&gpm[corner], lvl);
          }
        }
      }
    }
    else {
      multires_customdata_delete(mesh);
    }
  }

  multires_set_tot_level(ob, mmd, lvl);
}

void multiresModifier_del_levels(MultiresModifierData *mmd,
                                 Scene *scene,
                                 Object *ob,
                                 const int direction)
{
  Mesh *mesh = BKE_mesh_from_object(ob);
  const int lvl = multires_get_level(scene, ob, mmd, false, true);
  const int levels = mmd->totlvl - lvl;

  multires_set_tot_mdisps(mesh, mmd->totlvl);
  multiresModifier_ensure_external_read(mesh, mmd);
  MDisps *mdisps = static_cast<MDisps *>(
      CustomData_get_layer_for_write(&mesh->corner_data, CD_MDISPS, mesh->corners_num));

  multires_force_sculpt_rebuild(ob);

  if (mdisps && levels > 0 && direction == 1) {
    multires_del_higher(mmd, ob, lvl);
  }

  multires_set_tot_level(ob, mmd, lvl);
}

void multires_stitch_grids(Object *ob)
{
  using namespace blender;
  if (ob == nullptr) {
    return;
  }
  SculptSession *sculpt_session = ob->sculpt;
  if (sculpt_session == nullptr) {
    return;
  }
  SubdivCCG *subdiv_ccg = sculpt_session->subdiv_ccg;
  if (subdiv_ccg == nullptr) {
    return;
  }
  BLI_assert(bke::object::pbvh_get(*ob) &&
             bke::object::pbvh_get(*ob)->type() == blender::bke::pbvh::Type::Grids);
  BKE_subdiv_ccg_average_stitch_faces(*subdiv_ccg, IndexMask(subdiv_ccg->faces.size()));
}

void old_mdisps_bilinear(float out[3], float (*disps)[3], const int st, float u, float v)
{
  const int st_max = st - 1;
  float d[4][3], d2[2][3];

  if (!disps || isnan(u) || isnan(v)) {
    return;
  }

  if (u < 0) {
    u = 0;
  }
  else if (u >= st) {
    u = st_max;
  }
  if (v < 0) {
    v = 0;
  }
  else if (v >= st) {
    v = st_max;
  }

  const int x = floor(u);
  const int y = floor(v);
  int x2 = x + 1;
  int y2 = y + 1;

  if (x2 >= st) {
    x2 = st_max;
  }
  if (y2 >= st) {
    y2 = st_max;
  }

  const float urat = u - x;
  const float vrat = v - y;
  const float uopp = 1 - urat;

  mul_v3_v3fl(d[0], disps[y * st + x], uopp);
  mul_v3_v3fl(d[1], disps[y * st + x2], urat);
  mul_v3_v3fl(d[2], disps[y2 * st + x], uopp);
  mul_v3_v3fl(d[3], disps[y2 * st + x2], urat);

  add_v3_v3v3(d2[0], d[0], d[1]);
  add_v3_v3v3(d2[1], d[2], d[3]);
  mul_v3_fl(d2[0], 1 - vrat);
  mul_v3_fl(d2[1], vrat);

  add_v3_v3v3(out, d2[0], d2[1]);
}

void multiresModifier_sync_levels_ex(Object *ob_dst,
                                     const MultiresModifierData *mmd_src,
                                     MultiresModifierData *mmd_dst)
{
  if (mmd_src->totlvl == mmd_dst->totlvl) {
    return;
  }

  if (mmd_src->totlvl > mmd_dst->totlvl) {
    multiresModifier_subdivide_to_level(
        ob_dst, mmd_dst, mmd_src->totlvl, MultiresSubdivideModeType::CatmullClark);
  }
  else {
    multires_del_higher(mmd_dst, ob_dst, mmd_src->totlvl);
  }
}

static void multires_sync_levels(Scene *scene, Object *ob_src, Object *ob_dst)
{
  MultiresModifierData *mmd_src = get_multires_modifier(scene, ob_src, true);
  MultiresModifierData *mmd_dst = get_multires_modifier(scene, ob_dst, true);

  if (!mmd_src) {
    /* NOTE(@sergey): object could have MDISP even when there is no multires modifier
     * this could lead to troubles due to I've got no idea how mdisp could be
     * up-sampled correct without modifier data. Just remove mdisps if no multires present. */
    multires_customdata_delete(static_cast<Mesh *>(ob_src->data));
  }

  if (mmd_src && mmd_dst) {
    multiresModifier_sync_levels_ex(ob_dst, mmd_src, mmd_dst);
  }
}

static void multires_apply_uniform_scale(Object *object, const float scale)
{
  Mesh *mesh = static_cast<Mesh *>(object->data);
  MDisps *mdisps = static_cast<MDisps *>(
      CustomData_get_layer_for_write(&mesh->corner_data, CD_MDISPS, mesh->corners_num));
  for (int i = 0; i < mesh->corners_num; i++) {
    MDisps *grid = &mdisps[i];
    for (int j = 0; j < grid->totdisp; j++) {
      mul_v3_fl(grid->disps[j], scale);
    }
  }
}

static void multires_apply_smat(Depsgraph * /*depsgraph*/,
                                Scene *scene,
                                Object *object,
                                const float smat[3][3])
{
  const MultiresModifierData *mmd = get_multires_modifier(scene, object, true);
  if (mmd == nullptr || mmd->totlvl == 0) {
    return;
  }
  /* Make sure layer present. */
  Mesh *mesh = static_cast<Mesh *>(object->data);
  multiresModifier_ensure_external_read(mesh, mmd);
  if (!CustomData_get_layer(&mesh->corner_data, CD_MDISPS)) {
    return;
  }
  if (is_uniform_scaled_m3(smat)) {
    const float scale = mat3_to_scale(smat);
    multires_apply_uniform_scale(object, scale);
  }
  else {
    /* TODO(@sergey): This branch of code actually requires more work to
     * preserve all the details. */
    const float scale = mat3_to_scale(smat);
    multires_apply_uniform_scale(object, scale);
  }
}

int multires_mdisp_corners(const MDisps *s)
{
  int lvl = 13;

  while (lvl > 0) {
    const int side = (1 << (lvl - 1)) + 1;
    if ((s->totdisp % (side * side)) == 0) {
      return s->totdisp / (side * side);
    }
    lvl--;
  }

  return 0;
}

void multiresModifier_scale_disp(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  float smat[3][3];

  /* object's scale matrix */
  BKE_object_scale_to_mat3(ob, smat);

  multires_apply_smat(depsgraph, scene, ob, smat);
}

void multiresModifier_prepare_join(Depsgraph *depsgraph, Scene *scene, Object *ob, Object *to_ob)
{
  float smat[3][3], tmat[3][3], mat[3][3];
  multires_sync_levels(scene, to_ob, ob);

  /* construct scale matrix for displacement */
  BKE_object_scale_to_mat3(to_ob, tmat);
  invert_m3(tmat);
  BKE_object_scale_to_mat3(ob, smat);
  mul_m3_m3m3(mat, smat, tmat);

  multires_apply_smat(depsgraph, scene, ob, mat);
}

void multires_topology_changed(Mesh *mesh)
{

  CustomData_external_read(&mesh->corner_data, &mesh->id, CD_MASK_MDISPS, mesh->corners_num);
  MDisps *mdisp = static_cast<MDisps *>(
      CustomData_get_layer_for_write(&mesh->corner_data, CD_MDISPS, mesh->corners_num));

  if (!mdisp) {
    return;
  }

  MDisps *cur = mdisp;
  int grid = 0;
  for (int i = 0; i < mesh->corners_num; i++, cur++) {
    if (cur->totdisp) {
      grid = mdisp->totdisp;

      break;
    }
  }

  for (int i = 0; i < mesh->corners_num; i++, mdisp++) {
    /* allocate memory for mdisp, the whole disp layer would be erased otherwise */
    if (!mdisp->totdisp || !mdisp->disps) {
      if (grid) {
        mdisp->totdisp = grid;
        mdisp->disps = MEM_calloc_arrayN<float[3]>(mdisp->totdisp, "mdisp topology");
      }

      continue;
    }
  }
}

void multires_ensure_external_read(Mesh *mesh, const int top_level)
{
  if (!CustomData_external_test(&mesh->corner_data, CD_MDISPS)) {
    return;
  }

  /* Modify the data array from the original mesh, not the evaluated mesh.
   * When multiple objects share the same mesh, this can lead to memory leaks. */
  MDisps *mdisps = const_cast<MDisps *>(
      static_cast<const MDisps *>(CustomData_get_layer(&mesh->corner_data, CD_MDISPS)));
  if (mdisps == nullptr) {
    mdisps = static_cast<MDisps *>(
        CustomData_add_layer(&mesh->corner_data, CD_MDISPS, CD_SET_DEFAULT, mesh->corners_num));
  }

  const int totloop = mesh->corners_num;

  for (int i = 0; i < totloop; ++i) {
    if (mdisps[i].level != top_level) {
      MEM_SAFE_FREE(mdisps[i].disps);
    }

    /* NOTE: CustomData_external_read will take care of allocation of displacement vectors if
     * they are missing. */

    const int totdisp = multires_grid_tot[top_level];
    mdisps[i].totdisp = totdisp;
    mdisps[i].level = top_level;
  }

  CustomData_external_read(&mesh->corner_data, &mesh->id, CD_MASK_MDISPS, mesh->corners_num);
}
void multiresModifier_ensure_external_read(Mesh *mesh, const MultiresModifierData *mmd)
{
  multires_ensure_external_read(mesh, mmd->totlvl);
}
