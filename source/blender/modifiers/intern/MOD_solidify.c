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

#include "BLI_utildefines.h"

#include "BLI_bitmap.h"
#include "BLI_math.h"
#include "BLI_utildefines_stack.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_mesh.h"
#include "BKE_particle.h"
#include "BKE_deform.h"

#include "MOD_modifiertypes.h"
#include "MOD_util.h"

#ifdef __GNUC__
#  pragma GCC diagnostic error "-Wsign-conversion"
#endif

/* skip shell thickness for non-manifold edges, see [#35710] */
#define USE_NONMANIFOLD_WORKAROUND

/* *** derived mesh high quality normal calculation function  *** */
/* could be exposed for other functions to use */

typedef struct EdgeFaceRef {
  int p1; /* init as -1 */
  int p2;
} EdgeFaceRef;

BLI_INLINE bool edgeref_is_init(const EdgeFaceRef *edge_ref)
{
  return !((edge_ref->p1 == 0) && (edge_ref->p2 == 0));
}

/**
 * \param dm: Mesh to calculate normals for.
 * \param face_nors: Precalculated face normals.
 * \param r_vert_nors: Return vert normals.
 */
static void mesh_calc_hq_normal(Mesh *mesh, float (*poly_nors)[3], float (*r_vert_nors)[3])
{
  int i, numVerts, numEdges, numPolys;
  MPoly *mpoly, *mp;
  MLoop *mloop, *ml;
  MEdge *medge, *ed;
  MVert *mvert, *mv;

  numVerts = mesh->totvert;
  numEdges = mesh->totedge;
  numPolys = mesh->totpoly;
  mpoly = mesh->mpoly;
  medge = mesh->medge;
  mvert = mesh->mvert;
  mloop = mesh->mloop;

  /* we don't want to overwrite any referenced layers */

  /* Doesn't work here! */
#if 0
  mv = CustomData_duplicate_referenced_layer(&dm->vertData, CD_MVERT, numVerts);
  cddm->mvert = mv;
#endif

  mv = mvert;
  mp = mpoly;

  {
    EdgeFaceRef *edge_ref_array = MEM_calloc_arrayN(
        (size_t)numEdges, sizeof(EdgeFaceRef), "Edge Connectivity");
    EdgeFaceRef *edge_ref;
    float edge_normal[3];

    /* Add an edge reference if it's not there, pointing back to the face index. */
    for (i = 0; i < numPolys; i++, mp++) {
      int j;

      ml = mloop + mp->loopstart;

      for (j = 0; j < mp->totloop; j++, ml++) {
        /* --- add edge ref to face --- */
        edge_ref = &edge_ref_array[ml->e];
        if (!edgeref_is_init(edge_ref)) {
          edge_ref->p1 = i;
          edge_ref->p2 = -1;
        }
        else if ((edge_ref->p1 != -1) && (edge_ref->p2 == -1)) {
          edge_ref->p2 = i;
        }
        else {
          /* 3+ faces using an edge, we can't handle this usefully */
          edge_ref->p1 = edge_ref->p2 = -1;
#ifdef USE_NONMANIFOLD_WORKAROUND
          medge[ml->e].flag |= ME_EDGE_TMP_TAG;
#endif
        }
        /* --- done --- */
      }
    }

    for (i = 0, ed = medge, edge_ref = edge_ref_array; i < numEdges; i++, ed++, edge_ref++) {
      /* Get the edge vert indices, and edge value (the face indices that use it) */

      if (edgeref_is_init(edge_ref) && (edge_ref->p1 != -1)) {
        if (edge_ref->p2 != -1) {
          /* We have 2 faces using this edge, calculate the edges normal
           * using the angle between the 2 faces as a weighting */
#if 0
          add_v3_v3v3(edge_normal, face_nors[edge_ref->f1], face_nors[edge_ref->f2]);
          normalize_v3_length(
              edge_normal,
              angle_normalized_v3v3(face_nors[edge_ref->f1], face_nors[edge_ref->f2]));
#else
          mid_v3_v3v3_angle_weighted(
              edge_normal, poly_nors[edge_ref->p1], poly_nors[edge_ref->p2]);
#endif
        }
        else {
          /* only one face attached to that edge */
          /* an edge without another attached- the weight on this is undefined */
          copy_v3_v3(edge_normal, poly_nors[edge_ref->p1]);
        }
        add_v3_v3(r_vert_nors[ed->v1], edge_normal);
        add_v3_v3(r_vert_nors[ed->v2], edge_normal);
      }
    }
    MEM_freeN(edge_ref_array);
  }

  /* normalize vertex normals and assign */
  for (i = 0; i < numVerts; i++, mv++) {
    if (normalize_v3(r_vert_nors[i]) == 0.0f) {
      normal_short_to_float_v3(r_vert_nors[i], mv->no);
    }
  }
}

static void initData(ModifierData *md)
{
  SolidifyModifierData *smd = (SolidifyModifierData *)md;
  smd->offset = 0.01f;
  smd->offset_fac = -1.0f;
  smd->flag = MOD_SOLIDIFY_RIM;
}

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  SolidifyModifierData *smd = (SolidifyModifierData *)md;

  /* ask for vertexgroups if we need them */
  if (smd->defgrp_name[0] != '\0') {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

/* specific function for solidify - define locally */
BLI_INLINE void madd_v3v3short_fl(float r[3], const short a[3], const float f)
{
  r[0] += (float)a[0] * f;
  r[1] += (float)a[1] * f;
  r[2] += (float)a[2] * f;
}

static Mesh *applyModifier(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  Mesh *result;
  const SolidifyModifierData *smd = (SolidifyModifierData *)md;

  MVert *mv, *mvert, *orig_mvert;
  MEdge *ed, *medge, *orig_medge;
  MLoop *ml, *mloop, *orig_mloop;
  MPoly *mp, *mpoly, *orig_mpoly;
  const unsigned int numVerts = (unsigned int)mesh->totvert;
  const unsigned int numEdges = (unsigned int)mesh->totedge;
  const unsigned int numPolys = (unsigned int)mesh->totpoly;
  const unsigned int numLoops = (unsigned int)mesh->totloop;
  unsigned int newLoops = 0, newPolys = 0, newEdges = 0, newVerts = 0, rimVerts = 0;

  /* only use material offsets if we have 2 or more materials  */
  const short mat_nr_max = ctx->object->totcol > 1 ? ctx->object->totcol - 1 : 0;
  const short mat_ofs = mat_nr_max ? smd->mat_ofs : 0;
  const short mat_ofs_rim = mat_nr_max ? smd->mat_ofs_rim : 0;

  /* use for edges */
  /* over-alloc new_vert_arr, old_vert_arr */
  unsigned int *new_vert_arr = NULL;
  STACK_DECLARE(new_vert_arr);

  unsigned int *new_edge_arr = NULL;
  STACK_DECLARE(new_edge_arr);

  unsigned int *old_vert_arr = MEM_calloc_arrayN(
      numVerts, sizeof(*old_vert_arr), "old_vert_arr in solidify");

  unsigned int *edge_users = NULL;
  char *edge_order = NULL;

  float(*vert_nors)[3] = NULL;
  float(*poly_nors)[3] = NULL;

  const bool need_poly_normals = (smd->flag & MOD_SOLIDIFY_NORMAL_CALC) ||
                                 (smd->flag & MOD_SOLIDIFY_EVEN);

  const float ofs_orig = -(((-smd->offset_fac + 1.0f) * 0.5f) * smd->offset);
  const float ofs_new = smd->offset + ofs_orig;
  const float offset_fac_vg = smd->offset_fac_vg;
  const float offset_fac_vg_inv = 1.0f - smd->offset_fac_vg;
  const bool do_flip = (smd->flag & MOD_SOLIDIFY_FLIP) != 0;
  const bool do_clamp = (smd->offset_clamp != 0.0f);
  const bool do_shell = ((smd->flag & MOD_SOLIDIFY_RIM) && (smd->flag & MOD_SOLIDIFY_NOSHELL)) ==
                        0;

  /* weights */
  MDeformVert *dvert;
  const bool defgrp_invert = (smd->flag & MOD_SOLIDIFY_VGROUP_INV) != 0;
  int defgrp_index;

  /* array size is doubled in case of using a shell */
  const unsigned int stride = do_shell ? 2 : 1;

  MOD_get_vgroup(ctx->object, mesh, smd->defgrp_name, &dvert, &defgrp_index);

  orig_mvert = mesh->mvert;
  orig_medge = mesh->medge;
  orig_mloop = mesh->mloop;
  orig_mpoly = mesh->mpoly;

  if (need_poly_normals) {
    /* calculate only face normals */
    poly_nors = MEM_malloc_arrayN(numPolys, sizeof(*poly_nors), __func__);
    BKE_mesh_calc_normals_poly(orig_mvert,
                               NULL,
                               (int)numVerts,
                               orig_mloop,
                               orig_mpoly,
                               (int)numLoops,
                               (int)numPolys,
                               poly_nors,
                               true);
  }

  STACK_INIT(new_vert_arr, numVerts * 2);
  STACK_INIT(new_edge_arr, numEdges * 2);

  if (smd->flag & MOD_SOLIDIFY_RIM) {
    BLI_bitmap *orig_mvert_tag = BLI_BITMAP_NEW(numVerts, __func__);
    unsigned int eidx;
    unsigned int i;

#define INVALID_UNUSED ((unsigned int)-1)
#define INVALID_PAIR ((unsigned int)-2)

    new_vert_arr = MEM_malloc_arrayN(numVerts, 2 * sizeof(*new_vert_arr), __func__);
    new_edge_arr = MEM_malloc_arrayN(((numEdges * 2) + numVerts), sizeof(*new_edge_arr), __func__);

    edge_users = MEM_malloc_arrayN(numEdges, sizeof(*edge_users), "solid_mod edges");
    edge_order = MEM_malloc_arrayN(numEdges, sizeof(*edge_order), "solid_mod eorder");

    /* save doing 2 loops here... */
#if 0
    copy_vn_i(edge_users, numEdges, INVALID_UNUSED);
#endif

    for (eidx = 0, ed = orig_medge; eidx < numEdges; eidx++, ed++) {
      edge_users[eidx] = INVALID_UNUSED;
    }

    for (i = 0, mp = orig_mpoly; i < numPolys; i++, mp++) {
      MLoop *ml_prev;
      int j;

      ml = orig_mloop + mp->loopstart;
      ml_prev = ml + (mp->totloop - 1);

      for (j = 0; j < mp->totloop; j++, ml++) {
        /* add edge user */
        eidx = ml_prev->e;
        if (edge_users[eidx] == INVALID_UNUSED) {
          ed = orig_medge + eidx;
          BLI_assert(ELEM(ml_prev->v, ed->v1, ed->v2) && ELEM(ml->v, ed->v1, ed->v2));
          edge_users[eidx] = (ml_prev->v > ml->v) == (ed->v1 < ed->v2) ? i : (i + numPolys);
          edge_order[eidx] = j;
        }
        else {
          edge_users[eidx] = INVALID_PAIR;
        }
        ml_prev = ml;
      }
    }

    for (eidx = 0, ed = orig_medge; eidx < numEdges; eidx++, ed++) {
      if (!ELEM(edge_users[eidx], INVALID_UNUSED, INVALID_PAIR)) {
        BLI_BITMAP_ENABLE(orig_mvert_tag, ed->v1);
        BLI_BITMAP_ENABLE(orig_mvert_tag, ed->v2);
        STACK_PUSH(new_edge_arr, eidx);
        newPolys++;
        newLoops += 4;
      }
    }

    for (i = 0; i < numVerts; i++) {
      if (BLI_BITMAP_TEST(orig_mvert_tag, i)) {
        old_vert_arr[i] = STACK_SIZE(new_vert_arr);
        STACK_PUSH(new_vert_arr, i);
        rimVerts++;
      }
      else {
        old_vert_arr[i] = INVALID_UNUSED;
      }
    }

    MEM_freeN(orig_mvert_tag);
  }

  if (do_shell == false) {
    /* only add rim vertices */
    newVerts = rimVerts;
    /* each extruded face needs an opposite edge */
    newEdges = newPolys;
  }
  else {
    /* (stride == 2) in this case, so no need to add newVerts/newEdges */
    BLI_assert(newVerts == 0);
    BLI_assert(newEdges == 0);
  }

  if (smd->flag & MOD_SOLIDIFY_NORMAL_CALC) {
    vert_nors = MEM_calloc_arrayN(numVerts, 3 * sizeof(float), "mod_solid_vno_hq");
    mesh_calc_hq_normal(mesh, poly_nors, vert_nors);
  }

  result = BKE_mesh_new_nomain_from_template(mesh,
                                             (int)((numVerts * stride) + newVerts),
                                             (int)((numEdges * stride) + newEdges + rimVerts),
                                             0,
                                             (int)((numLoops * stride) + newLoops),
                                             (int)((numPolys * stride) + newPolys));

  mpoly = result->mpoly;
  mloop = result->mloop;
  medge = result->medge;
  mvert = result->mvert;

  if (do_shell) {
    CustomData_copy_data(&mesh->vdata, &result->vdata, 0, 0, (int)numVerts);
    CustomData_copy_data(&mesh->vdata, &result->vdata, 0, (int)numVerts, (int)numVerts);

    CustomData_copy_data(&mesh->edata, &result->edata, 0, 0, (int)numEdges);
    CustomData_copy_data(&mesh->edata, &result->edata, 0, (int)numEdges, (int)numEdges);

    CustomData_copy_data(&mesh->ldata, &result->ldata, 0, 0, (int)numLoops);
    /* DO NOT copy here the 'copied' part of loop data, we want to reverse loops
     * (so that winding of copied face get reversed, so that normals get reversed
     * and point in expected direction...).
     * If we also copy data here, then this data get overwritten
     * (and allocated memory becomes memleak). */

    CustomData_copy_data(&mesh->pdata, &result->pdata, 0, 0, (int)numPolys);
    CustomData_copy_data(&mesh->pdata, &result->pdata, 0, (int)numPolys, (int)numPolys);
  }
  else {
    int i, j;
    CustomData_copy_data(&mesh->vdata, &result->vdata, 0, 0, (int)numVerts);
    for (i = 0, j = (int)numVerts; i < numVerts; i++) {
      if (old_vert_arr[i] != INVALID_UNUSED) {
        CustomData_copy_data(&mesh->vdata, &result->vdata, i, j, 1);
        j++;
      }
    }

    CustomData_copy_data(&mesh->edata, &result->edata, 0, 0, (int)numEdges);

    for (i = 0, j = (int)numEdges; i < numEdges; i++) {
      if (!ELEM(edge_users[i], INVALID_UNUSED, INVALID_PAIR)) {
        MEdge *ed_src, *ed_dst;
        CustomData_copy_data(&mesh->edata, &result->edata, i, j, 1);

        ed_src = &medge[i];
        ed_dst = &medge[j];
        ed_dst->v1 = old_vert_arr[ed_src->v1] + numVerts;
        ed_dst->v2 = old_vert_arr[ed_src->v2] + numVerts;
        j++;
      }
    }

    /* will be created later */
    CustomData_copy_data(&mesh->ldata, &result->ldata, 0, 0, (int)numLoops);
    CustomData_copy_data(&mesh->pdata, &result->pdata, 0, 0, (int)numPolys);
  }

#undef INVALID_UNUSED
#undef INVALID_PAIR

  /* initializes: (i_end, do_shell_align, mv)  */
#define INIT_VERT_ARRAY_OFFSETS(test) \
  if (((ofs_new >= ofs_orig) == do_flip) == test) { \
    i_end = numVerts; \
    do_shell_align = true; \
    mv = mvert; \
  } \
  else { \
    if (do_shell) { \
      i_end = numVerts; \
      do_shell_align = true; \
    } \
    else { \
      i_end = newVerts; \
      do_shell_align = false; \
    } \
    mv = &mvert[numVerts]; \
  } \
  (void)0

  /* flip normals */

  if (do_shell) {
    unsigned int i;

    mp = mpoly + numPolys;
    for (i = 0; i < mesh->totpoly; i++, mp++) {
      const int loop_end = mp->totloop - 1;
      MLoop *ml2;
      unsigned int e;
      int j;

      /* reverses the loop direction (MLoop.v as well as custom-data)
       * MLoop.e also needs to be corrected too, done in a separate loop below. */
      ml2 = mloop + mp->loopstart + mesh->totloop;
#if 0
      for (j = 0; j < mp->totloop; j++) {
        CustomData_copy_data(&mesh->ldata,
                             &result->ldata,
                             mp->loopstart + j,
                             mp->loopstart + (loop_end - j) + mesh->totloop,
                             1);
      }
#else
      /* slightly more involved, keep the first vertex the same for the copy,
       * ensures the diagonals in the new face match the original. */
      j = 0;
      for (int j_prev = loop_end; j < mp->totloop; j_prev = j++) {
        CustomData_copy_data(&mesh->ldata,
                             &result->ldata,
                             mp->loopstart + j,
                             mp->loopstart + (loop_end - j_prev) + mesh->totloop,
                             1);
      }
#endif

      if (mat_ofs) {
        mp->mat_nr += mat_ofs;
        CLAMP(mp->mat_nr, 0, mat_nr_max);
      }

      e = ml2[0].e;
      for (j = 0; j < loop_end; j++) {
        ml2[j].e = ml2[j + 1].e;
      }
      ml2[loop_end].e = e;

      mp->loopstart += mesh->totloop;

      for (j = 0; j < mp->totloop; j++) {
        ml2[j].e += numEdges;
        ml2[j].v += numVerts;
      }
    }

    for (i = 0, ed = medge + numEdges; i < numEdges; i++, ed++) {
      ed->v1 += numVerts;
      ed->v2 += numVerts;
    }
  }

  /* note, copied vertex layers don't have flipped normals yet. do this after applying offset */
  if ((smd->flag & MOD_SOLIDIFY_EVEN) == 0) {
    /* no even thickness, very simple */
    float scalar_short;
    float scalar_short_vgroup;

    /* for clamping */
    float *vert_lens = NULL;
    const float offset = fabsf(smd->offset) * smd->offset_clamp;
    const float offset_sq = offset * offset;

    if (do_clamp) {
      unsigned int i;

      vert_lens = MEM_malloc_arrayN(numVerts, sizeof(float), "vert_lens");
      copy_vn_fl(vert_lens, (int)numVerts, FLT_MAX);
      for (i = 0; i < numEdges; i++) {
        const float ed_len_sq = len_squared_v3v3(mvert[medge[i].v1].co, mvert[medge[i].v2].co);
        vert_lens[medge[i].v1] = min_ff(vert_lens[medge[i].v1], ed_len_sq);
        vert_lens[medge[i].v2] = min_ff(vert_lens[medge[i].v2], ed_len_sq);
      }
    }

    if (ofs_new != 0.0f) {
      unsigned int i_orig, i_end;
      bool do_shell_align;

      scalar_short = scalar_short_vgroup = ofs_new / 32767.0f;

      INIT_VERT_ARRAY_OFFSETS(false);

      for (i_orig = 0; i_orig < i_end; i_orig++, mv++) {
        const unsigned int i = do_shell_align ? i_orig : new_vert_arr[i_orig];
        if (dvert) {
          MDeformVert *dv = &dvert[i];
          if (defgrp_invert) {
            scalar_short_vgroup = 1.0f - defvert_find_weight(dv, defgrp_index);
          }
          else {
            scalar_short_vgroup = defvert_find_weight(dv, defgrp_index);
          }
          scalar_short_vgroup = (offset_fac_vg + (scalar_short_vgroup * offset_fac_vg_inv)) *
                                scalar_short;
        }
        if (do_clamp) {
          /* always reset becaise we may have set before */
          if (dvert == NULL) {
            scalar_short_vgroup = scalar_short;
          }
          if (vert_lens[i] < offset_sq) {
            float scalar = sqrtf(vert_lens[i]) / offset;
            scalar_short_vgroup *= scalar;
          }
        }
        madd_v3v3short_fl(mv->co, mv->no, scalar_short_vgroup);
      }
    }

    if (ofs_orig != 0.0f) {
      unsigned int i_orig, i_end;
      bool do_shell_align;

      scalar_short = scalar_short_vgroup = ofs_orig / 32767.0f;

      /* as above but swapped */
      INIT_VERT_ARRAY_OFFSETS(true);

      for (i_orig = 0; i_orig < i_end; i_orig++, mv++) {
        const unsigned int i = do_shell_align ? i_orig : new_vert_arr[i_orig];
        if (dvert) {
          MDeformVert *dv = &dvert[i];
          if (defgrp_invert) {
            scalar_short_vgroup = 1.0f - defvert_find_weight(dv, defgrp_index);
          }
          else {
            scalar_short_vgroup = defvert_find_weight(dv, defgrp_index);
          }
          scalar_short_vgroup = (offset_fac_vg + (scalar_short_vgroup * offset_fac_vg_inv)) *
                                scalar_short;
        }
        if (do_clamp) {
          /* always reset becaise we may have set before */
          if (dvert == NULL) {
            scalar_short_vgroup = scalar_short;
          }
          if (vert_lens[i] < offset_sq) {
            float scalar = sqrtf(vert_lens[i]) / offset;
            scalar_short_vgroup *= scalar;
          }
        }
        madd_v3v3short_fl(mv->co, mv->no, scalar_short_vgroup);
      }
    }

    if (do_clamp) {
      MEM_freeN(vert_lens);
    }
  }
  else {
#ifdef USE_NONMANIFOLD_WORKAROUND
    const bool check_non_manifold = (smd->flag & MOD_SOLIDIFY_NORMAL_CALC) != 0;
#endif
    /* same as EM_solidify() in editmesh_lib.c */
    float *vert_angles = MEM_calloc_arrayN(
        numVerts, 2 * sizeof(float), "mod_solid_pair"); /* 2 in 1 */
    float *vert_accum = vert_angles + numVerts;
    unsigned int vidx;
    unsigned int i;

    if (vert_nors == NULL) {
      vert_nors = MEM_malloc_arrayN(numVerts, 3 * sizeof(float), "mod_solid_vno");
      for (i = 0, mv = mvert; i < numVerts; i++, mv++) {
        normal_short_to_float_v3(vert_nors[i], mv->no);
      }
    }

    for (i = 0, mp = mpoly; i < numPolys; i++, mp++) {
      /* #BKE_mesh_calc_poly_angles logic is inlined here */
      float nor_prev[3];
      float nor_next[3];

      int i_curr = mp->totloop - 1;
      int i_next = 0;

      ml = &mloop[mp->loopstart];

      sub_v3_v3v3(nor_prev, mvert[ml[i_curr - 1].v].co, mvert[ml[i_curr].v].co);
      normalize_v3(nor_prev);

      while (i_next < mp->totloop) {
        float angle;
        sub_v3_v3v3(nor_next, mvert[ml[i_curr].v].co, mvert[ml[i_next].v].co);
        normalize_v3(nor_next);
        angle = angle_normalized_v3v3(nor_prev, nor_next);

        /* --- not related to angle calc --- */
        if (angle < FLT_EPSILON) {
          angle = FLT_EPSILON;
        }

        vidx = ml[i_curr].v;
        vert_accum[vidx] += angle;

#ifdef USE_NONMANIFOLD_WORKAROUND
        /* skip 3+ face user edges */
        if ((check_non_manifold == false) ||
            LIKELY(((orig_medge[ml[i_curr].e].flag & ME_EDGE_TMP_TAG) == 0) &&
                   ((orig_medge[ml[i_next].e].flag & ME_EDGE_TMP_TAG) == 0))) {
          vert_angles[vidx] += shell_v3v3_normalized_to_dist(vert_nors[vidx], poly_nors[i]) *
                               angle;
        }
        else {
          vert_angles[vidx] += angle;
        }
#else
        vert_angles[vidx] += shell_v3v3_normalized_to_dist(vert_nors[vidx], poly_nors[i]) * angle;
#endif
        /* --- end non-angle-calc section --- */

        /* step */
        copy_v3_v3(nor_prev, nor_next);
        i_curr = i_next;
        i_next++;
      }
    }

    /* vertex group support */
    if (dvert) {
      MDeformVert *dv = dvert;
      float scalar;

      if (defgrp_invert) {
        for (i = 0; i < numVerts; i++, dv++) {
          scalar = 1.0f - defvert_find_weight(dv, defgrp_index);
          scalar = offset_fac_vg + (scalar * offset_fac_vg_inv);
          vert_angles[i] *= scalar;
        }
      }
      else {
        for (i = 0; i < numVerts; i++, dv++) {
          scalar = defvert_find_weight(dv, defgrp_index);
          scalar = offset_fac_vg + (scalar * offset_fac_vg_inv);
          vert_angles[i] *= scalar;
        }
      }
    }

    if (do_clamp) {
      float *vert_lens_sq = MEM_malloc_arrayN(numVerts, sizeof(float), "vert_lens");
      const float offset = fabsf(smd->offset) * smd->offset_clamp;
      const float offset_sq = offset * offset;
      copy_vn_fl(vert_lens_sq, (int)numVerts, FLT_MAX);
      for (i = 0; i < numEdges; i++) {
        const float ed_len = len_squared_v3v3(mvert[medge[i].v1].co, mvert[medge[i].v2].co);
        vert_lens_sq[medge[i].v1] = min_ff(vert_lens_sq[medge[i].v1], ed_len);
        vert_lens_sq[medge[i].v2] = min_ff(vert_lens_sq[medge[i].v2], ed_len);
      }
      for (i = 0; i < numVerts; i++) {
        if (vert_lens_sq[i] < offset_sq) {
          float scalar = sqrtf(vert_lens_sq[i]) / offset;
          vert_angles[i] *= scalar;
        }
      }
      MEM_freeN(vert_lens_sq);
    }

    if (ofs_new != 0.0f) {
      unsigned int i_orig, i_end;
      bool do_shell_align;

      INIT_VERT_ARRAY_OFFSETS(false);

      for (i_orig = 0; i_orig < i_end; i_orig++, mv++) {
        const unsigned int i_other = do_shell_align ? i_orig : new_vert_arr[i_orig];
        if (vert_accum[i_other]) { /* zero if unselected */
          madd_v3_v3fl(
              mv->co, vert_nors[i_other], ofs_new * (vert_angles[i_other] / vert_accum[i_other]));
        }
      }
    }

    if (ofs_orig != 0.0f) {
      unsigned int i_orig, i_end;
      bool do_shell_align;

      /* same as above but swapped, intentional use of 'ofs_new' */
      INIT_VERT_ARRAY_OFFSETS(true);

      for (i_orig = 0; i_orig < i_end; i_orig++, mv++) {
        const unsigned int i_other = do_shell_align ? i_orig : new_vert_arr[i_orig];
        if (vert_accum[i_other]) { /* zero if unselected */
          madd_v3_v3fl(
              mv->co, vert_nors[i_other], ofs_orig * (vert_angles[i_other] / vert_accum[i_other]));
        }
      }
    }

    MEM_freeN(vert_angles);
  }

  if (vert_nors) {
    MEM_freeN(vert_nors);
  }

  /* must recalculate normals with vgroups since they can displace unevenly [#26888] */
  if ((mesh->runtime.cd_dirty_vert & CD_MASK_NORMAL) || (smd->flag & MOD_SOLIDIFY_RIM) || dvert) {
    result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
  }
  else if (do_shell) {
    unsigned int i;
    /* flip vertex normals for copied verts */
    mv = mvert + numVerts;
    for (i = 0; i < numVerts; i++, mv++) {
      negate_v3_short(mv->no);
    }
  }

  if (smd->flag & MOD_SOLIDIFY_RIM) {
    unsigned int i;

    /* bugger, need to re-calculate the normals for the new edge faces.
     * This could be done in many ways, but probably the quickest way
     * is to calculate the average normals for side faces only.
     * Then blend them with the normals of the edge verts.
     *
     * at the moment its easiest to allocate an entire array for every vertex,
     * even though we only need edge verts - campbell
     */

#define SOLIDIFY_SIDE_NORMALS

#ifdef SOLIDIFY_SIDE_NORMALS
    /* Note that, due to the code setting cd_dirty_vert a few lines above,
     * do_side_normals is always false. - Sybren */
    const bool do_side_normals = !(result->runtime.cd_dirty_vert & CD_MASK_NORMAL);
    /* annoying to allocate these since we only need the edge verts, */
    float(*edge_vert_nos)[3] = do_side_normals ?
                                   MEM_calloc_arrayN(numVerts, 3 * sizeof(float), __func__) :
                                   NULL;
    float nor[3];
#endif
    const unsigned char crease_rim = smd->crease_rim * 255.0f;
    const unsigned char crease_outer = smd->crease_outer * 255.0f;
    const unsigned char crease_inner = smd->crease_inner * 255.0f;

    int *origindex_edge;
    int *orig_ed;
    unsigned int j;

    if (crease_rim || crease_outer || crease_inner) {
      result->cd_flag |= ME_CDFLAG_EDGE_CREASE;
    }

    /* add faces & edges */
    origindex_edge = CustomData_get_layer(&result->edata, CD_ORIGINDEX);
    orig_ed = (origindex_edge) ? &origindex_edge[(numEdges * stride) + newEdges] : NULL;
    ed = &medge[(numEdges * stride) + newEdges]; /* start after copied edges */
    for (i = 0; i < rimVerts; i++, ed++) {
      ed->v1 = new_vert_arr[i];
      ed->v2 = (do_shell ? new_vert_arr[i] : i) + numVerts;
      ed->flag |= ME_EDGEDRAW | ME_EDGERENDER;

      if (orig_ed) {
        *orig_ed = ORIGINDEX_NONE;
        orig_ed++;
      }

      if (crease_rim) {
        ed->crease = crease_rim;
      }
    }

    /* faces */
    mp = mpoly + (numPolys * stride);
    ml = mloop + (numLoops * stride);
    j = 0;
    for (i = 0; i < newPolys; i++, mp++) {
      unsigned int eidx = new_edge_arr[i];
      unsigned int pidx = edge_users[eidx];
      int k1, k2;
      bool flip;

      if (pidx >= numPolys) {
        pidx -= numPolys;
        flip = true;
      }
      else {
        flip = false;
      }

      ed = medge + eidx;

      /* copy most of the face settings */
      CustomData_copy_data(
          &mesh->pdata, &result->pdata, (int)pidx, (int)((numPolys * stride) + i), 1);
      mp->loopstart = (int)(j + (numLoops * stride));
      mp->flag = mpoly[pidx].flag;

      /* notice we use 'mp->totloop' which is later overwritten,
       * we could lookup the original face but there's no point since this is a copy
       * and will have the same value, just take care when changing order of assignment */

      /* prev loop */
      k1 = mpoly[pidx].loopstart + (((edge_order[eidx] - 1) + mp->totloop) % mp->totloop);

      k2 = mpoly[pidx].loopstart + (edge_order[eidx]);

      mp->totloop = 4;

      CustomData_copy_data(
          &mesh->ldata, &result->ldata, k2, (int)((numLoops * stride) + j + 0), 1);
      CustomData_copy_data(
          &mesh->ldata, &result->ldata, k1, (int)((numLoops * stride) + j + 1), 1);
      CustomData_copy_data(
          &mesh->ldata, &result->ldata, k1, (int)((numLoops * stride) + j + 2), 1);
      CustomData_copy_data(
          &mesh->ldata, &result->ldata, k2, (int)((numLoops * stride) + j + 3), 1);

      if (flip == false) {
        ml[j].v = ed->v1;
        ml[j++].e = eidx;

        ml[j].v = ed->v2;
        ml[j++].e = (numEdges * stride) + old_vert_arr[ed->v2] + newEdges;

        ml[j].v = (do_shell ? ed->v2 : old_vert_arr[ed->v2]) + numVerts;
        ml[j++].e = (do_shell ? eidx : i) + numEdges;

        ml[j].v = (do_shell ? ed->v1 : old_vert_arr[ed->v1]) + numVerts;
        ml[j++].e = (numEdges * stride) + old_vert_arr[ed->v1] + newEdges;
      }
      else {
        ml[j].v = ed->v2;
        ml[j++].e = eidx;

        ml[j].v = ed->v1;
        ml[j++].e = (numEdges * stride) + old_vert_arr[ed->v1] + newEdges;

        ml[j].v = (do_shell ? ed->v1 : old_vert_arr[ed->v1]) + numVerts;
        ml[j++].e = (do_shell ? eidx : i) + numEdges;

        ml[j].v = (do_shell ? ed->v2 : old_vert_arr[ed->v2]) + numVerts;
        ml[j++].e = (numEdges * stride) + old_vert_arr[ed->v2] + newEdges;
      }

      if (origindex_edge) {
        origindex_edge[ml[j - 3].e] = ORIGINDEX_NONE;
        origindex_edge[ml[j - 1].e] = ORIGINDEX_NONE;
      }

      /* use the next material index if option enabled */
      if (mat_ofs_rim) {
        mp->mat_nr += mat_ofs_rim;
        CLAMP(mp->mat_nr, 0, mat_nr_max);
      }
      if (crease_outer) {
        /* crease += crease_outer; without wrapping */
        char *cr = &(ed->crease);
        int tcr = *cr + crease_outer;
        *cr = tcr > 255 ? 255 : tcr;
      }

      if (crease_inner) {
        /* crease += crease_inner; without wrapping */
        char *cr = &(medge[numEdges + (do_shell ? eidx : i)].crease);
        int tcr = *cr + crease_inner;
        *cr = tcr > 255 ? 255 : tcr;
      }

#ifdef SOLIDIFY_SIDE_NORMALS
      if (do_side_normals) {
        normal_quad_v3(nor,
                       mvert[ml[j - 4].v].co,
                       mvert[ml[j - 3].v].co,
                       mvert[ml[j - 2].v].co,
                       mvert[ml[j - 1].v].co);

        add_v3_v3(edge_vert_nos[ed->v1], nor);
        add_v3_v3(edge_vert_nos[ed->v2], nor);
      }
#endif
    }

#ifdef SOLIDIFY_SIDE_NORMALS
    if (do_side_normals) {
      const MEdge *ed_orig = medge;
      ed = medge + (numEdges * stride);
      for (i = 0; i < rimVerts; i++, ed++, ed_orig++) {
        float nor_cpy[3];
        short *nor_short;
        int k;

        /* note, only the first vertex (lower half of the index) is calculated */
        BLI_assert(ed->v1 < numVerts);
        normalize_v3_v3(nor_cpy, edge_vert_nos[ed_orig->v1]);

        for (k = 0; k < 2; k++) { /* loop over both verts of the edge */
          nor_short = mvert[*(&ed->v1 + k)].no;
          normal_short_to_float_v3(nor, nor_short);
          add_v3_v3(nor, nor_cpy);
          normalize_v3(nor);
          normal_float_to_short_v3(nor_short, nor);
        }
      }

      MEM_freeN(edge_vert_nos);
    }
#endif

    MEM_freeN(new_vert_arr);
    MEM_freeN(new_edge_arr);

    MEM_freeN(edge_users);
    MEM_freeN(edge_order);
  }

  if (old_vert_arr) {
    MEM_freeN(old_vert_arr);
  }

  if (poly_nors) {
    MEM_freeN(poly_nors);
  }

  if (numPolys == 0 && numEdges != 0) {
    modifier_setError(md, "Faces needed for useful output");
  }

  return result;
}

#undef SOLIDIFY_SIDE_NORMALS

static bool dependsOnNormals(ModifierData *UNUSED(md))
{
  /* even when we calculate our own normals,
   * the vertex normals are used as a fallback */
  return true;
}

ModifierTypeInfo modifierType_Solidify = {
    /* name */ "Solidify",
    /* structName */ "SolidifyModifierData",
    /* structSize */ sizeof(SolidifyModifierData),
    /* type */ eModifierTypeType_Constructive,

    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_SupportsMapping | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode,

    /* copyData */ modifier_copyData_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* applyModifier */ applyModifier,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ dependsOnNormals,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
};
