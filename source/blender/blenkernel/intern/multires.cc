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
#include "BKE_mesh_legacy_derived_mesh.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_mesh_types.hh"
#include "BKE_modifier.hh"
#include "BKE_multires.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_scene.hh"
#include "BKE_subdiv_ccg.hh"
#include "BKE_subsurf.hh"

#include "BKE_object.hh"

#include "CCGSubSurf.h"

#include "DEG_depsgraph_query.hh"

#include <cmath>
#include <cstring>

/* MULTIRES MODIFIER */
static const int multires_grid_tot[] = {
    0, 4, 9, 25, 81, 289, 1089, 4225, 16641, 66049, 263169, 1050625, 4198401, 16785409};
static const int multires_side_tot[] = {
    0, 2, 3, 5, 9, 17, 33, 65, 129, 257, 513, 1025, 2049, 4097};

/* See multiresModifier_disp_run for description of each operation */
enum DispOp {
  APPLY_DISPLACEMENTS,
  CALC_DISPLACEMENTS,
  ADD_DISPLACEMENTS,
};

static void multiresModifier_disp_run(
    DerivedMesh *dm, Mesh *mesh, DerivedMesh *dm2, DispOp op, CCGElem **oldGridData, int totlvl);

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

/** Grid hiding */
static BLI_bitmap *multires_mdisps_upsample_hidden(BLI_bitmap *lo_hidden,
                                                   const int lo_level,
                                                   const int hi_level,

                                                   /* assumed to be at hi_level (or null) */
                                                   const BLI_bitmap *prev_hidden)
{
  const int hi_gridsize = BKE_ccg_gridsize(hi_level);
  const int lo_gridsize = BKE_ccg_gridsize(lo_level);

  BLI_assert(lo_level <= hi_level);

  /* fast case */
  if (lo_level == hi_level) {
    return static_cast<BLI_bitmap *>(MEM_dupallocN(lo_hidden));
  }

  BLI_bitmap *subd = BLI_BITMAP_NEW(square_i(hi_gridsize), "MDisps.hidden upsample");

  const int factor = BKE_ccg_factor(lo_level, hi_level);
  const int offset = 1 << (hi_level - lo_level - 1);

  /* low-res blocks */
  for (int yl = 0; yl < lo_gridsize; yl++) {
    for (int xl = 0; xl < lo_gridsize; xl++) {
      const int lo_val = BLI_BITMAP_TEST(lo_hidden, yl * lo_gridsize + xl);

      /* high-res blocks */
      for (int yo = -offset; yo <= offset; yo++) {
        const int yh = yl * factor + yo;
        if (yh < 0 || yh >= hi_gridsize) {
          continue;
        }

        for (int xo = -offset; xo <= offset; xo++) {
          const int xh = xl * factor + xo;
          if (xh < 0 || xh >= hi_gridsize) {
            continue;
          }

          const int hi_ndx = yh * hi_gridsize + xh;

          if (prev_hidden) {
            /* If prev_hidden is available, copy it to
             * subd, except when the equivalent element in
             * lo_hidden is different */
            if (lo_val != prev_hidden[hi_ndx]) {
              BLI_BITMAP_SET(subd, hi_ndx, lo_val);
            }
            else {
              BLI_BITMAP_SET(subd, hi_ndx, prev_hidden[hi_ndx]);
            }
          }
          else {
            BLI_BITMAP_SET(subd, hi_ndx, lo_val);
          }
        }
      }
    }
  }

  return subd;
}

static BLI_bitmap *multires_mdisps_downsample_hidden(const BLI_bitmap *old_hidden,
                                                     const int old_level,
                                                     const int new_level)
{
  const int new_gridsize = BKE_ccg_gridsize(new_level);
  const int old_gridsize = BKE_ccg_gridsize(old_level);

  BLI_assert(new_level <= old_level);
  const int factor = BKE_ccg_factor(new_level, old_level);
  BLI_bitmap *new_hidden = BLI_BITMAP_NEW(square_i(new_gridsize), "downsample hidden");

  for (int y = 0; y < new_gridsize; y++) {
    for (int x = 0; x < new_gridsize; x++) {
      const int old_value = BLI_BITMAP_TEST(old_hidden, factor * y * old_gridsize + x * factor);

      BLI_BITMAP_SET(new_hidden, y * new_gridsize + x, old_value);
    }
  }

  return new_hidden;
}

static void multires_output_hidden_to_ccgdm(CCGDerivedMesh *ccgdm, Mesh *mesh, const int level)
{
  const blender::OffsetIndices faces = mesh->faces();
  const MDisps *mdisps = static_cast<const MDisps *>(
      CustomData_get_layer(&mesh->corner_data, CD_MDISPS));
  BLI_bitmap **grid_hidden = ccgdm->gridHidden;

  const int *gridOffset = ccgdm->dm.getGridOffset(&ccgdm->dm);

  for (const int i : faces.index_range()) {
    for (int j = 0; j < faces[i].size(); j++) {
      int g = gridOffset[i] + j;
      const MDisps *md = &mdisps[g];

      if (BLI_bitmap *gh = md->hidden) {
        grid_hidden[g] = multires_mdisps_downsample_hidden(gh, md->level, level);
      }
    }
  }
}

/* subdivide mdisps.hidden if needed (assumes that md.level reflects
 * the current level of md.hidden) */
static void multires_mdisps_subdivide_hidden(MDisps *md, const int new_level)
{

  BLI_assert(md->hidden);

  /* nothing to do if already subdivided enough */
  if (md->level >= new_level) {
    return;
  }

  BLI_bitmap *subd = multires_mdisps_upsample_hidden(md->hidden, md->level, new_level, nullptr);

  /* swap in the subdivided data */
  MEM_freeN(md->hidden);
  md->hidden = subd;
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

static void multires_reallocate_mdisps(const int totloop, MDisps *mdisps, const int lvl)
{

  /* reallocate displacements to be filled in */
  for (int i = 0; i < totloop; i++) {
    const int totdisp = multires_grid_tot[lvl];
    float(*disps)[3] = MEM_calloc_arrayN<float[3]>(totdisp, __func__);

    if (mdisps[i].disps) {
      MEM_freeN(mdisps[i].disps);
    }

    if (mdisps[i].level && mdisps[i].hidden) {
      multires_mdisps_subdivide_hidden(&mdisps[i], lvl);
    }

    mdisps[i].disps = disps;
    mdisps[i].totdisp = totdisp;
    mdisps[i].level = lvl;
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
    const int gridsize = BKE_ccg_gridsize(level);
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

          float(*disps)[3] = MEM_calloc_arrayN<float[3]>(totdisp, "multires disps");

          if (mdisp->disps != nullptr) {
            float(*ndisps)[3] = disps;
            float(*hdisps)[3] = mdisp->disps;

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

static DerivedMesh *subsurf_dm_create_local(Scene *scene,
                                            Object *ob,
                                            DerivedMesh *dm,
                                            const int lvl,
                                            const bool is_simple,
                                            const bool is_optimal,
                                            const bool is_plain_uv,
                                            const bool alloc_paint_mask,
                                            const bool for_render,
                                            SubsurfFlags flags)
{
  SubsurfModifierData smd = {{nullptr}};

  smd.levels = smd.renderLevels = lvl;
  smd.quality = 3;
  if (!is_plain_uv) {
    smd.uv_smooth = SUBSURF_UV_SMOOTH_PRESERVE_BOUNDARIES;
  }
  else {
    smd.uv_smooth = SUBSURF_UV_SMOOTH_NONE;
  }
  if (is_simple) {
    smd.subdivType = ME_SIMPLE_SUBSURF;
  }
  if (is_optimal) {
    smd.flags |= eSubsurfModifierFlag_ControlEdges;
  }

  if (ob->mode & OB_MODE_EDIT) {
    flags |= SUBSURF_IN_EDIT_MODE;
  }
  if (alloc_paint_mask) {
    flags |= SUBSURF_ALLOC_PAINT_MASK;
  }
  if (for_render) {
    flags |= SUBSURF_USE_RENDER_PARAMS;
  }

  return subsurf_make_derived_from_derived(dm, &smd, scene, nullptr, flags);
}

static void grid_tangent(
    const CCGKey &key, const int x, const int y, const int axis, CCGElem *grid, float t[3])
{
  if (axis == 0) {
    if (x == key.grid_size - 1) {
      if (y == key.grid_size - 1) {
        sub_v3_v3v3(
            t, CCG_grid_elem_co(key, grid, x, y - 1), CCG_grid_elem_co(key, grid, x - 1, y - 1));
      }
      else {
        sub_v3_v3v3(t, CCG_grid_elem_co(key, grid, x, y), CCG_grid_elem_co(key, grid, x - 1, y));
      }
    }
    else {
      sub_v3_v3v3(t, CCG_grid_elem_co(key, grid, x + 1, y), CCG_grid_elem_co(key, grid, x, y));
    }
  }
  else if (axis == 1) {
    if (y == key.grid_size - 1) {
      if (x == key.grid_size - 1) {
        sub_v3_v3v3(
            t, CCG_grid_elem_co(key, grid, x - 1, y), CCG_grid_elem_co(key, grid, x - 1, (y - 1)));
      }
      else {
        sub_v3_v3v3(t, CCG_grid_elem_co(key, grid, x, y), CCG_grid_elem_co(key, grid, x, (y - 1)));
      }
    }
    else {
      sub_v3_v3v3(t, CCG_grid_elem_co(key, grid, x, (y + 1)), CCG_grid_elem_co(key, grid, x, y));
    }
  }
}

/* Construct 3x3 tangent-space matrix in 'mat' */
static void grid_tangent_matrix(
    float mat[3][3], const CCGKey &key, const int x, const int y, CCGElem *grid)
{
  grid_tangent(key, x, y, 0, grid, mat[0]);
  normalize_v3(mat[0]);

  grid_tangent(key, x, y, 1, grid, mat[1]);
  normalize_v3(mat[1]);

  copy_v3_v3(mat[2], CCG_grid_elem_no(key, grid, x, y));
}

struct MultiresThreadedData {
  DispOp op;
  CCGElem **gridData, **subGridData;
  CCGKey *key;
  CCGKey *sub_key;
  blender::OffsetIndices<int> faces;
  MDisps *mdisps;
  GridPaintMask *grid_paint_mask;
  int *gridOffset;
  int gridSize, dGridSize, dSkip;
  float (*smat)[3];
};

static void multires_disp_run_cb(void *__restrict userdata,
                                 const int pidx,
                                 const TaskParallelTLS *__restrict /*tls*/)
{
  MultiresThreadedData *tdata = static_cast<MultiresThreadedData *>(userdata);

  const DispOp op = tdata->op;
  CCGElem **gridData = tdata->gridData;
  CCGElem **subGridData = tdata->subGridData;
  const CCGKey key = *tdata->key;
  const blender::OffsetIndices<int> faces = tdata->faces;
  MDisps *mdisps = tdata->mdisps;
  GridPaintMask *grid_paint_mask = tdata->grid_paint_mask;
  const int *gridOffset = tdata->gridOffset;
  const int gridSize = tdata->gridSize;
  const int dGridSize = tdata->dGridSize;
  const int dSkip = tdata->dSkip;

  const int numVerts = faces[pidx].size();
  int gIndex = gridOffset[pidx];

  for (int S = 0; S < numVerts; S++, gIndex++) {
    GridPaintMask *gpm = grid_paint_mask ? &grid_paint_mask[gIndex] : nullptr;
    MDisps *mdisp = &mdisps[faces[pidx][S]];
    CCGElem *grid = gridData[gIndex];
    CCGElem *subgrid = subGridData[gIndex];
    float(*dispgrid)[3] = mdisp->disps;

    /* if needed, reallocate multires paint mask */
    if (gpm && gpm->level < key.level) {
      gpm->level = key.level;
      if (gpm->data) {
        MEM_freeN(gpm->data);
      }
      gpm->data = MEM_calloc_arrayN<float>(key.grid_area, "gpm.data");
    }

    for (int y = 0; y < gridSize; y++) {
      for (int x = 0; x < gridSize; x++) {
        float *co = CCG_grid_elem_co(key, grid, x, y);
        float *sco = CCG_grid_elem_co(key, subgrid, x, y);
        float *data = dispgrid[dGridSize * y * dSkip + x * dSkip];
        float mat[3][3], disp[3], d[3];

        /* construct tangent space matrix */
        grid_tangent_matrix(mat, key, x, y, subgrid);

        switch (op) {
          case APPLY_DISPLACEMENTS:
            /* Convert displacement to object space
             * and add to grid points */
            mul_v3_m3v3(disp, mat, data);
            add_v3_v3v3(co, sco, disp);
            break;
          case CALC_DISPLACEMENTS:
            /* Calculate displacement between new and old
             * grid points and convert to tangent space */
            sub_v3_v3v3(disp, co, sco);
            invert_m3(mat);
            mul_v3_m3v3(data, mat, disp);
            break;
          case ADD_DISPLACEMENTS:
            /* Convert subdivided displacements to tangent
             * space and add to the original displacements */
            invert_m3(mat);
            mul_v3_m3v3(d, mat, co);
            add_v3_v3(data, d);
            break;
        }

        if (gpm) {
          float mask;
          switch (op) {
            case APPLY_DISPLACEMENTS:
              /* Copy mask from gpm to DM */
              CCG_grid_elem_mask(key, grid, x, y) = paint_grid_paint_mask(gpm, key.level, x, y);
              break;
            case CALC_DISPLACEMENTS:
              /* Copy mask from DM to gpm */
              mask = CCG_grid_elem_mask(key, grid, x, y);
              gpm->data[y * gridSize + x] = std::clamp(mask, 0.0f, 1.0f);
              break;
            case ADD_DISPLACEMENTS:
              /* Add mask displacement to gpm */
              gpm->data[y * gridSize + x] += CCG_grid_elem_mask(key, grid, x, y);
              break;
          }
        }
      }
    }
  }
}

/* XXX WARNING: subsurf elements from dm and oldGridData *must* be of the same format (size),
 *              because this code uses CCGKey's info from dm to access oldGridData's normals
 *              (through the call to grid_tangent_matrix())! */
static void multiresModifier_disp_run(DerivedMesh *dm,
                                      Mesh *mesh,
                                      DerivedMesh *dm2,
                                      DispOp op,
                                      CCGElem **oldGridData,
                                      const int totlvl)
{
  CCGDerivedMesh *ccgdm = reinterpret_cast<CCGDerivedMesh *>(dm);
  blender::OffsetIndices faces = mesh->faces();
  MDisps *mdisps = static_cast<MDisps *>(
      CustomData_get_layer_for_write(&mesh->corner_data, CD_MDISPS, mesh->corners_num));
  GridPaintMask *grid_paint_mask = nullptr;
  int totloop, faces_num;

  /* this happens in the dm made by bmesh_mdisps_space_set */
  if (dm2 && CustomData_has_layer(&dm2->loopData, CD_MDISPS)) {
    faces = blender::OffsetIndices(
        blender::Span(dm2->getPolyArray(dm2), dm2->getNumPolys(dm2) + 1));
    mdisps = static_cast<MDisps *>(
        CustomData_get_layer_for_write(&dm2->loopData, CD_MDISPS, dm2->getNumLoops(dm)));
    totloop = dm2->numLoopData;
    faces_num = dm2->numPolyData;
  }
  else {
    totloop = mesh->corners_num;
    faces_num = mesh->faces_num;
  }

  if (!mdisps) {
    if (op == CALC_DISPLACEMENTS) {
      mdisps = static_cast<MDisps *>(
          CustomData_add_layer(&mesh->corner_data, CD_MDISPS, CD_SET_DEFAULT, mesh->corners_num));
    }
    else {
      return;
    }
  }

  // numGrids = dm->getNumGrids(dm); /* UNUSED */
  const int gridSize = dm->getGridSize(dm);
  CCGElem **gridData = dm->getGridData(dm);
  int *gridOffset = dm->getGridOffset(dm);
  CCGKey key;
  dm->getGridKey(dm, &key);
  CCGElem **subGridData = (oldGridData) ? oldGridData : gridData;

  const int dGridSize = multires_side_tot[totlvl];
  const int dSkip = (dGridSize - 1) / (gridSize - 1);

  /* multires paint masks */
  if (key.has_mask) {
    grid_paint_mask = static_cast<GridPaintMask *>(
        CustomData_get_layer_for_write(&mesh->corner_data, CD_GRID_PAINT_MASK, mesh->corners_num));
  }

  /* when adding new faces in edit mode, need to allocate disps */
  for (int i = 0; i < totloop; i++) {
    if (mdisps[i].disps == nullptr) {
      multires_reallocate_mdisps(totloop, mdisps, totlvl);
      break;
    }
  }

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.min_iter_per_thread = CCG_TASK_LIMIT;

  MultiresThreadedData data{};
  data.op = op;
  data.gridData = gridData;
  data.subGridData = subGridData;
  data.key = &key;
  data.faces = faces;
  data.mdisps = mdisps;
  data.grid_paint_mask = grid_paint_mask;
  data.gridOffset = gridOffset;
  data.gridSize = gridSize;
  data.dGridSize = dGridSize;
  data.dSkip = dSkip;

  BLI_task_parallel_range(0, faces_num, &data, multires_disp_run_cb, &settings);

  if (op == APPLY_DISPLACEMENTS) {
    ccgSubSurf_stitchFaces(ccgdm->ss, 0, nullptr, 0);
    ccgSubSurf_updateNormals(ccgdm->ss, nullptr, 0);
  }
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

DerivedMesh *multires_make_derived_from_derived(DerivedMesh *dm,
                                                MultiresModifierData *mmd,
                                                Scene *scene,
                                                Object *ob,
                                                const MultiresFlags flags)
{
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  const bool render = uint8_t(flags & MultiresFlags::UseRenderParams) != 0;
  const bool ignore_simplify = uint8_t(flags & MultiresFlags::IgnoreSimplify) != 0;
  const int lvl = multires_get_level(scene, ob, mmd, render, ignore_simplify);

  if (lvl == 0) {
    return dm;
  }

  const SubsurfFlags subsurf_flags = ignore_simplify ? SUBSURF_IGNORE_SIMPLIFY : SubsurfFlags(0);

  DerivedMesh *result = subsurf_dm_create_local(scene,
                                                ob,
                                                dm,
                                                lvl,
                                                false,
                                                mmd->flags & eMultiresModifierFlag_ControlEdges,
                                                mmd->uv_smooth == SUBSURF_UV_SMOOTH_NONE,
                                                uint8_t(flags & MultiresFlags::AllocPaintMask),
                                                render,
                                                subsurf_flags);

  CCGDerivedMesh *ccgdm = nullptr;
  if (!uint8_t(flags & MultiresFlags::UseLocalMMD)) {
    ccgdm = reinterpret_cast<CCGDerivedMesh *>(result);

    ccgdm->multires.ob = ob;
    ccgdm->multires.mmd = mmd;
    ccgdm->multires.local_mmd = 0;
    ccgdm->multires.lvl = lvl;
    ccgdm->multires.totlvl = mmd->totlvl;
  }

  const int numGrids = result->getNumGrids(result);
  const int gridSize = result->getGridSize(result);
  CCGElem **gridData = result->getGridData(result);
  CCGKey key;
  result->getGridKey(result, &key);

  CCGElem **subGridData = MEM_malloc_arrayN<CCGElem *>(size_t(numGrids), "subGridData*");

  for (int i = 0; i < numGrids; i++) {
    subGridData[i] = static_cast<CCGElem *>(
        MEM_malloc_arrayN(gridSize * gridSize, key.elem_size, "subGridData"));
    memcpy(subGridData[i], gridData[i], key.elem_size * gridSize * gridSize);
  }

  multires_set_tot_mdisps(mesh, mmd->totlvl);
  multiresModifier_ensure_external_read(mesh, mmd);

  /* Run displacement. */
  multiresModifier_disp_run(
      result, static_cast<Mesh *>(ob->data), dm, APPLY_DISPLACEMENTS, subGridData, mmd->totlvl);

  /* copy hidden elements for this level */
  if (ccgdm) {
    multires_output_hidden_to_ccgdm(ccgdm, mesh, lvl);
  }

  for (int i = 0; i < numGrids; i++) {
    MEM_freeN(static_cast<void *>(subGridData[i]));
  }
  MEM_freeN(subGridData);

  return result;
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

/***************** Multires interpolation stuff *****************/

int mdisp_rot_face_to_crn(
    const int face_size, const int face_side, const float u, const float v, float *x, float *y)
{
  const float offset = face_side * 0.5f - 0.5f;
  int S = 0;

  if (face_size == 4) {
    if (u <= offset && v <= offset) {
      S = 0;
    }
    else if (u > offset && v <= offset) {
      S = 1;
    }
    else if (u > offset && v > offset) {
      S = 2;
    }
    else if (u <= offset && v >= offset) {
      S = 3;
    }

    if (S == 0) {
      *y = offset - u;
      *x = offset - v;
    }
    else if (S == 1) {
      *x = u - offset;
      *y = offset - v;
    }
    else if (S == 2) {
      *y = u - offset;
      *x = v - offset;
    }
    else if (S == 3) {
      *x = offset - u;
      *y = v - offset;
    }
  }
  else if (face_size == 3) {
    int grid_size = offset;
    float w = (face_side - 1) - u - v;
    float W1, W2;

    if (u >= v && u >= w) {
      S = 0;
      W1 = w;
      W2 = v;
    }
    else if (v >= u && v >= w) {
      S = 1;
      W1 = u;
      W2 = w;
    }
    else {
      S = 2;
      W1 = v;
      W2 = u;
    }

    W1 /= (face_side - 1);
    W2 /= (face_side - 1);

    *x = (1 - (2 * W1) / (1 - W2)) * grid_size;
    *y = (1 - (2 * W2) / (1 - W1)) * grid_size;
  }
  else {
    /* the complicated ngon case: find the actual coordinate from
     * the barycentric coordinates and finally find the closest vertex
     * should work reliably for convex cases only but better than nothing */

#if 0
    int minS, i;
    float mindist = FLT_MAX;

    for (i = 0; i < poly->totloop; i++) {
      float len = len_v3v3(nullptr, positions[corner_verts[poly->loopstart + i]]);
      if (len < mindist) {
        mindist = len;
        minS = i;
      }
    }
    S = minS;
#endif
    /* temp not implemented yet and also not working properly in current master.
     * (was worked around by subdividing once) */
    S = 0;
    *x = 0;
    *y = 0;
  }

  return S;
}
