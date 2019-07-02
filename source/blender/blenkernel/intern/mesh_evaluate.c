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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 *
 * Functions to evaluate mesh data.
 */

#include <limits.h>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_memarena.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"
#include "BLI_bitmap.h"
#include "BLI_polyfill_2d.h"
#include "BLI_linklist.h"
#include "BLI_linklist_stack.h"
#include "BLI_alloca.h"
#include "BLI_stack.h"
#include "BLI_task.h"

#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"
#include "BKE_report.h"

#include "BLI_strict_flags.h"

#include "atomic_ops.h"
#include "mikktspace.h"

// #define DEBUG_TIME

#include "PIL_time.h"
#ifdef DEBUG_TIME
#  include "PIL_time_utildefines.h"
#endif

static CLG_LogRef LOG = {"bke.mesh_evaluate"};

/* -------------------------------------------------------------------- */
/** \name Mesh Normal Calculation
 * \{ */

/**
 * Call when there are no polygons.
 */
static void mesh_calc_normals_vert_fallback(MVert *mverts, int numVerts)
{
  int i;
  for (i = 0; i < numVerts; i++) {
    MVert *mv = &mverts[i];
    float no[3];

    normalize_v3_v3(no, mv->co);
    normal_float_to_short_v3(mv->no, no);
  }
}

/* TODO(Sybren): we can probably rename this to BKE_mesh_calc_normals_mapping(),
 * and remove the function of the same name below, as that one doesn't seem to be
 * called anywhere. */
void BKE_mesh_calc_normals_mapping_simple(struct Mesh *mesh)
{
  const bool only_face_normals = CustomData_is_referenced_layer(&mesh->vdata, CD_MVERT);

  BKE_mesh_calc_normals_mapping_ex(mesh->mvert,
                                   mesh->totvert,
                                   mesh->mloop,
                                   mesh->mpoly,
                                   mesh->totloop,
                                   mesh->totpoly,
                                   NULL,
                                   mesh->mface,
                                   mesh->totface,
                                   NULL,
                                   NULL,
                                   only_face_normals);
}

/* Calculate vertex and face normals, face normals are returned in *r_faceNors if non-NULL
 * and vertex normals are stored in actual mverts.
 */
void BKE_mesh_calc_normals_mapping(MVert *mverts,
                                   int numVerts,
                                   const MLoop *mloop,
                                   const MPoly *mpolys,
                                   int numLoops,
                                   int numPolys,
                                   float (*r_polyNors)[3],
                                   const MFace *mfaces,
                                   int numFaces,
                                   const int *origIndexFace,
                                   float (*r_faceNors)[3])
{
  BKE_mesh_calc_normals_mapping_ex(mverts,
                                   numVerts,
                                   mloop,
                                   mpolys,
                                   numLoops,
                                   numPolys,
                                   r_polyNors,
                                   mfaces,
                                   numFaces,
                                   origIndexFace,
                                   r_faceNors,
                                   false);
}
/* extended version of 'BKE_mesh_calc_normals_poly' with option not to calc vertex normals */
void BKE_mesh_calc_normals_mapping_ex(MVert *mverts,
                                      int numVerts,
                                      const MLoop *mloop,
                                      const MPoly *mpolys,
                                      int numLoops,
                                      int numPolys,
                                      float (*r_polyNors)[3],
                                      const MFace *mfaces,
                                      int numFaces,
                                      const int *origIndexFace,
                                      float (*r_faceNors)[3],
                                      const bool only_face_normals)
{
  float(*pnors)[3] = r_polyNors, (*fnors)[3] = r_faceNors;
  int i;
  const MFace *mf;
  const MPoly *mp;

  if (numPolys == 0) {
    if (only_face_normals == false) {
      mesh_calc_normals_vert_fallback(mverts, numVerts);
    }
    return;
  }

  /* if we are not calculating verts and no verts were passes then we have nothing to do */
  if ((only_face_normals == true) && (r_polyNors == NULL) && (r_faceNors == NULL)) {
    CLOG_WARN(&LOG, "called with nothing to do");
    return;
  }

  if (!pnors) {
    pnors = MEM_calloc_arrayN((size_t)numPolys, sizeof(float[3]), __func__);
  }
  /* NO NEED TO ALLOC YET */
  /* if (!fnors) fnors = MEM_calloc_arrayN(numFaces, sizeof(float[3]), "face nors mesh.c"); */

  if (only_face_normals == false) {
    /* vertex normals are optional, they require some extra calculations,
     * so make them optional */
    BKE_mesh_calc_normals_poly(
        mverts, NULL, numVerts, mloop, mpolys, numLoops, numPolys, pnors, false);
  }
  else {
    /* only calc poly normals */
    mp = mpolys;
    for (i = 0; i < numPolys; i++, mp++) {
      BKE_mesh_calc_poly_normal(mp, mloop + mp->loopstart, mverts, pnors[i]);
    }
  }

  if (origIndexFace &&
      /* fnors == r_faceNors */ /* NO NEED TO ALLOC YET */
          fnors != NULL &&
      numFaces) {
    mf = mfaces;
    for (i = 0; i < numFaces; i++, mf++, origIndexFace++) {
      if (*origIndexFace < numPolys) {
        copy_v3_v3(fnors[i], pnors[*origIndexFace]);
      }
      else {
        /* eek, we're not corresponding to polys */
        CLOG_ERROR(&LOG, "tessellation face indices are incorrect.  normals may look bad.");
      }
    }
  }

  if (pnors != r_polyNors) {
    MEM_freeN(pnors);
  }
  /* if (fnors != r_faceNors) MEM_freeN(fnors); */ /* NO NEED TO ALLOC YET */

  fnors = pnors = NULL;
}

typedef struct MeshCalcNormalsData {
  const MPoly *mpolys;
  const MLoop *mloop;
  MVert *mverts;
  float (*pnors)[3];
  float (*lnors_weighted)[3];
  float (*vnors)[3];
} MeshCalcNormalsData;

static void mesh_calc_normals_poly_cb(void *__restrict userdata,
                                      const int pidx,
                                      const ParallelRangeTLS *__restrict UNUSED(tls))
{
  MeshCalcNormalsData *data = userdata;
  const MPoly *mp = &data->mpolys[pidx];

  BKE_mesh_calc_poly_normal(mp, data->mloop + mp->loopstart, data->mverts, data->pnors[pidx]);
}

static void mesh_calc_normals_poly_prepare_cb(void *__restrict userdata,
                                              const int pidx,
                                              const ParallelRangeTLS *__restrict UNUSED(tls))
{
  MeshCalcNormalsData *data = userdata;
  const MPoly *mp = &data->mpolys[pidx];
  const MLoop *ml = &data->mloop[mp->loopstart];
  const MVert *mverts = data->mverts;

  float pnor_temp[3];
  float *pnor = data->pnors ? data->pnors[pidx] : pnor_temp;
  float(*lnors_weighted)[3] = data->lnors_weighted;

  const int nverts = mp->totloop;
  float(*edgevecbuf)[3] = BLI_array_alloca(edgevecbuf, (size_t)nverts);
  int i;

  /* Polygon Normal and edge-vector */
  /* inline version of #BKE_mesh_calc_poly_normal, also does edge-vectors */
  {
    int i_prev = nverts - 1;
    const float *v_prev = mverts[ml[i_prev].v].co;
    const float *v_curr;

    zero_v3(pnor);
    /* Newell's Method */
    for (i = 0; i < nverts; i++) {
      v_curr = mverts[ml[i].v].co;
      add_newell_cross_v3_v3v3(pnor, v_prev, v_curr);

      /* Unrelated to normalize, calculate edge-vector */
      sub_v3_v3v3(edgevecbuf[i_prev], v_prev, v_curr);
      normalize_v3(edgevecbuf[i_prev]);
      i_prev = i;

      v_prev = v_curr;
    }
    if (UNLIKELY(normalize_v3(pnor) == 0.0f)) {
      pnor[2] = 1.0f; /* other axes set to 0.0 */
    }
  }

  /* accumulate angle weighted face normal */
  /* inline version of #accumulate_vertex_normals_poly_v3,
   * split between this threaded callback and #mesh_calc_normals_poly_accum_cb. */
  {
    const float *prev_edge = edgevecbuf[nverts - 1];

    for (i = 0; i < nverts; i++) {
      const int lidx = mp->loopstart + i;
      const float *cur_edge = edgevecbuf[i];

      /* calculate angle between the two poly edges incident on
       * this vertex */
      const float fac = saacos(-dot_v3v3(cur_edge, prev_edge));

      /* Store for later accumulation */
      mul_v3_v3fl(lnors_weighted[lidx], pnor, fac);

      prev_edge = cur_edge;
    }
  }
}

static void mesh_calc_normals_poly_finalize_cb(void *__restrict userdata,
                                               const int vidx,
                                               const ParallelRangeTLS *__restrict UNUSED(tls))
{
  MeshCalcNormalsData *data = userdata;

  MVert *mv = &data->mverts[vidx];
  float *no = data->vnors[vidx];

  if (UNLIKELY(normalize_v3(no) == 0.0f)) {
    /* following Mesh convention; we use vertex coordinate itself for normal in this case */
    normalize_v3_v3(no, mv->co);
  }

  normal_float_to_short_v3(mv->no, no);
}

void BKE_mesh_calc_normals_poly(MVert *mverts,
                                float (*r_vertnors)[3],
                                int numVerts,
                                const MLoop *mloop,
                                const MPoly *mpolys,
                                int numLoops,
                                int numPolys,
                                float (*r_polynors)[3],
                                const bool only_face_normals)
{
  float(*pnors)[3] = r_polynors;

  ParallelRangeSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.min_iter_per_thread = 1024;

  if (only_face_normals) {
    BLI_assert((pnors != NULL) || (numPolys == 0));
    BLI_assert(r_vertnors == NULL);

    MeshCalcNormalsData data = {
        .mpolys = mpolys,
        .mloop = mloop,
        .mverts = mverts,
        .pnors = pnors,
    };

    BLI_task_parallel_range(0, numPolys, &data, mesh_calc_normals_poly_cb, &settings);
    return;
  }

  float(*vnors)[3] = r_vertnors;
  float(*lnors_weighted)[3] = MEM_malloc_arrayN(
      (size_t)numLoops, sizeof(*lnors_weighted), __func__);
  bool free_vnors = false;

  /* first go through and calculate normals for all the polys */
  if (vnors == NULL) {
    vnors = MEM_calloc_arrayN((size_t)numVerts, sizeof(*vnors), __func__);
    free_vnors = true;
  }
  else {
    memset(vnors, 0, sizeof(*vnors) * (size_t)numVerts);
  }

  MeshCalcNormalsData data = {
      .mpolys = mpolys,
      .mloop = mloop,
      .mverts = mverts,
      .pnors = pnors,
      .lnors_weighted = lnors_weighted,
      .vnors = vnors,
  };

  /* Compute poly normals, and prepare weighted loop normals. */
  BLI_task_parallel_range(0, numPolys, &data, mesh_calc_normals_poly_prepare_cb, &settings);

  /* Actually accumulate weighted loop normals into vertex ones. */
  /* Unfortunately, not possible to thread that
   * (not in a reasonable, totally lock- and barrier-free fashion),
   * since several loops will point to the same vertex... */
  for (int lidx = 0; lidx < numLoops; lidx++) {
    add_v3_v3(vnors[mloop[lidx].v], data.lnors_weighted[lidx]);
  }

  /* Normalize and validate computed vertex normals. */
  BLI_task_parallel_range(0, numVerts, &data, mesh_calc_normals_poly_finalize_cb, &settings);

  if (free_vnors) {
    MEM_freeN(vnors);
  }
  MEM_freeN(lnors_weighted);
}

void BKE_mesh_ensure_normals(Mesh *mesh)
{
  if (mesh->runtime.cd_dirty_vert & CD_MASK_NORMAL) {
    BKE_mesh_calc_normals(mesh);
  }
  BLI_assert((mesh->runtime.cd_dirty_vert & CD_MASK_NORMAL) == 0);
}

/**
 * Called after calculating all modifiers.
 */
void BKE_mesh_ensure_normals_for_display(Mesh *mesh)
{
  float(*poly_nors)[3] = CustomData_get_layer(&mesh->pdata, CD_NORMAL);
  const bool do_vert_normals = (mesh->runtime.cd_dirty_vert & CD_MASK_NORMAL) != 0;
  const bool do_poly_normals = (mesh->runtime.cd_dirty_poly & CD_MASK_NORMAL || poly_nors == NULL);

  if (do_vert_normals || do_poly_normals) {
    const bool do_add_poly_nors_cddata = (poly_nors == NULL);
    if (do_add_poly_nors_cddata) {
      poly_nors = MEM_malloc_arrayN((size_t)mesh->totpoly, sizeof(*poly_nors), __func__);
    }

    /* calculate poly/vert normals */
    BKE_mesh_calc_normals_poly(mesh->mvert,
                               NULL,
                               mesh->totvert,
                               mesh->mloop,
                               mesh->mpoly,
                               mesh->totloop,
                               mesh->totpoly,
                               poly_nors,
                               !do_vert_normals);

    if (do_add_poly_nors_cddata) {
      CustomData_add_layer(&mesh->pdata, CD_NORMAL, CD_ASSIGN, poly_nors, mesh->totpoly);
    }

    mesh->runtime.cd_dirty_vert &= ~CD_MASK_NORMAL;
    mesh->runtime.cd_dirty_poly &= ~CD_MASK_NORMAL;
  }
}

/* Note that this does not update the CD_NORMAL layer,
 * but does update the normals in the CD_MVERT layer. */
void BKE_mesh_calc_normals(Mesh *mesh)
{
#ifdef DEBUG_TIME
  TIMEIT_START_AVERAGED(BKE_mesh_calc_normals);
#endif
  BKE_mesh_calc_normals_poly(mesh->mvert,
                             NULL,
                             mesh->totvert,
                             mesh->mloop,
                             mesh->mpoly,
                             mesh->totloop,
                             mesh->totpoly,
                             NULL,
                             false);
#ifdef DEBUG_TIME
  TIMEIT_END_AVERAGED(BKE_mesh_calc_normals);
#endif
  mesh->runtime.cd_dirty_vert &= ~CD_MASK_NORMAL;
}

void BKE_mesh_calc_normals_tessface(
    MVert *mverts, int numVerts, const MFace *mfaces, int numFaces, float (*r_faceNors)[3])
{
  float(*tnorms)[3] = MEM_calloc_arrayN((size_t)numVerts, sizeof(*tnorms), "tnorms");
  float(*fnors)[3] = (r_faceNors) ?
                         r_faceNors :
                         MEM_calloc_arrayN((size_t)numFaces, sizeof(*fnors), "meshnormals");
  int i;

  if (!tnorms || !fnors) {
    goto cleanup;
  }

  for (i = 0; i < numFaces; i++) {
    const MFace *mf = &mfaces[i];
    float *f_no = fnors[i];
    float *n4 = (mf->v4) ? tnorms[mf->v4] : NULL;
    const float *c4 = (mf->v4) ? mverts[mf->v4].co : NULL;

    if (mf->v4) {
      normal_quad_v3(
          f_no, mverts[mf->v1].co, mverts[mf->v2].co, mverts[mf->v3].co, mverts[mf->v4].co);
    }
    else {
      normal_tri_v3(f_no, mverts[mf->v1].co, mverts[mf->v2].co, mverts[mf->v3].co);
    }

    accumulate_vertex_normals_v3(tnorms[mf->v1],
                                 tnorms[mf->v2],
                                 tnorms[mf->v3],
                                 n4,
                                 f_no,
                                 mverts[mf->v1].co,
                                 mverts[mf->v2].co,
                                 mverts[mf->v3].co,
                                 c4);
  }

  /* following Mesh convention; we use vertex coordinate itself for normal in this case */
  for (i = 0; i < numVerts; i++) {
    MVert *mv = &mverts[i];
    float *no = tnorms[i];

    if (UNLIKELY(normalize_v3(no) == 0.0f)) {
      normalize_v3_v3(no, mv->co);
    }

    normal_float_to_short_v3(mv->no, no);
  }

cleanup:
  MEM_freeN(tnorms);

  if (fnors != r_faceNors) {
    MEM_freeN(fnors);
  }
}

void BKE_mesh_calc_normals_looptri(MVert *mverts,
                                   int numVerts,
                                   const MLoop *mloop,
                                   const MLoopTri *looptri,
                                   int looptri_num,
                                   float (*r_tri_nors)[3])
{
  float(*tnorms)[3] = MEM_calloc_arrayN((size_t)numVerts, sizeof(*tnorms), "tnorms");
  float(*fnors)[3] = (r_tri_nors) ?
                         r_tri_nors :
                         MEM_calloc_arrayN((size_t)looptri_num, sizeof(*fnors), "meshnormals");
  int i;

  if (!tnorms || !fnors) {
    goto cleanup;
  }

  for (i = 0; i < looptri_num; i++) {
    const MLoopTri *lt = &looptri[i];
    float *f_no = fnors[i];
    const unsigned int vtri[3] = {
        mloop[lt->tri[0]].v,
        mloop[lt->tri[1]].v,
        mloop[lt->tri[2]].v,
    };

    normal_tri_v3(f_no, mverts[vtri[0]].co, mverts[vtri[1]].co, mverts[vtri[2]].co);

    accumulate_vertex_normals_tri_v3(tnorms[vtri[0]],
                                     tnorms[vtri[1]],
                                     tnorms[vtri[2]],
                                     f_no,
                                     mverts[vtri[0]].co,
                                     mverts[vtri[1]].co,
                                     mverts[vtri[2]].co);
  }

  /* following Mesh convention; we use vertex coordinate itself for normal in this case */
  for (i = 0; i < numVerts; i++) {
    MVert *mv = &mverts[i];
    float *no = tnorms[i];

    if (UNLIKELY(normalize_v3(no) == 0.0f)) {
      normalize_v3_v3(no, mv->co);
    }

    normal_float_to_short_v3(mv->no, no);
  }

cleanup:
  MEM_freeN(tnorms);

  if (fnors != r_tri_nors) {
    MEM_freeN(fnors);
  }
}

void BKE_lnor_spacearr_init(MLoopNorSpaceArray *lnors_spacearr,
                            const int numLoops,
                            const char data_type)
{
  if (!(lnors_spacearr->lspacearr && lnors_spacearr->loops_pool)) {
    MemArena *mem;

    if (!lnors_spacearr->mem) {
      lnors_spacearr->mem = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
    }
    mem = lnors_spacearr->mem;
    lnors_spacearr->lspacearr = BLI_memarena_calloc(mem,
                                                    sizeof(MLoopNorSpace *) * (size_t)numLoops);
    lnors_spacearr->loops_pool = BLI_memarena_alloc(mem, sizeof(LinkNode) * (size_t)numLoops);

    lnors_spacearr->num_spaces = 0;
  }
  BLI_assert(ELEM(data_type, MLNOR_SPACEARR_BMLOOP_PTR, MLNOR_SPACEARR_LOOP_INDEX));
  lnors_spacearr->data_type = data_type;
}

void BKE_lnor_spacearr_clear(MLoopNorSpaceArray *lnors_spacearr)
{
  lnors_spacearr->num_spaces = 0;
  lnors_spacearr->lspacearr = NULL;
  lnors_spacearr->loops_pool = NULL;
  BLI_memarena_clear(lnors_spacearr->mem);
}

void BKE_lnor_spacearr_free(MLoopNorSpaceArray *lnors_spacearr)
{
  lnors_spacearr->num_spaces = 0;
  lnors_spacearr->lspacearr = NULL;
  lnors_spacearr->loops_pool = NULL;
  BLI_memarena_free(lnors_spacearr->mem);
  lnors_spacearr->mem = NULL;
}

MLoopNorSpace *BKE_lnor_space_create(MLoopNorSpaceArray *lnors_spacearr)
{
  lnors_spacearr->num_spaces++;
  return BLI_memarena_calloc(lnors_spacearr->mem, sizeof(MLoopNorSpace));
}

/* This threshold is a bit touchy (usual float precision issue), this value seems OK. */
#define LNOR_SPACE_TRIGO_THRESHOLD (1.0f - 1e-4f)

/* Should only be called once.
 * Beware, this modifies ref_vec and other_vec in place!
 * In case no valid space can be generated, ref_alpha and ref_beta are set to zero
 * (which means 'use auto lnors').
 */
void BKE_lnor_space_define(MLoopNorSpace *lnor_space,
                           const float lnor[3],
                           float vec_ref[3],
                           float vec_other[3],
                           BLI_Stack *edge_vectors)
{
  const float pi2 = (float)M_PI * 2.0f;
  float tvec[3], dtp;
  const float dtp_ref = dot_v3v3(vec_ref, lnor);
  const float dtp_other = dot_v3v3(vec_other, lnor);

  if (UNLIKELY(fabsf(dtp_ref) >= LNOR_SPACE_TRIGO_THRESHOLD ||
               fabsf(dtp_other) >= LNOR_SPACE_TRIGO_THRESHOLD)) {
    /* If vec_ref or vec_other are too much aligned with lnor, we can't build lnor space,
     * tag it as invalid and abort. */
    lnor_space->ref_alpha = lnor_space->ref_beta = 0.0f;

    if (edge_vectors) {
      BLI_stack_clear(edge_vectors);
    }
    return;
  }

  copy_v3_v3(lnor_space->vec_lnor, lnor);

  /* Compute ref alpha, average angle of all available edge vectors to lnor. */
  if (edge_vectors) {
    float alpha = 0.0f;
    int nbr = 0;
    while (!BLI_stack_is_empty(edge_vectors)) {
      const float *vec = BLI_stack_peek(edge_vectors);
      alpha += saacosf(dot_v3v3(vec, lnor));
      BLI_stack_discard(edge_vectors);
      nbr++;
    }
    /* Note: In theory, this could be 'nbr > 2',
     *       but there is one case where we only have two edges for two loops:
     *       a smooth vertex with only two edges and two faces (our Monkey's nose has that, e.g.).
     */
    BLI_assert(nbr >= 2); /* This piece of code shall only be called for more than one loop... */
    lnor_space->ref_alpha = alpha / (float)nbr;
  }
  else {
    lnor_space->ref_alpha = (saacosf(dot_v3v3(vec_ref, lnor)) +
                             saacosf(dot_v3v3(vec_other, lnor))) /
                            2.0f;
  }

  /* Project vec_ref on lnor's ortho plane. */
  mul_v3_v3fl(tvec, lnor, dtp_ref);
  sub_v3_v3(vec_ref, tvec);
  normalize_v3_v3(lnor_space->vec_ref, vec_ref);

  cross_v3_v3v3(tvec, lnor, lnor_space->vec_ref);
  normalize_v3_v3(lnor_space->vec_ortho, tvec);

  /* Project vec_other on lnor's ortho plane. */
  mul_v3_v3fl(tvec, lnor, dtp_other);
  sub_v3_v3(vec_other, tvec);
  normalize_v3(vec_other);

  /* Beta is angle between ref_vec and other_vec, around lnor. */
  dtp = dot_v3v3(lnor_space->vec_ref, vec_other);
  if (LIKELY(dtp < LNOR_SPACE_TRIGO_THRESHOLD)) {
    const float beta = saacos(dtp);
    lnor_space->ref_beta = (dot_v3v3(lnor_space->vec_ortho, vec_other) < 0.0f) ? pi2 - beta : beta;
  }
  else {
    lnor_space->ref_beta = pi2;
  }
}

/**
 * Add a new given loop to given lnor_space.
 * Depending on \a lnor_space->data_type, we expect \a bm_loop to be a pointer to BMLoop struct
 * (in case of BMLOOP_PTR), or NULL (in case of LOOP_INDEX), loop index is then stored in pointer.
 * If \a is_single is set, the BMLoop or loop index is directly stored in \a lnor_space->loops
 * pointer (since there is only one loop in this fan),
 * else it is added to the linked list of loops in the fan.
 */
void BKE_lnor_space_add_loop(MLoopNorSpaceArray *lnors_spacearr,
                             MLoopNorSpace *lnor_space,
                             const int ml_index,
                             void *bm_loop,
                             const bool is_single)
{
  BLI_assert((lnors_spacearr->data_type == MLNOR_SPACEARR_LOOP_INDEX && bm_loop == NULL) ||
             (lnors_spacearr->data_type == MLNOR_SPACEARR_BMLOOP_PTR && bm_loop != NULL));

  lnors_spacearr->lspacearr[ml_index] = lnor_space;
  if (bm_loop == NULL) {
    bm_loop = POINTER_FROM_INT(ml_index);
  }
  if (is_single) {
    BLI_assert(lnor_space->loops == NULL);
    lnor_space->flags |= MLNOR_SPACE_IS_SINGLE;
    lnor_space->loops = bm_loop;
  }
  else {
    BLI_assert((lnor_space->flags & MLNOR_SPACE_IS_SINGLE) == 0);
    BLI_linklist_prepend_nlink(&lnor_space->loops, bm_loop, &lnors_spacearr->loops_pool[ml_index]);
  }
}

MINLINE float unit_short_to_float(const short val)
{
  return (float)val / (float)SHRT_MAX;
}

MINLINE short unit_float_to_short(const float val)
{
  /* Rounding... */
  return (short)floorf(val * (float)SHRT_MAX + 0.5f);
}

void BKE_lnor_space_custom_data_to_normal(MLoopNorSpace *lnor_space,
                                          const short clnor_data[2],
                                          float r_custom_lnor[3])
{
  /* NOP custom normal data or invalid lnor space, return. */
  if (clnor_data[0] == 0 || lnor_space->ref_alpha == 0.0f || lnor_space->ref_beta == 0.0f) {
    copy_v3_v3(r_custom_lnor, lnor_space->vec_lnor);
    return;
  }

  {
    /* TODO Check whether using sincosf() gives any noticeable benefit
     *      (could not even get it working under linux though)! */
    const float pi2 = (float)(M_PI * 2.0);
    const float alphafac = unit_short_to_float(clnor_data[0]);
    const float alpha = (alphafac > 0.0f ? lnor_space->ref_alpha : pi2 - lnor_space->ref_alpha) *
                        alphafac;
    const float betafac = unit_short_to_float(clnor_data[1]);

    mul_v3_v3fl(r_custom_lnor, lnor_space->vec_lnor, cosf(alpha));

    if (betafac == 0.0f) {
      madd_v3_v3fl(r_custom_lnor, lnor_space->vec_ref, sinf(alpha));
    }
    else {
      const float sinalpha = sinf(alpha);
      const float beta = (betafac > 0.0f ? lnor_space->ref_beta : pi2 - lnor_space->ref_beta) *
                         betafac;
      madd_v3_v3fl(r_custom_lnor, lnor_space->vec_ref, sinalpha * cosf(beta));
      madd_v3_v3fl(r_custom_lnor, lnor_space->vec_ortho, sinalpha * sinf(beta));
    }
  }
}

void BKE_lnor_space_custom_normal_to_data(MLoopNorSpace *lnor_space,
                                          const float custom_lnor[3],
                                          short r_clnor_data[2])
{
  /* We use null vector as NOP custom normal (can be simpler than giving autocomputed lnor...). */
  if (is_zero_v3(custom_lnor) || compare_v3v3(lnor_space->vec_lnor, custom_lnor, 1e-4f)) {
    r_clnor_data[0] = r_clnor_data[1] = 0;
    return;
  }

  {
    const float pi2 = (float)(M_PI * 2.0);
    const float cos_alpha = dot_v3v3(lnor_space->vec_lnor, custom_lnor);
    float vec[3], cos_beta;
    float alpha;

    alpha = saacosf(cos_alpha);
    if (alpha > lnor_space->ref_alpha) {
      /* Note we could stick to [0, pi] range here,
       * but makes decoding more complex, not worth it. */
      r_clnor_data[0] = unit_float_to_short(-(pi2 - alpha) / (pi2 - lnor_space->ref_alpha));
    }
    else {
      r_clnor_data[0] = unit_float_to_short(alpha / lnor_space->ref_alpha);
    }

    /* Project custom lnor on (vec_ref, vec_ortho) plane. */
    mul_v3_v3fl(vec, lnor_space->vec_lnor, -cos_alpha);
    add_v3_v3(vec, custom_lnor);
    normalize_v3(vec);

    cos_beta = dot_v3v3(lnor_space->vec_ref, vec);

    if (cos_beta < LNOR_SPACE_TRIGO_THRESHOLD) {
      float beta = saacosf(cos_beta);
      if (dot_v3v3(lnor_space->vec_ortho, vec) < 0.0f) {
        beta = pi2 - beta;
      }

      if (beta > lnor_space->ref_beta) {
        r_clnor_data[1] = unit_float_to_short(-(pi2 - beta) / (pi2 - lnor_space->ref_beta));
      }
      else {
        r_clnor_data[1] = unit_float_to_short(beta / lnor_space->ref_beta);
      }
    }
    else {
      r_clnor_data[1] = 0;
    }
  }
}

#define LOOP_SPLIT_TASK_BLOCK_SIZE 1024

typedef struct LoopSplitTaskData {
  /* Specific to each instance (each task). */

  /** We have to create those outside of tasks, since afaik memarena is not threadsafe. */
  MLoopNorSpace *lnor_space;
  float (*lnor)[3];
  const MLoop *ml_curr;
  const MLoop *ml_prev;
  int ml_curr_index;
  int ml_prev_index;
  /** Also used a flag to switch between single or fan process! */
  const int *e2l_prev;
  int mp_index;

  /** This one is special, it's owned and managed by worker tasks,
   * avoid to have to create it for each fan! */
  BLI_Stack *edge_vectors;

  char pad_c;
} LoopSplitTaskData;

typedef struct LoopSplitTaskDataCommon {
  /* Read/write.
   * Note we do not need to protect it, though, since two different tasks will *always* affect
   * different elements in the arrays. */
  MLoopNorSpaceArray *lnors_spacearr;
  float (*loopnors)[3];
  short (*clnors_data)[2];

  /* Read-only. */
  const MVert *mverts;
  const MEdge *medges;
  const MLoop *mloops;
  const MPoly *mpolys;
  int (*edge_to_loops)[2];
  int *loop_to_poly;
  const float (*polynors)[3];

  int numEdges;
  int numLoops;
  int numPolys;
} LoopSplitTaskDataCommon;

#define INDEX_UNSET INT_MIN
#define INDEX_INVALID -1
/* See comment about edge_to_loops below. */
#define IS_EDGE_SHARP(_e2l) (ELEM((_e2l)[1], INDEX_UNSET, INDEX_INVALID))

static void mesh_edges_sharp_tag(LoopSplitTaskDataCommon *data,
                                 const bool check_angle,
                                 const float split_angle,
                                 const bool do_sharp_edges_tag)
{
  const MVert *mverts = data->mverts;
  const MEdge *medges = data->medges;
  const MLoop *mloops = data->mloops;

  const MPoly *mpolys = data->mpolys;

  const int numEdges = data->numEdges;
  const int numPolys = data->numPolys;

  float(*loopnors)[3] = data->loopnors; /* Note: loopnors may be NULL here. */
  const float(*polynors)[3] = data->polynors;

  int(*edge_to_loops)[2] = data->edge_to_loops;
  int *loop_to_poly = data->loop_to_poly;

  BLI_bitmap *sharp_edges = do_sharp_edges_tag ? BLI_BITMAP_NEW(numEdges, __func__) : NULL;

  const MPoly *mp;
  int mp_index;

  const float split_angle_cos = check_angle ? cosf(split_angle) : -1.0f;

  for (mp = mpolys, mp_index = 0; mp_index < numPolys; mp++, mp_index++) {
    const MLoop *ml_curr;
    int *e2l;
    int ml_curr_index = mp->loopstart;
    const int ml_last_index = (ml_curr_index + mp->totloop) - 1;

    ml_curr = &mloops[ml_curr_index];

    for (; ml_curr_index <= ml_last_index; ml_curr++, ml_curr_index++) {
      e2l = edge_to_loops[ml_curr->e];

      loop_to_poly[ml_curr_index] = mp_index;

      /* Pre-populate all loop normals as if their verts were all-smooth,
       * this way we don't have to compute those later!
       */
      if (loopnors) {
        normal_short_to_float_v3(loopnors[ml_curr_index], mverts[ml_curr->v].no);
      }

      /* Check whether current edge might be smooth or sharp */
      if ((e2l[0] | e2l[1]) == 0) {
        /* 'Empty' edge until now, set e2l[0] (and e2l[1] to INDEX_UNSET to tag it as unset). */
        e2l[0] = ml_curr_index;
        /* We have to check this here too, else we might miss some flat faces!!! */
        e2l[1] = (mp->flag & ME_SMOOTH) ? INDEX_UNSET : INDEX_INVALID;
      }
      else if (e2l[1] == INDEX_UNSET) {
        const bool is_angle_sharp = (check_angle &&
                                     dot_v3v3(polynors[loop_to_poly[e2l[0]]], polynors[mp_index]) <
                                         split_angle_cos);

        /* Second loop using this edge, time to test its sharpness.
         * An edge is sharp if it is tagged as such, or its face is not smooth,
         * or both poly have opposed (flipped) normals, i.e. both loops on the same edge share the
         * same vertex, or angle between both its polys' normals is above split_angle value.
         */
        if (!(mp->flag & ME_SMOOTH) || (medges[ml_curr->e].flag & ME_SHARP) ||
            ml_curr->v == mloops[e2l[0]].v || is_angle_sharp) {
          /* Note: we are sure that loop != 0 here ;) */
          e2l[1] = INDEX_INVALID;

          /* We want to avoid tagging edges as sharp when it is already defined as such by
           * other causes than angle threshold... */
          if (do_sharp_edges_tag && is_angle_sharp) {
            BLI_BITMAP_SET(sharp_edges, ml_curr->e, true);
          }
        }
        else {
          e2l[1] = ml_curr_index;
        }
      }
      else if (!IS_EDGE_SHARP(e2l)) {
        /* More than two loops using this edge, tag as sharp if not yet done. */
        e2l[1] = INDEX_INVALID;

        /* We want to avoid tagging edges as sharp when it is already defined as such by
         * other causes than angle threshold... */
        if (do_sharp_edges_tag) {
          BLI_BITMAP_SET(sharp_edges, ml_curr->e, false);
        }
      }
      /* Else, edge is already 'disqualified' (i.e. sharp)! */
    }
  }

  /* If requested, do actual tagging of edges as sharp in another loop. */
  if (do_sharp_edges_tag) {
    MEdge *me;
    int me_index;
    for (me = (MEdge *)medges, me_index = 0; me_index < numEdges; me++, me_index++) {
      if (BLI_BITMAP_TEST(sharp_edges, me_index)) {
        me->flag |= ME_SHARP;
      }
    }

    MEM_freeN(sharp_edges);
  }
}

/** Define sharp edges as needed to mimic 'autosmooth' from angle threshold.
 *
 * Used when defining an empty custom loop normals data layer,
 * to keep same shading as with autosmooth!
 */
void BKE_edges_sharp_from_angle_set(const struct MVert *mverts,
                                    const int UNUSED(numVerts),
                                    struct MEdge *medges,
                                    const int numEdges,
                                    struct MLoop *mloops,
                                    const int numLoops,
                                    struct MPoly *mpolys,
                                    const float (*polynors)[3],
                                    const int numPolys,
                                    const float split_angle)
{
  if (split_angle >= (float)M_PI) {
    /* Nothing to do! */
    return;
  }

  /* Mapping edge -> loops. See BKE_mesh_normals_loop_split() for details. */
  int(*edge_to_loops)[2] = MEM_calloc_arrayN((size_t)numEdges, sizeof(*edge_to_loops), __func__);

  /* Simple mapping from a loop to its polygon index. */
  int *loop_to_poly = MEM_malloc_arrayN((size_t)numLoops, sizeof(*loop_to_poly), __func__);

  LoopSplitTaskDataCommon common_data = {
      .mverts = mverts,
      .medges = medges,
      .mloops = mloops,
      .mpolys = mpolys,
      .edge_to_loops = edge_to_loops,
      .loop_to_poly = loop_to_poly,
      .polynors = polynors,
      .numEdges = numEdges,
      .numPolys = numPolys,
  };

  mesh_edges_sharp_tag(&common_data, true, split_angle, true);

  MEM_freeN(edge_to_loops);
  MEM_freeN(loop_to_poly);
}

void BKE_mesh_loop_manifold_fan_around_vert_next(const MLoop *mloops,
                                                 const MPoly *mpolys,
                                                 const int *loop_to_poly,
                                                 const int *e2lfan_curr,
                                                 const uint mv_pivot_index,
                                                 const MLoop **r_mlfan_curr,
                                                 int *r_mlfan_curr_index,
                                                 int *r_mlfan_vert_index,
                                                 int *r_mpfan_curr_index)
{
  const MLoop *mlfan_next;
  const MPoly *mpfan_next;

  /* Warning! This is rather complex!
   * We have to find our next edge around the vertex (fan mode).
   * First we find the next loop, which is either previous or next to mlfan_curr_index, depending
   * whether both loops using current edge are in the same direction or not, and whether
   * mlfan_curr_index actually uses the vertex we are fanning around!
   * mlfan_curr_index is the index of mlfan_next here, and mlfan_next is not the real next one
   * (i.e. not the future mlfan_curr)...
   */
  *r_mlfan_curr_index = (e2lfan_curr[0] == *r_mlfan_curr_index) ? e2lfan_curr[1] : e2lfan_curr[0];
  *r_mpfan_curr_index = loop_to_poly[*r_mlfan_curr_index];

  BLI_assert(*r_mlfan_curr_index >= 0);
  BLI_assert(*r_mpfan_curr_index >= 0);

  mlfan_next = &mloops[*r_mlfan_curr_index];
  mpfan_next = &mpolys[*r_mpfan_curr_index];
  if (((*r_mlfan_curr)->v == mlfan_next->v && (*r_mlfan_curr)->v == mv_pivot_index) ||
      ((*r_mlfan_curr)->v != mlfan_next->v && (*r_mlfan_curr)->v != mv_pivot_index)) {
    /* We need the previous loop, but current one is our vertex's loop. */
    *r_mlfan_vert_index = *r_mlfan_curr_index;
    if (--(*r_mlfan_curr_index) < mpfan_next->loopstart) {
      *r_mlfan_curr_index = mpfan_next->loopstart + mpfan_next->totloop - 1;
    }
  }
  else {
    /* We need the next loop, which is also our vertex's loop. */
    if (++(*r_mlfan_curr_index) >= mpfan_next->loopstart + mpfan_next->totloop) {
      *r_mlfan_curr_index = mpfan_next->loopstart;
    }
    *r_mlfan_vert_index = *r_mlfan_curr_index;
  }
  *r_mlfan_curr = &mloops[*r_mlfan_curr_index];
  /* And now we are back in sync, mlfan_curr_index is the index of mlfan_curr! Pff! */
}

static void split_loop_nor_single_do(LoopSplitTaskDataCommon *common_data, LoopSplitTaskData *data)
{
  MLoopNorSpaceArray *lnors_spacearr = common_data->lnors_spacearr;
  short(*clnors_data)[2] = common_data->clnors_data;

  const MVert *mverts = common_data->mverts;
  const MEdge *medges = common_data->medges;
  const float(*polynors)[3] = common_data->polynors;

  MLoopNorSpace *lnor_space = data->lnor_space;
  float(*lnor)[3] = data->lnor;
  const MLoop *ml_curr = data->ml_curr;
  const MLoop *ml_prev = data->ml_prev;
  const int ml_curr_index = data->ml_curr_index;
#if 0 /* Not needed for 'single' loop. */
  const int ml_prev_index = data->ml_prev_index;
  const int *e2l_prev = data->e2l_prev;
#endif
  const int mp_index = data->mp_index;

  /* Simple case (both edges around that vertex are sharp in current polygon),
   * this loop just takes its poly normal.
   */
  copy_v3_v3(*lnor, polynors[mp_index]);

#if 0
  printf("BASIC: handling loop %d / edge %d / vert %d / poly %d\n",
         ml_curr_index,
         ml_curr->e,
         ml_curr->v,
         mp_index);
#endif

  /* If needed, generate this (simple!) lnor space. */
  if (lnors_spacearr) {
    float vec_curr[3], vec_prev[3];

    const unsigned int mv_pivot_index = ml_curr->v; /* The vertex we are "fanning" around! */
    const MVert *mv_pivot = &mverts[mv_pivot_index];
    const MEdge *me_curr = &medges[ml_curr->e];
    const MVert *mv_2 = (me_curr->v1 == mv_pivot_index) ? &mverts[me_curr->v2] :
                                                          &mverts[me_curr->v1];
    const MEdge *me_prev = &medges[ml_prev->e];
    const MVert *mv_3 = (me_prev->v1 == mv_pivot_index) ? &mverts[me_prev->v2] :
                                                          &mverts[me_prev->v1];

    sub_v3_v3v3(vec_curr, mv_2->co, mv_pivot->co);
    normalize_v3(vec_curr);
    sub_v3_v3v3(vec_prev, mv_3->co, mv_pivot->co);
    normalize_v3(vec_prev);

    BKE_lnor_space_define(lnor_space, *lnor, vec_curr, vec_prev, NULL);
    /* We know there is only one loop in this space,
     * no need to create a linklist in this case... */
    BKE_lnor_space_add_loop(lnors_spacearr, lnor_space, ml_curr_index, NULL, true);

    if (clnors_data) {
      BKE_lnor_space_custom_data_to_normal(lnor_space, clnors_data[ml_curr_index], *lnor);
    }
  }
}

static void split_loop_nor_fan_do(LoopSplitTaskDataCommon *common_data, LoopSplitTaskData *data)
{
  MLoopNorSpaceArray *lnors_spacearr = common_data->lnors_spacearr;
  float(*loopnors)[3] = common_data->loopnors;
  short(*clnors_data)[2] = common_data->clnors_data;

  const MVert *mverts = common_data->mverts;
  const MEdge *medges = common_data->medges;
  const MLoop *mloops = common_data->mloops;
  const MPoly *mpolys = common_data->mpolys;
  const int(*edge_to_loops)[2] = common_data->edge_to_loops;
  const int *loop_to_poly = common_data->loop_to_poly;
  const float(*polynors)[3] = common_data->polynors;

  MLoopNorSpace *lnor_space = data->lnor_space;
#if 0 /* Not needed for 'fan' loops. */
  float(*lnor)[3] = data->lnor;
#endif
  const MLoop *ml_curr = data->ml_curr;
  const MLoop *ml_prev = data->ml_prev;
  const int ml_curr_index = data->ml_curr_index;
  const int ml_prev_index = data->ml_prev_index;
  const int mp_index = data->mp_index;
  const int *e2l_prev = data->e2l_prev;

  BLI_Stack *edge_vectors = data->edge_vectors;

  /* Gah... We have to fan around current vertex, until we find the other non-smooth edge,
   * and accumulate face normals into the vertex!
   * Note in case this vertex has only one sharp edges, this is a waste because the normal is the
   * same as the vertex normal, but I do not see any easy way to detect that (would need to count
   * number of sharp edges per vertex, I doubt the additional memory usage would be worth it,
   * especially as it should not be a common case in real-life meshes anyway).
   */
  const unsigned int mv_pivot_index = ml_curr->v; /* The vertex we are "fanning" around! */
  const MVert *mv_pivot = &mverts[mv_pivot_index];

  /* ml_curr would be mlfan_prev if we needed that one. */
  const MEdge *me_org = &medges[ml_curr->e];

  const int *e2lfan_curr;
  float vec_curr[3], vec_prev[3], vec_org[3];
  const MLoop *mlfan_curr;
  float lnor[3] = {0.0f, 0.0f, 0.0f};
  /* mlfan_vert_index: the loop of our current edge might not be the loop of our current vertex! */
  int mlfan_curr_index, mlfan_vert_index, mpfan_curr_index;

  /* We validate clnors data on the fly - cheapest way to do! */
  int clnors_avg[2] = {0, 0};
  short(*clnor_ref)[2] = NULL;
  int clnors_nbr = 0;
  bool clnors_invalid = false;

  /* Temp loop normal stack. */
  BLI_SMALLSTACK_DECLARE(normal, float *);
  /* Temp clnors stack. */
  BLI_SMALLSTACK_DECLARE(clnors, short *);

  e2lfan_curr = e2l_prev;
  mlfan_curr = ml_prev;
  mlfan_curr_index = ml_prev_index;
  mlfan_vert_index = ml_curr_index;
  mpfan_curr_index = mp_index;

  BLI_assert(mlfan_curr_index >= 0);
  BLI_assert(mlfan_vert_index >= 0);
  BLI_assert(mpfan_curr_index >= 0);

  /* Only need to compute previous edge's vector once, then we can just reuse old current one! */
  {
    const MVert *mv_2 = (me_org->v1 == mv_pivot_index) ? &mverts[me_org->v2] : &mverts[me_org->v1];

    sub_v3_v3v3(vec_org, mv_2->co, mv_pivot->co);
    normalize_v3(vec_org);
    copy_v3_v3(vec_prev, vec_org);

    if (lnors_spacearr) {
      BLI_stack_push(edge_vectors, vec_org);
    }
  }

  //  printf("FAN: vert %d, start edge %d\n", mv_pivot_index, ml_curr->e);

  while (true) {
    const MEdge *me_curr = &medges[mlfan_curr->e];
    /* Compute edge vectors.
     * NOTE: We could pre-compute those into an array, in the first iteration, instead of computing
     *       them twice (or more) here. However, time gained is not worth memory and time lost,
     *       given the fact that this code should not be called that much in real-life meshes...
     */
    {
      const MVert *mv_2 = (me_curr->v1 == mv_pivot_index) ? &mverts[me_curr->v2] :
                                                            &mverts[me_curr->v1];

      sub_v3_v3v3(vec_curr, mv_2->co, mv_pivot->co);
      normalize_v3(vec_curr);
    }

    //      printf("\thandling edge %d / loop %d\n", mlfan_curr->e, mlfan_curr_index);

    {
      /* Code similar to accumulate_vertex_normals_poly_v3. */
      /* Calculate angle between the two poly edges incident on this vertex. */
      const float fac = saacos(dot_v3v3(vec_curr, vec_prev));
      /* Accumulate */
      madd_v3_v3fl(lnor, polynors[mpfan_curr_index], fac);

      if (clnors_data) {
        /* Accumulate all clnors, if they are not all equal we have to fix that! */
        short(*clnor)[2] = &clnors_data[mlfan_vert_index];
        if (clnors_nbr) {
          clnors_invalid |= ((*clnor_ref)[0] != (*clnor)[0] || (*clnor_ref)[1] != (*clnor)[1]);
        }
        else {
          clnor_ref = clnor;
        }
        clnors_avg[0] += (*clnor)[0];
        clnors_avg[1] += (*clnor)[1];
        clnors_nbr++;
        /* We store here a pointer to all custom lnors processed. */
        BLI_SMALLSTACK_PUSH(clnors, (short *)*clnor);
      }
    }

    /* We store here a pointer to all loop-normals processed. */
    BLI_SMALLSTACK_PUSH(normal, (float *)(loopnors[mlfan_vert_index]));

    if (lnors_spacearr) {
      /* Assign current lnor space to current 'vertex' loop. */
      BKE_lnor_space_add_loop(lnors_spacearr, lnor_space, mlfan_vert_index, NULL, false);
      if (me_curr != me_org) {
        /* We store here all edges-normalized vectors processed. */
        BLI_stack_push(edge_vectors, vec_curr);
      }
    }

    if (IS_EDGE_SHARP(e2lfan_curr) || (me_curr == me_org)) {
      /* Current edge is sharp and we have finished with this fan of faces around this vert,
       * or this vert is smooth, and we have completed a full turn around it.
       */
      //          printf("FAN: Finished!\n");
      break;
    }

    copy_v3_v3(vec_prev, vec_curr);

    /* Find next loop of the smooth fan. */
    BKE_mesh_loop_manifold_fan_around_vert_next(mloops,
                                                mpolys,
                                                loop_to_poly,
                                                e2lfan_curr,
                                                mv_pivot_index,
                                                &mlfan_curr,
                                                &mlfan_curr_index,
                                                &mlfan_vert_index,
                                                &mpfan_curr_index);

    e2lfan_curr = edge_to_loops[mlfan_curr->e];
  }

  {
    float lnor_len = normalize_v3(lnor);

    /* If we are generating lnor spacearr, we can now define the one for this fan,
     * and optionally compute final lnor from custom data too!
     */
    if (lnors_spacearr) {
      if (UNLIKELY(lnor_len == 0.0f)) {
        /* Use vertex normal as fallback! */
        copy_v3_v3(lnor, loopnors[mlfan_vert_index]);
        lnor_len = 1.0f;
      }

      BKE_lnor_space_define(lnor_space, lnor, vec_org, vec_curr, edge_vectors);

      if (clnors_data) {
        if (clnors_invalid) {
          short *clnor;

          clnors_avg[0] /= clnors_nbr;
          clnors_avg[1] /= clnors_nbr;
          /* Fix/update all clnors of this fan with computed average value. */
          if (G.debug & G_DEBUG) {
            printf("Invalid clnors in this fan!\n");
          }
          while ((clnor = BLI_SMALLSTACK_POP(clnors))) {
            // print_v2("org clnor", clnor);
            clnor[0] = (short)clnors_avg[0];
            clnor[1] = (short)clnors_avg[1];
          }
          // print_v2("new clnors", clnors_avg);
        }
        /* Extra bonus: since smallstack is local to this func,
         * no more need to empty it at all cost! */

        BKE_lnor_space_custom_data_to_normal(lnor_space, *clnor_ref, lnor);
      }
    }

    /* In case we get a zero normal here, just use vertex normal already set! */
    if (LIKELY(lnor_len != 0.0f)) {
      /* Copy back the final computed normal into all related loop-normals. */
      float *nor;

      while ((nor = BLI_SMALLSTACK_POP(normal))) {
        copy_v3_v3(nor, lnor);
      }
    }
    /* Extra bonus: since smallstack is local to this func,
     * no more need to empty it at all cost! */
  }
}

static void loop_split_worker_do(LoopSplitTaskDataCommon *common_data,
                                 LoopSplitTaskData *data,
                                 BLI_Stack *edge_vectors)
{
  BLI_assert(data->ml_curr);
  if (data->e2l_prev) {
    BLI_assert((edge_vectors == NULL) || BLI_stack_is_empty(edge_vectors));
    data->edge_vectors = edge_vectors;
    split_loop_nor_fan_do(common_data, data);
  }
  else {
    /* No need for edge_vectors for 'single' case! */
    split_loop_nor_single_do(common_data, data);
  }
}

static void loop_split_worker(TaskPool *__restrict pool, void *taskdata, int UNUSED(threadid))
{
  LoopSplitTaskDataCommon *common_data = BLI_task_pool_userdata(pool);
  LoopSplitTaskData *data = taskdata;

  /* Temp edge vectors stack, only used when computing lnor spacearr. */
  BLI_Stack *edge_vectors = common_data->lnors_spacearr ?
                                BLI_stack_new(sizeof(float[3]), __func__) :
                                NULL;

#ifdef DEBUG_TIME
  TIMEIT_START_AVERAGED(loop_split_worker);
#endif

  for (int i = 0; i < LOOP_SPLIT_TASK_BLOCK_SIZE; i++, data++) {
    /* A NULL ml_curr is used to tag ended data! */
    if (data->ml_curr == NULL) {
      break;
    }

    loop_split_worker_do(common_data, data, edge_vectors);
  }

  if (edge_vectors) {
    BLI_stack_free(edge_vectors);
  }

#ifdef DEBUG_TIME
  TIMEIT_END_AVERAGED(loop_split_worker);
#endif
}

/**
 * Check whether given loop is part of an unknown-so-far cyclic smooth fan, or not.
 * Needed because cyclic smooth fans have no obvious 'entry point',
 * and yet we need to walk them once, and only once.
 */
static bool loop_split_generator_check_cyclic_smooth_fan(const MLoop *mloops,
                                                         const MPoly *mpolys,
                                                         const int (*edge_to_loops)[2],
                                                         const int *loop_to_poly,
                                                         const int *e2l_prev,
                                                         BLI_bitmap *skip_loops,
                                                         const MLoop *ml_curr,
                                                         const MLoop *ml_prev,
                                                         const int ml_curr_index,
                                                         const int ml_prev_index,
                                                         const int mp_curr_index)
{
  const unsigned int mv_pivot_index = ml_curr->v; /* The vertex we are "fanning" around! */
  const int *e2lfan_curr;
  const MLoop *mlfan_curr;
  /* mlfan_vert_index: the loop of our current edge might not be the loop of our current vertex! */
  int mlfan_curr_index, mlfan_vert_index, mpfan_curr_index;

  e2lfan_curr = e2l_prev;
  if (IS_EDGE_SHARP(e2lfan_curr)) {
    /* Sharp loop, so not a cyclic smooth fan... */
    return false;
  }

  mlfan_curr = ml_prev;
  mlfan_curr_index = ml_prev_index;
  mlfan_vert_index = ml_curr_index;
  mpfan_curr_index = mp_curr_index;

  BLI_assert(mlfan_curr_index >= 0);
  BLI_assert(mlfan_vert_index >= 0);
  BLI_assert(mpfan_curr_index >= 0);

  BLI_assert(!BLI_BITMAP_TEST(skip_loops, mlfan_vert_index));
  BLI_BITMAP_ENABLE(skip_loops, mlfan_vert_index);

  while (true) {
    /* Find next loop of the smooth fan. */
    BKE_mesh_loop_manifold_fan_around_vert_next(mloops,
                                                mpolys,
                                                loop_to_poly,
                                                e2lfan_curr,
                                                mv_pivot_index,
                                                &mlfan_curr,
                                                &mlfan_curr_index,
                                                &mlfan_vert_index,
                                                &mpfan_curr_index);

    e2lfan_curr = edge_to_loops[mlfan_curr->e];

    if (IS_EDGE_SHARP(e2lfan_curr)) {
      /* Sharp loop/edge, so not a cyclic smooth fan... */
      return false;
    }
    /* Smooth loop/edge... */
    else if (BLI_BITMAP_TEST(skip_loops, mlfan_vert_index)) {
      if (mlfan_vert_index == ml_curr_index) {
        /* We walked around a whole cyclic smooth fan without finding any already-processed loop,
         * means we can use initial ml_curr/ml_prev edge as start for this smooth fan. */
        return true;
      }
      /* ... already checked in some previous looping, we can abort. */
      return false;
    }
    else {
      /* ... we can skip it in future, and keep checking the smooth fan. */
      BLI_BITMAP_ENABLE(skip_loops, mlfan_vert_index);
    }
  }
}

static void loop_split_generator(TaskPool *pool, LoopSplitTaskDataCommon *common_data)
{
  MLoopNorSpaceArray *lnors_spacearr = common_data->lnors_spacearr;
  float(*loopnors)[3] = common_data->loopnors;

  const MLoop *mloops = common_data->mloops;
  const MPoly *mpolys = common_data->mpolys;
  const int *loop_to_poly = common_data->loop_to_poly;
  const int(*edge_to_loops)[2] = common_data->edge_to_loops;
  const int numLoops = common_data->numLoops;
  const int numPolys = common_data->numPolys;

  const MPoly *mp;
  int mp_index;

  const MLoop *ml_curr;
  const MLoop *ml_prev;
  int ml_curr_index;
  int ml_prev_index;

  BLI_bitmap *skip_loops = BLI_BITMAP_NEW(numLoops, __func__);

  LoopSplitTaskData *data_buff = NULL;
  int data_idx = 0;

  /* Temp edge vectors stack, only used when computing lnor spacearr
   * (and we are not multi-threading). */
  BLI_Stack *edge_vectors = NULL;

#ifdef DEBUG_TIME
  TIMEIT_START_AVERAGED(loop_split_generator);
#endif

  if (!pool) {
    if (lnors_spacearr) {
      edge_vectors = BLI_stack_new(sizeof(float[3]), __func__);
    }
  }

  /* We now know edges that can be smoothed (with their vector, and their two loops),
   * and edges that will be hard! Now, time to generate the normals.
   */
  for (mp = mpolys, mp_index = 0; mp_index < numPolys; mp++, mp_index++) {
    float(*lnors)[3];
    const int ml_last_index = (mp->loopstart + mp->totloop) - 1;
    ml_curr_index = mp->loopstart;
    ml_prev_index = ml_last_index;

    ml_curr = &mloops[ml_curr_index];
    ml_prev = &mloops[ml_prev_index];
    lnors = &loopnors[ml_curr_index];

    for (; ml_curr_index <= ml_last_index; ml_curr++, ml_curr_index++, lnors++) {
      const int *e2l_curr = edge_to_loops[ml_curr->e];
      const int *e2l_prev = edge_to_loops[ml_prev->e];

#if 0
      printf("Checking loop %d / edge %u / vert %u (sharp edge: %d, skiploop: %d)...",
             ml_curr_index,
             ml_curr->e,
             ml_curr->v,
             IS_EDGE_SHARP(e2l_curr),
             BLI_BITMAP_TEST_BOOL(skip_loops, ml_curr_index));
#endif

      /* A smooth edge, we have to check for cyclic smooth fan case.
       * If we find a new, never-processed cyclic smooth fan, we can do it now using that loop/edge
       * as 'entry point', otherwise we can skip it. */

      /* Note: In theory, we could make loop_split_generator_check_cyclic_smooth_fan() store
       * mlfan_vert_index'es and edge indexes in two stacks, to avoid having to fan again around
       * the vert during actual computation of clnor & clnorspace. However, this would complicate
       * the code, add more memory usage, and despite its logical complexity,
       * loop_manifold_fan_around_vert_next() is quite cheap in term of CPU cycles,
       * so really think it's not worth it. */
      if (!IS_EDGE_SHARP(e2l_curr) && (BLI_BITMAP_TEST(skip_loops, ml_curr_index) ||
                                       !loop_split_generator_check_cyclic_smooth_fan(mloops,
                                                                                     mpolys,
                                                                                     edge_to_loops,
                                                                                     loop_to_poly,
                                                                                     e2l_prev,
                                                                                     skip_loops,
                                                                                     ml_curr,
                                                                                     ml_prev,
                                                                                     ml_curr_index,
                                                                                     ml_prev_index,
                                                                                     mp_index))) {
        //              printf("SKIPPING!\n");
      }
      else {
        LoopSplitTaskData *data, data_local;

        //              printf("PROCESSING!\n");

        if (pool) {
          if (data_idx == 0) {
            data_buff = MEM_calloc_arrayN(
                LOOP_SPLIT_TASK_BLOCK_SIZE, sizeof(*data_buff), __func__);
          }
          data = &data_buff[data_idx];
        }
        else {
          data = &data_local;
          memset(data, 0, sizeof(*data));
        }

        if (IS_EDGE_SHARP(e2l_curr) && IS_EDGE_SHARP(e2l_prev)) {
          data->lnor = lnors;
          data->ml_curr = ml_curr;
          data->ml_prev = ml_prev;
          data->ml_curr_index = ml_curr_index;
#if 0 /* Not needed for 'single' loop. */
          data->ml_prev_index = ml_prev_index;
          data->e2l_prev = NULL; /* Tag as 'single' task. */
#endif
          data->mp_index = mp_index;
          if (lnors_spacearr) {
            data->lnor_space = BKE_lnor_space_create(lnors_spacearr);
          }
        }
        /* We *do not need* to check/tag loops as already computed!
         * Due to the fact a loop only links to one of its two edges,
         * a same fan *will never be walked more than once!*
         * Since we consider edges having neighbor polys with inverted
         * (flipped) normals as sharp, we are sure that no fan will be skipped,
         * even only considering the case (sharp curr_edge, smooth prev_edge),
         * and not the alternative (smooth curr_edge, sharp prev_edge).
         * All this due/thanks to link between normals and loop ordering (i.e. winding).
         */
        else {
#if 0 /* Not needed for 'fan' loops. */
          data->lnor = lnors;
#endif
          data->ml_curr = ml_curr;
          data->ml_prev = ml_prev;
          data->ml_curr_index = ml_curr_index;
          data->ml_prev_index = ml_prev_index;
          data->e2l_prev = e2l_prev; /* Also tag as 'fan' task. */
          data->mp_index = mp_index;
          if (lnors_spacearr) {
            data->lnor_space = BKE_lnor_space_create(lnors_spacearr);
          }
        }

        if (pool) {
          data_idx++;
          if (data_idx == LOOP_SPLIT_TASK_BLOCK_SIZE) {
            BLI_task_pool_push(pool, loop_split_worker, data_buff, true, TASK_PRIORITY_LOW);
            data_idx = 0;
          }
        }
        else {
          loop_split_worker_do(common_data, data, edge_vectors);
        }
      }

      ml_prev = ml_curr;
      ml_prev_index = ml_curr_index;
    }
  }

  /* Last block of data... Since it is calloc'ed and we use first NULL item as stopper,
   * everything is fine. */
  if (pool && data_idx) {
    BLI_task_pool_push(pool, loop_split_worker, data_buff, true, TASK_PRIORITY_LOW);
  }

  if (edge_vectors) {
    BLI_stack_free(edge_vectors);
  }
  MEM_freeN(skip_loops);

#ifdef DEBUG_TIME
  TIMEIT_END_AVERAGED(loop_split_generator);
#endif
}

/**
 * Compute split normals, i.e. vertex normals associated with each poly (hence 'loop normals').
 * Useful to materialize sharp edges (or non-smooth faces) without actually modifying the geometry
 * (splitting edges).
 */
void BKE_mesh_normals_loop_split(const MVert *mverts,
                                 const int UNUSED(numVerts),
                                 MEdge *medges,
                                 const int numEdges,
                                 MLoop *mloops,
                                 float (*r_loopnors)[3],
                                 const int numLoops,
                                 MPoly *mpolys,
                                 const float (*polynors)[3],
                                 const int numPolys,
                                 const bool use_split_normals,
                                 const float split_angle,
                                 MLoopNorSpaceArray *r_lnors_spacearr,
                                 short (*clnors_data)[2],
                                 int *r_loop_to_poly)
{
  /* For now this is not supported.
   * If we do not use split normals, we do not generate anything fancy! */
  BLI_assert(use_split_normals || !(r_lnors_spacearr));

  if (!use_split_normals) {
    /* In this case, we simply fill lnors with vnors (or fnors for flat faces), quite simple!
     * Note this is done here to keep some logic and consistency in this quite complex code,
     * since we may want to use lnors even when mesh's 'autosmooth' is disabled
     * (see e.g. mesh mapping code).
     * As usual, we could handle that on case-by-case basis,
     * but simpler to keep it well confined here.
     */
    int mp_index;

    for (mp_index = 0; mp_index < numPolys; mp_index++) {
      MPoly *mp = &mpolys[mp_index];
      int ml_index = mp->loopstart;
      const int ml_index_end = ml_index + mp->totloop;
      const bool is_poly_flat = ((mp->flag & ME_SMOOTH) == 0);

      for (; ml_index < ml_index_end; ml_index++) {
        if (r_loop_to_poly) {
          r_loop_to_poly[ml_index] = mp_index;
        }
        if (is_poly_flat) {
          copy_v3_v3(r_loopnors[ml_index], polynors[mp_index]);
        }
        else {
          normal_short_to_float_v3(r_loopnors[ml_index], mverts[mloops[ml_index].v].no);
        }
      }
    }
    return;
  }

  /**
   * Mapping edge -> loops.
   * If that edge is used by more than two loops (polys),
   * it is always sharp (and tagged as such, see below).
   * We also use the second loop index as a kind of flag:
   *
   * - smooth edge: > 0.
   * - sharp edge: < 0 (INDEX_INVALID || INDEX_UNSET).
   * - unset: INDEX_UNSET.
   *
   * Note that currently we only have two values for second loop of sharp edges.
   * However, if needed, we can store the negated value of loop index instead of INDEX_INVALID
   * to retrieve the real value later in code).
   * Note also that lose edges always have both values set to 0! */
  int(*edge_to_loops)[2] = MEM_calloc_arrayN((size_t)numEdges, sizeof(*edge_to_loops), __func__);

  /* Simple mapping from a loop to its polygon index. */
  int *loop_to_poly = r_loop_to_poly ?
                          r_loop_to_poly :
                          MEM_malloc_arrayN((size_t)numLoops, sizeof(*loop_to_poly), __func__);

  /* When using custom loop normals, disable the angle feature! */
  const bool check_angle = (split_angle < (float)M_PI) && (clnors_data == NULL);

  MLoopNorSpaceArray _lnors_spacearr = {NULL};

#ifdef DEBUG_TIME
  TIMEIT_START_AVERAGED(BKE_mesh_normals_loop_split);
#endif

  if (!r_lnors_spacearr && clnors_data) {
    /* We need to compute lnor spacearr if some custom lnor data are given to us! */
    r_lnors_spacearr = &_lnors_spacearr;
  }
  if (r_lnors_spacearr) {
    BKE_lnor_spacearr_init(r_lnors_spacearr, numLoops, MLNOR_SPACEARR_LOOP_INDEX);
  }

  /* Init data common to all tasks. */
  LoopSplitTaskDataCommon common_data = {
      .lnors_spacearr = r_lnors_spacearr,
      .loopnors = r_loopnors,
      .clnors_data = clnors_data,
      .mverts = mverts,
      .medges = medges,
      .mloops = mloops,
      .mpolys = mpolys,
      .edge_to_loops = edge_to_loops,
      .loop_to_poly = loop_to_poly,
      .polynors = polynors,
      .numEdges = numEdges,
      .numLoops = numLoops,
      .numPolys = numPolys,
  };

  /* This first loop check which edges are actually smooth, and compute edge vectors. */
  mesh_edges_sharp_tag(&common_data, check_angle, split_angle, false);

  if (numLoops < LOOP_SPLIT_TASK_BLOCK_SIZE * 8) {
    /* Not enough loops to be worth the whole threading overhead... */
    loop_split_generator(NULL, &common_data);
  }
  else {
    TaskScheduler *task_scheduler;
    TaskPool *task_pool;

    task_scheduler = BLI_task_scheduler_get();
    task_pool = BLI_task_pool_create(task_scheduler, &common_data);

    loop_split_generator(task_pool, &common_data);

    BLI_task_pool_work_and_wait(task_pool);

    BLI_task_pool_free(task_pool);
  }

  MEM_freeN(edge_to_loops);
  if (!r_loop_to_poly) {
    MEM_freeN(loop_to_poly);
  }

  if (r_lnors_spacearr) {
    if (r_lnors_spacearr == &_lnors_spacearr) {
      BKE_lnor_spacearr_free(r_lnors_spacearr);
    }
  }

#ifdef DEBUG_TIME
  TIMEIT_END_AVERAGED(BKE_mesh_normals_loop_split);
#endif
}

#undef INDEX_UNSET
#undef INDEX_INVALID
#undef IS_EDGE_SHARP

/**
 * Compute internal representation of given custom normals (as an array of float[2]).
 * It also makes sure the mesh matches those custom normals, by setting sharp edges flag as needed
 * to get a same custom lnor for all loops sharing a same smooth fan.
 * If use_vertices if true, r_custom_loopnors is assumed to be per-vertex, not per-loop
 * (this allows to set whole vert's normals at once, useful in some cases).
 * r_custom_loopnors is expected to have normalized normals, or zero ones,
 * in which case they will be replaced by default loop/vertex normal.
 */
static void mesh_normals_loop_custom_set(const MVert *mverts,
                                         const int numVerts,
                                         MEdge *medges,
                                         const int numEdges,
                                         MLoop *mloops,
                                         float (*r_custom_loopnors)[3],
                                         const int numLoops,
                                         MPoly *mpolys,
                                         const float (*polynors)[3],
                                         const int numPolys,
                                         short (*r_clnors_data)[2],
                                         const bool use_vertices)
{
  /* We *may* make that poor BKE_mesh_normals_loop_split() even more complex by making it handling
   * that feature too, would probably be more efficient in absolute.
   * However, this function *is not* performance-critical, since it is mostly expected to be called
   * by io addons when importing custom normals, and modifier
   * (and perhaps from some editing tools later?).
   * So better to keep some simplicity here, and just call BKE_mesh_normals_loop_split() twice!
   */
  MLoopNorSpaceArray lnors_spacearr = {NULL};
  BLI_bitmap *done_loops = BLI_BITMAP_NEW((size_t)numLoops, __func__);
  float(*lnors)[3] = MEM_calloc_arrayN((size_t)numLoops, sizeof(*lnors), __func__);
  int *loop_to_poly = MEM_malloc_arrayN((size_t)numLoops, sizeof(int), __func__);
  /* In this case we always consider split nors as ON,
   * and do not want to use angle to define smooth fans! */
  const bool use_split_normals = true;
  const float split_angle = (float)M_PI;
  int i;

  BLI_SMALLSTACK_DECLARE(clnors_data, short *);

  /* Compute current lnor spacearr. */
  BKE_mesh_normals_loop_split(mverts,
                              numVerts,
                              medges,
                              numEdges,
                              mloops,
                              lnors,
                              numLoops,
                              mpolys,
                              polynors,
                              numPolys,
                              use_split_normals,
                              split_angle,
                              &lnors_spacearr,
                              NULL,
                              loop_to_poly);

  /* Set all given zero vectors to their default value. */
  if (use_vertices) {
    for (i = 0; i < numVerts; i++) {
      if (is_zero_v3(r_custom_loopnors[i])) {
        normal_short_to_float_v3(r_custom_loopnors[i], mverts[i].no);
      }
    }
  }
  else {
    for (i = 0; i < numLoops; i++) {
      if (is_zero_v3(r_custom_loopnors[i])) {
        copy_v3_v3(r_custom_loopnors[i], lnors[i]);
      }
    }
  }

  BLI_assert(lnors_spacearr.data_type == MLNOR_SPACEARR_LOOP_INDEX);

  /* Now, check each current smooth fan (one lnor space per smooth fan!),
   * and if all its matching custom lnors are not (enough) equal, add sharp edges as needed.
   * This way, next time we run BKE_mesh_normals_loop_split(), we'll get lnor spacearr/smooth fans
   * matching given custom lnors.
   * Note this code *will never* unsharp edges! And quite obviously,
   * when we set custom normals per vertices, running this is absolutely useless.
   */
  if (!use_vertices) {
    for (i = 0; i < numLoops; i++) {
      if (!lnors_spacearr.lspacearr[i]) {
        /* This should not happen in theory, but in some rare case (probably ugly geometry)
         * we can get some NULL loopspacearr at this point. :/
         * Maybe we should set those loops' edges as sharp?
         */
        BLI_BITMAP_ENABLE(done_loops, i);
        if (G.debug & G_DEBUG) {
          printf("WARNING! Getting invalid NULL loop space for loop %d!\n", i);
        }
        continue;
      }

      if (!BLI_BITMAP_TEST(done_loops, i)) {
        /* Notes:
         * * In case of mono-loop smooth fan, we have nothing to do.
         * * Loops in this linklist are ordered (in reversed order compared to how they were
         *   discovered by BKE_mesh_normals_loop_split(), but this is not a problem).
         *   Which means if we find a mismatching clnor,
         *   we know all remaining loops will have to be in a new, different smooth fan/lnor space.
         * * In smooth fan case, we compare each clnor against a ref one,
         *   to avoid small differences adding up into a real big one in the end!
         */
        if (lnors_spacearr.lspacearr[i]->flags & MLNOR_SPACE_IS_SINGLE) {
          BLI_BITMAP_ENABLE(done_loops, i);
          continue;
        }

        LinkNode *loops = lnors_spacearr.lspacearr[i]->loops;
        MLoop *prev_ml = NULL;
        const float *org_nor = NULL;

        while (loops) {
          const int lidx = POINTER_AS_INT(loops->link);
          MLoop *ml = &mloops[lidx];
          const int nidx = lidx;
          float *nor = r_custom_loopnors[nidx];

          if (!org_nor) {
            org_nor = nor;
          }
          else if (dot_v3v3(org_nor, nor) < LNOR_SPACE_TRIGO_THRESHOLD) {
            /* Current normal differs too much from org one, we have to tag the edge between
             * previous loop's face and current's one as sharp.
             * We know those two loops do not point to the same edge,
             * since we do not allow reversed winding in a same smooth fan.
             */
            const MPoly *mp = &mpolys[loop_to_poly[lidx]];
            const MLoop *mlp =
                &mloops[(lidx == mp->loopstart) ? mp->loopstart + mp->totloop - 1 : lidx - 1];
            medges[(prev_ml->e == mlp->e) ? prev_ml->e : ml->e].flag |= ME_SHARP;

            org_nor = nor;
          }

          prev_ml = ml;
          loops = loops->next;
          BLI_BITMAP_ENABLE(done_loops, lidx);
        }

        /* We also have to check between last and first loops,
         * otherwise we may miss some sharp edges here!
         * This is just a simplified version of above while loop.
         * See T45984. */
        loops = lnors_spacearr.lspacearr[i]->loops;
        if (loops && org_nor) {
          const int lidx = POINTER_AS_INT(loops->link);
          MLoop *ml = &mloops[lidx];
          const int nidx = lidx;
          float *nor = r_custom_loopnors[nidx];

          if (dot_v3v3(org_nor, nor) < LNOR_SPACE_TRIGO_THRESHOLD) {
            const MPoly *mp = &mpolys[loop_to_poly[lidx]];
            const MLoop *mlp =
                &mloops[(lidx == mp->loopstart) ? mp->loopstart + mp->totloop - 1 : lidx - 1];
            medges[(prev_ml->e == mlp->e) ? prev_ml->e : ml->e].flag |= ME_SHARP;
          }
        }
      }
    }

    /* And now, recompute our new auto lnors and lnor spacearr! */
    BKE_lnor_spacearr_clear(&lnors_spacearr);
    BKE_mesh_normals_loop_split(mverts,
                                numVerts,
                                medges,
                                numEdges,
                                mloops,
                                lnors,
                                numLoops,
                                mpolys,
                                polynors,
                                numPolys,
                                use_split_normals,
                                split_angle,
                                &lnors_spacearr,
                                NULL,
                                loop_to_poly);
  }
  else {
    BLI_bitmap_set_all(done_loops, true, (size_t)numLoops);
  }

  /* And we just have to convert plain object-space custom normals to our
   * lnor space-encoded ones. */
  for (i = 0; i < numLoops; i++) {
    if (!lnors_spacearr.lspacearr[i]) {
      BLI_BITMAP_DISABLE(done_loops, i);
      if (G.debug & G_DEBUG) {
        printf("WARNING! Still getting invalid NULL loop space in second loop for loop %d!\n", i);
      }
      continue;
    }

    if (BLI_BITMAP_TEST_BOOL(done_loops, i)) {
      /* Note we accumulate and average all custom normals in current smooth fan,
       * to avoid getting different clnors data (tiny differences in plain custom normals can
       * give rather huge differences in computed 2D factors).
       */
      LinkNode *loops = lnors_spacearr.lspacearr[i]->loops;
      if (lnors_spacearr.lspacearr[i]->flags & MLNOR_SPACE_IS_SINGLE) {
        BLI_assert(POINTER_AS_INT(loops) == i);
        const int nidx = use_vertices ? (int)mloops[i].v : i;
        float *nor = r_custom_loopnors[nidx];

        BKE_lnor_space_custom_normal_to_data(lnors_spacearr.lspacearr[i], nor, r_clnors_data[i]);
        BLI_BITMAP_DISABLE(done_loops, i);
      }
      else {
        int nbr_nors = 0;
        float avg_nor[3];
        short clnor_data_tmp[2], *clnor_data;

        zero_v3(avg_nor);
        while (loops) {
          const int lidx = POINTER_AS_INT(loops->link);
          const int nidx = use_vertices ? (int)mloops[lidx].v : lidx;
          float *nor = r_custom_loopnors[nidx];

          nbr_nors++;
          add_v3_v3(avg_nor, nor);
          BLI_SMALLSTACK_PUSH(clnors_data, (short *)r_clnors_data[lidx]);

          loops = loops->next;
          BLI_BITMAP_DISABLE(done_loops, lidx);
        }

        mul_v3_fl(avg_nor, 1.0f / (float)nbr_nors);
        BKE_lnor_space_custom_normal_to_data(lnors_spacearr.lspacearr[i], avg_nor, clnor_data_tmp);

        while ((clnor_data = BLI_SMALLSTACK_POP(clnors_data))) {
          clnor_data[0] = clnor_data_tmp[0];
          clnor_data[1] = clnor_data_tmp[1];
        }
      }
    }
  }

  MEM_freeN(lnors);
  MEM_freeN(loop_to_poly);
  MEM_freeN(done_loops);
  BKE_lnor_spacearr_free(&lnors_spacearr);
}

void BKE_mesh_normals_loop_custom_set(const MVert *mverts,
                                      const int numVerts,
                                      MEdge *medges,
                                      const int numEdges,
                                      MLoop *mloops,
                                      float (*r_custom_loopnors)[3],
                                      const int numLoops,
                                      MPoly *mpolys,
                                      const float (*polynors)[3],
                                      const int numPolys,
                                      short (*r_clnors_data)[2])
{
  mesh_normals_loop_custom_set(mverts,
                               numVerts,
                               medges,
                               numEdges,
                               mloops,
                               r_custom_loopnors,
                               numLoops,
                               mpolys,
                               polynors,
                               numPolys,
                               r_clnors_data,
                               false);
}

void BKE_mesh_normals_loop_custom_from_vertices_set(const MVert *mverts,
                                                    float (*r_custom_vertnors)[3],
                                                    const int numVerts,
                                                    MEdge *medges,
                                                    const int numEdges,
                                                    MLoop *mloops,
                                                    const int numLoops,
                                                    MPoly *mpolys,
                                                    const float (*polynors)[3],
                                                    const int numPolys,
                                                    short (*r_clnors_data)[2])
{
  mesh_normals_loop_custom_set(mverts,
                               numVerts,
                               medges,
                               numEdges,
                               mloops,
                               r_custom_vertnors,
                               numLoops,
                               mpolys,
                               polynors,
                               numPolys,
                               r_clnors_data,
                               true);
}

static void mesh_set_custom_normals(Mesh *mesh, float (*r_custom_nors)[3], const bool use_vertices)
{
  short(*clnors)[2];
  const int numloops = mesh->totloop;

  clnors = CustomData_get_layer(&mesh->ldata, CD_CUSTOMLOOPNORMAL);
  if (clnors != NULL) {
    memset(clnors, 0, sizeof(*clnors) * (size_t)numloops);
  }
  else {
    clnors = CustomData_add_layer(&mesh->ldata, CD_CUSTOMLOOPNORMAL, CD_CALLOC, NULL, numloops);
  }

  float(*polynors)[3] = CustomData_get_layer(&mesh->pdata, CD_NORMAL);
  bool free_polynors = false;
  if (polynors == NULL) {
    polynors = MEM_mallocN(sizeof(float[3]) * (size_t)mesh->totpoly, __func__);
    BKE_mesh_calc_normals_poly(mesh->mvert,
                               NULL,
                               mesh->totvert,
                               mesh->mloop,
                               mesh->mpoly,
                               mesh->totloop,
                               mesh->totpoly,
                               polynors,
                               false);
    free_polynors = true;
  }

  mesh_normals_loop_custom_set(mesh->mvert,
                               mesh->totvert,
                               mesh->medge,
                               mesh->totedge,
                               mesh->mloop,
                               r_custom_nors,
                               mesh->totloop,
                               mesh->mpoly,
                               polynors,
                               mesh->totpoly,
                               clnors,
                               use_vertices);

  if (free_polynors) {
    MEM_freeN(polynors);
  }
}

/**
 * Higher level functions hiding most of the code needed around call to
 * #BKE_mesh_normals_loop_custom_set().
 *
 * \param r_custom_loopnors: is not const, since code will replace zero_v3 normals there
 * with automatically computed vectors.
 */
void BKE_mesh_set_custom_normals(Mesh *mesh, float (*r_custom_loopnors)[3])
{
  mesh_set_custom_normals(mesh, r_custom_loopnors, false);
}

/**
 * Higher level functions hiding most of the code needed around call to
 * #BKE_mesh_normals_loop_custom_from_vertices_set().
 *
 * \param r_custom_loopnors: is not const, since code will replace zero_v3 normals there
 * with automatically computed vectors.
 */
void BKE_mesh_set_custom_normals_from_vertices(Mesh *mesh, float (*r_custom_vertnors)[3])
{
  mesh_set_custom_normals(mesh, r_custom_vertnors, true);
}

/**
 * Computes average per-vertex normals from given custom loop normals.
 *
 * \param clnors: The computed custom loop normals.
 * \param r_vert_clnors: The (already allocated) array where to store averaged per-vertex normals.
 */
void BKE_mesh_normals_loop_to_vertex(const int numVerts,
                                     const MLoop *mloops,
                                     const int numLoops,
                                     const float (*clnors)[3],
                                     float (*r_vert_clnors)[3])
{
  const MLoop *ml;
  int i;

  int *vert_loops_nbr = MEM_calloc_arrayN((size_t)numVerts, sizeof(*vert_loops_nbr), __func__);

  copy_vn_fl((float *)r_vert_clnors, 3 * numVerts, 0.0f);

  for (i = 0, ml = mloops; i < numLoops; i++, ml++) {
    const unsigned int v = ml->v;

    add_v3_v3(r_vert_clnors[v], clnors[i]);
    vert_loops_nbr[v]++;
  }

  for (i = 0; i < numVerts; i++) {
    mul_v3_fl(r_vert_clnors[i], 1.0f / (float)vert_loops_nbr[i]);
  }

  MEM_freeN(vert_loops_nbr);
}

#undef LNOR_SPACE_TRIGO_THRESHOLD

/** \} */

/* -------------------------------------------------------------------- */
/** \name Polygon Calculations
 * \{ */

/*
 * COMPUTE POLY NORMAL
 *
 * Computes the normal of a planar
 * polygon See Graphics Gems for
 * computing newell normal.
 */
static void mesh_calc_ngon_normal(const MPoly *mpoly,
                                  const MLoop *loopstart,
                                  const MVert *mvert,
                                  float normal[3])
{
  const int nverts = mpoly->totloop;
  const float *v_prev = mvert[loopstart[nverts - 1].v].co;
  const float *v_curr;
  int i;

  zero_v3(normal);

  /* Newell's Method */
  for (i = 0; i < nverts; i++) {
    v_curr = mvert[loopstart[i].v].co;
    add_newell_cross_v3_v3v3(normal, v_prev, v_curr);
    v_prev = v_curr;
  }

  if (UNLIKELY(normalize_v3(normal) == 0.0f)) {
    normal[2] = 1.0f; /* other axis set to 0.0 */
  }
}

void BKE_mesh_calc_poly_normal(const MPoly *mpoly,
                               const MLoop *loopstart,
                               const MVert *mvarray,
                               float r_no[3])
{
  if (mpoly->totloop > 4) {
    mesh_calc_ngon_normal(mpoly, loopstart, mvarray, r_no);
  }
  else if (mpoly->totloop == 3) {
    normal_tri_v3(
        r_no, mvarray[loopstart[0].v].co, mvarray[loopstart[1].v].co, mvarray[loopstart[2].v].co);
  }
  else if (mpoly->totloop == 4) {
    normal_quad_v3(r_no,
                   mvarray[loopstart[0].v].co,
                   mvarray[loopstart[1].v].co,
                   mvarray[loopstart[2].v].co,
                   mvarray[loopstart[3].v].co);
  }
  else { /* horrible, two sided face! */
    r_no[0] = 0.0;
    r_no[1] = 0.0;
    r_no[2] = 1.0;
  }
}
/* duplicate of function above _but_ takes coords rather then mverts */
static void mesh_calc_ngon_normal_coords(const MPoly *mpoly,
                                         const MLoop *loopstart,
                                         const float (*vertex_coords)[3],
                                         float r_normal[3])
{
  const int nverts = mpoly->totloop;
  const float *v_prev = vertex_coords[loopstart[nverts - 1].v];
  const float *v_curr;
  int i;

  zero_v3(r_normal);

  /* Newell's Method */
  for (i = 0; i < nverts; i++) {
    v_curr = vertex_coords[loopstart[i].v];
    add_newell_cross_v3_v3v3(r_normal, v_prev, v_curr);
    v_prev = v_curr;
  }

  if (UNLIKELY(normalize_v3(r_normal) == 0.0f)) {
    r_normal[2] = 1.0f; /* other axis set to 0.0 */
  }
}

void BKE_mesh_calc_poly_normal_coords(const MPoly *mpoly,
                                      const MLoop *loopstart,
                                      const float (*vertex_coords)[3],
                                      float r_no[3])
{
  if (mpoly->totloop > 4) {
    mesh_calc_ngon_normal_coords(mpoly, loopstart, vertex_coords, r_no);
  }
  else if (mpoly->totloop == 3) {
    normal_tri_v3(r_no,
                  vertex_coords[loopstart[0].v],
                  vertex_coords[loopstart[1].v],
                  vertex_coords[loopstart[2].v]);
  }
  else if (mpoly->totloop == 4) {
    normal_quad_v3(r_no,
                   vertex_coords[loopstart[0].v],
                   vertex_coords[loopstart[1].v],
                   vertex_coords[loopstart[2].v],
                   vertex_coords[loopstart[3].v]);
  }
  else { /* horrible, two sided face! */
    r_no[0] = 0.0;
    r_no[1] = 0.0;
    r_no[2] = 1.0;
  }
}

static void mesh_calc_ngon_center(const MPoly *mpoly,
                                  const MLoop *loopstart,
                                  const MVert *mvert,
                                  float cent[3])
{
  const float w = 1.0f / (float)mpoly->totloop;
  int i;

  zero_v3(cent);

  for (i = 0; i < mpoly->totloop; i++) {
    madd_v3_v3fl(cent, mvert[(loopstart++)->v].co, w);
  }
}

void BKE_mesh_calc_poly_center(const MPoly *mpoly,
                               const MLoop *loopstart,
                               const MVert *mvarray,
                               float r_cent[3])
{
  if (mpoly->totloop == 3) {
    mid_v3_v3v3v3(r_cent,
                  mvarray[loopstart[0].v].co,
                  mvarray[loopstart[1].v].co,
                  mvarray[loopstart[2].v].co);
  }
  else if (mpoly->totloop == 4) {
    mid_v3_v3v3v3v3(r_cent,
                    mvarray[loopstart[0].v].co,
                    mvarray[loopstart[1].v].co,
                    mvarray[loopstart[2].v].co,
                    mvarray[loopstart[3].v].co);
  }
  else {
    mesh_calc_ngon_center(mpoly, loopstart, mvarray, r_cent);
  }
}

/* note, passing polynormal is only a speedup so we can skip calculating it */
float BKE_mesh_calc_poly_area(const MPoly *mpoly, const MLoop *loopstart, const MVert *mvarray)
{
  if (mpoly->totloop == 3) {
    return area_tri_v3(
        mvarray[loopstart[0].v].co, mvarray[loopstart[1].v].co, mvarray[loopstart[2].v].co);
  }
  else {
    int i;
    const MLoop *l_iter = loopstart;
    float area;
    float(*vertexcos)[3] = BLI_array_alloca(vertexcos, (size_t)mpoly->totloop);

    /* pack vertex cos into an array for area_poly_v3 */
    for (i = 0; i < mpoly->totloop; i++, l_iter++) {
      copy_v3_v3(vertexcos[i], mvarray[l_iter->v].co);
    }

    /* finally calculate the area */
    area = area_poly_v3((const float(*)[3])vertexcos, (unsigned int)mpoly->totloop);

    return area;
  }
}

/**
 * Calculate the volume and volume-weighted centroid of the volume
 * formed by the polygon and the origin.
 * Results will be negative if the origin is "outside" the polygon
 * (+ve normal side), but the polygon may be non-planar with no effect.
 *
 * Method from:
 * - http://forums.cgsociety.org/archive/index.php?t-756235.html
 * - http://www.globalspec.com/reference/52702/203279/4-8-the-centroid-of-a-tetrahedron
 *
 * \note
 * - Volume is 6x actual volume, and centroid is 4x actual volume-weighted centroid
 *   (so division can be done once at the end).
 * - Results will have bias if polygon is non-planar.
 * - The resulting volume will only be correct if the mesh is manifold and has consistent
 *   face winding (non-contiguous face normals or holes in the mesh surface).
 */
static float mesh_calc_poly_volume_centroid(const MPoly *mpoly,
                                            const MLoop *loopstart,
                                            const MVert *mvarray,
                                            float r_cent[3])
{
  const float *v_pivot, *v_step1;
  float total_volume = 0.0f;

  zero_v3(r_cent);

  v_pivot = mvarray[loopstart[0].v].co;
  v_step1 = mvarray[loopstart[1].v].co;

  for (int i = 2; i < mpoly->totloop; i++) {
    const float *v_step2 = mvarray[loopstart[i].v].co;

    /* Calculate the 6x volume of the tetrahedron formed by the 3 vertices
     * of the triangle and the origin as the fourth vertex */
    float v_cross[3];
    cross_v3_v3v3(v_cross, v_pivot, v_step1);
    const float tetra_volume = dot_v3v3(v_cross, v_step2);
    total_volume += tetra_volume;

    /* Calculate the centroid of the tetrahedron formed by the 3 vertices
     * of the triangle and the origin as the fourth vertex.
     * The centroid is simply the average of the 4 vertices.
     *
     * Note that the vector is 4x the actual centroid
     * so the division can be done once at the end. */
    for (uint j = 0; j < 3; j++) {
      r_cent[j] += tetra_volume * (v_pivot[j] + v_step1[j] + v_step2[j]);
    }

    v_step1 = v_step2;
  }

  return total_volume;
}

/**
 * \note
 * - Results won't be correct if polygon is non-planar.
 * - This has the advantage over #mesh_calc_poly_volume_centroid
 *   that it doesn't depend on solid geometry, instead it weights the surface by volume.
 */
static float mesh_calc_poly_area_centroid(const MPoly *mpoly,
                                          const MLoop *loopstart,
                                          const MVert *mvarray,
                                          float r_cent[3])
{
  int i;
  float tri_area;
  float total_area = 0.0f;
  float v1[3], v2[3], v3[3], normal[3], tri_cent[3];

  BKE_mesh_calc_poly_normal(mpoly, loopstart, mvarray, normal);
  copy_v3_v3(v1, mvarray[loopstart[0].v].co);
  copy_v3_v3(v2, mvarray[loopstart[1].v].co);
  zero_v3(r_cent);

  for (i = 2; i < mpoly->totloop; i++) {
    copy_v3_v3(v3, mvarray[loopstart[i].v].co);

    tri_area = area_tri_signed_v3(v1, v2, v3, normal);
    total_area += tri_area;

    mid_v3_v3v3v3(tri_cent, v1, v2, v3);
    madd_v3_v3fl(r_cent, tri_cent, tri_area);

    copy_v3_v3(v2, v3);
  }

  mul_v3_fl(r_cent, 1.0f / total_area);

  return total_area;
}

void BKE_mesh_calc_poly_angles(const MPoly *mpoly,
                               const MLoop *loopstart,
                               const MVert *mvarray,
                               float angles[])
{
  float nor_prev[3];
  float nor_next[3];

  int i_this = mpoly->totloop - 1;
  int i_next = 0;

  sub_v3_v3v3(nor_prev, mvarray[loopstart[i_this - 1].v].co, mvarray[loopstart[i_this].v].co);
  normalize_v3(nor_prev);

  while (i_next < mpoly->totloop) {
    sub_v3_v3v3(nor_next, mvarray[loopstart[i_this].v].co, mvarray[loopstart[i_next].v].co);
    normalize_v3(nor_next);
    angles[i_this] = angle_normalized_v3v3(nor_prev, nor_next);

    /* step */
    copy_v3_v3(nor_prev, nor_next);
    i_this = i_next;
    i_next++;
  }
}

void BKE_mesh_poly_edgehash_insert(EdgeHash *ehash, const MPoly *mp, const MLoop *mloop)
{
  const MLoop *ml, *ml_next;
  int i = mp->totloop;

  ml_next = mloop;      /* first loop */
  ml = &ml_next[i - 1]; /* last loop */

  while (i-- != 0) {
    BLI_edgehash_reinsert(ehash, ml->v, ml_next->v, NULL);

    ml = ml_next;
    ml_next++;
  }
}

void BKE_mesh_poly_edgebitmap_insert(unsigned int *edge_bitmap,
                                     const MPoly *mp,
                                     const MLoop *mloop)
{
  const MLoop *ml;
  int i = mp->totloop;

  ml = mloop;

  while (i-- != 0) {
    BLI_BITMAP_ENABLE(edge_bitmap, ml->e);
    ml++;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Center Calculation
 * \{ */

bool BKE_mesh_center_median(const Mesh *me, float r_cent[3])
{
  int i = me->totvert;
  const MVert *mvert;
  zero_v3(r_cent);
  for (mvert = me->mvert; i--; mvert++) {
    add_v3_v3(r_cent, mvert->co);
  }
  /* otherwise we get NAN for 0 verts */
  if (me->totvert) {
    mul_v3_fl(r_cent, 1.0f / (float)me->totvert);
  }

  return (me->totvert != 0);
}

bool BKE_mesh_center_bounds(const Mesh *me, float r_cent[3])
{
  float min[3], max[3];
  INIT_MINMAX(min, max);
  if (BKE_mesh_minmax(me, min, max)) {
    mid_v3_v3v3(r_cent, min, max);
    return true;
  }

  return false;
}

bool BKE_mesh_center_of_surface(const Mesh *me, float r_cent[3])
{
  int i = me->totpoly;
  MPoly *mpoly;
  float poly_area;
  float total_area = 0.0f;
  float poly_cent[3];

  zero_v3(r_cent);

  /* calculate a weighted average of polygon centroids */
  for (mpoly = me->mpoly; i--; mpoly++) {
    poly_area = mesh_calc_poly_area_centroid(
        mpoly, me->mloop + mpoly->loopstart, me->mvert, poly_cent);

    madd_v3_v3fl(r_cent, poly_cent, poly_area);
    total_area += poly_area;
  }
  /* otherwise we get NAN for 0 polys */
  if (me->totpoly) {
    mul_v3_fl(r_cent, 1.0f / total_area);
  }

  /* zero area faces cause this, fallback to median */
  if (UNLIKELY(!is_finite_v3(r_cent))) {
    return BKE_mesh_center_median(me, r_cent);
  }

  return (me->totpoly != 0);
}

/**
 * \note Mesh must be manifold with consistent face-winding,
 * see #mesh_calc_poly_volume_centroid for details.
 */
bool BKE_mesh_center_of_volume(const Mesh *me, float r_cent[3])
{
  int i = me->totpoly;
  MPoly *mpoly;
  float poly_volume;
  float total_volume = 0.0f;
  float poly_cent[3];

  zero_v3(r_cent);

  /* calculate a weighted average of polyhedron centroids */
  for (mpoly = me->mpoly; i--; mpoly++) {
    poly_volume = mesh_calc_poly_volume_centroid(
        mpoly, me->mloop + mpoly->loopstart, me->mvert, poly_cent);

    /* poly_cent is already volume-weighted, so no need to multiply by the volume */
    add_v3_v3(r_cent, poly_cent);
    total_volume += poly_volume;
  }
  /* otherwise we get NAN for 0 polys */
  if (total_volume != 0.0f) {
    /* multiply by 0.25 to get the correct centroid */
    /* no need to divide volume by 6 as the centroid is weighted by 6x the volume,
     * so it all cancels out. */
    mul_v3_fl(r_cent, 0.25f / total_volume);
  }

  /* this can happen for non-manifold objects, fallback to median */
  if (UNLIKELY(!is_finite_v3(r_cent))) {
    return BKE_mesh_center_median(me, r_cent);
  }

  return (me->totpoly != 0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Volume Calculation
 * \{ */

static bool mesh_calc_center_centroid_ex(const MVert *mverts,
                                         int UNUSED(mverts_num),
                                         const MLoopTri *looptri,
                                         int looptri_num,
                                         const MLoop *mloop,
                                         float r_center[3])
{
  const MLoopTri *lt;
  float totweight;
  int i;

  zero_v3(r_center);

  if (looptri_num == 0) {
    return false;
  }

  totweight = 0.0f;
  for (i = 0, lt = looptri; i < looptri_num; i++, lt++) {
    const MVert *v1 = &mverts[mloop[lt->tri[0]].v];
    const MVert *v2 = &mverts[mloop[lt->tri[1]].v];
    const MVert *v3 = &mverts[mloop[lt->tri[2]].v];
    float area;

    area = area_tri_v3(v1->co, v2->co, v3->co);
    madd_v3_v3fl(r_center, v1->co, area);
    madd_v3_v3fl(r_center, v2->co, area);
    madd_v3_v3fl(r_center, v3->co, area);
    totweight += area;
  }
  if (totweight == 0.0f) {
    return false;
  }

  mul_v3_fl(r_center, 1.0f / (3.0f * totweight));

  return true;
}

/**
 * Calculate the volume and center.
 *
 * \param r_volume: Volume (unsigned).
 * \param r_center: Center of mass.
 */
void BKE_mesh_calc_volume(const MVert *mverts,
                          const int mverts_num,
                          const MLoopTri *looptri,
                          const int looptri_num,
                          const MLoop *mloop,
                          float *r_volume,
                          float r_center[3])
{
  const MLoopTri *lt;
  float center[3];
  float totvol;
  int i;

  if (r_volume) {
    *r_volume = 0.0f;
  }
  if (r_center) {
    zero_v3(r_center);
  }

  if (looptri_num == 0) {
    return;
  }

  if (!mesh_calc_center_centroid_ex(mverts, mverts_num, looptri, looptri_num, mloop, center)) {
    return;
  }

  totvol = 0.0f;

  for (i = 0, lt = looptri; i < looptri_num; i++, lt++) {
    const MVert *v1 = &mverts[mloop[lt->tri[0]].v];
    const MVert *v2 = &mverts[mloop[lt->tri[1]].v];
    const MVert *v3 = &mverts[mloop[lt->tri[2]].v];
    float vol;

    vol = volume_tetrahedron_signed_v3(center, v1->co, v2->co, v3->co);
    if (r_volume) {
      totvol += vol;
    }
    if (r_center) {
      /* averaging factor 1/3 is applied in the end */
      madd_v3_v3fl(r_center, v1->co, vol);
      madd_v3_v3fl(r_center, v2->co, vol);
      madd_v3_v3fl(r_center, v3->co, vol);
    }
  }

  /* Note: Depending on arbitrary centroid position,
   * totvol can become negative even for a valid mesh.
   * The true value is always the positive value.
   */
  if (r_volume) {
    *r_volume = fabsf(totvol);
  }
  if (r_center) {
    /* Note: Factor 1/3 is applied once for all vertices here.
     * This also automatically negates the vector if totvol is negative.
     */
    if (totvol != 0.0f) {
      mul_v3_fl(r_center, (1.0f / 3.0f) / totvol);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NGon Tessellation (NGon/Tessface Conversion)
 * \{ */

/**
 * Convert a triangle or quadrangle of loop/poly data to tessface data
 */
void BKE_mesh_loops_to_mface_corners(
    CustomData *fdata,
    CustomData *ldata,
    CustomData *UNUSED(pdata),
    unsigned int lindex[4],
    int findex,
    const int UNUSED(polyindex),
    const int mf_len, /* 3 or 4 */

    /* cache values to avoid lookups every time */
    const int numUV,         /* CustomData_number_of_layers(ldata, CD_MLOOPUV) */
    const int numCol,        /* CustomData_number_of_layers(ldata, CD_MLOOPCOL) */
    const bool hasPCol,      /* CustomData_has_layer(ldata, CD_PREVIEW_MLOOPCOL) */
    const bool hasOrigSpace, /* CustomData_has_layer(ldata, CD_ORIGSPACE_MLOOP) */
    const bool hasLNor       /* CustomData_has_layer(ldata, CD_NORMAL) */
)
{
  MTFace *texface;
  MCol *mcol;
  MLoopCol *mloopcol;
  MLoopUV *mloopuv;
  int i, j;

  for (i = 0; i < numUV; i++) {
    texface = CustomData_get_n(fdata, CD_MTFACE, findex, i);

    for (j = 0; j < mf_len; j++) {
      mloopuv = CustomData_get_n(ldata, CD_MLOOPUV, (int)lindex[j], i);
      copy_v2_v2(texface->uv[j], mloopuv->uv);
    }
  }

  for (i = 0; i < numCol; i++) {
    mcol = CustomData_get_n(fdata, CD_MCOL, findex, i);

    for (j = 0; j < mf_len; j++) {
      mloopcol = CustomData_get_n(ldata, CD_MLOOPCOL, (int)lindex[j], i);
      MESH_MLOOPCOL_TO_MCOL(mloopcol, &mcol[j]);
    }
  }

  if (hasPCol) {
    mcol = CustomData_get(fdata, findex, CD_PREVIEW_MCOL);

    for (j = 0; j < mf_len; j++) {
      mloopcol = CustomData_get(ldata, (int)lindex[j], CD_PREVIEW_MLOOPCOL);
      MESH_MLOOPCOL_TO_MCOL(mloopcol, &mcol[j]);
    }
  }

  if (hasOrigSpace) {
    OrigSpaceFace *of = CustomData_get(fdata, findex, CD_ORIGSPACE);
    OrigSpaceLoop *lof;

    for (j = 0; j < mf_len; j++) {
      lof = CustomData_get(ldata, (int)lindex[j], CD_ORIGSPACE_MLOOP);
      copy_v2_v2(of->uv[j], lof->uv);
    }
  }

  if (hasLNor) {
    short(*tlnors)[3] = CustomData_get(fdata, findex, CD_TESSLOOPNORMAL);

    for (j = 0; j < mf_len; j++) {
      normal_float_to_short_v3(tlnors[j], CustomData_get(ldata, (int)lindex[j], CD_NORMAL));
    }
  }
}

/**
 * Convert all CD layers from loop/poly to tessface data.
 *
 * \param loopindices: is an array of an int[4] per tessface,
 * mapping tessface's verts to loops indices.
 *
 * \note when mface is not NULL, mface[face_index].v4
 * is used to test quads, else, loopindices[face_index][3] is used.
 */
void BKE_mesh_loops_to_tessdata(CustomData *fdata,
                                CustomData *ldata,
                                MFace *mface,
                                int *polyindices,
                                unsigned int (*loopindices)[4],
                                const int num_faces)
{
  /* Note: performances are sub-optimal when we get a NULL mface,
   *       we could be ~25% quicker with dedicated code...
   *       Issue is, unless having two different functions with nearly the same code,
   *       there's not much ways to solve this. Better imho to live with it for now. :/ --mont29
   */
  const int numUV = CustomData_number_of_layers(ldata, CD_MLOOPUV);
  const int numCol = CustomData_number_of_layers(ldata, CD_MLOOPCOL);
  const bool hasPCol = CustomData_has_layer(ldata, CD_PREVIEW_MLOOPCOL);
  const bool hasOrigSpace = CustomData_has_layer(ldata, CD_ORIGSPACE_MLOOP);
  const bool hasLoopNormal = CustomData_has_layer(ldata, CD_NORMAL);
  const bool hasLoopTangent = CustomData_has_layer(ldata, CD_TANGENT);
  int findex, i, j;
  const int *pidx;
  unsigned int(*lidx)[4];

  for (i = 0; i < numUV; i++) {
    MTFace *texface = CustomData_get_layer_n(fdata, CD_MTFACE, i);
    MLoopUV *mloopuv = CustomData_get_layer_n(ldata, CD_MLOOPUV, i);

    for (findex = 0, pidx = polyindices, lidx = loopindices; findex < num_faces;
         pidx++, lidx++, findex++, texface++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        copy_v2_v2(texface->uv[j], mloopuv[(*lidx)[j]].uv);
      }
    }
  }

  for (i = 0; i < numCol; i++) {
    MCol(*mcol)[4] = CustomData_get_layer_n(fdata, CD_MCOL, i);
    MLoopCol *mloopcol = CustomData_get_layer_n(ldata, CD_MLOOPCOL, i);

    for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, mcol++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        MESH_MLOOPCOL_TO_MCOL(&mloopcol[(*lidx)[j]], &(*mcol)[j]);
      }
    }
  }

  if (hasPCol) {
    MCol(*mcol)[4] = CustomData_get_layer(fdata, CD_PREVIEW_MCOL);
    MLoopCol *mloopcol = CustomData_get_layer(ldata, CD_PREVIEW_MLOOPCOL);

    for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, mcol++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        MESH_MLOOPCOL_TO_MCOL(&mloopcol[(*lidx)[j]], &(*mcol)[j]);
      }
    }
  }

  if (hasOrigSpace) {
    OrigSpaceFace *of = CustomData_get_layer(fdata, CD_ORIGSPACE);
    OrigSpaceLoop *lof = CustomData_get_layer(ldata, CD_ORIGSPACE_MLOOP);

    for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, of++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        copy_v2_v2(of->uv[j], lof[(*lidx)[j]].uv);
      }
    }
  }

  if (hasLoopNormal) {
    short(*fnors)[4][3] = CustomData_get_layer(fdata, CD_TESSLOOPNORMAL);
    float(*lnors)[3] = CustomData_get_layer(ldata, CD_NORMAL);

    for (findex = 0, lidx = loopindices; findex < num_faces; lidx++, findex++, fnors++) {
      for (j = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3; j--;) {
        normal_float_to_short_v3((*fnors)[j], lnors[(*lidx)[j]]);
      }
    }
  }

  if (hasLoopTangent) {
    /* need to do for all uv maps at some point */
    float(*ftangents)[4] = CustomData_get_layer(fdata, CD_TANGENT);
    float(*ltangents)[4] = CustomData_get_layer(ldata, CD_TANGENT);

    for (findex = 0, pidx = polyindices, lidx = loopindices; findex < num_faces;
         pidx++, lidx++, findex++) {
      int nverts = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3;
      for (j = nverts; j--;) {
        copy_v4_v4(ftangents[findex * 4 + j], ltangents[(*lidx)[j]]);
      }
    }
  }
}

void BKE_mesh_tangent_loops_to_tessdata(CustomData *fdata,
                                        CustomData *ldata,
                                        MFace *mface,
                                        int *polyindices,
                                        unsigned int (*loopindices)[4],
                                        const int num_faces,
                                        const char *layer_name)
{
  /* Note: performances are sub-optimal when we get a NULL mface,
   *       we could be ~25% quicker with dedicated code...
   *       Issue is, unless having two different functions with nearly the same code,
   *       there's not much ways to solve this. Better imho to live with it for now. :/ --mont29
   */

  float(*ftangents)[4] = NULL;
  float(*ltangents)[4] = NULL;

  int findex, j;
  const int *pidx;
  unsigned int(*lidx)[4];

  if (layer_name) {
    ltangents = CustomData_get_layer_named(ldata, CD_TANGENT, layer_name);
  }
  else {
    ltangents = CustomData_get_layer(ldata, CD_TANGENT);
  }

  if (ltangents) {
    /* need to do for all uv maps at some point */
    if (layer_name) {
      ftangents = CustomData_get_layer_named(fdata, CD_TANGENT, layer_name);
    }
    else {
      ftangents = CustomData_get_layer(fdata, CD_TANGENT);
    }
    if (ftangents) {
      for (findex = 0, pidx = polyindices, lidx = loopindices; findex < num_faces;
           pidx++, lidx++, findex++) {
        int nverts = (mface ? mface[findex].v4 : (*lidx)[3]) ? 4 : 3;
        for (j = nverts; j--;) {
          copy_v4_v4(ftangents[findex * 4 + j], ltangents[(*lidx)[j]]);
        }
      }
    }
  }
}

/**
 * Recreate tessellation.
 *
 * \param do_face_nor_copy: Controls whether the normals from the poly
 * are copied to the tessellated faces.
 *
 * \return number of tessellation faces.
 */
int BKE_mesh_recalc_tessellation(CustomData *fdata,
                                 CustomData *ldata,
                                 CustomData *pdata,
                                 MVert *mvert,
                                 int totface,
                                 int totloop,
                                 int totpoly,
                                 const bool do_face_nor_copy)
{
  /* use this to avoid locking pthread for _every_ polygon
   * and calling the fill function */

#define USE_TESSFACE_SPEEDUP
#define USE_TESSFACE_QUADS /* NEEDS FURTHER TESTING */

/* We abuse MFace->edcode to tag quad faces. See below for details. */
#define TESSFACE_IS_QUAD 1

  const int looptri_num = poly_to_tri_count(totpoly, totloop);

  MPoly *mp, *mpoly;
  MLoop *ml, *mloop;
  MFace *mface, *mf;
  MemArena *arena = NULL;
  int *mface_to_poly_map;
  unsigned int(*lindices)[4];
  int poly_index, mface_index;
  unsigned int j;

  mpoly = CustomData_get_layer(pdata, CD_MPOLY);
  mloop = CustomData_get_layer(ldata, CD_MLOOP);

  /* allocate the length of totfaces, avoid many small reallocs,
   * if all faces are tri's it will be correct, quads == 2x allocs */
  /* take care. we are _not_ calloc'ing so be sure to initialize each field */
  mface_to_poly_map = MEM_malloc_arrayN((size_t)looptri_num, sizeof(*mface_to_poly_map), __func__);
  mface = MEM_malloc_arrayN((size_t)looptri_num, sizeof(*mface), __func__);
  lindices = MEM_malloc_arrayN((size_t)looptri_num, sizeof(*lindices), __func__);

  mface_index = 0;
  mp = mpoly;
  for (poly_index = 0; poly_index < totpoly; poly_index++, mp++) {
    const unsigned int mp_loopstart = (unsigned int)mp->loopstart;
    const unsigned int mp_totloop = (unsigned int)mp->totloop;
    unsigned int l1, l2, l3, l4;
    unsigned int *lidx;
    if (mp_totloop < 3) {
      /* do nothing */
    }

#ifdef USE_TESSFACE_SPEEDUP

#  define ML_TO_MF(i1, i2, i3) \
    mface_to_poly_map[mface_index] = poly_index; \
    mf = &mface[mface_index]; \
    lidx = lindices[mface_index]; \
    /* set loop indices, transformed to vert indices later */ \
    l1 = mp_loopstart + i1; \
    l2 = mp_loopstart + i2; \
    l3 = mp_loopstart + i3; \
    mf->v1 = mloop[l1].v; \
    mf->v2 = mloop[l2].v; \
    mf->v3 = mloop[l3].v; \
    mf->v4 = 0; \
    lidx[0] = l1; \
    lidx[1] = l2; \
    lidx[2] = l3; \
    lidx[3] = 0; \
    mf->mat_nr = mp->mat_nr; \
    mf->flag = mp->flag; \
    mf->edcode = 0; \
    (void)0

/* ALMOST IDENTICAL TO DEFINE ABOVE (see EXCEPTION) */
#  define ML_TO_MF_QUAD() \
    mface_to_poly_map[mface_index] = poly_index; \
    mf = &mface[mface_index]; \
    lidx = lindices[mface_index]; \
    /* set loop indices, transformed to vert indices later */ \
    l1 = mp_loopstart + 0; /* EXCEPTION */ \
    l2 = mp_loopstart + 1; /* EXCEPTION */ \
    l3 = mp_loopstart + 2; /* EXCEPTION */ \
    l4 = mp_loopstart + 3; /* EXCEPTION */ \
    mf->v1 = mloop[l1].v; \
    mf->v2 = mloop[l2].v; \
    mf->v3 = mloop[l3].v; \
    mf->v4 = mloop[l4].v; \
    lidx[0] = l1; \
    lidx[1] = l2; \
    lidx[2] = l3; \
    lidx[3] = l4; \
    mf->mat_nr = mp->mat_nr; \
    mf->flag = mp->flag; \
    mf->edcode = TESSFACE_IS_QUAD; \
    (void)0

    else if (mp_totloop == 3) {
      ML_TO_MF(0, 1, 2);
      mface_index++;
    }
    else if (mp_totloop == 4) {
#  ifdef USE_TESSFACE_QUADS
      ML_TO_MF_QUAD();
      mface_index++;
#  else
      ML_TO_MF(0, 1, 2);
      mface_index++;
      ML_TO_MF(0, 2, 3);
      mface_index++;
#  endif
    }
#endif /* USE_TESSFACE_SPEEDUP */
    else {
      const float *co_curr, *co_prev;

      float normal[3];

      float axis_mat[3][3];
      float(*projverts)[2];
      unsigned int(*tris)[3];

      const unsigned int totfilltri = mp_totloop - 2;

      if (UNLIKELY(arena == NULL)) {
        arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
      }

      tris = BLI_memarena_alloc(arena, sizeof(*tris) * (size_t)totfilltri);
      projverts = BLI_memarena_alloc(arena, sizeof(*projverts) * (size_t)mp_totloop);

      zero_v3(normal);

      /* calc normal, flipped: to get a positive 2d cross product */
      ml = mloop + mp_loopstart;
      co_prev = mvert[ml[mp_totloop - 1].v].co;
      for (j = 0; j < mp_totloop; j++, ml++) {
        co_curr = mvert[ml->v].co;
        add_newell_cross_v3_v3v3(normal, co_prev, co_curr);
        co_prev = co_curr;
      }
      if (UNLIKELY(normalize_v3(normal) == 0.0f)) {
        normal[2] = 1.0f;
      }

      /* project verts to 2d */
      axis_dominant_v3_to_m3_negate(axis_mat, normal);

      ml = mloop + mp_loopstart;
      for (j = 0; j < mp_totloop; j++, ml++) {
        mul_v2_m3v3(projverts[j], axis_mat, mvert[ml->v].co);
      }

      BLI_polyfill_calc_arena(projverts, mp_totloop, 1, tris, arena);

      /* apply fill */
      for (j = 0; j < totfilltri; j++) {
        unsigned int *tri = tris[j];
        lidx = lindices[mface_index];

        mface_to_poly_map[mface_index] = poly_index;
        mf = &mface[mface_index];

        /* set loop indices, transformed to vert indices later */
        l1 = mp_loopstart + tri[0];
        l2 = mp_loopstart + tri[1];
        l3 = mp_loopstart + tri[2];

        mf->v1 = mloop[l1].v;
        mf->v2 = mloop[l2].v;
        mf->v3 = mloop[l3].v;
        mf->v4 = 0;

        lidx[0] = l1;
        lidx[1] = l2;
        lidx[2] = l3;
        lidx[3] = 0;

        mf->mat_nr = mp->mat_nr;
        mf->flag = mp->flag;
        mf->edcode = 0;

        mface_index++;
      }

      BLI_memarena_clear(arena);
    }
  }

  if (arena) {
    BLI_memarena_free(arena);
    arena = NULL;
  }

  CustomData_free(fdata, totface);
  totface = mface_index;

  BLI_assert(totface <= looptri_num);

  /* not essential but without this we store over-alloc'd memory in the CustomData layers */
  if (LIKELY(looptri_num != totface)) {
    mface = MEM_reallocN(mface, sizeof(*mface) * (size_t)totface);
    mface_to_poly_map = MEM_reallocN(mface_to_poly_map,
                                     sizeof(*mface_to_poly_map) * (size_t)totface);
  }

  CustomData_add_layer(fdata, CD_MFACE, CD_ASSIGN, mface, totface);

  /* CD_ORIGINDEX will contain an array of indices from tessfaces to the polygons
   * they are directly tessellated from */
  CustomData_add_layer(fdata, CD_ORIGINDEX, CD_ASSIGN, mface_to_poly_map, totface);
  CustomData_from_bmeshpoly(fdata, ldata, totface);

  if (do_face_nor_copy) {
    /* If polys have a normals layer, copying that to faces can help
     * avoid the need to recalculate normals later */
    if (CustomData_has_layer(pdata, CD_NORMAL)) {
      float(*pnors)[3] = CustomData_get_layer(pdata, CD_NORMAL);
      float(*fnors)[3] = CustomData_add_layer(fdata, CD_NORMAL, CD_CALLOC, NULL, totface);
      for (mface_index = 0; mface_index < totface; mface_index++) {
        copy_v3_v3(fnors[mface_index], pnors[mface_to_poly_map[mface_index]]);
      }
    }
  }

  /* NOTE: quad detection issue - fourth vertidx vs fourth loopidx:
   * Polygons take care of their loops ordering, hence not of their vertices ordering.
   * Currently, our tfaces' fourth vertex index might be 0 even for a quad. However,
   * we know our fourth loop index is never 0 for quads (because they are sorted for polygons,
   * and our quads are still mere copies of their polygons).
   * So we pass NULL as MFace pointer, and BKE_mesh_loops_to_tessdata
   * will use the fourth loop index as quad test.
   * ...
   */
  BKE_mesh_loops_to_tessdata(fdata, ldata, NULL, mface_to_poly_map, lindices, totface);

  /* NOTE: quad detection issue - fourth vertidx vs fourth loopidx:
   * ...However, most TFace code uses 'MFace->v4 == 0' test to check whether it is a tri or quad.
   * test_index_face() will check this and rotate the tessellated face if needed.
   */
#ifdef USE_TESSFACE_QUADS
  mf = mface;
  for (mface_index = 0; mface_index < totface; mface_index++, mf++) {
    if (mf->edcode == TESSFACE_IS_QUAD) {
      test_index_face(mf, fdata, mface_index, 4);
      mf->edcode = 0;
    }
  }
#endif

  MEM_freeN(lindices);

  return totface;

#undef USE_TESSFACE_SPEEDUP
#undef USE_TESSFACE_QUADS

#undef ML_TO_MF
#undef ML_TO_MF_QUAD
}

/**
 * Calculate tessellation into #MLoopTri which exist only for this purpose.
 */
void BKE_mesh_recalc_looptri(const MLoop *mloop,
                             const MPoly *mpoly,
                             const MVert *mvert,
                             int totloop,
                             int totpoly,
                             MLoopTri *mlooptri)
{
  /* use this to avoid locking pthread for _every_ polygon
   * and calling the fill function */

#define USE_TESSFACE_SPEEDUP

  const MPoly *mp;
  const MLoop *ml;
  MLoopTri *mlt;
  MemArena *arena = NULL;
  int poly_index, mlooptri_index;
  unsigned int j;

  mlooptri_index = 0;
  mp = mpoly;
  for (poly_index = 0; poly_index < totpoly; poly_index++, mp++) {
    const unsigned int mp_loopstart = (unsigned int)mp->loopstart;
    const unsigned int mp_totloop = (unsigned int)mp->totloop;
    unsigned int l1, l2, l3;
    if (mp_totloop < 3) {
      /* do nothing */
    }

#ifdef USE_TESSFACE_SPEEDUP

#  define ML_TO_MLT(i1, i2, i3) \
    { \
      mlt = &mlooptri[mlooptri_index]; \
      l1 = mp_loopstart + i1; \
      l2 = mp_loopstart + i2; \
      l3 = mp_loopstart + i3; \
      ARRAY_SET_ITEMS(mlt->tri, l1, l2, l3); \
      mlt->poly = (unsigned int)poly_index; \
    } \
    ((void)0)

    else if (mp_totloop == 3) {
      ML_TO_MLT(0, 1, 2);
      mlooptri_index++;
    }
    else if (mp_totloop == 4) {
      ML_TO_MLT(0, 1, 2);
      MLoopTri *mlt_a = mlt;
      mlooptri_index++;
      ML_TO_MLT(0, 2, 3);
      MLoopTri *mlt_b = mlt;
      mlooptri_index++;

      if (UNLIKELY(is_quad_flip_v3_first_third_fast(mvert[mloop[mlt_a->tri[0]].v].co,
                                                    mvert[mloop[mlt_a->tri[1]].v].co,
                                                    mvert[mloop[mlt_a->tri[2]].v].co,
                                                    mvert[mloop[mlt_b->tri[2]].v].co))) {
        /* flip out of degenerate 0-2 state. */
        mlt_a->tri[2] = mlt_b->tri[2];
        mlt_b->tri[0] = mlt_a->tri[1];
      }
    }
#endif /* USE_TESSFACE_SPEEDUP */
    else {
      const float *co_curr, *co_prev;

      float normal[3];

      float axis_mat[3][3];
      float(*projverts)[2];
      unsigned int(*tris)[3];

      const unsigned int totfilltri = mp_totloop - 2;

      if (UNLIKELY(arena == NULL)) {
        arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
      }

      tris = BLI_memarena_alloc(arena, sizeof(*tris) * (size_t)totfilltri);
      projverts = BLI_memarena_alloc(arena, sizeof(*projverts) * (size_t)mp_totloop);

      zero_v3(normal);

      /* calc normal, flipped: to get a positive 2d cross product */
      ml = mloop + mp_loopstart;
      co_prev = mvert[ml[mp_totloop - 1].v].co;
      for (j = 0; j < mp_totloop; j++, ml++) {
        co_curr = mvert[ml->v].co;
        add_newell_cross_v3_v3v3(normal, co_prev, co_curr);
        co_prev = co_curr;
      }
      if (UNLIKELY(normalize_v3(normal) == 0.0f)) {
        normal[2] = 1.0f;
      }

      /* project verts to 2d */
      axis_dominant_v3_to_m3_negate(axis_mat, normal);

      ml = mloop + mp_loopstart;
      for (j = 0; j < mp_totloop; j++, ml++) {
        mul_v2_m3v3(projverts[j], axis_mat, mvert[ml->v].co);
      }

      BLI_polyfill_calc_arena(projverts, mp_totloop, 1, tris, arena);

      /* apply fill */
      for (j = 0; j < totfilltri; j++) {
        unsigned int *tri = tris[j];

        mlt = &mlooptri[mlooptri_index];

        /* set loop indices, transformed to vert indices later */
        l1 = mp_loopstart + tri[0];
        l2 = mp_loopstart + tri[1];
        l3 = mp_loopstart + tri[2];

        ARRAY_SET_ITEMS(mlt->tri, l1, l2, l3);
        mlt->poly = (unsigned int)poly_index;

        mlooptri_index++;
      }

      BLI_memarena_clear(arena);
    }
  }

  if (arena) {
    BLI_memarena_free(arena);
    arena = NULL;
  }

  BLI_assert(mlooptri_index == poly_to_tri_count(totpoly, totloop));
  UNUSED_VARS_NDEBUG(totloop);

#undef USE_TESSFACE_SPEEDUP
#undef ML_TO_MLT
}

static void bm_corners_to_loops_ex(ID *id,
                                   CustomData *fdata,
                                   CustomData *ldata,
                                   MFace *mface,
                                   int totloop,
                                   int findex,
                                   int loopstart,
                                   int numTex,
                                   int numCol)
{
  MTFace *texface;
  MCol *mcol;
  MLoopCol *mloopcol;
  MLoopUV *mloopuv;
  MFace *mf;
  int i;

  mf = mface + findex;

  for (i = 0; i < numTex; i++) {
    texface = CustomData_get_n(fdata, CD_MTFACE, findex, i);

    mloopuv = CustomData_get_n(ldata, CD_MLOOPUV, loopstart, i);
    copy_v2_v2(mloopuv->uv, texface->uv[0]);
    mloopuv++;
    copy_v2_v2(mloopuv->uv, texface->uv[1]);
    mloopuv++;
    copy_v2_v2(mloopuv->uv, texface->uv[2]);
    mloopuv++;

    if (mf->v4) {
      copy_v2_v2(mloopuv->uv, texface->uv[3]);
      mloopuv++;
    }
  }

  for (i = 0; i < numCol; i++) {
    mloopcol = CustomData_get_n(ldata, CD_MLOOPCOL, loopstart, i);
    mcol = CustomData_get_n(fdata, CD_MCOL, findex, i);

    MESH_MLOOPCOL_FROM_MCOL(mloopcol, &mcol[0]);
    mloopcol++;
    MESH_MLOOPCOL_FROM_MCOL(mloopcol, &mcol[1]);
    mloopcol++;
    MESH_MLOOPCOL_FROM_MCOL(mloopcol, &mcol[2]);
    mloopcol++;
    if (mf->v4) {
      MESH_MLOOPCOL_FROM_MCOL(mloopcol, &mcol[3]);
      mloopcol++;
    }
  }

  if (CustomData_has_layer(fdata, CD_TESSLOOPNORMAL)) {
    float(*lnors)[3] = CustomData_get(ldata, loopstart, CD_NORMAL);
    short(*tlnors)[3] = CustomData_get(fdata, findex, CD_TESSLOOPNORMAL);
    const int max = mf->v4 ? 4 : 3;

    for (i = 0; i < max; i++, lnors++, tlnors++) {
      normal_short_to_float_v3(*lnors, *tlnors);
    }
  }

  if (CustomData_has_layer(fdata, CD_MDISPS)) {
    MDisps *ld = CustomData_get(ldata, loopstart, CD_MDISPS);
    MDisps *fd = CustomData_get(fdata, findex, CD_MDISPS);
    float(*disps)[3] = fd->disps;
    int tot = mf->v4 ? 4 : 3;
    int corners;

    if (CustomData_external_test(fdata, CD_MDISPS)) {
      if (id && fdata->external) {
        CustomData_external_add(ldata, id, CD_MDISPS, totloop, fdata->external->filename);
      }
    }

    corners = multires_mdisp_corners(fd);

    if (corners == 0) {
      /* Empty MDisp layers appear in at least one of the sintel.blend files.
       * Not sure why this happens, but it seems fine to just ignore them here.
       * If (corners == 0) for a non-empty layer though, something went wrong. */
      BLI_assert(fd->totdisp == 0);
    }
    else {
      const int side = (int)sqrtf((float)(fd->totdisp / corners));
      const int side_sq = side * side;

      for (i = 0; i < tot; i++, disps += side_sq, ld++) {
        ld->totdisp = side_sq;
        ld->level = (int)(logf((float)side - 1.0f) / (float)M_LN2) + 1;

        if (ld->disps) {
          MEM_freeN(ld->disps);
        }

        ld->disps = MEM_malloc_arrayN((size_t)side_sq, sizeof(float[3]), "converted loop mdisps");
        if (fd->disps) {
          memcpy(ld->disps, disps, (size_t)side_sq * sizeof(float[3]));
        }
        else {
          memset(ld->disps, 0, (size_t)side_sq * sizeof(float[3]));
        }
      }
    }
  }
}

void BKE_mesh_convert_mfaces_to_mpolys(Mesh *mesh)
{
  BKE_mesh_convert_mfaces_to_mpolys_ex(&mesh->id,
                                       &mesh->fdata,
                                       &mesh->ldata,
                                       &mesh->pdata,
                                       mesh->totedge,
                                       mesh->totface,
                                       mesh->totloop,
                                       mesh->totpoly,
                                       mesh->medge,
                                       mesh->mface,
                                       &mesh->totloop,
                                       &mesh->totpoly,
                                       &mesh->mloop,
                                       &mesh->mpoly);

  BKE_mesh_update_customdata_pointers(mesh, true);
}

/**
 * The same as #BKE_mesh_convert_mfaces_to_mpolys
 * but oriented to be used in #do_versions from readfile.c
 * the difference is how active/render/clone/stencil indices are handled here
 *
 * normally thay're being set from pdata which totally makes sense for meshes which are already
 * converted to bmesh structures, but when loading older files indices shall be updated in other
 * way around, so newly added pdata and ldata would have this indices set based on fdata layer
 *
 * this is normally only needed when reading older files,
 * in all other cases #BKE_mesh_convert_mfaces_to_mpolys shall be always used
 */
void BKE_mesh_do_versions_convert_mfaces_to_mpolys(Mesh *mesh)
{
  BKE_mesh_convert_mfaces_to_mpolys_ex(&mesh->id,
                                       &mesh->fdata,
                                       &mesh->ldata,
                                       &mesh->pdata,
                                       mesh->totedge,
                                       mesh->totface,
                                       mesh->totloop,
                                       mesh->totpoly,
                                       mesh->medge,
                                       mesh->mface,
                                       &mesh->totloop,
                                       &mesh->totpoly,
                                       &mesh->mloop,
                                       &mesh->mpoly);

  CustomData_bmesh_do_versions_update_active_layers(&mesh->fdata, &mesh->ldata);

  BKE_mesh_update_customdata_pointers(mesh, true);
}

void BKE_mesh_convert_mfaces_to_mpolys_ex(ID *id,
                                          CustomData *fdata,
                                          CustomData *ldata,
                                          CustomData *pdata,
                                          int totedge_i,
                                          int totface_i,
                                          int totloop_i,
                                          int totpoly_i,
                                          MEdge *medge,
                                          MFace *mface,
                                          int *r_totloop,
                                          int *r_totpoly,
                                          MLoop **r_mloop,
                                          MPoly **r_mpoly)
{
  MFace *mf;
  MLoop *ml, *mloop;
  MPoly *mp, *mpoly;
  MEdge *me;
  EdgeHash *eh;
  int numTex, numCol;
  int i, j, totloop, totpoly, *polyindex;

  /* old flag, clear to allow for reuse */
#define ME_FGON (1 << 3)

  /* just in case some of these layers are filled in (can happen with python created meshes) */
  CustomData_free(ldata, totloop_i);
  CustomData_free(pdata, totpoly_i);

  totpoly = totface_i;
  mpoly = MEM_calloc_arrayN((size_t)totpoly, sizeof(MPoly), "mpoly converted");
  CustomData_add_layer(pdata, CD_MPOLY, CD_ASSIGN, mpoly, totpoly);

  numTex = CustomData_number_of_layers(fdata, CD_MTFACE);
  numCol = CustomData_number_of_layers(fdata, CD_MCOL);

  totloop = 0;
  mf = mface;
  for (i = 0; i < totface_i; i++, mf++) {
    totloop += mf->v4 ? 4 : 3;
  }

  mloop = MEM_calloc_arrayN((size_t)totloop, sizeof(MLoop), "mloop converted");

  CustomData_add_layer(ldata, CD_MLOOP, CD_ASSIGN, mloop, totloop);

  CustomData_to_bmeshpoly(fdata, ldata, totloop);

  if (id) {
    /* ensure external data is transferred */
    CustomData_external_read(fdata, id, CD_MASK_MDISPS, totface_i);
  }

  eh = BLI_edgehash_new_ex(__func__, (unsigned int)totedge_i);

  /* build edge hash */
  me = medge;
  for (i = 0; i < totedge_i; i++, me++) {
    BLI_edgehash_insert(eh, me->v1, me->v2, POINTER_FROM_UINT(i));

    /* unrelated but avoid having the FGON flag enabled,
     * so we can reuse it later for something else */
    me->flag &= ~ME_FGON;
  }

  polyindex = CustomData_get_layer(fdata, CD_ORIGINDEX);

  j = 0; /* current loop index */
  ml = mloop;
  mf = mface;
  mp = mpoly;
  for (i = 0; i < totface_i; i++, mf++, mp++) {
    mp->loopstart = j;

    mp->totloop = mf->v4 ? 4 : 3;

    mp->mat_nr = mf->mat_nr;
    mp->flag = mf->flag;

#define ML(v1, v2) \
  { \
    ml->v = mf->v1; \
    ml->e = POINTER_AS_UINT(BLI_edgehash_lookup(eh, mf->v1, mf->v2)); \
    ml++; \
    j++; \
  } \
  (void)0

    ML(v1, v2);
    ML(v2, v3);
    if (mf->v4) {
      ML(v3, v4);
      ML(v4, v1);
    }
    else {
      ML(v3, v1);
    }

#undef ML

    bm_corners_to_loops_ex(id, fdata, ldata, mface, totloop, i, mp->loopstart, numTex, numCol);

    if (polyindex) {
      *polyindex = i;
      polyindex++;
    }
  }

  /* note, we don't convert NGons at all, these are not even real ngons,
   * they have their own UV's, colors etc - its more an editing feature. */

  BLI_edgehash_free(eh, NULL);

  *r_totpoly = totpoly;
  *r_totloop = totloop;
  *r_mpoly = mpoly;
  *r_mloop = mloop;

#undef ME_FGON
}
/** \} */

/**
 * Flip a single MLoop's #MDisps structure,
 * low level function to be called from face-flipping code which re-arranged the mdisps themselves.
 */
void BKE_mesh_mdisp_flip(MDisps *md, const bool use_loop_mdisp_flip)
{
  if (UNLIKELY(!md->totdisp || !md->disps)) {
    return;
  }

  const int sides = (int)sqrt(md->totdisp);
  float(*co)[3] = md->disps;

  for (int x = 0; x < sides; x++) {
    float *co_a, *co_b;

    for (int y = 0; y < x; y++) {
      co_a = co[y * sides + x];
      co_b = co[x * sides + y];

      swap_v3_v3(co_a, co_b);
      SWAP(float, co_a[0], co_a[1]);
      SWAP(float, co_b[0], co_b[1]);

      if (use_loop_mdisp_flip) {
        co_a[2] *= -1.0f;
        co_b[2] *= -1.0f;
      }
    }

    co_a = co[x * sides + x];

    SWAP(float, co_a[0], co_a[1]);

    if (use_loop_mdisp_flip) {
      co_a[2] *= -1.0f;
    }
  }
}

/**
 * Flip (invert winding of) the given \a mpoly, i.e. reverse order of its loops
 * (keeping the same vertex as 'start point').
 *
 * \param mpoly: the polygon to flip.
 * \param mloop: the full loops array.
 * \param ldata: the loops custom data.
 */
void BKE_mesh_polygon_flip_ex(MPoly *mpoly,
                              MLoop *mloop,
                              CustomData *ldata,
                              float (*lnors)[3],
                              MDisps *mdisp,
                              const bool use_loop_mdisp_flip)
{
  int loopstart = mpoly->loopstart;
  int loopend = loopstart + mpoly->totloop - 1;
  const bool loops_in_ldata = (CustomData_get_layer(ldata, CD_MLOOP) == mloop);

  if (mdisp) {
    for (int i = loopstart; i <= loopend; i++) {
      BKE_mesh_mdisp_flip(&mdisp[i], use_loop_mdisp_flip);
    }
  }

  /* Note that we keep same start vertex for flipped face. */

  /* We also have to update loops edge
   * (they will get their original 'other edge', that is,
   * the original edge of their original previous loop)... */
  unsigned int prev_edge_index = mloop[loopstart].e;
  mloop[loopstart].e = mloop[loopend].e;

  for (loopstart++; loopend > loopstart; loopstart++, loopend--) {
    mloop[loopend].e = mloop[loopend - 1].e;
    SWAP(unsigned int, mloop[loopstart].e, prev_edge_index);

    if (!loops_in_ldata) {
      SWAP(MLoop, mloop[loopstart], mloop[loopend]);
    }
    if (lnors) {
      swap_v3_v3(lnors[loopstart], lnors[loopend]);
    }
    CustomData_swap(ldata, loopstart, loopend);
  }
  /* Even if we did not swap the other 'pivot' loop, we need to set its swapped edge. */
  if (loopstart == loopend) {
    mloop[loopstart].e = prev_edge_index;
  }
}

void BKE_mesh_polygon_flip(MPoly *mpoly, MLoop *mloop, CustomData *ldata)
{
  MDisps *mdisp = CustomData_get_layer(ldata, CD_MDISPS);
  BKE_mesh_polygon_flip_ex(mpoly, mloop, ldata, NULL, mdisp, true);
}

/**
 * Flip (invert winding of) all polygons (used to inverse their normals).
 *
 * \note Invalidates tessellation, caller must handle that.
 */
void BKE_mesh_polygons_flip(MPoly *mpoly, MLoop *mloop, CustomData *ldata, int totpoly)
{
  MDisps *mdisp = CustomData_get_layer(ldata, CD_MDISPS);
  MPoly *mp;
  int i;

  for (mp = mpoly, i = 0; i < totpoly; mp++, i++) {
    BKE_mesh_polygon_flip_ex(mp, mloop, ldata, NULL, mdisp, true);
  }
}

/* -------------------------------------------------------------------- */
/** \name Mesh Flag Flushing
 * \{ */

/* update the hide flag for edges and faces from the corresponding
 * flag in verts */
void BKE_mesh_flush_hidden_from_verts_ex(const MVert *mvert,
                                         const MLoop *mloop,
                                         MEdge *medge,
                                         const int totedge,
                                         MPoly *mpoly,
                                         const int totpoly)
{
  int i, j;

  for (i = 0; i < totedge; i++) {
    MEdge *e = &medge[i];
    if (mvert[e->v1].flag & ME_HIDE || mvert[e->v2].flag & ME_HIDE) {
      e->flag |= ME_HIDE;
    }
    else {
      e->flag &= ~ME_HIDE;
    }
  }
  for (i = 0; i < totpoly; i++) {
    MPoly *p = &mpoly[i];
    p->flag &= (char)~ME_HIDE;
    for (j = 0; j < p->totloop; j++) {
      if (mvert[mloop[p->loopstart + j].v].flag & ME_HIDE) {
        p->flag |= ME_HIDE;
      }
    }
  }
}
void BKE_mesh_flush_hidden_from_verts(Mesh *me)
{
  BKE_mesh_flush_hidden_from_verts_ex(
      me->mvert, me->mloop, me->medge, me->totedge, me->mpoly, me->totpoly);
}

void BKE_mesh_flush_hidden_from_polys_ex(MVert *mvert,
                                         const MLoop *mloop,
                                         MEdge *medge,
                                         const int UNUSED(totedge),
                                         const MPoly *mpoly,
                                         const int totpoly)
{
  const MPoly *mp;
  int i;

  i = totpoly;
  for (mp = mpoly; i--; mp++) {
    if (mp->flag & ME_HIDE) {
      const MLoop *ml;
      int j;
      j = mp->totloop;
      for (ml = &mloop[mp->loopstart]; j--; ml++) {
        mvert[ml->v].flag |= ME_HIDE;
        medge[ml->e].flag |= ME_HIDE;
      }
    }
  }

  i = totpoly;
  for (mp = mpoly; i--; mp++) {
    if ((mp->flag & ME_HIDE) == 0) {
      const MLoop *ml;
      int j;
      j = mp->totloop;
      for (ml = &mloop[mp->loopstart]; j--; ml++) {
        mvert[ml->v].flag &= (char)~ME_HIDE;
        medge[ml->e].flag &= (short)~ME_HIDE;
      }
    }
  }
}
void BKE_mesh_flush_hidden_from_polys(Mesh *me)
{
  BKE_mesh_flush_hidden_from_polys_ex(
      me->mvert, me->mloop, me->medge, me->totedge, me->mpoly, me->totpoly);
}

/**
 * simple poly -> vert/edge selection.
 */
void BKE_mesh_flush_select_from_polys_ex(MVert *mvert,
                                         const int totvert,
                                         const MLoop *mloop,
                                         MEdge *medge,
                                         const int totedge,
                                         const MPoly *mpoly,
                                         const int totpoly)
{
  MVert *mv;
  MEdge *med;
  const MPoly *mp;
  int i;

  i = totvert;
  for (mv = mvert; i--; mv++) {
    mv->flag &= (char)~SELECT;
  }

  i = totedge;
  for (med = medge; i--; med++) {
    med->flag &= ~SELECT;
  }

  i = totpoly;
  for (mp = mpoly; i--; mp++) {
    /* assume if its selected its not hidden and none of its verts/edges are hidden
     * (a common assumption)*/
    if (mp->flag & ME_FACE_SEL) {
      const MLoop *ml;
      int j;
      j = mp->totloop;
      for (ml = &mloop[mp->loopstart]; j--; ml++) {
        mvert[ml->v].flag |= SELECT;
        medge[ml->e].flag |= SELECT;
      }
    }
  }
}
void BKE_mesh_flush_select_from_polys(Mesh *me)
{
  BKE_mesh_flush_select_from_polys_ex(
      me->mvert, me->totvert, me->mloop, me->medge, me->totedge, me->mpoly, me->totpoly);
}

void BKE_mesh_flush_select_from_verts_ex(const MVert *mvert,
                                         const int UNUSED(totvert),
                                         const MLoop *mloop,
                                         MEdge *medge,
                                         const int totedge,
                                         MPoly *mpoly,
                                         const int totpoly)
{
  MEdge *med;
  MPoly *mp;
  int i;

  /* edges */
  i = totedge;
  for (med = medge; i--; med++) {
    if ((med->flag & ME_HIDE) == 0) {
      if ((mvert[med->v1].flag & SELECT) && (mvert[med->v2].flag & SELECT)) {
        med->flag |= SELECT;
      }
      else {
        med->flag &= ~SELECT;
      }
    }
  }

  /* polys */
  i = totpoly;
  for (mp = mpoly; i--; mp++) {
    if ((mp->flag & ME_HIDE) == 0) {
      bool ok = true;
      const MLoop *ml;
      int j;
      j = mp->totloop;
      for (ml = &mloop[mp->loopstart]; j--; ml++) {
        if ((mvert[ml->v].flag & SELECT) == 0) {
          ok = false;
          break;
        }
      }

      if (ok) {
        mp->flag |= ME_FACE_SEL;
      }
      else {
        mp->flag &= (char)~ME_FACE_SEL;
      }
    }
  }
}
void BKE_mesh_flush_select_from_verts(Mesh *me)
{
  BKE_mesh_flush_select_from_verts_ex(
      me->mvert, me->totvert, me->mloop, me->medge, me->totedge, me->mpoly, me->totpoly);
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Spatial Calculation
 * \{ */

/**
 * This function takes the difference between 2 vertex-coord-arrays
 * (\a vert_cos_src, \a vert_cos_dst),
 * and applies the difference to \a vert_cos_new relative to \a vert_cos_org.
 *
 * \param vert_cos_src: reference deform source.
 * \param vert_cos_dst: reference deform destination.
 *
 * \param vert_cos_org: reference for the output location.
 * \param vert_cos_new: resulting coords.
 */
void BKE_mesh_calc_relative_deform(const MPoly *mpoly,
                                   const int totpoly,
                                   const MLoop *mloop,
                                   const int totvert,

                                   const float (*vert_cos_src)[3],
                                   const float (*vert_cos_dst)[3],

                                   const float (*vert_cos_org)[3],
                                   float (*vert_cos_new)[3])
{
  const MPoly *mp;
  int i;

  int *vert_accum = MEM_calloc_arrayN((size_t)totvert, sizeof(*vert_accum), __func__);

  memset(vert_cos_new, '\0', sizeof(*vert_cos_new) * (size_t)totvert);

  for (i = 0, mp = mpoly; i < totpoly; i++, mp++) {
    const MLoop *loopstart = mloop + mp->loopstart;
    int j;

    for (j = 0; j < mp->totloop; j++) {
      unsigned int v_prev = loopstart[(mp->totloop + (j - 1)) % mp->totloop].v;
      unsigned int v_curr = loopstart[j].v;
      unsigned int v_next = loopstart[(j + 1) % mp->totloop].v;

      float tvec[3];

      transform_point_by_tri_v3(tvec,
                                vert_cos_dst[v_curr],
                                vert_cos_org[v_prev],
                                vert_cos_org[v_curr],
                                vert_cos_org[v_next],
                                vert_cos_src[v_prev],
                                vert_cos_src[v_curr],
                                vert_cos_src[v_next]);

      add_v3_v3(vert_cos_new[v_curr], tvec);
      vert_accum[v_curr] += 1;
    }
  }

  for (i = 0; i < totvert; i++) {
    if (vert_accum[i]) {
      mul_v3_fl(vert_cos_new[i], 1.0f / (float)vert_accum[i]);
    }
    else {
      copy_v3_v3(vert_cos_new[i], vert_cos_org[i]);
    }
  }

  MEM_freeN(vert_accum);
}
/** \} */
