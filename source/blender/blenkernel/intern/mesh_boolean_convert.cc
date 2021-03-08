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
 */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_customdata.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_mesh_boolean_convert.h"

#include "BLI_alloca.h"
#include "BLI_float2.hh"
#include "BLI_float4x4.hh"
#include "BLI_math.h"
#include "BLI_mesh_boolean.hh"
#include "BLI_mesh_intersect.hh"
#include "BLI_span.hh"

namespace blender::meshintersect {

#ifdef WITH_GMP

constexpr int estimated_max_facelen = 100; /* Used for initial size of some Vectors. */

/* Snap entries that are near 0 or 1 or -1 to those values.
 * Sometimes Blender's rotation matrices for multiples of 90 degrees have
 * tiny numbers where there should be zeros. That messes makes some things
 * every so slightly non-coplanar when users expect coplanarity,
 * so this is a hack to clean up such matrices.
 * Would be better to change the transformation code itself.
 */
static void clean_obmat(float4x4 &cleaned, const float4x4 &mat)
{
  const float fuzz = 1e-6f;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      float f = mat.values[i][j];
      if (fabsf(f) <= fuzz) {
        f = 0.0f;
      }
      else if (fabsf(f - 1.0f) <= fuzz) {
        f = 1.0f;
      }
      else if (fabsf(f + 1.0f) <= fuzz) {
        f = -1.0f;
      }
      cleaned.values[i][j] = f;
    }
  }
}

/* `MeshesToIMeshInfo` keeps track of information used when combining a number
 * of `Mesh`es into a single `IMesh` for doing boolean on.
 * Mostly this means keeping track of the index offsets for various mesh elements. */
class MeshesToIMeshInfo {
 public:
  /* The input meshes, */
  Span<const Mesh *> meshes;
  /* Numbering the vertices of the meshes in order of meshes,
   * at what offset does the vertex range for mesh[i] start? */
  Array<int> mesh_vert_offset;
  /* Similarly for edges of meshes. */
  Array<int> mesh_edge_offset;
  /* Similarly for polys of meshes. */
  Array<int> mesh_poly_offset;
  /* For each Mesh vertex in all the meshes (with concatenated indexing),
   * what is the IMesh Vert* allocated for it in the input IMesh? */
  Array<const Vert *> mesh_to_imesh_vert;
  /* Similarly for each Mesh poly. */
  Array<Face *> mesh_to_imesh_face;
  /* Transformation matrix to transform a coordinate in the corresponding
   * Mesh to the local space of the first Mesh. */
  Array<float4x4> to_obj0;
  /* Total number of input mesh vertices. */
  int tot_meshes_verts;
  /* Total number of input mesh edges. */
  int tot_meshes_edges;
  /* Total number of input mesh polys. */
  int tot_meshes_polys;

  int input_mesh_for_imesh_vert(int imesh_v) const;
  int input_mesh_for_imesh_edge(int imesh_e) const;
  int input_mesh_for_imesh_face(int imesh_f) const;
  const MPoly *input_mpoly_for_orig_index(int orig_index,
                                          const Mesh **r_orig_mesh,
                                          int *r_orig_mesh_index,
                                          int *r_index_in_orig_mesh) const;
  const MVert *input_mvert_for_orig_index(int orig_index,
                                          const Mesh **r_orig_mesh,
                                          int *r_index_in_orig_mesh) const;
  const MEdge *input_medge_for_orig_index(int orig_index,
                                          const Mesh **r_orig_mesh,
                                          int *r_index_in_orig_mesh) const;
};

/* Given an index `imesh_v` in the `IMesh`, return the index of the
 * input `Mesh` that contained the `MVert` that it came from. */
int MeshesToIMeshInfo::input_mesh_for_imesh_vert(int imesh_v) const
{
  int n = static_cast<int>(mesh_vert_offset.size());
  for (int i = 0; i < n - 1; ++i) {
    if (imesh_v < mesh_vert_offset[i + 1]) {
      return i;
    }
  }
  return n - 1;
}

/* Given an index `imesh_e` used as an original index in the `IMesh`,
 * return the index of the input `Mesh` that contained the `MVert` that it came from. */
int MeshesToIMeshInfo::input_mesh_for_imesh_edge(int imesh_e) const
{
  int n = static_cast<int>(mesh_edge_offset.size());
  for (int i = 0; i < n - 1; ++i) {
    if (imesh_e < mesh_edge_offset[i + 1]) {
      return i;
    }
  }
  return n - 1;
}

/* Given an index `imesh_f` in the `IMesh`, return the index of the
 * input `Mesh` that contained the `MPoly` that it came from. */
int MeshesToIMeshInfo::input_mesh_for_imesh_face(int imesh_f) const
{
  int n = static_cast<int>(mesh_poly_offset.size());
  for (int i = 0; i < n - 1; ++i) {
    if (imesh_f < mesh_poly_offset[i + 1]) {
      return i;
    }
  }
  return n - 1;
}

/* Given an index of an original face in the `IMesh`, find out the input
 * `Mesh` that it came from and return it in `*r_orig_mesh`,
 * and also return the index of that `Mesh` in  `*r_orig_mesh_index`.
 * Finally, return the index of the corresponding `MPoly` in that `Mesh`
 * in `*r_index_in_orig_mesh`. */
const MPoly *MeshesToIMeshInfo::input_mpoly_for_orig_index(int orig_index,
                                                           const Mesh **r_orig_mesh,
                                                           int *r_orig_mesh_index,
                                                           int *r_index_in_orig_mesh) const
{
  int orig_mesh_index = input_mesh_for_imesh_face(orig_index);
  BLI_assert(0 <= orig_mesh_index && orig_mesh_index < meshes.size());
  const Mesh *me = meshes[orig_mesh_index];
  int index_in_mesh = orig_index - mesh_poly_offset[orig_mesh_index];
  BLI_assert(0 <= index_in_mesh && index_in_mesh < me->totpoly);
  const MPoly *mp = &me->mpoly[index_in_mesh];
  if (r_orig_mesh) {
    *r_orig_mesh = me;
  }
  if (r_orig_mesh_index) {
    *r_orig_mesh_index = orig_mesh_index;
  }
  if (r_index_in_orig_mesh) {
    *r_index_in_orig_mesh = index_in_mesh;
  }
  return mp;
}

/* Given an index of an original vertex in the `IMesh`, find out the input
 * `Mesh` that it came from and return it in `*r_orig_mesh`.
 * Also find the index of the `MVert` in that `Mesh` and return it in
 * `*r_index_in_orig_mesh`. */
const MVert *MeshesToIMeshInfo::input_mvert_for_orig_index(int orig_index,
                                                           const Mesh **r_orig_mesh,
                                                           int *r_index_in_orig_mesh) const
{
  int orig_mesh_index = input_mesh_for_imesh_vert(orig_index);
  BLI_assert(0 <= orig_mesh_index && orig_mesh_index < meshes.size());
  const Mesh *me = meshes[orig_mesh_index];
  int index_in_mesh = orig_index - mesh_vert_offset[orig_mesh_index];
  BLI_assert(0 <= index_in_mesh && index_in_mesh < me->totvert);
  const MVert *mv = &me->mvert[index_in_mesh];
  if (r_orig_mesh) {
    *r_orig_mesh = me;
  }
  if (r_index_in_orig_mesh) {
    *r_index_in_orig_mesh = index_in_mesh;
  }
  return mv;
}

/* Similarly for edges. */
const MEdge *MeshesToIMeshInfo::input_medge_for_orig_index(int orig_index,
                                                           const Mesh **r_orig_mesh,
                                                           int *r_index_in_orig_mesh) const
{
  int orig_mesh_index = input_mesh_for_imesh_edge(orig_index);
  BLI_assert(0 <= orig_mesh_index && orig_mesh_index < meshes.size());
  const Mesh *me = meshes[orig_mesh_index];
  int index_in_mesh = orig_index - mesh_edge_offset[orig_mesh_index];
  BLI_assert(0 <= index_in_mesh && index_in_mesh < me->totedge);
  const MEdge *medge = &me->medge[index_in_mesh];
  if (r_orig_mesh) {
    *r_orig_mesh = me;
  }
  if (r_index_in_orig_mesh) {
    *r_index_in_orig_mesh = index_in_mesh;
  }
  return medge;
}

/** Convert all of the meshes in `meshes` to an `IMesh` and return that.
 * All of the coordinates are transformed into the local space of the
 * first Mesh. To do this transformation, we also need the transformation
 * obmats corresponding to the Meshes, so they are in the `obmats` argument.
 * The 'original' indexes in the IMesh are the indexes you get by
 * a scheme that offsets each MVert, MEdge, and MPoly index by the sum of the
 * vertices, edges, and polys in the preceding Meshes in the mesh span.
 * The `*r_info class` is filled in with information needed to make the
 * correspondence between the Mesh MVerts/MPolys and the IMesh Verts/Faces.
 * All allocation of memory for the IMesh comes from `arena`.
 */
static IMesh meshes_to_imesh(Span<const Mesh *> meshes,
                             Span<const float4x4 *> obmats,
                             IMeshArena &arena,
                             MeshesToIMeshInfo *r_info)
{
  int nmeshes = meshes.size();
  BLI_assert(nmeshes > 0);
  r_info->meshes = meshes;
  r_info->tot_meshes_verts = 0;
  r_info->tot_meshes_polys = 0;
  int &totvert = r_info->tot_meshes_verts;
  int &totedge = r_info->tot_meshes_edges;
  int &totpoly = r_info->tot_meshes_polys;
  for (const Mesh *me : meshes) {
    totvert += me->totvert;
    totedge += me->totedge;
    totpoly += me->totpoly;
  }

  /* Estimate the number of vertices and faces in the boolean output,
   * so that the memory arena can reserve some space. It is OK if these
   * estimates are wrong. */
  const int estimate_num_outv = 3 * totvert;
  const int estimate_num_outf = 4 * totpoly;
  arena.reserve(estimate_num_outv, estimate_num_outf);
  r_info->mesh_to_imesh_vert = Array<const Vert *>(totvert);
  r_info->mesh_to_imesh_face = Array<Face *>(totpoly);
  r_info->mesh_vert_offset = Array<int>(nmeshes);
  r_info->mesh_edge_offset = Array<int>(nmeshes);
  r_info->mesh_poly_offset = Array<int>(nmeshes);
  r_info->to_obj0 = Array<float4x4>(nmeshes);
  int v = 0;
  int e = 0;
  int f = 0;

  /* Put these Vectors here, with a size unlikely to need resizing,
   * so that the loop to make new Faces will likely not need to allocate
   * over and over. */
  Vector<const Vert *, estimated_max_facelen> face_vert;
  Vector<int, estimated_max_facelen> face_edge_orig;

  /* To convert the coordinates of objects 1, 2, etc. into the local space
   * of object 0, we multiply each object's `obmat` by the inverse of
   * object 0's `obmat`. Exact Boolean works better if these matrices
   * are 'cleaned' -- see the comment for the `clean_obmat` function, above. */
  float4x4 obj0_mat;
  float4x4 inv_obj0_mat;
  if (obmats[0] == nullptr) {
    unit_m4(obj0_mat.values);
    unit_m4(inv_obj0_mat.values);
  }
  else {
    clean_obmat(obj0_mat, *obmats[0]);
    invert_m4_m4(inv_obj0_mat.values, obj0_mat.values);
  }

  /* For each input `Mesh`, make `Vert`s and `Face`s for the corresponding
   * `MVert`s and `MPoly`s, and keep track of the original indices (using the
   * concatenating offset scheme) inside the `Vert`s and `Face`s.
   * When making `Face`s, we also put in the original indices for `MEdge`s that
   * make up the `MPoly`s using the same scheme. */
  for (int mi : meshes.index_range()) {
    float4x4 objn_to_obj0_mat;
    const Mesh *me = meshes[mi];
    if (mi == 0) {
      r_info->mesh_vert_offset[mi] = 0;
      r_info->mesh_edge_offset[mi] = 0;
      r_info->mesh_poly_offset[mi] = 0;
      unit_m4(r_info->to_obj0[0].values);
    }
    else {
      r_info->mesh_vert_offset[mi] = v;
      r_info->mesh_edge_offset[mi] = e;
      r_info->mesh_poly_offset[mi] = f;
      /* Get matrix that transforms a coordinate in objects[mi]'s local space
       * to object[0]'s local space.*/
      float4x4 objn_mat;
      if (obmats[mi] == nullptr) {
        unit_m4(objn_mat.values);
      }
      else {
        clean_obmat(objn_mat, *obmats[mi]);
      }
      objn_to_obj0_mat = inv_obj0_mat * objn_mat;
      r_info->to_obj0[mi] = objn_to_obj0_mat;
    }
    for (int vi = 0; vi < me->totvert; ++vi) {
      float3 co = me->mvert[vi].co;
      if (mi > 0) {
        co = objn_to_obj0_mat * co;
      }
      r_info->mesh_to_imesh_vert[v] = arena.add_or_find_vert(mpq3(co.x, co.y, co.z), v);
      ++v;
    }
    for (const MPoly &poly : Span(me->mpoly, me->totpoly)) {
      int flen = poly.totloop;
      face_vert.clear();
      face_edge_orig.clear();
      const MLoop *l = &me->mloop[poly.loopstart];
      for (int i = 0; i < flen; ++i) {
        int mverti = r_info->mesh_vert_offset[mi] + l->v;
        const Vert *fv = r_info->mesh_to_imesh_vert[mverti];
        face_vert.append(fv);
        face_edge_orig.append(e + l->e);
        ++l;
      }
      r_info->mesh_to_imesh_face[f] = arena.add_face(face_vert, f, face_edge_orig);
      ++f;
    }
    e += me->totedge;
  }
  return IMesh(r_info->mesh_to_imesh_face);
}

/* Copy vertex attributes, including customdata, from `orig_mv` to `mv`.
 * `mv` is in `dest_mesh` with index `mv_index`.
 * The `orig_mv` vertex came from Mesh `orig_me` and had index `index_in_orig_me` there. */
static void copy_vert_attributes(Mesh *dest_mesh,
                                 MVert *mv,
                                 const MVert *orig_mv,
                                 const Mesh *orig_me,
                                 int mv_index,
                                 int index_in_orig_me)
{
  mv->bweight = orig_mv->bweight;
  mv->flag = orig_mv->flag;

  /* For all layers in the orig mesh, copy the layer information. */
  CustomData *target_cd = &dest_mesh->vdata;
  const CustomData *source_cd = &orig_me->vdata;
  for (int source_layer_i = 0; source_layer_i < source_cd->totlayer; ++source_layer_i) {
    int ty = source_cd->layers[source_layer_i].type;
    /* The (first) CD_MVERT layer is the same as dest_mesh->vdata, so we've
     * already set the coordinate to the right value. */
    if (ty == CD_MVERT) {
      continue;
    }
    const char *name = source_cd->layers[source_layer_i].name;
    int target_layer_i = CustomData_get_named_layer_index(target_cd, ty, name);
    /* Not all layers were merged in target: some are marked CD_FLAG_NOCOPY
     * and some are not in the CD_MASK_MESH.vdata. */
    if (target_layer_i != -1) {
      CustomData_copy_data_layer(
          source_cd, target_cd, source_layer_i, target_layer_i, index_in_orig_me, mv_index, 1);
    }
  }
}

/* Similar to copy_vert_attributes but for poly attributes. */
static void copy_poly_attributes(Mesh *dest_mesh,
                                 MPoly *mp,
                                 const MPoly *orig_mp,
                                 const Mesh *orig_me,
                                 int mp_index,
                                 int index_in_orig_me)
{
  mp->mat_nr = orig_mp->mat_nr;
  if (mp->mat_nr >= dest_mesh->totcol) {
    mp->mat_nr = 0;
  }
  mp->flag = orig_mp->flag;
  CustomData *target_cd = &dest_mesh->pdata;
  const CustomData *source_cd = &orig_me->pdata;
  for (int source_layer_i = 0; source_layer_i < source_cd->totlayer; ++source_layer_i) {
    int ty = source_cd->layers[source_layer_i].type;
    if (ty == CD_MPOLY) {
      continue;
    }
    const char *name = source_cd->layers[source_layer_i].name;
    int target_layer_i = CustomData_get_named_layer_index(target_cd, ty, name);
    if (target_layer_i != -1) {
      CustomData_copy_data_layer(
          source_cd, target_cd, source_layer_i, target_layer_i, index_in_orig_me, mp_index, 1);
    }
  }
}

/* Similar to copy_vert_attributes but for edge attributes. */
static void copy_edge_attributes(Mesh *dest_mesh,
                                 MEdge *medge,
                                 const MEdge *orig_medge,
                                 const Mesh *orig_me,
                                 int medge_index,
                                 int index_in_orig_me)
{
  medge->bweight = orig_medge->bweight;
  medge->crease = orig_medge->crease;
  medge->flag = orig_medge->flag;
  CustomData *target_cd = &dest_mesh->edata;
  const CustomData *source_cd = &orig_me->edata;
  for (int source_layer_i = 0; source_layer_i < source_cd->totlayer; ++source_layer_i) {
    int ty = source_cd->layers[source_layer_i].type;
    if (ty == CD_MEDGE) {
      continue;
    }
    const char *name = source_cd->layers[source_layer_i].name;
    int target_layer_i = CustomData_get_named_layer_index(target_cd, ty, name);
    if (target_layer_i != -1) {
      CustomData_copy_data_layer(
          source_cd, target_cd, source_layer_i, target_layer_i, index_in_orig_me, medge_index, 1);
    }
  }
}

/**
 * For #IMesh face `f`, with corresponding output Mesh poly `mp`,
 * where the original Mesh poly is `orig_mp`, coming from the Mesh
 * `orig_me`, which has index `orig_me_index` in `mim`:
 * fill in the `orig_loops` Array with corresponding indices of MLoops from `orig_me`
 * where they have the same start and end vertices; for cases where that is
 * not true, put -1 in the `orig_loops` slot.
 * For now, we only try to do this if `mp` and `orig_mp` have the same size.
 * Return the number of non-null MLoops filled in.
 */
static int fill_orig_loops(const Face *f,
                           const MPoly *orig_mp,
                           const Mesh *orig_me,
                           int orig_me_index,
                           MeshesToIMeshInfo &mim,
                           Array<int> &orig_loops)
{
  orig_loops.fill(-1);
  int orig_mplen = orig_mp->totloop;
  if (f->size() != orig_mplen) {
    return 0;
  }
  BLI_assert(orig_loops.size() == orig_mplen);
  /* We'll look for the case where the first vertex in f has an original vertex
   * that is the same as one in orig_me (after correcting for offset in mim meshes).
   * Then see that loop and any subsequent ones have the same start and end vertex.
   * This may miss some cases of partial alignment, but that's OK since discovering
   * aligned loops is only an optimization to avoid some re-interpolation.
   */
  int first_orig_v = f->vert[0]->orig;
  if (first_orig_v == NO_INDEX) {
    return 0;
  }
  /* It is possible that the original vert was merged with another in another mesh. */
  if (orig_me_index != mim.input_mesh_for_imesh_vert(first_orig_v)) {
    return 0;
  }
  int orig_me_vert_offset = mim.mesh_vert_offset[orig_me_index];
  int first_orig_v_in_orig_me = first_orig_v - orig_me_vert_offset;
  BLI_assert(0 <= first_orig_v_in_orig_me && first_orig_v_in_orig_me < orig_me->totvert);
  /* Assume all vertices in an mpoly are unique. */
  int offset = -1;
  for (int i = 0; i < orig_mplen; ++i) {
    int loop_i = i + orig_mp->loopstart;
    if (orig_me->mloop[loop_i].v == first_orig_v_in_orig_me) {
      offset = i;
      break;
    }
  }
  if (offset == -1) {
    return 0;
  }
  int num_orig_loops_found = 0;
  for (int mp_loop_index = 0; mp_loop_index < orig_mplen; ++mp_loop_index) {
    int orig_mp_loop_index = (mp_loop_index + offset) % orig_mplen;
    MLoop *l = &orig_me->mloop[orig_mp->loopstart + orig_mp_loop_index];
    int fv_orig = f->vert[mp_loop_index]->orig;
    if (fv_orig != NO_INDEX) {
      fv_orig -= orig_me_vert_offset;
      if (fv_orig < 0 || fv_orig >= orig_me->totvert) {
        fv_orig = NO_INDEX;
      }
    }
    if (l->v == fv_orig) {
      MLoop *lnext = &orig_me->mloop[orig_mp->loopstart + ((orig_mp_loop_index + 1) % orig_mplen)];
      int fvnext_orig = f->vert[(mp_loop_index + 1) % orig_mplen]->orig;
      if (fvnext_orig != NO_INDEX) {
        fvnext_orig -= orig_me_vert_offset;
        if (fvnext_orig < 0 || fvnext_orig >= orig_me->totvert) {
          fvnext_orig = NO_INDEX;
        }
      }
      if (lnext->v == fvnext_orig) {
        orig_loops[mp_loop_index] = orig_mp->loopstart + orig_mp_loop_index;
        ++num_orig_loops_found;
      }
    }
  }
  return num_orig_loops_found;
}

/* Fill `cos_2d` with the 2d coordinates found by projection MPoly `mp` along
 * its normal. Also fill in r_axis_mat with the matrix that does that projection.
 * But before projecting, also transform the 3d coordinate by multiplying by trans_mat.
 * `cos_2d` should have room for `mp->totloop` entries. */
static void get_poly2d_cos(const Mesh *me,
                           const MPoly *mp,
                           float (*cos_2d)[2],
                           const float4x4 &trans_mat,
                           float r_axis_mat[3][3])
{
  int n = mp->totloop;

  /* Project coordinates to 2d in cos_2d, using normal as projection axis. */
  float axis_dominant[3];
  BKE_mesh_calc_poly_normal(mp, &me->mloop[mp->loopstart], me->mvert, axis_dominant);
  axis_dominant_v3_to_m3(r_axis_mat, axis_dominant);
  MLoop *ml = &me->mloop[mp->loopstart];
  const MVert *mverts = me->mvert;
  for (int i = 0; i < n; ++i) {
    float3 co = mverts[ml->v].co;
    co = trans_mat * co;
    mul_v2_m3v3(cos_2d[i], r_axis_mat, co);
    ++ml;
  }
}

/* For the loops of `mp`, see if the face is unchanged from `orig_mp`, and if so,
 * copy the Loop attributes from corresponding loops to corresponding loops.
 * Otherwise, interpolate the Loop attributes in the face `orig_mp`. */
static void copy_or_interp_loop_attributes(Mesh *dest_mesh,
                                           const Face *f,
                                           MPoly *mp,
                                           const MPoly *orig_mp,
                                           const Mesh *orig_me,
                                           int orig_me_index,
                                           MeshesToIMeshInfo &mim)
{
  Array<int> orig_loops(mp->totloop);
  int norig = fill_orig_loops(f, orig_mp, orig_me, orig_me_index, mim, orig_loops);
  /* We may need these arrays if we have to interpolate Loop attributes rather than just copy.
   * Right now, trying Array<float[2]> complains, so declare cos_2d a different way. */
  float(*cos_2d)[2];
  Array<float> weights;
  Array<const void *> src_blocks_ofs;
  float axis_mat[3][3];
  if (norig != mp->totloop) {
    /* We will need to interpolate. Make `cos_2d` hold 2d-projected coordinates of `orig_mp`,
     * which are transformed into object 0's local space before projecting.
     * At this point we cannot yet calculate the interpolation weights, as they depend on
     * the coordinate where interpolation is to happen, but we can allocate the needed arrays,
     * so they don't have to be allocated per-layer. */
    cos_2d = (float(*)[2])BLI_array_alloca(cos_2d, orig_mp->totloop);
    weights = Array<float>(orig_mp->totloop);
    src_blocks_ofs = Array<const void *>(orig_mp->totloop);
    get_poly2d_cos(orig_me, orig_mp, cos_2d, mim.to_obj0[orig_me_index], axis_mat);
  }
  CustomData *target_cd = &dest_mesh->ldata;
  for (int i = 0; i < mp->totloop; ++i) {
    int loop_index = mp->loopstart + i;
    int orig_loop_index = norig > 0 ? orig_loops[i] : -1;
    const CustomData *source_cd = &orig_me->ldata;
    if (orig_loop_index == -1) {
      /* Will need interpolation weights for this loop's vertex's coordinates.
       * The coordinate needs to be projected into 2d,  just like the interpolating polygon's
       * coordinates were. The `dest_mesh` coordinates are already in object 0 local space. */
      float co[2];
      mul_v2_m3v3(co, axis_mat, dest_mesh->mvert[dest_mesh->mloop[loop_index].v].co);
      interp_weights_poly_v2(weights.data(), cos_2d, orig_mp->totloop, co);
    }
    for (int source_layer_i = 0; source_layer_i < source_cd->totlayer; ++source_layer_i) {
      int ty = source_cd->layers[source_layer_i].type;
      if (ty == CD_MLOOP) {
        continue;
      }
      const char *name = source_cd->layers[source_layer_i].name;
      int target_layer_i = CustomData_get_named_layer_index(target_cd, ty, name);
      if (target_layer_i == -1) {
        continue;
      }
      if (orig_loop_index != -1) {
        CustomData_copy_data_layer(
            source_cd, target_cd, source_layer_i, target_layer_i, orig_loop_index, loop_index, 1);
      }
      else {
        /* Note: although CustomData_bmesh_interp_n function has bmesh in its name, nothing about
         * it is BMesh-specific. We can't use CustomData_interp because it assumes that
         * all source layers exist in the dest.
         * A non bmesh version could have the benefit of not copying data into src_blocks_ofs -
         * using the contiguous data instead. TODO: add to the custom data API. */
        int target_layer_type_index = CustomData_get_named_layer(target_cd, ty, name);
        if (!CustomData_layer_has_interp(source_cd, source_layer_i)) {
          continue;
        }
        int source_layer_type_index = source_layer_i - source_cd->typemap[ty];
        BLI_assert(target_layer_type_index != -1 && source_layer_type_index >= 0);
        for (int j = 0; j < orig_mp->totloop; ++j) {
          src_blocks_ofs[j] = CustomData_get_n(
              source_cd, ty, orig_mp->loopstart + j, source_layer_type_index);
        }
        void *dst_block_ofs = CustomData_get_n(target_cd, ty, loop_index, target_layer_type_index);
        CustomData_bmesh_interp_n(target_cd,
                                  src_blocks_ofs.data(),
                                  weights.data(),
                                  nullptr,
                                  orig_mp->totloop,
                                  dst_block_ofs,
                                  target_layer_i);
      }
    }
  }
}

/** Make sure that there are custom data layers in the target mesh
 * corresponding to all target layers in all of the operands after the first.
 * (The target should already have layers for those in the first operand mesh).
 * Edges done separately -- will have to be done later, after edges are made.
 */
static void merge_vertex_loop_poly_customdata_layers(Mesh *target, MeshesToIMeshInfo &mim)
{
  for (int mesh_index = 1; mesh_index < mim.meshes.size(); ++mesh_index) {
    const Mesh *me = mim.meshes[mesh_index];
    if (me->totvert) {
      CustomData_merge(
          &me->vdata, &target->vdata, CD_MASK_MESH.vmask, CD_DEFAULT, target->totvert);
    }
    if (me->totloop) {
      CustomData_merge(
          &me->ldata, &target->ldata, CD_MASK_MESH.lmask, CD_DEFAULT, target->totloop);
    }
    if (me->totpoly) {
      CustomData_merge(
          &me->pdata, &target->pdata, CD_MASK_MESH.pmask, CD_DEFAULT, target->totpoly);
    }
  }
}

static void merge_edge_customdata_layers(Mesh *target, MeshesToIMeshInfo &mim)
{
  for (int mesh_index = 1; mesh_index < mim.meshes.size(); ++mesh_index) {
    const Mesh *me = mim.meshes[mesh_index];
    if (me->totedge) {
      CustomData_merge(
          &me->edata, &target->edata, CD_MASK_MESH.emask, CD_DEFAULT, target->totedge);
    }
  }
}

/** Convert the output IMesh im to a Blender Mesh,
 * using the information in mim to get all the attributes right.
 */
static Mesh *imesh_to_mesh(IMesh *im, MeshesToIMeshInfo &mim)
{
  constexpr int dbg_level = 0;

  im->populate_vert();
  int out_totvert = im->vert_size();
  int out_totpoly = im->face_size();
  int out_totloop = 0;
  for (const Face *f : im->faces()) {
    out_totloop += f->size();
  }
  /* Will calculate edges later. */
  Mesh *result = BKE_mesh_new_nomain_from_template(
      mim.meshes[0], out_totvert, 0, 0, out_totloop, out_totpoly);

  merge_vertex_loop_poly_customdata_layers(result, mim);
  /* Set the vertex coordinate values and other data. */
  for (int vi : im->vert_index_range()) {
    const Vert *v = im->vert(vi);
    MVert *mv = &result->mvert[vi];
    copy_v3fl_v3db(mv->co, v->co);
    if (v->orig != NO_INDEX) {
      const Mesh *orig_me;
      int index_in_orig_me;
      const MVert *orig_mv = mim.input_mvert_for_orig_index(v->orig, &orig_me, &index_in_orig_me);
      copy_vert_attributes(result, mv, orig_mv, orig_me, vi, index_in_orig_me);
    }
  }

  /* Set the loopstart and totloop for each output poly,
   * and set the vertices in the appropriate loops. */
  int cur_loop_index = 0;
  MLoop *l = result->mloop;
  for (int fi : im->face_index_range()) {
    const Face *f = im->face(fi);
    const Mesh *orig_me;
    int index_in_orig_me;
    int orig_me_index;
    const MPoly *orig_mp = mim.input_mpoly_for_orig_index(
        f->orig, &orig_me, &orig_me_index, &index_in_orig_me);
    MPoly *mp = &result->mpoly[fi];
    mp->totloop = f->size();
    mp->loopstart = cur_loop_index;
    for (int j : f->index_range()) {
      const Vert *vf = f->vert[j];
      const int vfi = im->lookup_vert(vf);
      l->v = vfi;
      ++l;
      ++cur_loop_index;
    }
    copy_poly_attributes(result, mp, orig_mp, orig_me, fi, index_in_orig_me);
    copy_or_interp_loop_attributes(result, f, mp, orig_mp, orig_me, orig_me_index, mim);
  }

  /* BKE_mesh_calc_edges will calculate and populate all the
   * MEdges from the MPolys. */
  BKE_mesh_calc_edges(result, false, false);
  merge_edge_customdata_layers(result, mim);

  /* Now that the MEdges are populated, we can copy over the required attributes and custom layers.
   */
  for (int fi : im->face_index_range()) {
    const Face *f = im->face(fi);
    MPoly *mp = &result->mpoly[fi];
    for (int j : f->index_range()) {
      if (f->edge_orig[j] != NO_INDEX) {
        const Mesh *orig_me;
        int index_in_orig_me;
        const MEdge *orig_medge = mim.input_medge_for_orig_index(
            f->edge_orig[j], &orig_me, &index_in_orig_me);
        int e_index = result->mloop[mp->loopstart + j].e;
        MEdge *medge = &result->medge[e_index];
        copy_edge_attributes(result, medge, orig_medge, orig_me, e_index, index_in_orig_me);
      }
    }
  }

  BKE_mesh_calc_normals(result);
  if (dbg_level > 0) {
    BKE_mesh_validate(result, true, true);
  }
  return result;
}

/**
 * Do Exact Boolean directly, without a round trip through #BMesh.
 * The Mesh operands are in `meshes`, with corresponding transforms in in `obmats`.
 */
static Mesh *direct_mesh_boolean(Span<const Mesh *> meshes,
                                 Span<const float4x4 *> obmats,
                                 const bool use_self,
                                 const bool hole_tolerant,
                                 const BoolOpType boolean_mode)
{
  const int dbg_level = 0;
  BLI_assert(meshes.size() == obmats.size());
  const int meshes_len = meshes.size();
  if (meshes_len <= 0) {
    return nullptr;
  }
  if (dbg_level > 0) {
    std::cout << "\nDIRECT_MESH_INTERSECT, nmeshes = " << meshes_len << "\n";
  }
  MeshesToIMeshInfo mim;
  IMeshArena arena;
  IMesh m_in = meshes_to_imesh(meshes, obmats, arena, &mim);
  std::function<int(int)> shape_fn = [&mim](int f) {
    for (int mi = 0; mi < mim.mesh_poly_offset.size() - 1; ++mi) {
      if (f < mim.mesh_poly_offset[mi + 1]) {
        return mi;
      }
    }
    return static_cast<int>(mim.mesh_poly_offset.size()) - 1;
  };
  IMesh m_out = boolean_mesh(
      m_in, boolean_mode, meshes_len, shape_fn, use_self, hole_tolerant, nullptr, &arena);
  if (dbg_level > 1) {
    std::cout << m_out;
    write_obj_mesh(m_out, "m_out");
  }

  return imesh_to_mesh(&m_out, mim);
}

#endif  // WITH_GMP
}  // namespace blender::meshintersect

extern "C" {

#ifdef WITH_GMP
/* Do a mesh boolean directly on meshes (without going back and forth to BMesh).
 * The \a meshes argument is an array of \a meshes_len of Mesh pointers.
 * The \a obmats argument is an array of \a meshes_len of pointers to the obmat
 * matrices that transform local coordinates to global ones. It is allowed
 * for the pointers to be nullptr, meaning the transformation is the identity. */
Mesh *BKE_mesh_boolean(const Mesh **meshes,
                       const float (*obmats[])[4][4],
                       const int meshes_len,
                       const bool use_self,
                       const bool hole_tolerant,
                       const int boolean_mode)
{
  const blender::float4x4 **transforms = (const blender::float4x4 **)obmats;
  return blender::meshintersect::direct_mesh_boolean(
      blender::Span(meshes, meshes_len),
      blender::Span(transforms, meshes_len),
      use_self,
      hole_tolerant,
      static_cast<blender::meshintersect::BoolOpType>(boolean_mode));
}

#else
Mesh *BKE_mesh_boolean(const Mesh **UNUSED(meshes),
                       const float (*obmats[])[4][4],
                       const int UNUSED(meshes_len),
                       const bool UNUSED(use_self),
                       const bool UNUSED(hole_tolerant),
                       const int UNUSED(boolean_mode))
{
  UNUSED_VARS(obmats);
  return NULL;
}

#endif

}  // extern "C"
