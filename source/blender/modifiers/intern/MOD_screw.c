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

/* Screw modifier: revolves the edges about an axis */
#include <limits.h>

#include "BLI_utildefines.h"

#include "BLI_math.h"
#include "BLI_alloca.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_library_query.h"
#include "BKE_mesh.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"
#include "MEM_guardedalloc.h"

#include "BLI_strict_flags.h"

/* used for gathering edge connectivity */
typedef struct ScrewVertConnect {
  float dist;        /* distance from the center axis */
  float co[3];       /* location relative to the transformed axis */
  float no[3];       /* calc normal of the vertex */
  unsigned int v[2]; /* 2  verts on either side of this one */
  MEdge *e[2];       /* edges on either side, a bit of a waste since each edge ref's 2 edges */
  char flag;
} ScrewVertConnect;

typedef struct ScrewVertIter {
  ScrewVertConnect *v_array;
  ScrewVertConnect *v_poin;
  unsigned int v, v_other;
  MEdge *e;
} ScrewVertIter;

#define SV_UNUSED (UINT_MAX)
#define SV_INVALID ((UINT_MAX)-1)
#define SV_IS_VALID(v) ((v) < SV_INVALID)

static void screwvert_iter_init(ScrewVertIter *iter,
                                ScrewVertConnect *array,
                                unsigned int v_init,
                                unsigned int dir)
{
  iter->v_array = array;
  iter->v = v_init;

  if (SV_IS_VALID(v_init)) {
    iter->v_poin = &array[v_init];
    iter->v_other = iter->v_poin->v[dir];
    iter->e = iter->v_poin->e[!dir];
  }
  else {
    iter->v_poin = NULL;
    iter->e = NULL;
  }
}

static void screwvert_iter_step(ScrewVertIter *iter)
{
  if (iter->v_poin->v[0] == iter->v_other) {
    iter->v_other = iter->v;
    iter->v = iter->v_poin->v[1];
  }
  else if (iter->v_poin->v[1] == iter->v_other) {
    iter->v_other = iter->v;
    iter->v = iter->v_poin->v[0];
  }
  if (SV_IS_VALID(iter->v)) {
    iter->v_poin = &iter->v_array[iter->v];
    iter->e = iter->v_poin->e[(iter->v_poin->e[0] == iter->e)];
  }
  else {
    iter->e = NULL;
    iter->v_poin = NULL;
  }
}

static Mesh *mesh_remove_doubles_on_axis(Mesh *result,
                                         MVert *mvert_new,
                                         const uint totvert,
                                         const uint step_tot,
                                         const float axis_vec[3],
                                         const float axis_offset[3],
                                         const float merge_threshold)
{
  const float merge_threshold_sq = SQUARE(merge_threshold);
  const bool use_offset = axis_offset != NULL;
  uint tot_doubles = 0;
  for (uint i = 0; i < totvert; i += 1) {
    float axis_co[3];
    if (use_offset) {
      float offset_co[3];
      sub_v3_v3v3(offset_co, mvert_new[i].co, axis_offset);
      project_v3_v3v3_normalized(axis_co, offset_co, axis_vec);
      add_v3_v3(axis_co, axis_offset);
    }
    else {
      project_v3_v3v3_normalized(axis_co, mvert_new[i].co, axis_vec);
    }
    const float dist_sq = len_squared_v3v3(axis_co, mvert_new[i].co);
    if (dist_sq <= merge_threshold_sq) {
      mvert_new[i].flag |= ME_VERT_TMP_TAG;
      tot_doubles += 1;
      copy_v3_v3(mvert_new[i].co, axis_co);
    }
  }

  if (tot_doubles != 0) {
    uint tot = totvert * step_tot;
    int *full_doubles_map = MEM_malloc_arrayN(tot, sizeof(int), __func__);
    copy_vn_i(full_doubles_map, (int)tot, -1);

    uint tot_doubles_left = tot_doubles;
    for (uint i = 0; i < totvert; i += 1) {
      if (mvert_new[i].flag & ME_VERT_TMP_TAG) {
        int *doubles_map = &full_doubles_map[totvert + i];
        for (uint step = 1; step < step_tot; step += 1) {
          *doubles_map = (int)i;
          doubles_map += totvert;
        }
        tot_doubles_left -= 1;
        if (tot_doubles_left == 0) {
          break;
        }
      }
    }
    result = BKE_mesh_merge_verts(result,
                                  full_doubles_map,
                                  (int)(tot_doubles * (step_tot - 1)),
                                  MESH_MERGE_VERTS_DUMP_IF_MAPPED);
    MEM_freeN(full_doubles_map);
  }
  return result;
}

static void initData(ModifierData *md)
{
  ScrewModifierData *ltmd = (ScrewModifierData *)md;
  ltmd->ob_axis = NULL;
  ltmd->angle = (float)(M_PI * 2.0);
  ltmd->axis = 2;
  ltmd->flag = MOD_SCREW_SMOOTH_SHADING;
  ltmd->steps = 16;
  ltmd->render_steps = 16;
  ltmd->iter = 1;
  ltmd->merge_dist = 0.01f;
}

static Mesh *applyModifier(ModifierData *md, const ModifierEvalContext *ctx, Mesh *meshData)
{
  Mesh *mesh = meshData;
  Mesh *result;
  ScrewModifierData *ltmd = (ScrewModifierData *)md;
  const bool use_render_params = (ctx->flag & MOD_APPLY_RENDER) != 0;

  int *origindex;
  int mpoly_index = 0;
  unsigned int step;
  unsigned int i, j;
  unsigned int i1, i2;
  unsigned int step_tot = use_render_params ? ltmd->render_steps : ltmd->steps;
  const bool do_flip = (ltmd->flag & MOD_SCREW_NORMAL_FLIP) != 0;

  const int quad_ord[4] = {
      do_flip ? 3 : 0,
      do_flip ? 2 : 1,
      do_flip ? 1 : 2,
      do_flip ? 0 : 3,
  };
  const int quad_ord_ofs[4] = {
      do_flip ? 2 : 0,
      1,
      do_flip ? 0 : 2,
      3,
  };

  unsigned int maxVerts = 0, maxEdges = 0, maxPolys = 0;
  const unsigned int totvert = (unsigned int)mesh->totvert;
  const unsigned int totedge = (unsigned int)mesh->totedge;
  const unsigned int totpoly = (unsigned int)mesh->totpoly;

  unsigned int *edge_poly_map = NULL; /* orig edge to orig poly */
  unsigned int *vert_loop_map = NULL; /* orig vert to orig loop */

  /* UV Coords */
  const unsigned int mloopuv_layers_tot = (unsigned int)CustomData_number_of_layers(&mesh->ldata,
                                                                                    CD_MLOOPUV);
  MLoopUV **mloopuv_layers = BLI_array_alloca(mloopuv_layers, mloopuv_layers_tot);
  float uv_u_scale;
  float uv_v_minmax[2] = {FLT_MAX, -FLT_MAX};
  float uv_v_range_inv;
  float uv_axis_plane[4];

  char axis_char = 'X';
  bool close;
  float angle = ltmd->angle;
  float screw_ofs = ltmd->screw_ofs;
  float axis_vec[3] = {0.0f, 0.0f, 0.0f};
  float tmp_vec1[3], tmp_vec2[3];
  float mat3[3][3];
  /* transform the coords by an object relative to this objects transformation */
  float mtx_tx[4][4];
  float mtx_tx_inv[4][4]; /* inverted */
  float mtx_tmp_a[4][4];

  unsigned int vc_tot_linked = 0;
  short other_axis_1, other_axis_2;
  const float *tmpf1, *tmpf2;

  unsigned int edge_offset;

  MPoly *mpoly_orig, *mpoly_new, *mp_new;
  MLoop *mloop_orig, *mloop_new, *ml_new;
  MEdge *medge_orig, *med_orig, *med_new, *med_new_firstloop, *medge_new;
  MVert *mvert_new, *mvert_orig, *mv_orig, *mv_new, *mv_new_base;

  Object *ob_axis = ltmd->ob_axis;

  ScrewVertConnect *vc, *vc_tmp, *vert_connect = NULL;

  const char mpoly_flag = (ltmd->flag & MOD_SCREW_SMOOTH_SHADING) ? ME_SMOOTH : 0;

  /* don't do anything? */
  if (!totvert) {
    return BKE_mesh_new_nomain_from_template(mesh, 0, 0, 0, 0, 0);
  }

  switch (ltmd->axis) {
    case 0:
      other_axis_1 = 1;
      other_axis_2 = 2;
      break;
    case 1:
      other_axis_1 = 0;
      other_axis_2 = 2;
      break;
    default: /* 2, use default to quiet warnings */
      other_axis_1 = 0;
      other_axis_2 = 1;
      break;
  }

  axis_vec[ltmd->axis] = 1.0f;

  if (ob_axis != NULL) {
    /* calc the matrix relative to the axis object */
    invert_m4_m4(mtx_tmp_a, ctx->object->obmat);
    copy_m4_m4(mtx_tx_inv, ob_axis->obmat);
    mul_m4_m4m4(mtx_tx, mtx_tmp_a, mtx_tx_inv);

    /* calc the axis vec */
    mul_mat3_m4_v3(mtx_tx, axis_vec); /* only rotation component */
    normalize_v3(axis_vec);

    /* screw */
    if (ltmd->flag & MOD_SCREW_OBJECT_OFFSET) {
      /* find the offset along this axis relative to this objects matrix */
      float totlen = len_v3(mtx_tx[3]);

      if (totlen != 0.0f) {
        float zero[3] = {0.0f, 0.0f, 0.0f};
        float cp[3];
        screw_ofs = closest_to_line_v3(cp, mtx_tx[3], zero, axis_vec);
      }
      else {
        screw_ofs = 0.0f;
      }
    }

    /* angle */

#if 0 /* can't include this, not predictable enough, though quite fun. */
    if (ltmd->flag & MOD_SCREW_OBJECT_ANGLE) {
      float mtx3_tx[3][3];
      copy_m3_m4(mtx3_tx, mtx_tx);

      float vec[3] = {0, 1, 0};
      float cross1[3];
      float cross2[3];
      cross_v3_v3v3(cross1, vec, axis_vec);

      mul_v3_m3v3(cross2, mtx3_tx, cross1);
      {
        float c1[3];
        float c2[3];
        float axis_tmp[3];

        cross_v3_v3v3(c1, cross2, axis_vec);
        cross_v3_v3v3(c2, axis_vec, c1);

        angle = angle_v3v3(cross1, c2);

        cross_v3_v3v3(axis_tmp, cross1, c2);
        normalize_v3(axis_tmp);

        if (len_v3v3(axis_tmp, axis_vec) > 1.0f) {
          angle = -angle;
        }
      }
    }
#endif
  }
  else {
    /* exis char is used by i_rotate*/
    axis_char = (char)(axis_char + ltmd->axis); /* 'X' + axis */

    /* useful to be able to use the axis vec in some cases still */
    zero_v3(axis_vec);
    axis_vec[ltmd->axis] = 1.0f;
  }

  /* apply the multiplier */
  angle *= (float)ltmd->iter;
  screw_ofs *= (float)ltmd->iter;
  uv_u_scale = 1.0f / (float)(step_tot);

  /* multiplying the steps is a bit tricky, this works best */
  step_tot = ((step_tot + 1) * ltmd->iter) - (ltmd->iter - 1);

  /* Will the screw be closed?
   * Note! smaller then `FLT_EPSILON * 100`
   * gives problems with float precision so its never closed. */
  if (fabsf(screw_ofs) <= (FLT_EPSILON * 100.0f) &&
      fabsf(fabsf(angle) - ((float)M_PI * 2.0f)) <= (FLT_EPSILON * 100.0f)) {
    close = 1;
    step_tot--;
    if (step_tot < 3) {
      step_tot = 3;
    }

    maxVerts = totvert * step_tot;    /* -1 because we're joining back up */
    maxEdges = (totvert * step_tot) + /* these are the edges between new verts */
               (totedge * step_tot);  /* -1 because vert edges join */
    maxPolys = totedge * step_tot;

    screw_ofs = 0.0f;
  }
  else {
    close = 0;
    if (step_tot < 3) {
      step_tot = 3;
    }

    maxVerts = totvert * step_tot;          /* -1 because we're joining back up */
    maxEdges = (totvert * (step_tot - 1)) + /* these are the edges between new verts */
               (totedge * step_tot);        /* -1 because vert edges join */
    maxPolys = totedge * (step_tot - 1);
  }

  if ((ltmd->flag & MOD_SCREW_UV_STRETCH_U) == 0) {
    uv_u_scale = (uv_u_scale / (float)ltmd->iter) * (angle / ((float)M_PI * 2.0f));
  }

  result = BKE_mesh_new_nomain_from_template(
      mesh, (int)maxVerts, (int)maxEdges, 0, (int)maxPolys * 4, (int)maxPolys);

  /* copy verts from mesh */
  mvert_orig = mesh->mvert;
  medge_orig = mesh->medge;

  mvert_new = result->mvert;
  mpoly_new = result->mpoly;
  mloop_new = result->mloop;
  medge_new = result->medge;

  if (!CustomData_has_layer(&result->pdata, CD_ORIGINDEX)) {
    CustomData_add_layer(&result->pdata, CD_ORIGINDEX, CD_CALLOC, NULL, (int)maxPolys);
  }

  origindex = CustomData_get_layer(&result->pdata, CD_ORIGINDEX);

  CustomData_copy_data(&mesh->vdata, &result->vdata, 0, 0, (int)totvert);

  if (mloopuv_layers_tot) {
    float zero_co[3] = {0};
    plane_from_point_normal_v3(uv_axis_plane, zero_co, axis_vec);
  }

  if (mloopuv_layers_tot) {
    unsigned int uv_lay;
    for (uv_lay = 0; uv_lay < mloopuv_layers_tot; uv_lay++) {
      mloopuv_layers[uv_lay] = CustomData_get_layer_n(&result->ldata, CD_MLOOPUV, (int)uv_lay);
    }

    if (ltmd->flag & MOD_SCREW_UV_STRETCH_V) {
      for (i = 0, mv_orig = mvert_orig; i < totvert; i++, mv_orig++) {
        const float v = dist_signed_squared_to_plane_v3(mv_orig->co, uv_axis_plane);
        uv_v_minmax[0] = min_ff(v, uv_v_minmax[0]);
        uv_v_minmax[1] = max_ff(v, uv_v_minmax[1]);
      }
      uv_v_minmax[0] = sqrtf_signed(uv_v_minmax[0]);
      uv_v_minmax[1] = sqrtf_signed(uv_v_minmax[1]);
    }

    uv_v_range_inv = uv_v_minmax[1] - uv_v_minmax[0];
    uv_v_range_inv = uv_v_range_inv ? 1.0f / uv_v_range_inv : 0.0f;
  }

  /* Set the locations of the first set of verts */

  mv_new = mvert_new;
  mv_orig = mvert_orig;

  /* Copy the first set of edges */
  med_orig = medge_orig;
  med_new = medge_new;
  for (i = 0; i < totedge; i++, med_orig++, med_new++) {
    med_new->v1 = med_orig->v1;
    med_new->v2 = med_orig->v2;
    med_new->crease = med_orig->crease;
    med_new->flag = med_orig->flag & ~ME_LOOSEEDGE;
  }

  /* build polygon -> edge map */
  if (totpoly) {
    MPoly *mp_orig;

    mpoly_orig = mesh->mpoly;
    mloop_orig = mesh->mloop;
    edge_poly_map = MEM_malloc_arrayN(totedge, sizeof(*edge_poly_map), __func__);
    memset(edge_poly_map, 0xff, sizeof(*edge_poly_map) * totedge);

    vert_loop_map = MEM_malloc_arrayN(totvert, sizeof(*vert_loop_map), __func__);
    memset(vert_loop_map, 0xff, sizeof(*vert_loop_map) * totvert);

    for (i = 0, mp_orig = mpoly_orig; i < totpoly; i++, mp_orig++) {
      unsigned int loopstart = (unsigned int)mp_orig->loopstart;
      unsigned int loopend = loopstart + (unsigned int)mp_orig->totloop;

      MLoop *ml_orig = &mloop_orig[loopstart];
      unsigned int k;
      for (k = loopstart; k < loopend; k++, ml_orig++) {
        edge_poly_map[ml_orig->e] = i;
        vert_loop_map[ml_orig->v] = k;

        /* also order edges based on faces */
        if (medge_new[ml_orig->e].v1 != ml_orig->v) {
          SWAP(unsigned int, medge_new[ml_orig->e].v1, medge_new[ml_orig->e].v2);
        }
      }
    }
  }

  if (ltmd->flag & MOD_SCREW_NORMAL_CALC) {
    /*
     * Normal Calculation (for face flipping)
     * Sort edge verts for correct face flipping
     * NOT REALLY NEEDED but face flipping is nice.
     *
     * */

    /* Notice!
     *
     * Since we are only ordering the edges here it can avoid mallocing the
     * extra space by abusing the vert array before its filled with new verts.
     * The new array for vert_connect must be at least sizeof(ScrewVertConnect) * totvert
     * and the size of our resulting meshes array is sizeof(MVert) * totvert * 3
     * so its safe to use the second 2 thrids of MVert the array for vert_connect,
     * just make sure ScrewVertConnect struct is no more than twice as big as MVert,
     * at the moment there is no chance of that being a problem,
     * unless MVert becomes half its current size.
     *
     * once the edges are ordered, vert_connect is not needed and it can be used for verts
     *
     * This makes the modifier faster with one less alloc.
     */

    vert_connect = MEM_malloc_arrayN(totvert, sizeof(ScrewVertConnect), "ScrewVertConnect");
    /* skip the first slice of verts. */
    // vert_connect = (ScrewVertConnect *) &medge_new[totvert];
    vc = vert_connect;

    /* Copy Vert Locations */
    /* - We can do this in a later loop - only do here if no normal calc */
    if (!totedge) {
      for (i = 0; i < totvert; i++, mv_orig++, mv_new++) {
        copy_v3_v3(mv_new->co, mv_orig->co);
        normalize_v3_v3(vc->no, mv_new->co); /* no edges- this is really a dummy normal */
      }
    }
    else {
      /*printf("\n\n\n\n\nStarting Modifier\n");*/
      /* set edge users */
      med_new = medge_new;
      mv_new = mvert_new;

      if (ob_axis != NULL) {
        /*mtx_tx is initialized early on */
        for (i = 0; i < totvert; i++, mv_new++, mv_orig++, vc++) {
          vc->co[0] = mv_new->co[0] = mv_orig->co[0];
          vc->co[1] = mv_new->co[1] = mv_orig->co[1];
          vc->co[2] = mv_new->co[2] = mv_orig->co[2];

          vc->flag = 0;
          vc->e[0] = vc->e[1] = NULL;
          vc->v[0] = vc->v[1] = SV_UNUSED;

          mul_m4_v3(mtx_tx, vc->co);
          /* length in 2d, don't sqrt because this is only for comparison */
          vc->dist = vc->co[other_axis_1] * vc->co[other_axis_1] +
                     vc->co[other_axis_2] * vc->co[other_axis_2];

          /* printf("location %f %f %f -- %f\n", vc->co[0], vc->co[1], vc->co[2], vc->dist);*/
        }
      }
      else {
        for (i = 0; i < totvert; i++, mv_new++, mv_orig++, vc++) {
          vc->co[0] = mv_new->co[0] = mv_orig->co[0];
          vc->co[1] = mv_new->co[1] = mv_orig->co[1];
          vc->co[2] = mv_new->co[2] = mv_orig->co[2];

          vc->flag = 0;
          vc->e[0] = vc->e[1] = NULL;
          vc->v[0] = vc->v[1] = SV_UNUSED;

          /* length in 2d, don't sqrt because this is only for comparison */
          vc->dist = vc->co[other_axis_1] * vc->co[other_axis_1] +
                     vc->co[other_axis_2] * vc->co[other_axis_2];

          /* printf("location %f %f %f -- %f\n", vc->co[0], vc->co[1], vc->co[2], vc->dist);*/
        }
      }

      /* this loop builds connectivity info for verts */
      for (i = 0; i < totedge; i++, med_new++) {
        vc = &vert_connect[med_new->v1];

        if (vc->v[0] == SV_UNUSED) { /* unused */
          vc->v[0] = med_new->v2;
          vc->e[0] = med_new;
        }
        else if (vc->v[1] == SV_UNUSED) {
          vc->v[1] = med_new->v2;
          vc->e[1] = med_new;
        }
        else {
          vc->v[0] = vc->v[1] = SV_INVALID; /* error value  - don't use, 3 edges on vert */
        }

        vc = &vert_connect[med_new->v2];

        /* same as above but swap v1/2 */
        if (vc->v[0] == SV_UNUSED) { /* unused */
          vc->v[0] = med_new->v1;
          vc->e[0] = med_new;
        }
        else if (vc->v[1] == SV_UNUSED) {
          vc->v[1] = med_new->v1;
          vc->e[1] = med_new;
        }
        else {
          vc->v[0] = vc->v[1] = SV_INVALID; /* error value  - don't use, 3 edges on vert */
        }
      }

      /* find the first vert */
      vc = vert_connect;
      for (i = 0; i < totvert; i++, vc++) {
        /* Now do search for connected verts, order all edges and flip them
         * so resulting faces are flipped the right way */
        vc_tot_linked = 0; /* count the number of linked verts for this loop */
        if (vc->flag == 0) {
          unsigned int v_best = SV_UNUSED, ed_loop_closed = 0; /* vert and vert new */
          ScrewVertIter lt_iter;
          float fl = -1.0f;

          /* compiler complains if not initialized, but it should be initialized below */
          bool ed_loop_flip = false;

          /*printf("Loop on connected vert: %i\n", i);*/

          for (j = 0; j < 2; j++) {
            /*printf("\tSide: %i\n", j);*/
            screwvert_iter_init(&lt_iter, vert_connect, i, j);
            if (j == 1) {
              screwvert_iter_step(&lt_iter);
            }
            while (lt_iter.v_poin) {
              /*printf("\t\tVERT: %i\n", lt_iter.v);*/
              if (lt_iter.v_poin->flag) {
                /*printf("\t\t\tBreaking Found end\n");*/
                // endpoints[0] = endpoints[1] = SV_UNUSED;
                ed_loop_closed = 1; /* circle */
                break;
              }
              lt_iter.v_poin->flag = 1;
              vc_tot_linked++;
              /*printf("Testing 2 floats %f : %f\n", fl, lt_iter.v_poin->dist);*/
              if (fl <= lt_iter.v_poin->dist) {
                fl = lt_iter.v_poin->dist;
                v_best = lt_iter.v;
                /*printf("\t\t\tVERT BEST: %i\n", v_best);*/
              }
              screwvert_iter_step(&lt_iter);
              if (!lt_iter.v_poin) {
                /*printf("\t\t\tFound End Also Num %i\n", j);*/
                /*endpoints[j] = lt_iter.v_other;*/ /* other is still valid */
                break;
              }
            }
          }

          /* now we have a collection of used edges. flip their edges the right way*/
          /*if (v_best != SV_UNUSED) - */

          /*printf("Done Looking - vc_tot_linked: %i\n", vc_tot_linked);*/

          if (vc_tot_linked > 1) {
            float vf_1, vf_2, vf_best;

            vc_tmp = &vert_connect[v_best];

            tmpf1 = vert_connect[vc_tmp->v[0]].co;
            tmpf2 = vert_connect[vc_tmp->v[1]].co;

            /* edge connects on each side! */
            if (SV_IS_VALID(vc_tmp->v[0]) && SV_IS_VALID(vc_tmp->v[1])) {
              /*printf("Verts on each side (%i %i)\n", vc_tmp->v[0], vc_tmp->v[1]);*/
              /* find out which is higher */

              vf_1 = tmpf1[ltmd->axis];
              vf_2 = tmpf2[ltmd->axis];
              vf_best = vc_tmp->co[ltmd->axis];

              if (vf_1 < vf_best && vf_best < vf_2) {
                ed_loop_flip = 0;
              }
              else if (vf_1 > vf_best && vf_best > vf_2) {
                ed_loop_flip = 1;
              }
              else {
                /* not so simple to work out which edge is higher */
                sub_v3_v3v3(tmp_vec1, tmpf1, vc_tmp->co);
                sub_v3_v3v3(tmp_vec2, tmpf2, vc_tmp->co);
                normalize_v3(tmp_vec1);
                normalize_v3(tmp_vec2);

                if (tmp_vec1[ltmd->axis] < tmp_vec2[ltmd->axis]) {
                  ed_loop_flip = 1;
                }
                else {
                  ed_loop_flip = 0;
                }
              }
            }
            else if (SV_IS_VALID(vc_tmp->v[0])) { /*vertex only connected on 1 side */
              /*printf("Verts on ONE side (%i %i)\n", vc_tmp->v[0], vc_tmp->v[1]);*/
              if (tmpf1[ltmd->axis] < vc_tmp->co[ltmd->axis]) { /* best is above */
                ed_loop_flip = 1;
              }
              else { /* best is below or even... in even case we can't know what to do. */
                ed_loop_flip = 0;
              }
            }
#if 0
            else {
              printf("No Connected ___\n");
            }
#endif

            /*printf("flip direction %i\n", ed_loop_flip);*/

            /* switch the flip option if set
             * note: flip is now done at face level so copying vgroup slizes is easier */
#if 0
            if (do_flip) {
              ed_loop_flip = !ed_loop_flip;
            }
#endif

            if (angle < 0.0f) {
              ed_loop_flip = !ed_loop_flip;
            }

            /* if its closed, we only need 1 loop */
            for (j = ed_loop_closed; j < 2; j++) {
              /*printf("Ordering Side J %i\n", j);*/

              screwvert_iter_init(&lt_iter, vert_connect, v_best, j);
              /*printf("\n\nStarting - Loop\n");*/
              lt_iter.v_poin->flag = 1; /* so a non loop will traverse the other side */

              /* If this is the vert off the best vert and
               * the best vert has 2 edges connected too it
               * then swap the flip direction */
              if (j == 1 && SV_IS_VALID(vc_tmp->v[0]) && SV_IS_VALID(vc_tmp->v[1])) {
                ed_loop_flip = !ed_loop_flip;
              }

              while (lt_iter.v_poin && lt_iter.v_poin->flag != 2) {
                /*printf("\tOrdering Vert V %i\n", lt_iter.v);*/

                lt_iter.v_poin->flag = 2;
                if (lt_iter.e) {
                  if (lt_iter.v == lt_iter.e->v1) {
                    if (ed_loop_flip == 0) {
                      /*printf("\t\t\tFlipping 0\n");*/
                      SWAP(unsigned int, lt_iter.e->v1, lt_iter.e->v2);
                    }
#if 0
                    else {
                      printf("\t\t\tFlipping Not 0\n");
                    }
#endif
                  }
                  else if (lt_iter.v == lt_iter.e->v2) {
                    if (ed_loop_flip == 1) {
                      /*printf("\t\t\tFlipping 1\n");*/
                      SWAP(unsigned int, lt_iter.e->v1, lt_iter.e->v2);
                    }
#if 0
                    else {
                      printf("\t\t\tFlipping Not 1\n");
                    }
#endif
                  }
#if 0
                  else {
                    printf("\t\tIncorrect edge topology");
                  }
#endif
                }
#if 0
                else {
                  printf("\t\tNo Edge at this point\n");
                }
#endif
                screwvert_iter_step(&lt_iter);
              }
            }
          }
        }

        /* *VERTEX NORMALS*
         * we know the surrounding edges are ordered correctly now
         * so its safe to create vertex normals.
         *
         * calculate vertex normals that can be propagated on lathing
         * use edge connectivity work this out */
        if (SV_IS_VALID(vc->v[0])) {
          if (SV_IS_VALID(vc->v[1])) {
            /* 2 edges connedted */
            /* make 2 connecting vert locations relative to the middle vert */
            sub_v3_v3v3(tmp_vec1, mvert_new[vc->v[0]].co, mvert_new[i].co);
            sub_v3_v3v3(tmp_vec2, mvert_new[vc->v[1]].co, mvert_new[i].co);
            /* normalize so both edges have the same influence, no matter their length */
            normalize_v3(tmp_vec1);
            normalize_v3(tmp_vec2);

            /* vc_no_tmp1 - this line is the average direction of both connecting edges
             *
             * Use the edge order to make the subtraction, flip the normal the right way
             * edge should be there but check just in case... */
            if (vc->e[0]->v1 == i) {
              sub_v3_v3(tmp_vec1, tmp_vec2);
            }
            else {
              sub_v3_v3v3(tmp_vec1, tmp_vec2, tmp_vec1);
            }
          }
          else {
            /* only 1 edge connected - same as above except
             * don't need to average edge direction */
            if (vc->e[0]->v2 == i) {
              sub_v3_v3v3(tmp_vec1, mvert_new[i].co, mvert_new[vc->v[0]].co);
            }
            else {
              sub_v3_v3v3(tmp_vec1, mvert_new[vc->v[0]].co, mvert_new[i].co);
            }
          }

          /* tmp_vec2 - is a line 90d from the pivot to the vec
           * This is used so the resulting normal points directly away from the middle */
          cross_v3_v3v3(tmp_vec2, axis_vec, vc->co);

          if (UNLIKELY(is_zero_v3(tmp_vec2))) {
            /* we're _on_ the axis, so copy it based on our winding */
            if (vc->e[0]->v2 == i) {
              negate_v3_v3(vc->no, axis_vec);
            }
            else {
              copy_v3_v3(vc->no, axis_vec);
            }
          }
          else {
            /* edge average vector and right angle to the pivot make the normal */
            cross_v3_v3v3(vc->no, tmp_vec1, tmp_vec2);
          }
        }
        else {
          copy_v3_v3(vc->no, vc->co);
        }

        /* we won't be looping on this data again so copy normals here */
        if ((angle < 0.0f) != do_flip) {
          negate_v3(vc->no);
        }

        normalize_v3(vc->no);
        normal_float_to_short_v3(mvert_new[i].no, vc->no);

        /* Done with normals */
      }
    }
  }
  else {
    mv_orig = mvert_orig;
    mv_new = mvert_new;

    for (i = 0; i < totvert; i++, mv_new++, mv_orig++) {
      copy_v3_v3(mv_new->co, mv_orig->co);
    }
  }
  /* done with edge connectivity based normal flipping */

  /* Add Faces */
  for (step = 1; step < step_tot; step++) {
    const unsigned int varray_stride = totvert * step;
    float step_angle;
    float nor_tx[3];
    float mat[4][4];
    /* Rotation Matrix */
    step_angle = (angle / (float)(step_tot - (!close))) * (float)step;

    if (ob_axis != NULL) {
      axis_angle_normalized_to_mat3(mat3, axis_vec, step_angle);
    }
    else {
      axis_angle_to_mat3_single(mat3, axis_char, step_angle);
    }
    copy_m4_m3(mat, mat3);

    if (screw_ofs) {
      madd_v3_v3fl(mat[3], axis_vec, screw_ofs * ((float)step / (float)(step_tot - 1)));
    }

    /* copy a slice */
    CustomData_copy_data(&mesh->vdata, &result->vdata, 0, (int)varray_stride, (int)totvert);

    mv_new_base = mvert_new;
    mv_new = &mvert_new[varray_stride]; /* advance to the next slice */

    for (j = 0; j < totvert; j++, mv_new_base++, mv_new++) {
      /* set normal */
      if (vert_connect) {
        mul_v3_m3v3(nor_tx, mat3, vert_connect[j].no);

        /* set the normal now its transformed */
        normal_float_to_short_v3(mv_new->no, nor_tx);
      }

      /* set location */
      copy_v3_v3(mv_new->co, mv_new_base->co);

      /* only need to set these if using non cleared memory */
      /*mv_new->mat_nr = mv_new->flag = 0;*/

      if (ob_axis != NULL) {
        sub_v3_v3(mv_new->co, mtx_tx[3]);

        mul_m4_v3(mat, mv_new->co);

        add_v3_v3(mv_new->co, mtx_tx[3]);
      }
      else {
        mul_m4_v3(mat, mv_new->co);
      }

      /* add the new edge */
      med_new->v1 = varray_stride + j;
      med_new->v2 = med_new->v1 - totvert;
      med_new->flag = ME_EDGEDRAW | ME_EDGERENDER;
      med_new++;
    }
  }

  /* we can avoid if using vert alloc trick */
  if (vert_connect) {
    MEM_freeN(vert_connect);
    vert_connect = NULL;
  }

  if (close) {
    /* last loop of edges, previous loop doesn't account for the last set of edges */
    const unsigned int varray_stride = (step_tot - 1) * totvert;

    for (i = 0; i < totvert; i++) {
      med_new->v1 = i;
      med_new->v2 = varray_stride + i;
      med_new->flag = ME_EDGEDRAW | ME_EDGERENDER;
      med_new++;
    }
  }

  mp_new = mpoly_new;
  ml_new = mloop_new;
  med_new_firstloop = medge_new;

  /* more of an offset in this case */
  edge_offset = totedge + (totvert * (step_tot - (close ? 0 : 1)));

  for (i = 0; i < totedge; i++, med_new_firstloop++) {
    const unsigned int step_last = step_tot - (close ? 1 : 2);
    const unsigned int mpoly_index_orig = totpoly ? edge_poly_map[i] : UINT_MAX;
    const bool has_mpoly_orig = (mpoly_index_orig != UINT_MAX);
    float uv_v_offset_a, uv_v_offset_b;

    const unsigned int mloop_index_orig[2] = {
        vert_loop_map ? vert_loop_map[medge_new[i].v1] : UINT_MAX,
        vert_loop_map ? vert_loop_map[medge_new[i].v2] : UINT_MAX,
    };
    const bool has_mloop_orig = mloop_index_orig[0] != UINT_MAX;

    short mat_nr;

    /* for each edge, make a cylinder of quads */
    i1 = med_new_firstloop->v1;
    i2 = med_new_firstloop->v2;

    if (has_mpoly_orig) {
      mat_nr = mpoly_orig[mpoly_index_orig].mat_nr;
    }
    else {
      mat_nr = 0;
    }

    if (has_mloop_orig == false && mloopuv_layers_tot) {
      uv_v_offset_a = dist_signed_to_plane_v3(mvert_new[medge_new[i].v1].co, uv_axis_plane);
      uv_v_offset_b = dist_signed_to_plane_v3(mvert_new[medge_new[i].v2].co, uv_axis_plane);

      if (ltmd->flag & MOD_SCREW_UV_STRETCH_V) {
        uv_v_offset_a = (uv_v_offset_a - uv_v_minmax[0]) * uv_v_range_inv;
        uv_v_offset_b = (uv_v_offset_b - uv_v_minmax[0]) * uv_v_range_inv;
      }
    }

    for (step = 0; step <= step_last; step++) {

      /* Polygon */
      if (has_mpoly_orig) {
        CustomData_copy_data(
            &mesh->pdata, &result->pdata, (int)mpoly_index_orig, (int)mpoly_index, 1);
        origindex[mpoly_index] = (int)mpoly_index_orig;
      }
      else {
        origindex[mpoly_index] = ORIGINDEX_NONE;
        mp_new->flag = mpoly_flag;
        mp_new->mat_nr = mat_nr;
      }
      mp_new->loopstart = mpoly_index * 4;
      mp_new->totloop = 4;

      /* Loop-Custom-Data */
      if (has_mloop_orig) {
        int l_index = (int)(ml_new - mloop_new);

        CustomData_copy_data(
            &mesh->ldata, &result->ldata, (int)mloop_index_orig[0], l_index + 0, 1);
        CustomData_copy_data(
            &mesh->ldata, &result->ldata, (int)mloop_index_orig[1], l_index + 1, 1);
        CustomData_copy_data(
            &mesh->ldata, &result->ldata, (int)mloop_index_orig[1], l_index + 2, 1);
        CustomData_copy_data(
            &mesh->ldata, &result->ldata, (int)mloop_index_orig[0], l_index + 3, 1);

        if (mloopuv_layers_tot) {
          unsigned int uv_lay;
          const float uv_u_offset_a = (float)(step)*uv_u_scale;
          const float uv_u_offset_b = (float)(step + 1) * uv_u_scale;
          for (uv_lay = 0; uv_lay < mloopuv_layers_tot; uv_lay++) {
            MLoopUV *mluv = &mloopuv_layers[uv_lay][l_index];

            mluv[quad_ord[0]].uv[0] += uv_u_offset_a;
            mluv[quad_ord[1]].uv[0] += uv_u_offset_a;
            mluv[quad_ord[2]].uv[0] += uv_u_offset_b;
            mluv[quad_ord[3]].uv[0] += uv_u_offset_b;
          }
        }
      }
      else {
        if (mloopuv_layers_tot) {
          int l_index = (int)(ml_new - mloop_new);

          unsigned int uv_lay;
          const float uv_u_offset_a = (float)(step)*uv_u_scale;
          const float uv_u_offset_b = (float)(step + 1) * uv_u_scale;
          for (uv_lay = 0; uv_lay < mloopuv_layers_tot; uv_lay++) {
            MLoopUV *mluv = &mloopuv_layers[uv_lay][l_index];

            copy_v2_fl2(mluv[quad_ord[0]].uv, uv_u_offset_a, uv_v_offset_a);
            copy_v2_fl2(mluv[quad_ord[1]].uv, uv_u_offset_a, uv_v_offset_b);
            copy_v2_fl2(mluv[quad_ord[2]].uv, uv_u_offset_b, uv_v_offset_b);
            copy_v2_fl2(mluv[quad_ord[3]].uv, uv_u_offset_b, uv_v_offset_a);
          }
        }
      }

      /* Loop-Data */
      if (!(close && step == step_last)) {
        /* regular segments */
        ml_new[quad_ord[0]].v = i1;
        ml_new[quad_ord[1]].v = i2;
        ml_new[quad_ord[2]].v = i2 + totvert;
        ml_new[quad_ord[3]].v = i1 + totvert;

        ml_new[quad_ord_ofs[0]].e = step == 0 ? i :
                                                (edge_offset + step + (i * (step_tot - 1))) - 1;
        ml_new[quad_ord_ofs[1]].e = totedge + i2;
        ml_new[quad_ord_ofs[2]].e = edge_offset + step + (i * (step_tot - 1));
        ml_new[quad_ord_ofs[3]].e = totedge + i1;

        /* new vertical edge */
        if (step) { /* The first set is already done */
          med_new->v1 = i1;
          med_new->v2 = i2;
          med_new->flag = med_new_firstloop->flag;
          med_new->crease = med_new_firstloop->crease;
          med_new++;
        }
        i1 += totvert;
        i2 += totvert;
      }
      else {
        /* last segment */
        ml_new[quad_ord[0]].v = i1;
        ml_new[quad_ord[1]].v = i2;
        ml_new[quad_ord[2]].v = med_new_firstloop->v2;
        ml_new[quad_ord[3]].v = med_new_firstloop->v1;

        ml_new[quad_ord_ofs[0]].e = (edge_offset + step + (i * (step_tot - 1))) - 1;
        ml_new[quad_ord_ofs[1]].e = totedge + i2;
        ml_new[quad_ord_ofs[2]].e = i;
        ml_new[quad_ord_ofs[3]].e = totedge + i1;
      }

      mp_new++;
      ml_new += 4;
      mpoly_index++;
    }

    /* new vertical edge */
    med_new->v1 = i1;
    med_new->v2 = i2;
    med_new->flag = med_new_firstloop->flag & ~ME_LOOSEEDGE;
    med_new->crease = med_new_firstloop->crease;
    med_new++;
  }

  /* validate loop edges */
#if 0
  {
    unsigned i = 0;
    printf("\n");
    for (; i < maxPolys * 4; i += 4) {
      unsigned int ii;
      ml_new = mloop_new + i;
      ii = findEd(medge_new, maxEdges, ml_new[0].v, ml_new[1].v);
      printf("%d %d -- ", ii, ml_new[0].e);
      ml_new[0].e = ii;

      ii = findEd(medge_new, maxEdges, ml_new[1].v, ml_new[2].v);
      printf("%d %d -- ", ii, ml_new[1].e);
      ml_new[1].e = ii;

      ii = findEd(medge_new, maxEdges, ml_new[2].v, ml_new[3].v);
      printf("%d %d -- ", ii, ml_new[2].e);
      ml_new[2].e = ii;

      ii = findEd(medge_new, maxEdges, ml_new[3].v, ml_new[0].v);
      printf("%d %d\n", ii, ml_new[3].e);
      ml_new[3].e = ii;
    }
  }
#endif

  if (edge_poly_map) {
    MEM_freeN(edge_poly_map);
  }

  if (vert_loop_map) {
    MEM_freeN(vert_loop_map);
  }

  if ((ltmd->flag & MOD_SCREW_MERGE) && (screw_ofs == 0.0f)) {
    Mesh *result_prev = result;
    result = mesh_remove_doubles_on_axis(result,
                                         mvert_new,
                                         totvert,
                                         step_tot,
                                         axis_vec,
                                         ob_axis != NULL ? mtx_tx[3] : NULL,
                                         ltmd->merge_dist);
    if (result != result_prev) {
      result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
    }
  }

  if ((ltmd->flag & MOD_SCREW_NORMAL_CALC) == 0) {
    result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
  }

  return result;
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  ScrewModifierData *ltmd = (ScrewModifierData *)md;
  if (ltmd->ob_axis != NULL) {
    DEG_add_object_relation(ctx->node, ltmd->ob_axis, DEG_OB_COMP_TRANSFORM, "Screw Modifier");
    DEG_add_modifier_to_transform_relation(ctx->node, "Screw Modifier");
  }
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  ScrewModifierData *ltmd = (ScrewModifierData *)md;

  walk(userData, ob, &ltmd->ob_axis, IDWALK_CB_NOP);
}

ModifierTypeInfo modifierType_Screw = {
    /* name */ "Screw",
    /* structName */ "ScrewModifierData",
    /* structSize */ sizeof(ScrewModifierData),
    /* type */ eModifierTypeType_Constructive,

    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode,

    /* copyData */ modifier_copyData_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* applyModifier */ applyModifier,

    /* initData */ initData,
    /* requiredDataMask */ NULL,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
};
