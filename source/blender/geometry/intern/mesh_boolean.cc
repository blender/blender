/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <iostream>

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"

#include "BLI_alloca.h"
#include "BLI_array.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"
#include "BLI_mesh_boolean.hh"
#include "BLI_mesh_intersect.hh"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_task.hh"
#include "BLI_virtual_array.hh"

#include "DNA_node_types.h"

#include "GEO_mesh_boolean.hh"

#include "bmesh.hh"
#include "bmesh_tools.hh"
#include "tools/bmesh_boolean.hh"
#include "tools/bmesh_intersect.hh"

namespace blender::geometry::boolean {

/* -------------------------------------------------------------------- */
/** \name Mesh Arrangements (Old Exact Boolean)
 * \{ */

#ifdef WITH_GMP

constexpr int estimated_max_facelen = 100; /* Used for initial size of some Vectors. */

/* Snap entries that are near 0 or 1 or -1 to those values.
 * Sometimes Blender's rotation matrices for multiples of 90 degrees have
 * tiny numbers where there should be zeros. That messes makes some things
 * every so slightly non-coplanar when users expect coplanarity,
 * so this is a hack to clean up such matrices.
 * Would be better to change the transformation code itself.
 */
static float4x4 clean_transform(const float4x4 &mat)
{
  float4x4 cleaned;
  const float fuzz = 1e-6f;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      float f = mat[i][j];
      if (fabsf(f) <= fuzz) {
        f = 0.0f;
      }
      else if (fabsf(f - 1.0f) <= fuzz) {
        f = 1.0f;
      }
      else if (fabsf(f + 1.0f) <= fuzz) {
        f = -1.0f;
      }
      cleaned[i][j] = f;
    }
  }
  return cleaned;
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
  /* Similarly for faces of meshes. */
  Array<int> mesh_face_offset;
  /* For each Mesh vertex in all the meshes (with concatenated indexing),
   * what is the IMesh Vert* allocated for it in the input IMesh? */
  Array<const meshintersect::Vert *> mesh_to_imesh_vert;
  /* Similarly for each Mesh face. */
  Array<meshintersect::Face *> mesh_to_imesh_face;
  /* Transformation matrix to transform a coordinate in the corresponding
   * Mesh to the local space of the first Mesh. */
  Array<float4x4> to_target_transform;
  /* For each input mesh, whether or not their transform is negative. */
  Array<bool> has_negative_transform;
  /* For each input mesh, how to remap the material slot numbers to
   * the material slots in the first mesh. */
  Span<Array<short>> material_remaps;
  /* Total number of input mesh vertices. */
  int tot_meshes_verts;
  /* Total number of input mesh edges. */
  int tot_meshes_edges;
  /* Total number of input mesh polys. */
  int tot_meshes_polys;

  int input_mesh_for_imesh_vert(int imesh_v) const;
  int input_mesh_for_imesh_edge(int imesh_e) const;
  int input_mesh_for_imesh_face(int imesh_f) const;
  const IndexRange input_face_for_orig_index(int orig_index,
                                             const Mesh **r_orig_mesh,
                                             int *r_orig_mesh_index,
                                             int *r_index_in_orig_mesh) const;
  void input_mvert_for_orig_index(int orig_index,
                                  const Mesh **r_orig_mesh,
                                  int *r_index_in_orig_mesh) const;
  void input_medge_for_orig_index(int orig_index,
                                  const Mesh **r_orig_mesh,
                                  int *r_index_in_orig_mesh) const;
};

/* Given an index `imesh_v` in the `IMesh`, return the index of the
 * input `Mesh` that contained the vertex that it came from. */
int MeshesToIMeshInfo::input_mesh_for_imesh_vert(int imesh_v) const
{
  int n = int(mesh_vert_offset.size());
  for (int i = 0; i < n - 1; ++i) {
    if (imesh_v < mesh_vert_offset[i + 1]) {
      return i;
    }
  }
  return n - 1;
}

/* Given an index `imesh_e` used as an original index in the `IMesh`,
 * return the index of the input `Mesh` that contained the vertex that it came from. */
int MeshesToIMeshInfo::input_mesh_for_imesh_edge(int imesh_e) const
{
  int n = int(mesh_edge_offset.size());
  for (int i = 0; i < n - 1; ++i) {
    if (imesh_e < mesh_edge_offset[i + 1]) {
      return i;
    }
  }
  return n - 1;
}

/* Given an index `imesh_f` in the `IMesh`, return the index of the
 * input `Mesh` that contained the face that it came from. */
int MeshesToIMeshInfo::input_mesh_for_imesh_face(int imesh_f) const
{
  int n = int(mesh_face_offset.size());
  for (int i = 0; i < n - 1; ++i) {
    if (imesh_f < mesh_face_offset[i + 1]) {
      return i;
    }
  }
  return n - 1;
}

/* Given an index of an original face in the `IMesh`, find out the input
 * `Mesh` that it came from and return it in `*r_orig_mesh`,
 * and also return the index of that `Mesh` in  `*r_orig_mesh_index`.
 * Finally, return the index of the corresponding face in that `Mesh`
 * in `*r_index_in_orig_mesh`. */
const IndexRange MeshesToIMeshInfo::input_face_for_orig_index(int orig_index,
                                                              const Mesh **r_orig_mesh,
                                                              int *r_orig_mesh_index,
                                                              int *r_index_in_orig_mesh) const
{
  int orig_mesh_index = input_mesh_for_imesh_face(orig_index);
  BLI_assert(0 <= orig_mesh_index && orig_mesh_index < meshes.size());
  const Mesh *mesh = meshes[orig_mesh_index];
  const OffsetIndices faces = mesh->faces();
  int index_in_mesh = orig_index - mesh_face_offset[orig_mesh_index];
  BLI_assert(0 <= index_in_mesh && index_in_mesh < mesh->faces_num);
  const IndexRange face = faces[index_in_mesh];
  if (r_orig_mesh) {
    *r_orig_mesh = mesh;
  }
  if (r_orig_mesh_index) {
    *r_orig_mesh_index = orig_mesh_index;
  }
  if (r_index_in_orig_mesh) {
    *r_index_in_orig_mesh = index_in_mesh;
  }
  return face;
}

/* Given an index of an original vertex in the `IMesh`, find out the input
 * `Mesh` that it came from and return it in `*r_orig_mesh`.
 * Also find the index of the vertex in that `Mesh` and return it in
 * `*r_index_in_orig_mesh`. */
void MeshesToIMeshInfo::input_mvert_for_orig_index(int orig_index,
                                                   const Mesh **r_orig_mesh,
                                                   int *r_index_in_orig_mesh) const
{
  int orig_mesh_index = input_mesh_for_imesh_vert(orig_index);
  BLI_assert(0 <= orig_mesh_index && orig_mesh_index < meshes.size());
  const Mesh *mesh = meshes[orig_mesh_index];
  int index_in_mesh = orig_index - mesh_vert_offset[orig_mesh_index];
  BLI_assert(0 <= index_in_mesh && index_in_mesh < mesh->verts_num);
  if (r_orig_mesh) {
    *r_orig_mesh = mesh;
  }
  if (r_index_in_orig_mesh) {
    *r_index_in_orig_mesh = index_in_mesh;
  }
}

/* Similarly for edges. */
void MeshesToIMeshInfo::input_medge_for_orig_index(int orig_index,
                                                   const Mesh **r_orig_mesh,
                                                   int *r_index_in_orig_mesh) const
{
  int orig_mesh_index = input_mesh_for_imesh_edge(orig_index);
  BLI_assert(0 <= orig_mesh_index && orig_mesh_index < meshes.size());
  const Mesh *mesh = meshes[orig_mesh_index];
  int index_in_mesh = orig_index - mesh_edge_offset[orig_mesh_index];
  BLI_assert(0 <= index_in_mesh && index_in_mesh < mesh->edges_num);
  if (r_orig_mesh) {
    *r_orig_mesh = mesh;
  }
  if (r_index_in_orig_mesh) {
    *r_index_in_orig_mesh = index_in_mesh;
  }
}

/**
 * Convert all of the meshes in `meshes` to an `IMesh` and return that.
 * All of the coordinates are transformed into the local space of the
 * first Mesh. To do this transformation, we also need the transformation
 * obmats corresponding to the Meshes, so they are in the `obmats` argument.
 * The 'original' indexes in the IMesh are the indexes you get by
 * a scheme that offsets each vertex, edge, and face index by the sum of the
 * vertices, edges, and polys in the preceding Meshes in the mesh span.
 * The `*r_info class` is filled in with information needed to make the
 * correspondence between the Mesh MVerts/MPolys and the IMesh Verts/Faces.
 * All allocation of memory for the IMesh comes from `arena`.
 */
static meshintersect::IMesh meshes_to_imesh(Span<const Mesh *> meshes,
                                            Span<float4x4> obmats,
                                            Span<Array<short>> material_remaps,
                                            const float4x4 &target_transform,
                                            meshintersect::IMeshArena &arena,
                                            MeshesToIMeshInfo *r_info)
{
  int nmeshes = meshes.size();
  BLI_assert(nmeshes > 0);
  r_info->meshes = meshes;
  r_info->tot_meshes_verts = 0;
  r_info->tot_meshes_polys = 0;
  int &totvert = r_info->tot_meshes_verts;
  int &totedge = r_info->tot_meshes_edges;
  int &faces_num = r_info->tot_meshes_polys;
  for (const Mesh *mesh : meshes) {
    totvert += mesh->verts_num;
    totedge += mesh->edges_num;
    faces_num += mesh->faces_num;
  }

  /* Estimate the number of vertices and faces in the boolean output,
   * so that the memory arena can reserve some space. It is OK if these
   * estimates are wrong. */
  const int estimate_num_outv = 3 * totvert;
  const int estimate_num_outf = 4 * faces_num;
  arena.reserve(estimate_num_outv, estimate_num_outf);
  r_info->mesh_to_imesh_vert.reinitialize(totvert);
  r_info->mesh_to_imesh_face.reinitialize(faces_num);
  r_info->mesh_vert_offset.reinitialize(nmeshes);
  r_info->mesh_edge_offset.reinitialize(nmeshes);
  r_info->mesh_face_offset.reinitialize(nmeshes);
  r_info->to_target_transform.reinitialize(nmeshes);
  r_info->has_negative_transform.reinitialize(nmeshes);
  r_info->material_remaps = material_remaps;
  int v = 0;
  int e = 0;
  int f = 0;

  /* Put these Vectors here, with a size unlikely to need resizing,
   * so that the loop to make new Faces will likely not need to allocate
   * over and over. */
  Vector<const meshintersect::Vert *, estimated_max_facelen> face_vert;
  Vector<int, estimated_max_facelen> face_edge_orig;

  /* To convert the coordinates of meshes 1, 2, etc. into the local space
   * of the target, multiply each transform by the inverse of the
   * target matrix. Exact Boolean works better if these matrices are 'cleaned'
   *  -- see the comment for the `clean_transform` function, above. */
  const float4x4 inv_target_mat = math::invert(clean_transform(target_transform));

  /* For each input `Mesh`, make `Vert`s and `Face`s for the corresponding
   * vertices and polygons, and keep track of the original indices (using the
   * concatenating offset scheme) inside the `Vert`s and `Face`s.
   * When making `Face`s, we also put in the original indices for edges that
   * make up the polygons using the same scheme. */
  for (int mi : meshes.index_range()) {
    const Mesh *mesh = meshes[mi];
    r_info->mesh_vert_offset[mi] = v;
    r_info->mesh_edge_offset[mi] = e;
    r_info->mesh_face_offset[mi] = f;
    /* Get matrix that transforms a coordinate in meshes[mi]'s local space
     * to the target space. */
    const float4x4 objn_mat = obmats.is_empty() ? float4x4::identity() :
                                                  clean_transform(obmats[mi]);
    r_info->to_target_transform[mi] = inv_target_mat * objn_mat;
    r_info->has_negative_transform[mi] = math::is_negative(objn_mat);

    /* All meshes 1 and up will be transformed into the local space of operand 0.
     * Historical behavior of the modifier has been to flip the faces of any meshes
     * that would have a negative transform if you do that. */
    bool need_face_flip = r_info->has_negative_transform[mi] != r_info->has_negative_transform[0];

    Vector<meshintersect::Vert *> verts(mesh->verts_num);
    const Span<float3> vert_positions = mesh->vert_positions();
    const OffsetIndices faces = mesh->faces();
    const Span<int> corner_verts = mesh->corner_verts();
    const Span<int> corner_edges = mesh->corner_edges();

    /* Allocate verts
     * Skip the matrix multiplication for each point when there is no transform for a mesh,
     * for example when the first mesh is already in the target space. (Note the logic
     * directly above, which uses an identity matrix with an empty input transform). */
    if (obmats.is_empty() || r_info->to_target_transform[mi] == float4x4::identity()) {
      threading::parallel_for(vert_positions.index_range(), 2048, [&](IndexRange range) {
        for (int i : range) {
          float3 co = vert_positions[i];
          mpq3 mco = mpq3(co.x, co.y, co.z);
          double3 dco(mco[0].get_d(), mco[1].get_d(), mco[2].get_d());
          verts[i] = new meshintersect::Vert(mco, dco, meshintersect::NO_INDEX, i);
        }
      });
    }
    else {
      threading::parallel_for(vert_positions.index_range(), 2048, [&](IndexRange range) {
        for (int i : range) {
          float3 co = math::transform_point(r_info->to_target_transform[mi], vert_positions[i]);
          mpq3 mco = mpq3(co.x, co.y, co.z);
          double3 dco(mco[0].get_d(), mco[1].get_d(), mco[2].get_d());
          verts[i] = new meshintersect::Vert(mco, dco, meshintersect::NO_INDEX, i);
        }
      });
    }
    for (int i : vert_positions.index_range()) {
      r_info->mesh_to_imesh_vert[v] = arena.add_or_find_vert(verts[i]);
      ++v;
    }

    for (const int face_i : faces.index_range()) {
      const IndexRange face = faces[face_i];
      int flen = face.size();
      face_vert.resize(flen);
      face_edge_orig.resize(flen);
      for (int i = 0; i < flen; ++i) {
        const int corner_i = face[i];
        int mverti = r_info->mesh_vert_offset[mi] + corner_verts[corner_i];
        const meshintersect::Vert *fv = r_info->mesh_to_imesh_vert[mverti];
        if (need_face_flip) {
          face_vert[flen - i - 1] = fv;
          int iedge = i < flen - 1 ? flen - i - 2 : flen - 1;
          face_edge_orig[iedge] = e + corner_edges[corner_i];
        }
        else {
          face_vert[i] = fv;
          face_edge_orig[i] = e + corner_edges[corner_i];
        }
      }
      r_info->mesh_to_imesh_face[f] = arena.add_face(face_vert, f, face_edge_orig);
      ++f;
    }
    e += mesh->edges_num;
  }
  return meshintersect::IMesh(r_info->mesh_to_imesh_face);
}

/* Copy vertex attributes, including customdata, from `orig_mv` to `mv`.
 * `mv` is in `dest_mesh` with index `mv_index`.
 * The `orig_mv` vertex came from Mesh `orig_me` and had index `index_in_orig_me` there. */
static void copy_vert_attributes(Mesh *dest_mesh,
                                 const Mesh *orig_me,
                                 int mv_index,
                                 int index_in_orig_me)
{
  /* For all layers in the orig mesh, copy the layer information. */
  CustomData *target_cd = &dest_mesh->vert_data;
  const CustomData *source_cd = &orig_me->vert_data;
  for (int source_layer_i = 0; source_layer_i < source_cd->totlayer; ++source_layer_i) {
    const eCustomDataType ty = eCustomDataType(source_cd->layers[source_layer_i].type);
    if (StringRef(source_cd->layers->name) == "position") {
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

/* Similar to copy_vert_attributes but for face attributes. */
static void copy_face_attributes(Mesh *dest_mesh,
                                 const Mesh *orig_me,
                                 int face_index,
                                 int index_in_orig_me,
                                 Span<short> material_remap,
                                 MutableSpan<int> dst_material_indices)
{
  CustomData *target_cd = &dest_mesh->face_data;
  const CustomData *source_cd = &orig_me->face_data;
  for (int source_layer_i = 0; source_layer_i < source_cd->totlayer; ++source_layer_i) {
    const eCustomDataType ty = eCustomDataType(source_cd->layers[source_layer_i].type);
    const char *name = source_cd->layers[source_layer_i].name;
    int target_layer_i = CustomData_get_named_layer_index(target_cd, ty, name);
    if (target_layer_i != -1) {
      CustomData_copy_data_layer(
          source_cd, target_cd, source_layer_i, target_layer_i, index_in_orig_me, face_index, 1);
    }
  }

  /* Fix material indices after they have been transferred as a generic attribute. */
  const VArray<int> src_material_indices = *orig_me->attributes().lookup_or_default<int>(
      "material_index", bke::AttrDomain::Face, 0);
  const int src_index = src_material_indices[index_in_orig_me];
  if (material_remap.index_range().contains(src_index)) {
    const int remapped_index = material_remap[src_index];
    dst_material_indices[face_index] = remapped_index >= 0 ? remapped_index : src_index;
  }
  else {
    dst_material_indices[face_index] = src_index;
  }
  BLI_assert(dst_material_indices[face_index] >= 0);
}

/* Similar to copy_vert_attributes but for edge attributes. */
static void copy_edge_attributes(Mesh *dest_mesh,
                                 const Mesh *orig_me,
                                 int medge_index,
                                 int index_in_orig_me)
{
  CustomData *target_cd = &dest_mesh->edge_data;
  const CustomData *source_cd = &orig_me->edge_data;
  for (int source_layer_i = 0; source_layer_i < source_cd->totlayer; ++source_layer_i) {
    const eCustomDataType ty = eCustomDataType(source_cd->layers[source_layer_i].type);
    if (ty == CD_PROP_INT32_2D) {
      if (STREQ(source_cd->layers[source_layer_i].name, ".edge_verts")) {
        continue;
      }
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
 * For #IMesh face `f`, with corresponding output Mesh face `face`,
 * where the original Mesh face is `orig_face`, coming from the Mesh
 * `orig_me`, which has index `orig_me_index` in `mim`:
 * fill in the `orig_loops` Array with corresponding indices of MLoops from `orig_me`
 * where they have the same start and end vertices; for cases where that is
 * not true, put -1 in the `orig_loops` slot.
 * For now, we only try to do this if `face` and `orig_face` have the same size.
 * Return the number of non-null MLoops filled in.
 */
static int fill_orig_loops(const meshintersect::Face *f,
                           const IndexRange orig_face,
                           const Mesh *orig_me,
                           int orig_me_index,
                           MeshesToIMeshInfo &mim,
                           MutableSpan<int> r_orig_loops)
{
  r_orig_loops.fill(-1);
  const Span<int> orig_corner_verts = orig_me->corner_verts();

  int orig_mplen = orig_face.size();
  if (f->size() != orig_mplen) {
    return 0;
  }
  BLI_assert(r_orig_loops.size() == orig_mplen);
  /* We'll look for the case where the first vertex in f has an original vertex
   * that is the same as one in orig_me (after correcting for offset in mim meshes).
   * Then see that loop and any subsequent ones have the same start and end vertex.
   * This may miss some cases of partial alignment, but that's OK since discovering
   * aligned loops is only an optimization to avoid some re-interpolation.
   */
  int first_orig_v = f->vert[0]->orig;
  if (first_orig_v == meshintersect::NO_INDEX) {
    return 0;
  }
  /* It is possible that the original vert was merged with another in another mesh. */
  if (orig_me_index != mim.input_mesh_for_imesh_vert(first_orig_v)) {
    return 0;
  }
  int orig_me_vert_offset = mim.mesh_vert_offset[orig_me_index];
  int first_orig_v_in_orig_me = first_orig_v - orig_me_vert_offset;
  BLI_assert(0 <= first_orig_v_in_orig_me && first_orig_v_in_orig_me < orig_me->verts_num);
  /* Assume all vertices in each face is unique. */
  int offset = -1;
  for (int i = 0; i < orig_mplen; ++i) {
    int loop_i = i + orig_face.start();
    if (orig_corner_verts[loop_i] == first_orig_v_in_orig_me) {
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
    const int vert_i = orig_corner_verts[orig_face.start() + orig_mp_loop_index];
    int fv_orig = f->vert[mp_loop_index]->orig;
    if (fv_orig != meshintersect::NO_INDEX) {
      fv_orig -= orig_me_vert_offset;
      if (fv_orig < 0 || fv_orig >= orig_me->verts_num) {
        fv_orig = meshintersect::NO_INDEX;
      }
    }
    if (vert_i == fv_orig) {
      const int vert_next =
          orig_corner_verts[orig_face.start() + ((orig_mp_loop_index + 1) % orig_mplen)];
      int fvnext_orig = f->vert[(mp_loop_index + 1) % orig_mplen]->orig;
      if (fvnext_orig != meshintersect::NO_INDEX) {
        fvnext_orig -= orig_me_vert_offset;
        if (fvnext_orig < 0 || fvnext_orig >= orig_me->verts_num) {
          fvnext_orig = meshintersect::NO_INDEX;
        }
      }
      if (vert_next == fvnext_orig) {
        r_orig_loops[mp_loop_index] = orig_face.start() + orig_mp_loop_index;
        ++num_orig_loops_found;
      }
    }
  }
  return num_orig_loops_found;
}

/* Fill `cos_2d` with the 2d coordinates found by projection face `face` along
 * its normal. Also fill in r_axis_mat with the matrix that does that projection.
 * But before projecting, also transform the 3d coordinate by multiplying by trans_mat.
 * `cos_2d` should have room for `face.size()` entries. */
static void get_poly2d_cos(const Mesh *mesh,
                           const IndexRange face,
                           float (*cos_2d)[2],
                           const float4x4 &trans_mat,
                           float r_axis_mat[3][3])
{
  const Span<float3> positions = mesh->vert_positions();
  const Span<int> corner_verts = mesh->corner_verts();
  const Span<int> face_verts = corner_verts.slice(face);

  /* Project coordinates to 2d in cos_2d, using normal as projection axis. */
  const float3 axis_dominant = bke::mesh::face_normal_calc(positions, face_verts);
  axis_dominant_v3_to_m3(r_axis_mat, axis_dominant);
  for (const int i : face_verts.index_range()) {
    float3 co = positions[face_verts[i]];
    co = math::transform_point(trans_mat, co);
    *reinterpret_cast<float2 *>(&cos_2d[i]) = (float3x3(r_axis_mat) * co).xy();
  }
}

/* For the loops of `face`, see if the face is unchanged from `orig_face`, and if so,
 * copy the Loop attributes from corresponding loops to corresponding loops.
 * Otherwise, interpolate the Loop attributes in the face `orig_face`. */
static void copy_or_interp_loop_attributes(Mesh *dest_mesh,
                                           const meshintersect::Face *f,
                                           const IndexRange face,
                                           const IndexRange orig_face,
                                           const Mesh *orig_me,
                                           int orig_me_index,
                                           MeshesToIMeshInfo &mim)
{
  Array<int> orig_loops(face.size());
  int norig = fill_orig_loops(f, orig_face, orig_me, orig_me_index, mim, orig_loops);
  /* We may need these arrays if we have to interpolate Loop attributes rather than just copy.
   * Right now, trying Array<float[2]> complains, so declare cos_2d a different way. */
  float(*cos_2d)[2];
  Array<float> weights;
  Array<const void *> src_blocks_ofs;
  float axis_mat[3][3];
  if (norig != face.size()) {
    /* We will need to interpolate. Make `cos_2d` hold 2d-projected coordinates of `orig_face`,
     * which are transformed into object 0's local space before projecting.
     * At this point we cannot yet calculate the interpolation weights, as they depend on
     * the coordinate where interpolation is to happen, but we can allocate the needed arrays,
     * so they don't have to be allocated per-layer. */
    cos_2d = (float(*)[2])BLI_array_alloca(cos_2d, orig_face.size());
    weights = Array<float>(orig_face.size());
    src_blocks_ofs = Array<const void *>(orig_face.size());
    get_poly2d_cos(orig_me, orig_face, cos_2d, mim.to_target_transform[orig_me_index], axis_mat);
  }
  CustomData *target_cd = &dest_mesh->corner_data;
  const Span<float3> dst_positions = dest_mesh->vert_positions();
  const Span<int> dst_corner_verts = dest_mesh->corner_verts();
  for (int i = 0; i < face.size(); ++i) {
    int loop_index = face[i];
    int orig_loop_index = norig > 0 ? orig_loops[i] : -1;
    const CustomData *source_cd = &orig_me->corner_data;
    if (orig_loop_index == -1) {
      /* Will need interpolation weights for this loop's vertex's coordinates.
       * The coordinate needs to be projected into 2d,  just like the interpolating face's
       * coordinates were. The `dest_mesh` coordinates are already in object 0 local space. */
      float co[2];
      mul_v2_m3v3(co, axis_mat, dst_positions[dst_corner_verts[loop_index]]);
      interp_weights_poly_v2(weights.data(), cos_2d, orig_face.size(), co);
    }
    for (int source_layer_i = 0; source_layer_i < source_cd->totlayer; ++source_layer_i) {
      const eCustomDataType ty = eCustomDataType(source_cd->layers[source_layer_i].type);
      if (STR_ELEM(source_cd->layers[source_layer_i].name, ".corner_vert", ".corner_edge")) {
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
        /* NOTE: although CustomData_bmesh_interp_n function has bmesh in its name, nothing about
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
        const int size = CustomData_sizeof(ty);
        for (int j = 0; j < orig_face.size(); ++j) {
          const void *layer = CustomData_get_layer_n(source_cd, ty, source_layer_type_index);
          src_blocks_ofs[j] = POINTER_OFFSET(layer, size * (orig_face[j]));
        }
        void *dst_layer = CustomData_get_layer_n_for_write(
            target_cd, ty, target_layer_type_index, dest_mesh->corners_num);
        void *dst_block_ofs = POINTER_OFFSET(dst_layer, size * loop_index);
        CustomData_bmesh_interp_n(target_cd,
                                  src_blocks_ofs.data(),
                                  weights.data(),
                                  nullptr,
                                  orig_face.size(),
                                  dst_block_ofs,
                                  target_layer_i);
      }
    }
  }
}

/**
 * Make sure that there are custom data layers in the target mesh
 * corresponding to all target layers in all of the operands after the first.
 * (The target should already have layers for those in the first operand mesh).
 * Edges done separately -- will have to be done later, after edges are made.
 */
static void merge_vertex_loop_face_customdata_layers(Mesh *target, MeshesToIMeshInfo &mim)
{
  for (int mesh_index = 1; mesh_index < mim.meshes.size(); ++mesh_index) {
    const Mesh *mesh = mim.meshes[mesh_index];
    if (mesh->verts_num) {
      CustomData_merge_layout(&mesh->vert_data,
                              &target->vert_data,
                              CD_MASK_MESH.vmask,
                              CD_SET_DEFAULT,
                              target->verts_num);
    }
    if (mesh->corners_num) {
      CustomData_merge_layout(&mesh->corner_data,
                              &target->corner_data,
                              CD_MASK_MESH.lmask,
                              CD_SET_DEFAULT,
                              target->corners_num);
    }
    if (mesh->faces_num) {
      CustomData_merge_layout(&mesh->face_data,
                              &target->face_data,
                              CD_MASK_MESH.pmask,
                              CD_SET_DEFAULT,
                              target->faces_num);
    }
  }
}

static void merge_edge_customdata_layers(Mesh *target, MeshesToIMeshInfo &mim)
{
  for (int mesh_index = 0; mesh_index < mim.meshes.size(); ++mesh_index) {
    const Mesh *mesh = mim.meshes[mesh_index];
    if (mesh->edges_num) {
      CustomData_merge_layout(&mesh->edge_data,
                              &target->edge_data,
                              CD_MASK_MESH.emask,
                              CD_SET_DEFAULT,
                              target->edges_num);
    }
  }
}

/**
 * Convert the output IMesh im to a Blender Mesh,
 * using the information in mim to get all the attributes right.
 */
static Mesh *imesh_to_mesh(meshintersect::IMesh *im, MeshesToIMeshInfo &mim)
{
  constexpr int dbg_level = 0;

  im->populate_vert();
  int out_totvert = im->vert_size();
  int out_faces_num = im->face_size();
  int out_totloop = 0;
  for (const meshintersect::Face *f : im->faces()) {
    out_totloop += f->size();
  }
  /* Will calculate edges later. */
  Mesh *result = BKE_mesh_new_nomain_from_template(
      mim.meshes[0], out_totvert, 0, out_faces_num, out_totloop);

  merge_vertex_loop_face_customdata_layers(result, mim);
  /* Set the vertex coordinate values and other data. */
  MutableSpan<float3> positions = result->vert_positions_for_write();
  for (int vi : im->vert_index_range()) {
    const meshintersect::Vert *v = im->vert(vi);
    if (v->orig != meshintersect::NO_INDEX) {
      const Mesh *orig_me;
      int index_in_orig_me;
      mim.input_mvert_for_orig_index(v->orig, &orig_me, &index_in_orig_me);
      copy_vert_attributes(result, orig_me, vi, index_in_orig_me);
    }
    copy_v3fl_v3db(positions[vi], v->co);
  }

  /* Set the loop-start and total-loops for each output face,
   * and set the vertices in the appropriate loops. */
  bke::SpanAttributeWriter<int> dst_material_indices =
      result->attributes_for_write().lookup_or_add_for_write_only_span<int>("material_index",
                                                                            bke::AttrDomain::Face);
  int cur_loop_index = 0;
  MutableSpan<int> dst_corner_verts = result->corner_verts_for_write();
  MutableSpan<int> dst_face_offsets = result->face_offsets_for_write();
  for (int fi : im->face_index_range()) {
    const meshintersect::Face *f = im->face(fi);
    const Mesh *orig_me;
    int index_in_orig_me;
    int orig_me_index;
    const IndexRange orig_face = mim.input_face_for_orig_index(
        f->orig, &orig_me, &orig_me_index, &index_in_orig_me);
    dst_face_offsets[fi] = cur_loop_index;
    for (int j : f->index_range()) {
      const meshintersect::Vert *vf = f->vert[j];
      const int vfi = im->lookup_vert(vf);
      dst_corner_verts[cur_loop_index] = vfi;
      ++cur_loop_index;
    }

    copy_face_attributes(result,
                         orig_me,
                         fi,
                         index_in_orig_me,
                         (mim.material_remaps.size() > 0) ?
                             mim.material_remaps[orig_me_index].as_span() :
                             Span<short>(),
                         dst_material_indices.span);
    copy_or_interp_loop_attributes(result,
                                   f,
                                   IndexRange(dst_face_offsets[fi], f->size()),
                                   orig_face,
                                   orig_me,
                                   orig_me_index,
                                   mim);
  }
  dst_material_indices.finish();

  bke::mesh_calc_edges(*result, false, false);
  merge_edge_customdata_layers(result, mim);

  /* Now that the MEdges are populated, we can copy over the required attributes and custom layers.
   */
  const OffsetIndices dst_polys = result->faces();
  const Span<int> dst_corner_edges = result->corner_edges();
  for (int fi : im->face_index_range()) {
    const meshintersect::Face *f = im->face(fi);
    const IndexRange face = dst_polys[fi];
    for (int j : f->index_range()) {
      if (f->edge_orig[j] != meshintersect::NO_INDEX) {
        const Mesh *orig_me;
        int index_in_orig_me;
        mim.input_medge_for_orig_index(f->edge_orig[j], &orig_me, &index_in_orig_me);
        int e_index = dst_corner_edges[face[j]];
        copy_edge_attributes(result, orig_me, e_index, index_in_orig_me);
      }
    }
  }

  if (dbg_level > 0) {
    BKE_mesh_validate(result, true, true);
  }
  return result;
}

static meshintersect::BoolOpType operation_to_mesh_arr_mode(const Operation operation)
{
  switch (operation) {
    case Operation::Intersect:
      return meshintersect::BoolOpType::Intersect;
    case Operation::Union:
      return meshintersect::BoolOpType::Union;
    case Operation::Difference:
      return meshintersect::BoolOpType::Difference;
  }
  BLI_assert_unreachable();
  return meshintersect::BoolOpType::None;
}

static Mesh *mesh_boolean_mesh_arr(Span<const Mesh *> meshes,
                                   Span<float4x4> transforms,
                                   const float4x4 &target_transform,
                                   Span<Array<short>> material_remaps,
                                   const bool use_self,
                                   const bool hole_tolerant,
                                   const meshintersect::BoolOpType boolean_mode,
                                   Vector<int> *r_intersecting_edges)
{
  BLI_assert(transforms.is_empty() || meshes.size() == transforms.size());
  BLI_assert(material_remaps.is_empty() || material_remaps.size() == meshes.size());
  if (meshes.size() <= 0) {
    return nullptr;
  }

  const int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "\nOLD_MESH_INTERSECT, nmeshes = " << meshes.size() << "\n";
  }
  MeshesToIMeshInfo mim;
  meshintersect::IMeshArena arena;
  meshintersect::IMesh m_in = meshes_to_imesh(
      meshes, transforms, material_remaps, target_transform, arena, &mim);
  std::function<int(int)> shape_fn = [&mim](int f) {
    for (int mi = 0; mi < mim.mesh_face_offset.size() - 1; ++mi) {
      if (f < mim.mesh_face_offset[mi + 1]) {
        return mi;
      }
    }
    return int(mim.mesh_face_offset.size()) - 1;
  };
  meshintersect::IMesh m_out = boolean_mesh(
      m_in, boolean_mode, meshes.size(), shape_fn, use_self, hole_tolerant, nullptr, &arena);
  if (dbg_level > 0) {
    std::cout << m_out;
    write_obj_mesh(m_out, "m_out");
  }

  Mesh *result = imesh_to_mesh(&m_out, mim);

  /* Store intersecting edge indices. */
  if (r_intersecting_edges != nullptr) {
    const OffsetIndices faces = result->faces();
    const Span<int> corner_edges = result->corner_edges();
    for (int fi : m_out.face_index_range()) {
      const meshintersect::Face &face = *m_out.face(fi);
      const IndexRange mesh_face = faces[fi];
      for (int i : face.index_range()) {
        if (face.is_intersect[i]) {
          int e_index = corner_edges[mesh_face[i]];
          r_intersecting_edges->append(e_index);
        }
      }
    }
  }

  return result;
}

#endif  // WITH_GMP

/** \} */

/* -------------------------------------------------------------------- */
/** \name Float Boolean
 * \{ */

/* has no meaning for faces, do this so we can tell which face is which */
#define BM_FACE_TAG BM_ELEM_DRAW

/**
 *  Function use to say what operand a face is part of, based on the `BM_FACE_TAG`,`
 *  which is set in `bm_mesh_create`.
 */
static int face_boolean_operand(BMFace *f, void * /*user_data*/)
{
  return BM_elem_flag_test(f, BM_FACE_TAG) ? 0 : 1;
}

/* Create a BMesh that is the concatenation of the given meshes.
 * The corresponding mesh-to-world transformations are also given,
 * as well as a target_tranform.
 * A triangulation is also calculated and returned in the last two
 * parameters.
 * The faces of the first mesh are tagged with BM_FACE_TAG so that the
 * face_boolean_operand() function can distinguish those faces from the
 * rest.
 * The caller is responsible for using `BM_mesh_free` on the returned
 * BMesh, and calling `MEM_freeN` on the returned looptris.
 *
 * TODO: maybe figure out how to use the join_geometries() function
 * to join all the meshes into one mesh first, and then convert
 * that single mesh to BMesh. Issues with that include needing
 * to apply the transforms and material remaps.
 */
static BMesh *mesh_bm_concat(Span<const Mesh *> meshes,
                             Span<float4x4> transforms,
                             const float4x4 &target_transform,
                             Span<Array<short>> material_remaps,
                             Array<std::array<BMLoop *, 3>> &r_looptris)
{
  const int meshes_num = meshes.size();
  BLI_assert(meshes_num >= 1);
  bool ok;
  float4x4 inv_target_mat = math::invert(target_transform, ok);
  if (!ok) {
    BLI_assert_unreachable();
    inv_target_mat = float4x4::identity();
  }
  Array<float4x4> to_target(meshes_num);
  Array<bool> is_negative_transform(meshes_num);
  Array<bool> is_flip(meshes_num);
  const int tsize = transforms.size();
  for (const int i : IndexRange(meshes_num)) {
    if (tsize > i) {
      to_target[i] = inv_target_mat * transforms[i];
      is_negative_transform[i] = math::is_negative(transforms[i]);
      is_flip[i] = is_negative_transform[i] != is_negative_transform[0];
    }
    else {
      to_target[i] = inv_target_mat;
      is_negative_transform[i] = false;
      is_flip[i] = false;
    }
  }

  /* Make a BMesh that will be a concatenation of the elements of all the meshes */
  BMAllocTemplate allocsize;
  allocsize.totvert = 0;
  allocsize.totedge = 0;
  allocsize.totloop = 0;
  allocsize.totface = 0;
  for (const int i : meshes.index_range()) {
    allocsize.totvert += meshes[i]->verts_num;
    allocsize.totedge += meshes[i]->edges_num;
    allocsize.totloop += meshes[i]->corners_num;
    allocsize.totface += meshes[i]->faces_num;
  }

  BMeshCreateParams bmesh_create_params{};
  BMesh *bm = BM_mesh_create(&allocsize, &bmesh_create_params);

  BM_mesh_copy_init_customdata_from_mesh_array(
      bm, const_cast<const Mesh **>(meshes.begin()), meshes_num, &allocsize);

  BMeshFromMeshParams bmesh_from_mesh_params{};
  bmesh_from_mesh_params.calc_face_normal = true;
  bmesh_from_mesh_params.calc_vert_normal = true;

  Array<int> verts_end(meshes_num);
  Array<int> faces_end(meshes_num);
  verts_end[0] = meshes[0]->verts_num;
  faces_end[0] = meshes[0]->faces_num;
  for (const int i : meshes.index_range()) {
    /* Append meshes[i] elements and data to bm. */
    BM_mesh_bm_from_me(bm, meshes[i], &bmesh_from_mesh_params);
    if (i > 0) {
      verts_end[i] = verts_end[i - 1] + meshes[i]->verts_num;
      faces_end[i] = faces_end[i - 1] + meshes[i]->faces_num;
      if (is_flip[i]) {
        /* Need to flip face normals to match that of mesh[0]. */
        const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);
        BM_mesh_elem_table_ensure(bm, BM_FACE);
        for (int j = faces_end[i - 1]; j < faces_end[i]; j++) {
          BMFace *efa = bm->ftable[j];
          BM_face_normal_flip_ex(bm, efa, cd_loop_mdisp_offset, true);
        }
      }
    }
  }

  /* Make a triangulation of all polys before transforming vertices
   * so we can use the original normals. */
  const int looptris_tot = poly_to_tri_count(bm->totface, bm->totloop);
  r_looptris.reinitialize(looptris_tot);
  BM_mesh_calc_tessellation_beauty(bm, r_looptris);

  /* Transform the vertices that into the desired target_transform space. */
  BMIter iter;
  BMVert *eve;
  int i = 0;
  int mesh_index = 0;
  BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
    copy_v3_v3(eve->co, math::transform_point(to_target[mesh_index], float3(eve->co)));
    ++i;
    if (i == verts_end[mesh_index]) {
      mesh_index++;
    }
  }

  /* Transform face normals and tag the first-operand faces.
   * Also, apply material remaps. */
  BMFace *efa;
  i = 0;
  mesh_index = 0;
  BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
    copy_v3_v3(efa->no, math::transform_direction(to_target[mesh_index], float3(efa->no)));
    if (is_negative_transform[mesh_index]) {
      negate_v3(efa->no);
    }
    normalize_v3(efa->no);

    /* Temp tag used in `face_boolean_operand()` to test for operand 0. */
    if (i < faces_end[0]) {
      BM_elem_flag_enable(efa, BM_FACE_TAG);
    }

    /* Remap material. */
    int cur_mat = efa->mat_nr;
    if (cur_mat < material_remaps[mesh_index].size()) {
      int new_mat = material_remaps[mesh_index][cur_mat];
      if (new_mat >= 0) {
        efa->mat_nr = material_remaps[mesh_index][cur_mat];
      }
    }

    ++i;
    if (i == faces_end[mesh_index]) {
      mesh_index++;
    }
  }

  return bm;
}

static int operation_to_float_mode(const Operation operation)
{
  switch (operation) {
    case Operation::Intersect:
      return BMESH_ISECT_BOOLEAN_ISECT;
    case Operation::Union:
      return BMESH_ISECT_BOOLEAN_UNION;
    case Operation::Difference:
      return BMESH_ISECT_BOOLEAN_DIFFERENCE;
  }
  BLI_assert_unreachable();
  return BMESH_ISECT_BOOLEAN_NONE;
}

static Mesh *mesh_boolean_float(Span<const Mesh *> meshes,
                                Span<float4x4> transforms,
                                const float4x4 &target_transform,
                                Span<Array<short>> material_remaps,
                                const int boolean_mode,
                                Vector<int> * /*r_intersecting_edges*/)
{
  BLI_assert(meshes.size() == transforms.size() || transforms.size() == 0);
  BLI_assert(material_remaps.size() == 0 || material_remaps.size() == meshes.size());
  if (meshes.is_empty()) {
    return nullptr;
  }

  if (meshes.size() == 1) {
    /* The float solver doesn't do self union. Just return nullptr, which will
     * cause geometry nodes to leave the input as is. */
    return BKE_mesh_copy_for_eval(meshes[0]);
  }

  Array<std::array<BMLoop *, 3>> looptris;
  if (meshes.size() == 2) {
    BMesh *bm = mesh_bm_concat(meshes, transforms, target_transform, material_remaps, looptris);
    BM_mesh_intersect(bm,
                      looptris,
                      face_boolean_operand,
                      nullptr,
                      false,
                      false,
                      true,
                      true,
                      false,
                      false,
                      boolean_mode,
                      1e-6f);
    Mesh *result = BKE_mesh_from_bmesh_for_eval_nomain(bm, nullptr, meshes[0]);
    BM_mesh_free(bm);
    return result;
  }

  /* Iteratively operate with each operand. */
  Array<const Mesh *> two_meshes = {meshes[0], meshes[1]};
  Array<float4x4> two_transforms = {transforms[0], transforms[1]};
  Array<Array<short>> two_remaps = {material_remaps[0], material_remaps[1]};
  Mesh *prev_result_mesh = nullptr;
  for (const int i : meshes.index_range().drop_back(1)) {
    BMesh *bm = mesh_bm_concat(
        two_meshes, two_transforms, float4x4::identity(), two_remaps, looptris);
    BM_mesh_intersect(bm,
                      looptris,
                      face_boolean_operand,
                      nullptr,
                      false,
                      false,
                      true,
                      true,
                      false,
                      false,
                      boolean_mode,
                      1e-6f);
    Mesh *result_i_mesh = BKE_mesh_from_bmesh_for_eval_nomain(bm, nullptr, meshes[0]);
    BM_mesh_free(bm);
    if (prev_result_mesh != nullptr) {
      /* Except in the first iteration, two_meshes[0] holds the intermediate
       * mesh result from the previous iteration. */
      BKE_id_free(nullptr, prev_result_mesh);
    }
    if (i < meshes.size() - 2) {
      two_meshes[0] = result_i_mesh;
      two_meshes[1] = meshes[i + 2];
      two_transforms[0] = float4x4::identity();
      two_transforms[1] = transforms[i + 2];
      two_remaps[0] = {};
      two_remaps[1] = material_remaps[i + 2];
      prev_result_mesh = result_i_mesh;
    }
    else {
      return result_i_mesh;
    }
  }

  BLI_assert_unreachable();
  return nullptr;
}

/** \} */

Mesh *mesh_boolean(Span<const Mesh *> meshes,
                   Span<float4x4> transforms,
                   const float4x4 &target_transform,
                   Span<Array<short>> material_remaps,
                   BooleanOpParameters op_params,
                   Solver solver,
                   Vector<int> *r_intersecting_edges)
{

  switch (solver) {
    case Solver::Float:
      return mesh_boolean_float(meshes,
                                transforms,
                                target_transform,
                                material_remaps,
                                operation_to_float_mode(op_params.boolean_mode),
                                r_intersecting_edges);
    case Solver::MeshArr:
#ifdef WITH_GMP
      return mesh_boolean_mesh_arr(meshes,
                                   transforms,
                                   target_transform,
                                   material_remaps,
                                   !op_params.no_self_intersections,
                                   !op_params.watertight,
                                   operation_to_mesh_arr_mode(op_params.boolean_mode),
                                   r_intersecting_edges);
#else
      return nullptr;
#endif
    default:
      BLI_assert_unreachable();
  }
  return nullptr;
}

}  // namespace blender::geometry::boolean
