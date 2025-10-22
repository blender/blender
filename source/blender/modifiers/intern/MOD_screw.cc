/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

/* Screw modifier: revolves the edges about an axis */
#include <algorithm>
#include <climits>

#include "BLI_utildefines.h"

#include "BLI_bitmap.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_span.hh"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_mesh.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "DEG_depsgraph_build.hh"

#include "MEM_guardedalloc.h"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#include "GEO_mesh_merge_by_distance.hh"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

using namespace blender;

static void init_data(ModifierData *md)
{
  ScrewModifierData *ltmd = (ScrewModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(ltmd, modifier));

  MEMCPY_STRUCT_AFTER(ltmd, DNA_struct_default_get(ScrewModifierData), modifier);
}

/** Used for gathering edge connectivity. */
struct ScrewVertConnect {
  /** Distance from the center axis. */
  float dist_sq;
  /** Location relative to the transformed axis. */
  float co[3];
  /** 2 verts on either side of this one. */
  uint v[2];
  /** Edges on either side, a bit of a waste since each edge ref's 2 edges. */
  blender::int2 *e[2];
  char flag;
};

struct ScrewVertIter {
  ScrewVertConnect *v_array;
  ScrewVertConnect *v_poin;
  uint v, v_other;
  blender::int2 *e;
};

#define SV_UNUSED (UINT_MAX)
#define SV_INVALID ((UINT_MAX) - 1)
#define SV_IS_VALID(v) ((v) < SV_INVALID)

static void screwvert_iter_init(ScrewVertIter *iter,
                                ScrewVertConnect *array,
                                uint v_init,
                                uint dir)
{
  iter->v_array = array;
  iter->v = v_init;

  if (SV_IS_VALID(v_init)) {
    iter->v_poin = &array[v_init];
    iter->v_other = iter->v_poin->v[dir];
    iter->e = iter->v_poin->e[!dir];
  }
  else {
    iter->v_poin = nullptr;
    iter->e = nullptr;
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
    iter->e = nullptr;
    iter->v_poin = nullptr;
  }
}

static Mesh *mesh_remove_doubles_on_axis(Mesh *result,
                                         blender::MutableSpan<blender::float3> vert_positions_new,
                                         const uint totvert,
                                         const uint step_tot,
                                         const float axis_vec[3],
                                         const float axis_offset[3],
                                         const float merge_threshold)
{
  BLI_bitmap *vert_tag = BLI_BITMAP_NEW(totvert, __func__);

  const float merge_threshold_sq = square_f(merge_threshold);
  const bool use_offset = axis_offset != nullptr;
  uint tot_doubles = 0;
  for (uint i = 0; i < totvert; i += 1) {
    float axis_co[3];
    if (use_offset) {
      float offset_co[3];
      sub_v3_v3v3(offset_co, vert_positions_new[i], axis_offset);
      project_v3_v3v3_normalized(axis_co, offset_co, axis_vec);
      add_v3_v3(axis_co, axis_offset);
    }
    else {
      project_v3_v3v3_normalized(axis_co, vert_positions_new[i], axis_vec);
    }
    const float dist_sq = len_squared_v3v3(axis_co, vert_positions_new[i]);
    if (dist_sq <= merge_threshold_sq) {
      BLI_BITMAP_ENABLE(vert_tag, i);
      tot_doubles += 1;
      copy_v3_v3(vert_positions_new[i], axis_co);
    }
  }

  if (tot_doubles != 0) {
    uint tot = totvert * step_tot;
    int *full_doubles_map = MEM_malloc_arrayN<int>(tot, __func__);
    copy_vn_i(full_doubles_map, int(tot), -1);

    uint tot_doubles_left = tot_doubles;
    for (uint i = 0; i < totvert; i += 1) {
      if (BLI_BITMAP_TEST(vert_tag, i)) {
        int *doubles_map = &full_doubles_map[totvert + i];
        for (uint step = 1; step < step_tot; step += 1) {
          *doubles_map = int(i);
          doubles_map += totvert;
        }
        tot_doubles_left -= 1;
        if (tot_doubles_left == 0) {
          break;
        }
      }
    }

    Mesh *tmp = result;

    /* TODO(mano-wii): Polygons with all vertices merged are the ones that form duplicates.
     * Therefore the duplicate face test can be skipped. */
    result = geometry::mesh_merge_verts(*tmp,
                                        MutableSpan<int>{full_doubles_map, result->verts_num},
                                        int(tot_doubles * (step_tot - 1)),
                                        false);

    BKE_id_free(nullptr, tmp);
    MEM_freeN(full_doubles_map);
  }

  MEM_freeN(vert_tag);

  return result;
}

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *meshData)
{
  using namespace blender;
  const Mesh *mesh = meshData;
  Mesh *result;
  ScrewModifierData *ltmd = (ScrewModifierData *)md;
  const bool use_render_params = (ctx->flag & MOD_APPLY_RENDER) != 0;

  int face_index = 0;
  uint step;
  uint j;
  uint i1, i2;
  uint step_tot = use_render_params ? ltmd->render_steps : ltmd->steps;
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

  uint maxVerts = 0, maxEdges = 0, maxPolys = 0;
  const uint totvert = uint(mesh->verts_num);
  const uint totedge = uint(mesh->edges_num);
  const uint faces_num = uint(mesh->faces_num);

  uint *edge_face_map = nullptr; /* orig edge to orig face */
  uint *vert_loop_map = nullptr; /* orig vert to orig loop */

  /* UV Coords */
  const VectorSet<StringRefNull> uv_map_names = mesh->uv_map_names();
  blender::Array<bke::SpanAttributeWriter<float2>> uv_map_layers(uv_map_names.size());
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

  uint vc_tot_linked = 0;
  short other_axis_1, other_axis_2;
  const float *tmpf1, *tmpf2;

  uint edge_offset;

  blender::int2 *edge_new, *med_new_firstloop;
  Object *ob_axis = ltmd->ob_axis;

  ScrewVertConnect *vc, *vc_tmp, *vert_connect = nullptr;

  const bool use_flat_shading = (ltmd->flag & MOD_SCREW_SMOOTH_SHADING) == 0;

  /* don't do anything? */
  if (!totvert) {
    return BKE_mesh_new_nomain_from_template(mesh, 0, 0, 0, 0);
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

  if (ob_axis != nullptr) {
    /* Calculate the matrix relative to the axis object. */
    invert_m4_m4(mtx_tmp_a, ctx->object->object_to_world().ptr());
    copy_m4_m4(mtx_tx_inv, ob_axis->object_to_world().ptr());
    mul_m4_m4m4(mtx_tx, mtx_tmp_a, mtx_tx_inv);

    /* Calculate the axis vector. */
    mul_mat3_m4_v3(mtx_tx, axis_vec); /* only rotation component */
    normalize_v3(axis_vec);

    /* screw */
    if (ltmd->flag & MOD_SCREW_OBJECT_OFFSET) {
      /* Find the offset along this axis relative to this objects matrix. */
      float totlen = len_v3(mtx_tx[3]);

      if (totlen != 0.0f) {
        const float zero[3] = {0.0f, 0.0f, 0.0f};
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
    axis_char = char(axis_char + ltmd->axis); /* 'X' + axis */

    /* Useful to be able to use the axis vector in some cases still. */
    zero_v3(axis_vec);
    axis_vec[ltmd->axis] = 1.0f;
  }

  /* apply the multiplier */
  angle *= float(ltmd->iter);
  screw_ofs *= float(ltmd->iter);
  uv_u_scale = 1.0f / float(step_tot);

  /* multiplying the steps is a bit tricky, this works best */
  step_tot = ((step_tot + 1) * ltmd->iter) - (ltmd->iter - 1);

  /* Will the screw be closed?
   * NOTE: smaller than `FLT_EPSILON * 100`
   * gives problems with float precision so its never closed. */
  if (fabsf(screw_ofs) <= (FLT_EPSILON * 100.0f) &&
      fabsf(fabsf(angle) - (float(M_PI) * 2.0f)) <= (FLT_EPSILON * 100.0f) && step_tot > 3)
  {
    close = true;
    step_tot--;

    maxVerts = totvert * step_tot;    /* -1 because we're joining back up */
    maxEdges = (totvert * step_tot) + /* these are the edges between new verts */
               (totedge * step_tot);  /* -1 because vert edges join */
    maxPolys = totedge * step_tot;

    screw_ofs = 0.0f;
  }
  else {
    close = false;
    step_tot = std::max<uint>(step_tot, 2);

    maxVerts = totvert * step_tot;          /* -1 because we're joining back up */
    maxEdges = (totvert * (step_tot - 1)) + /* these are the edges between new verts */
               (totedge * step_tot);        /* -1 because vert edges join */
    maxPolys = totedge * (step_tot - 1);
  }

  if ((ltmd->flag & MOD_SCREW_UV_STRETCH_U) == 0) {
    uv_u_scale = (uv_u_scale / float(ltmd->iter)) * (angle / (float(M_PI) * 2.0f));
  }

  /* The `screw_ofs` cannot change from now on. */
  const bool do_remove_doubles = (ltmd->flag & MOD_SCREW_MERGE) && (screw_ofs == 0.0f);

  result = BKE_mesh_new_nomain_from_template(
      mesh, int(maxVerts), int(maxEdges), int(maxPolys), int(maxPolys) * 4);
  /* The modifier doesn't support original index mapping on the edge or face domains. Remove
   * original index layers, since otherwise edges aren't displayed at all in wireframe view. */
  CustomData_free_layers(&result->edge_data, CD_ORIGINDEX);
  CustomData_free_layers(&result->face_data, CD_ORIGINDEX);

  const blender::Span<float3> vert_positions_orig = mesh->vert_positions();
  const blender::Span<int2> edges_orig = mesh->edges();
  const OffsetIndices faces_orig = mesh->faces();
  const blender::Span<int> corner_verts_orig = mesh->corner_verts();
  const blender::Span<int> corner_edges_orig = mesh->corner_edges();

  blender::MutableSpan<float3> vert_positions_new = result->vert_positions_for_write();
  blender::MutableSpan<int2> edges_new = result->edges_for_write();
  MutableSpan<int> face_offests_new = result->face_offsets_for_write();
  blender::MutableSpan<int> corner_verts_new = result->corner_verts_for_write();
  blender::MutableSpan<int> corner_edges_new = result->corner_edges_for_write();
  bke::MutableAttributeAccessor attributes = result->attributes_for_write();
  bke::SpanAttributeWriter<bool> sharp_faces = attributes.lookup_or_add_for_write_span<bool>(
      "sharp_face", bke::AttrDomain::Face);

  if (!CustomData_has_layer(&result->face_data, CD_ORIGINDEX)) {
    CustomData_add_layer(&result->face_data, CD_ORIGINDEX, CD_SET_DEFAULT, int(maxPolys));
  }

  int *origindex = static_cast<int *>(
      CustomData_get_layer_for_write(&result->face_data, CD_ORIGINDEX, result->faces_num));

  CustomData_copy_data(&mesh->vert_data, &result->vert_data, 0, 0, int(totvert));

  if (!uv_map_names.is_empty()) {
    const float zero_co[3] = {0};
    plane_from_point_normal_v3(uv_axis_plane, zero_co, axis_vec);
  }

  if (!uv_map_names.is_empty()) {
    for (const int64_t uv_lay : uv_map_names.index_range()) {
      uv_map_layers[uv_lay] = attributes.lookup_for_write_span<float2>(uv_map_names[uv_lay]);
    }

    if (ltmd->flag & MOD_SCREW_UV_STRETCH_V) {
      for (uint i = 0; i < totvert; i++) {
        const float v = dist_signed_squared_to_plane_v3(vert_positions_orig[i], uv_axis_plane);
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

  /* Copy the first set of edges */
  const blender::int2 *edge_orig = edges_orig.data();
  edge_new = edges_new.data();
  for (uint i = 0; i < totedge; i++, edge_orig++, edge_new++) {
    *edge_new = *edge_orig;
  }

  /* build face -> edge map */
  if (faces_num) {

    edge_face_map = MEM_malloc_arrayN<uint>(totedge, __func__);
    memset(edge_face_map, 0xff, sizeof(*edge_face_map) * totedge);

    vert_loop_map = MEM_malloc_arrayN<uint>(totvert, __func__);
    memset(vert_loop_map, 0xff, sizeof(*vert_loop_map) * totvert);

    for (const int64_t i : faces_orig.index_range()) {
      for (const int64_t corner : faces_orig[i]) {
        const int vert_i = corner_verts_orig[corner];
        const int edge_i = corner_edges_orig[corner];
        edge_face_map[edge_i] = uint(i);
        vert_loop_map[vert_i] = uint(corner);

        /* also order edges based on faces */
        if (edges_new[edge_i][0] != vert_i) {
          std::swap(edges_new[edge_i][0], edges_new[edge_i][1]);
        }
      }
    }
  }

  if (ltmd->flag & MOD_SCREW_NORMAL_CALC) {

    /* Normal Calculation (for face flipping)
     * Sort edge verts for correct face flipping
     * NOT REALLY NEEDED but face flipping is nice. */

    vert_connect = MEM_malloc_arrayN<ScrewVertConnect>(totvert, __func__);
    /* skip the first slice of verts. */
    // vert_connect = (ScrewVertConnect *) &medge_new[totvert];
    vc = vert_connect;

    /* Copy Vert Locations */
    if (totedge != 0) {
      // printf("\n\n\n\n\nStarting Modifier\n");
      /* set edge users */
      edge_new = edges_new.data();

      if (ob_axis != nullptr) {
        /* `mtx_tx` is initialized early on. */
        for (uint i = 0; i < totvert; i++, vc++) {
          vc->co[0] = vert_positions_new[i][0] = vert_positions_orig[i][0];
          vc->co[1] = vert_positions_new[i][1] = vert_positions_orig[i][1];
          vc->co[2] = vert_positions_new[i][2] = vert_positions_orig[i][2];

          vc->flag = 0;
          vc->e[0] = vc->e[1] = nullptr;
          vc->v[0] = vc->v[1] = SV_UNUSED;

          mul_m4_v3(mtx_tx, vc->co);
          /* Length in 2D, don't `sqrt` because this is only for comparison. */
          vc->dist_sq = vc->co[other_axis_1] * vc->co[other_axis_1] +
                        vc->co[other_axis_2] * vc->co[other_axis_2];

          // printf("location %f %f %f -- %f\n", vc->co[0], vc->co[1], vc->co[2], vc->dist_sq);
        }
      }
      else {
        for (uint i = 0; i < totvert; i++, vc++) {
          vc->co[0] = vert_positions_new[i][0] = vert_positions_orig[i][0];
          vc->co[1] = vert_positions_new[i][1] = vert_positions_orig[i][1];
          vc->co[2] = vert_positions_new[i][2] = vert_positions_orig[i][2];

          vc->flag = 0;
          vc->e[0] = vc->e[1] = nullptr;
          vc->v[0] = vc->v[1] = SV_UNUSED;

          /* Length in 2D, don't `sqrt` because this is only for comparison. */
          vc->dist_sq = vc->co[other_axis_1] * vc->co[other_axis_1] +
                        vc->co[other_axis_2] * vc->co[other_axis_2];

          // printf("location %f %f %f -- %f\n", vc->co[0], vc->co[1], vc->co[2], vc->dist_sq);
        }
      }

      /* this loop builds connectivity info for verts */
      for (uint i = 0; i < totedge; i++, edge_new++) {
        vc = &vert_connect[(*edge_new)[0]];

        if (vc->v[0] == SV_UNUSED) { /* unused */
          vc->v[0] = uint((*edge_new)[1]);
          vc->e[0] = edge_new;
        }
        else if (vc->v[1] == SV_UNUSED) {
          vc->v[1] = uint((*edge_new)[1]);
          vc->e[1] = edge_new;
        }
        else {
          vc->v[0] = vc->v[1] = SV_INVALID; /* error value  - don't use, 3 edges on vert */
        }

        vc = &vert_connect[(*edge_new)[1]];

        /* same as above but swap v1/2 */
        if (vc->v[0] == SV_UNUSED) { /* unused */
          vc->v[0] = uint((*edge_new)[0]);
          vc->e[0] = edge_new;
        }
        else if (vc->v[1] == SV_UNUSED) {
          vc->v[1] = uint((*edge_new)[0]);
          vc->e[1] = edge_new;
        }
        else {
          vc->v[0] = vc->v[1] = SV_INVALID; /* error value  - don't use, 3 edges on vert */
        }
      }

      /* find the first vert */
      vc = vert_connect;
      for (uint i = 0; i < totvert; i++, vc++) {
        /* Now do search for connected verts, order all edges and flip them
         * so resulting faces are flipped the right way */
        vc_tot_linked = 0; /* count the number of linked verts for this loop */
        if (vc->flag == 0) {
          uint v_best = SV_UNUSED, ed_loop_closed = 0; /* vert and vert new */
          ScrewVertIter lt_iter;
          float fl = -1.0f;

          /* compiler complains if not initialized, but it should be initialized below */
          bool ed_loop_flip = false;

          // printf("Loop on connected vert: %i\n", i);

          for (j = 0; j < 2; j++) {
            // printf("\tSide: %i\n", j);
            screwvert_iter_init(&lt_iter, vert_connect, i, j);
            if (j == 1) {
              screwvert_iter_step(&lt_iter);
            }
            while (lt_iter.v_poin) {
              // printf("\t\tVERT: %i\n", lt_iter.v);
              if (lt_iter.v_poin->flag) {
                // printf("\t\t\tBreaking Found end\n");
                // endpoints[0] = endpoints[1] = SV_UNUSED;
                ed_loop_closed = 1; /* circle */
                break;
              }
              lt_iter.v_poin->flag = 1;
              vc_tot_linked++;
              // printf("Testing 2 floats %f : %f\n", fl, lt_iter.v_poin->dist_sq);
              if (fl <= lt_iter.v_poin->dist_sq) {
                fl = lt_iter.v_poin->dist_sq;
                v_best = lt_iter.v;
                // printf("\t\t\tVERT BEST: %i\n", v_best);
              }
              screwvert_iter_step(&lt_iter);
              if (!lt_iter.v_poin) {
                // printf("\t\t\tFound End Also Num %i\n", j);
                // endpoints[j] = lt_iter.v_other; /* other is still valid */
                break;
              }
            }
          }

          /* Now we have a collection of used edges. flip their edges the right way. */
          /* if (v_best != SV_UNUSED) - */

          // printf("Done Looking - vc_tot_linked: %i\n", vc_tot_linked);

          if (vc_tot_linked > 1) {
            float vf_1, vf_2, vf_best;

            vc_tmp = &vert_connect[v_best];

            tmpf1 = vert_connect[vc_tmp->v[0]].co;
            tmpf2 = vert_connect[vc_tmp->v[1]].co;

            /* edge connects on each side! */
            if (SV_IS_VALID(vc_tmp->v[0]) && SV_IS_VALID(vc_tmp->v[1])) {
              // printf("Verts on each side (%i %i)\n", vc_tmp->v[0], vc_tmp->v[1]);
              /* Find out which is higher. */

              vf_1 = tmpf1[ltmd->axis];
              vf_2 = tmpf2[ltmd->axis];
              vf_best = vc_tmp->co[ltmd->axis];

              if (vf_1 < vf_best && vf_best < vf_2) {
                ed_loop_flip = false;
              }
              else if (vf_1 > vf_best && vf_best > vf_2) {
                ed_loop_flip = true;
              }
              else {
                /* not so simple to work out which edge is higher */
                sub_v3_v3v3(tmp_vec1, tmpf1, vc_tmp->co);
                sub_v3_v3v3(tmp_vec2, tmpf2, vc_tmp->co);
                normalize_v3(tmp_vec1);
                normalize_v3(tmp_vec2);

                if (tmp_vec1[ltmd->axis] < tmp_vec2[ltmd->axis]) {
                  ed_loop_flip = true;
                }
                else {
                  ed_loop_flip = false;
                }
              }
            }
            else if (SV_IS_VALID(vc_tmp->v[0])) { /* Vertex only connected on 1 side. */
              // printf("Verts on ONE side (%i %i)\n", vc_tmp->v[0], vc_tmp->v[1]);
              if (tmpf1[ltmd->axis] < vc_tmp->co[ltmd->axis]) { /* best is above */
                ed_loop_flip = true;
              }
              else { /* best is below or even... in even case we can't know what to do. */
                ed_loop_flip = false;
              }
            }
#if 0
            else {
              printf("No Connected ___\n");
            }
#endif

            // printf("flip direction %i\n", ed_loop_flip);

            /* Switch the flip option if set
             * NOTE: flip is now done at face level so copying group slices is easier. */
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
              // printf("Ordering Side J %i\n", j);

              screwvert_iter_init(&lt_iter, vert_connect, v_best, j);
              // printf("\n\nStarting - Loop\n");
              lt_iter.v_poin->flag = 1; /* so a non loop will traverse the other side */

              /* If this is the vert off the best vert and
               * the best vert has 2 edges connected too it
               * then swap the flip direction */
              if (j == 1 && SV_IS_VALID(vc_tmp->v[0]) && SV_IS_VALID(vc_tmp->v[1])) {
                ed_loop_flip = !ed_loop_flip;
              }

              while (lt_iter.v_poin && lt_iter.v_poin->flag != 2) {
                // printf("\tOrdering Vert V %i\n", lt_iter.v);

                lt_iter.v_poin->flag = 2;
                if (lt_iter.e) {
                  if (lt_iter.v == uint((*lt_iter.e)[0])) {
                    if (ed_loop_flip == 0) {
                      // printf("\t\t\tFlipping 0\n");
                      std::swap((*lt_iter.e)[0], (*lt_iter.e)[1]);
                    }
#if 0
                    else {
                      printf("\t\t\tFlipping Not 0\n");
                    }
#endif
                  }
                  else if (lt_iter.v == uint((*lt_iter.e)[1])) {
                    if (ed_loop_flip == 1) {
                      // printf("\t\t\tFlipping 1\n");
                      std::swap((*lt_iter.e)[0], (*lt_iter.e)[1]);
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
      }
    }
  }
  else {
    for (uint i = 0; i < totvert; i++) {
      copy_v3_v3(vert_positions_new[i], vert_positions_orig[i]);
    }
  }
  /* done with edge connectivity based normal flipping */

  /* Add Faces */
  for (step = 1; step < step_tot; step++) {
    const uint varray_stride = totvert * step;
    float step_angle;
    float mat[4][4];
    /* Rotation Matrix */
    step_angle = (angle / float(step_tot - (!close))) * float(step);

    if (ob_axis != nullptr) {
      axis_angle_normalized_to_mat3(mat3, axis_vec, step_angle);
    }
    else {
      axis_angle_to_mat3_single(mat3, axis_char, step_angle);
    }
    copy_m4_m3(mat, mat3);

    if (screw_ofs) {
      madd_v3_v3fl(mat[3], axis_vec, screw_ofs * (float(step) / float(step_tot - 1)));
    }

    /* copy a slice */
    CustomData_copy_data(
        &mesh->vert_data, &result->vert_data, 0, int(varray_stride), int(totvert));

    /* set location */
    for (j = 0; j < totvert; j++) {
      const int vert_new = int(varray_stride) + int(j);

      copy_v3_v3(vert_positions_new[vert_new], vert_positions_new[j]);

      /* only need to set these if using non cleared memory */
      // mv_new->mat_nr = mv_new->flag = 0;

      if (ob_axis != nullptr) {
        sub_v3_v3(vert_positions_new[vert_new], mtx_tx[3]);

        mul_m4_v3(mat, vert_positions_new[vert_new]);

        add_v3_v3(vert_positions_new[vert_new], mtx_tx[3]);
      }
      else {
        mul_m4_v3(mat, vert_positions_new[vert_new]);
      }

      /* add the new edge */
      (*edge_new)[0] = int(varray_stride + j);
      (*edge_new)[1] = (*edge_new)[0] - int(totvert);
      edge_new++;
    }
  }

  /* we can avoid if using vert alloc trick */
  if (vert_connect) {
    MEM_freeN(vert_connect);
    vert_connect = nullptr;
  }

  if (close) {
    /* last loop of edges, previous loop doesn't account for the last set of edges */
    const uint varray_stride = (step_tot - 1) * totvert;

    for (uint i = 0; i < totvert; i++) {
      (*edge_new)[0] = int(i);
      (*edge_new)[1] = int(varray_stride + i);
      edge_new++;
    }
  }

  int new_loop_index = 0;
  med_new_firstloop = edges_new.data();

  /* more of an offset in this case */
  edge_offset = totedge + (totvert * (step_tot - (close ? 0 : 1)));

  const bke::AttributeAccessor src_attributes = mesh->attributes();
  const VArraySpan src_material_index = *src_attributes.lookup<int>("material_index",
                                                                    bke::AttrDomain::Face);

  bke::MutableAttributeAccessor dst_attributes = result->attributes_for_write();
  bke::SpanAttributeWriter dst_material_index = dst_attributes.lookup_or_add_for_write_span<int>(
      "material_index", bke::AttrDomain::Face);

  for (uint i = 0; i < totedge; i++, med_new_firstloop++) {
    const uint step_last = step_tot - (close ? 1 : 2);
    const uint face_index_orig = faces_num ? edge_face_map[i] : UINT_MAX;
    const bool has_mpoly_orig = (face_index_orig != UINT_MAX);
    float uv_v_offset_a, uv_v_offset_b;

    const uint mloop_index_orig[2] = {
        vert_loop_map ? vert_loop_map[edges_new[i][0]] : UINT_MAX,
        vert_loop_map ? vert_loop_map[edges_new[i][1]] : UINT_MAX,
    };
    const bool has_mloop_orig = mloop_index_orig[0] != UINT_MAX;

    int mat_nr;

    /* for each edge, make a cylinder of quads */
    i1 = uint((*med_new_firstloop)[0]);
    i2 = uint((*med_new_firstloop)[1]);

    if (has_mpoly_orig) {
      mat_nr = src_material_index.is_empty() ? 0 : src_material_index[face_index_orig];
    }
    else {
      mat_nr = 0;
    }

    if (has_mloop_orig == false && !uv_map_names.is_empty()) {
      uv_v_offset_a = dist_signed_to_plane_v3(vert_positions_new[edges_new[i][0]], uv_axis_plane);
      uv_v_offset_b = dist_signed_to_plane_v3(vert_positions_new[edges_new[i][1]], uv_axis_plane);

      if (ltmd->flag & MOD_SCREW_UV_STRETCH_V) {
        uv_v_offset_a = (uv_v_offset_a - uv_v_minmax[0]) * uv_v_range_inv;
        uv_v_offset_b = (uv_v_offset_b - uv_v_minmax[0]) * uv_v_range_inv;
      }
    }

    for (step = 0; step <= step_last; step++) {

      /* Polygon */
      if (has_mpoly_orig) {
        CustomData_copy_data(
            &mesh->face_data, &result->face_data, int(face_index_orig), face_index, 1);
        origindex[face_index] = int(face_index_orig);
      }
      else {
        origindex[face_index] = ORIGINDEX_NONE;
        dst_material_index.span[face_index] = mat_nr;
        sharp_faces.span[face_index] = use_flat_shading;
      }
      face_offests_new[face_index] = face_index * 4;

      /* Loop-Custom-Data */
      if (has_mloop_orig) {

        CustomData_copy_data(&mesh->corner_data,
                             &result->corner_data,
                             int(mloop_index_orig[0]),
                             new_loop_index + 0,
                             1);
        CustomData_copy_data(&mesh->corner_data,
                             &result->corner_data,
                             int(mloop_index_orig[1]),
                             new_loop_index + 1,
                             1);
        CustomData_copy_data(&mesh->corner_data,
                             &result->corner_data,
                             int(mloop_index_orig[1]),
                             new_loop_index + 2,
                             1);
        CustomData_copy_data(&mesh->corner_data,
                             &result->corner_data,
                             int(mloop_index_orig[0]),
                             new_loop_index + 3,
                             1);

        if (!uv_map_names.is_empty()) {
          const float uv_u_offset_a = float(step) * uv_u_scale;
          const float uv_u_offset_b = float(step + 1) * uv_u_scale;
          for (const int64_t uv_lay : uv_map_layers.index_range()) {
            blender::float2 *mluv = &uv_map_layers[uv_lay].span[new_loop_index];

            mluv[quad_ord[0]][0] += uv_u_offset_a;
            mluv[quad_ord[1]][0] += uv_u_offset_a;
            mluv[quad_ord[2]][0] += uv_u_offset_b;
            mluv[quad_ord[3]][0] += uv_u_offset_b;
          }
        }
      }
      else {
        if (!uv_map_names.is_empty()) {
          const float uv_u_offset_a = float(step) * uv_u_scale;
          const float uv_u_offset_b = float(step + 1) * uv_u_scale;
          for (const int64_t uv_lay : uv_map_layers.index_range()) {
            blender::float2 *mluv = &uv_map_layers[uv_lay].span[new_loop_index];

            copy_v2_fl2(mluv[quad_ord[0]], uv_u_offset_a, uv_v_offset_a);
            copy_v2_fl2(mluv[quad_ord[1]], uv_u_offset_a, uv_v_offset_b);
            copy_v2_fl2(mluv[quad_ord[2]], uv_u_offset_b, uv_v_offset_b);
            copy_v2_fl2(mluv[quad_ord[3]], uv_u_offset_b, uv_v_offset_a);
          }
        }
      }

      /* Loop-Data */
      if (!(close && step == step_last)) {
        /* regular segments */
        corner_verts_new[new_loop_index + quad_ord[0]] = int(i1);
        corner_verts_new[new_loop_index + quad_ord[1]] = int(i2);
        corner_verts_new[new_loop_index + quad_ord[2]] = int(i2 + totvert);
        corner_verts_new[new_loop_index + quad_ord[3]] = int(i1 + totvert);

        corner_edges_new[new_loop_index + quad_ord_ofs[0]] = int(
            step == 0 ? i : (edge_offset + step + (i * (step_tot - 1))) - 1);
        corner_edges_new[new_loop_index + quad_ord_ofs[1]] = int(totedge + i2);
        corner_edges_new[new_loop_index + quad_ord_ofs[2]] = int(edge_offset + step +
                                                                 (i * (step_tot - 1)));
        corner_edges_new[new_loop_index + quad_ord_ofs[3]] = int(totedge + i1);

        /* new vertical edge */
        if (step) { /* The first set is already done */
          (*edge_new)[0] = int(i1);
          (*edge_new)[1] = int(i2);
          edge_new++;
        }
        i1 += totvert;
        i2 += totvert;
      }
      else {
        /* last segment */
        corner_verts_new[new_loop_index + quad_ord[0]] = int(i1);
        corner_verts_new[new_loop_index + quad_ord[1]] = int(i2);
        corner_verts_new[new_loop_index + quad_ord[2]] = int((*med_new_firstloop)[1]);
        corner_verts_new[new_loop_index + quad_ord[3]] = int((*med_new_firstloop)[0]);

        corner_edges_new[new_loop_index + quad_ord_ofs[0]] = int(
            (edge_offset + step + (i * (step_tot - 1))) - 1);
        corner_edges_new[new_loop_index + quad_ord_ofs[1]] = int(totedge + i2);
        corner_edges_new[new_loop_index + quad_ord_ofs[2]] = int(i);
        corner_edges_new[new_loop_index + quad_ord_ofs[3]] = int(totedge + i1);
      }

      new_loop_index += 4;
      face_index++;
    }

    /* new vertical edge */
    (*edge_new)[0] = int(i1);
    (*edge_new)[1] = int(i2);
    edge_new++;
  }

/* validate loop edges */
#if 0
  {
    uint i = 0;
    printf("\n");
    for (; i < maxPolys * 4; i += 4) {
      uint ii;
      ml_new = mloop_new + i;
      ii = findEd(edges_new, maxEdges, ml_new[0].v, ml_new[1].v);
      printf("%d %d -- ", ii, ml_new[0].e);
      ml_new[0].e = ii;

      ii = findEd(edges_new, maxEdges, ml_new[1].v, ml_new[2].v);
      printf("%d %d -- ", ii, ml_new[1].e);
      ml_new[1].e = ii;

      ii = findEd(edges_new, maxEdges, ml_new[2].v, ml_new[3].v);
      printf("%d %d -- ", ii, ml_new[2].e);
      ml_new[2].e = ii;

      ii = findEd(edges_new, maxEdges, ml_new[3].v, ml_new[0].v);
      printf("%d %d\n", ii, ml_new[3].e);
      ml_new[3].e = ii;
    }
  }
#endif

  sharp_faces.finish();
  dst_material_index.finish();
  for (bke::SpanAttributeWriter<float2> &uv_map : uv_map_layers) {
    uv_map.finish();
  }

  if (edge_face_map) {
    MEM_freeN(edge_face_map);
  }

  if (vert_loop_map) {
    MEM_freeN(vert_loop_map);
  }

  if (do_remove_doubles) {
    result = mesh_remove_doubles_on_axis(result,
                                         vert_positions_new,
                                         totvert,
                                         step_tot,
                                         axis_vec,
                                         ob_axis != nullptr ? mtx_tx[3] : nullptr,
                                         ltmd->merge_dist);
  }

  return result;
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  ScrewModifierData *ltmd = (ScrewModifierData *)md;
  if (ltmd->ob_axis != nullptr) {
    DEG_add_object_relation(ctx->node, ltmd->ob_axis, DEG_OB_COMP_TRANSFORM, "Screw Modifier");
    DEG_add_depends_on_transform_relation(ctx->node, "Screw Modifier");
  }
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  ScrewModifierData *ltmd = (ScrewModifierData *)md;

  walk(user_data, ob, (ID **)&ltmd->ob_axis, IDWALK_CB_NOP);
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *sub, *row, *col;
  uiLayout *layout = panel->layout;
  const eUI_Item_Flag toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  PointerRNA screw_obj_ptr = RNA_pointer_get(ptr, "object");

  layout->use_property_split_set(true);

  col = &layout->column(false);
  col->prop(ptr, "angle", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  row = &col->row(false);
  row->active_set(RNA_pointer_is_null(&screw_obj_ptr) ||
                  !RNA_boolean_get(ptr, "use_object_screw_offset"));
  row->prop(ptr, "screw_offset", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(ptr, "iterations", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->separator();
  col = &layout->column(false);
  row = &col->row(false);
  row->prop(ptr, "axis", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  col->prop(ptr, "object", UI_ITEM_NONE, IFACE_("Axis Object"), ICON_NONE);
  sub = &col->column(false);
  sub->active_set(!RNA_pointer_is_null(&screw_obj_ptr));
  sub->prop(ptr, "use_object_screw_offset", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->separator();

  col = &layout->column(true);
  col->prop(ptr, "steps", UI_ITEM_NONE, IFACE_("Steps Viewport"), ICON_NONE);
  col->prop(ptr, "render_steps", UI_ITEM_NONE, IFACE_("Render"), ICON_NONE);

  layout->separator();

  row = &layout->row(true, IFACE_("Merge"));
  row->prop(ptr, "use_merge_vertices", UI_ITEM_NONE, "", ICON_NONE);
  sub = &row->row(true);
  sub->active_set(RNA_boolean_get(ptr, "use_merge_vertices"));
  sub->prop(ptr, "merge_threshold", UI_ITEM_NONE, "", ICON_NONE);

  layout->separator();

  row = &layout->row(true, IFACE_("Stretch UVs"));
  row->prop(ptr, "use_stretch_u", toggles_flag, IFACE_("U"), ICON_NONE);
  row->prop(ptr, "use_stretch_v", toggles_flag, IFACE_("V"), ICON_NONE);

  modifier_error_message_draw(layout, ptr);
}

static void normals_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->use_property_split_set(true);

  col = &layout->column(false);
  col->prop(ptr, "use_smooth_shade", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(ptr, "use_normal_calculate", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(ptr, "use_normal_flip", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Screw, panel_draw);
  modifier_subpanel_register(
      region_type, "normals", "Normals", nullptr, normals_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_Screw = {
    /*idname*/ "Screw",
    /*name*/ N_("Screw"),
    /*struct_name*/ "ScrewModifierData",
    /*struct_size*/ sizeof(ScrewModifierData),
    /*srna*/ &RNA_ScrewModifier,
    /*type*/ ModifierTypeType::Constructive,

    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_SupportsEditmode | eModifierTypeFlag_EnableInEditmode,
    /*icon*/ ICON_MOD_SCREW,

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
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
