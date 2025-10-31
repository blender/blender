/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <iostream>

#include "BKE_attribute.hh"
#include "BKE_attribute_filters.hh"
#include "BKE_attribute_math.hh"
#include "BKE_geometry_set.hh"
#include "BKE_instances.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"

#include "BLI_array.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"
#include "BLI_mesh_boolean.hh"
#include "BLI_mesh_intersect.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"
#include "BLI_virtual_array.hh"

#include "GEO_mesh_boolean.hh"
#include "mesh_boolean_intern.hh"
#include "mesh_boolean_manifold.hh"

#include "bmesh.hh"
#include "tools/bmesh_intersect.hh"

// #define BENCHMARK_TIME
#ifdef BENCHMARK_TIME
#  include "BLI_threads.h"
#  include "BLI_timeit.hh"
#  include <filesystem>
#  include <fstream>
#  define BENCHMARK_FILE "/tmp/blender_benchmark.csv"
#endif

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

static float3 clean_float3(const float3 &co)
{
  float3 cleaned = co;
  if (UNLIKELY(!isfinite(co[0]))) {
    cleaned[0] = 0.0f;
  }
  if (UNLIKELY(!isfinite(co[1]))) {
    cleaned[1] = 0.0f;
  }
  if (UNLIKELY(!isfinite(co[2]))) {
    cleaned[2] = 0.0f;
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
  MeshOffsets mesh_offsets;
  /** All input meshes joined together. */
  const Mesh *joined_mesh;
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

  MeshesToIMeshInfo(Span<const Mesh *> meshes) : mesh_offsets(meshes) {}

  int input_mesh_for_imesh_vert(int imesh_v) const;
  int input_mesh_for_imesh_edge(int imesh_e) const;
  int input_mesh_for_imesh_face(int imesh_f) const;
};

/* Given an index `imesh_v` in the `IMesh`, return the index of the
 * input `Mesh` that contained the vertex that it came from. */
int MeshesToIMeshInfo::input_mesh_for_imesh_vert(int imesh_v) const
{
  const Span<int> offsets = this->mesh_offsets.vert_offsets.data();
  int n = int(offsets.size());
  for (int i = 0; i < n - 1; ++i) {
    if (imesh_v < offsets[i + 1]) {
      return i;
    }
  }
  return n - 1;
}

/* Given an index `imesh_e` used as an original index in the `IMesh`,
 * return the index of the input `Mesh` that contained the vertex that it came from. */
int MeshesToIMeshInfo::input_mesh_for_imesh_edge(int imesh_e) const
{
  const Span<int> offsets = this->mesh_offsets.edge_offsets.data();
  int n = int(offsets.size());
  for (int i = 0; i < n - 1; ++i) {
    if (imesh_e < offsets[i + 1]) {
      return i;
    }
  }
  return n - 1;
}

/* Given an index `imesh_f` in the `IMesh`, return the index of the
 * input `Mesh` that contained the face that it came from. */
int MeshesToIMeshInfo::input_mesh_for_imesh_face(int imesh_f) const
{
  const Span<int> offsets = this->mesh_offsets.face_offsets.data();
  int n = int(offsets.size());
  for (int i = 0; i < n - 1; ++i) {
    if (imesh_f < offsets[i + 1]) {
      return i;
    }
  }
  return n - 1;
}

/**
 * Convert all of the meshes in `meshes` to an `IMesh` and return that.
 * All of the coordinates are transformed into the local space of the
 * first Mesh. To do this transformation, we also need the transformation
 * object matrices corresponding to the Meshes, so they are in the `transforms` argument.
 * The 'original' indexes in the IMesh are the indexes you get by
 * a scheme that offsets each vertex, edge, and face index by the sum of the
 * vertices, edges, and polys in the preceding Meshes in the mesh span.
 * The `*r_info class` is filled in with information needed to make the
 * correspondence between the Mesh MVerts/MPolys and the IMesh Verts/Faces.
 * All allocation of memory for the IMesh comes from `arena`.
 */
static meshintersect::IMesh meshes_to_imesh(Span<const Mesh *> meshes,
                                            Span<float4x4> transforms,
                                            Span<Array<short>> material_remaps,
                                            meshintersect::IMeshArena &arena,
                                            MeshesToIMeshInfo *r_info)
{
  int nmeshes = meshes.size();
  BLI_assert(nmeshes > 0);
  r_info->meshes = meshes;
  const int totvert = r_info->mesh_offsets.vert_offsets.total_size();
  const int faces_num = r_info->mesh_offsets.face_offsets.total_size();

  /* Estimate the number of vertices and faces in the boolean output,
   * so that the memory arena can reserve some space. It is OK if these
   * estimates are wrong. */
  const int estimate_num_outv = 3 * totvert;
  const int estimate_num_outf = 4 * faces_num;
  arena.reserve(estimate_num_outv, estimate_num_outf);
  r_info->mesh_to_imesh_vert.reinitialize(totvert);
  r_info->mesh_to_imesh_face.reinitialize(faces_num);
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

  /* For each input `Mesh`, make `Vert`s and `Face`s for the corresponding
   * vertices and polygons, and keep track of the original indices (using the
   * concatenating offset scheme) inside the `Vert`s and `Face`s.
   * When making `Face`s, we also put in the original indices for edges that
   * make up the polygons using the same scheme. */
  for (int mi : meshes.index_range()) {
    const Mesh *mesh = meshes[mi];
    /* Get matrix that transforms a coordinate in meshes[mi]'s local space
     * to the target space. */
    r_info->to_target_transform[mi] = transforms.is_empty() ? float4x4::identity() :
                                                              clean_transform(transforms[mi]);
    r_info->has_negative_transform[mi] = math::is_negative(r_info->to_target_transform[mi]);

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
    if (transforms.is_empty() || r_info->to_target_transform[mi] == float4x4::identity()) {
      threading::parallel_for(vert_positions.index_range(), 2048, [&](IndexRange range) {
        for (int i : range) {
          float3 co = clean_float3(vert_positions[i]);
          mpq3 mco = mpq3(co.x, co.y, co.z);
          double3 dco(mco[0].get_d(), mco[1].get_d(), mco[2].get_d());
          verts[i] = new meshintersect::Vert(mco, dco, meshintersect::NO_INDEX, i);
        }
      });
    }
    else {
      threading::parallel_for(vert_positions.index_range(), 2048, [&](IndexRange range) {
        for (int i : range) {
          float3 co = math::transform_point(r_info->to_target_transform[mi],
                                            clean_float3(vert_positions[i]));
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
        int mverti = r_info->mesh_offsets.vert_offsets[mi][corner_verts[corner_i]];
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
                           const Span<int> orig_corner_verts,
                           int orig_me_index,
                           MeshesToIMeshInfo &mim,
                           MutableSpan<int> r_orig_loops)
{
  r_orig_loops.fill(-1);
  const IndexRange orig_mesh_verts_range = mim.mesh_offsets.vert_offsets[orig_me_index];

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
  /* Assume all vertices in each face is unique. */
  int offset = -1;
  for (int i = 0; i < orig_mplen; ++i) {
    int loop_i = i + orig_face.start();
    if (orig_corner_verts[loop_i] == first_orig_v) {
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
      if (!orig_mesh_verts_range.contains(fv_orig)) {
        fv_orig = meshintersect::NO_INDEX;
      }
    }
    if (vert_i == fv_orig) {
      const int vert_next =
          orig_corner_verts[orig_face.start() + ((orig_mp_loop_index + 1) % orig_mplen)];
      int fvnext_orig = f->vert[(mp_loop_index + 1) % orig_mplen]->orig;
      if (fvnext_orig != meshintersect::NO_INDEX) {
        if (!orig_mesh_verts_range.contains(fvnext_orig)) {
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

static int mesh_id_for_face(const int face_id, const MeshOffsets &mesh_offsets)
{
  const OffsetIndices<int> offsets = mesh_offsets.face_offsets;
  for (const int mesh_id : offsets.index_range()) {
    if (offsets[mesh_id].contains(face_id)) {
      return mesh_id;
    }
  }
  return -1;
}

/* For the loops of `face`, see if the face is unchanged from `orig_face`, and if so,
 * copy the Loop attributes from corresponding loops to corresponding loops.
 * Otherwise, interpolate the Loop attributes in the face `orig_face`. */
static void copy_or_interp_loop_attributes(const meshintersect::IMesh *im,
                                           Mesh *dest_mesh,
                                           const Span<int> dst_to_src_face_map,
                                           MeshesToIMeshInfo &mim)
{
  const OffsetIndices<int> src_faces = mim.joined_mesh->faces();
  const Span<int> orig_corner_verts = mim.joined_mesh->corner_verts();
  const OffsetIndices<int> dst_faces = dest_mesh->faces();

  Array<int> dst_to_src_corner_map(dst_faces.total_size());
  for (const int face : dst_faces.index_range()) {
    const meshintersect::Face *f = im->face(face);
    const int mesh_index = mesh_id_for_face(f->orig, mim.mesh_offsets);
    fill_orig_loops(f,
                    src_faces[f->orig],
                    orig_corner_verts,
                    mesh_index,
                    mim,
                    dst_to_src_corner_map.as_mutable_span().slice(dst_faces[face]));
  }

  interpolate_corner_attributes(dest_mesh->attributes_for_write(),
                                mim.joined_mesh->attributes(),
                                dest_mesh,
                                mim.joined_mesh,
                                dst_to_src_corner_map,
                                dst_to_src_face_map);
}

static void gather_attributes_with_check(const bke::AttributeAccessor src_attributes,
                                         const bke::AttrDomain src_domain,
                                         const bke::AttrDomain dst_domain,
                                         const bke::AttributeFilter &attribute_filter,
                                         const Span<int> dst_to_src_map,
                                         bke::MutableAttributeAccessor dst_attributes)
{
  src_attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.domain != src_domain) {
      return;
    }
    if (iter.data_type == bke::AttrType::String) {
      return;
    }
    if (attribute_filter.allow_skip(iter.name)) {
      return;
    }
    const bke::GAttributeReader src = iter.get(src_domain);
    bke::GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        iter.name, dst_domain, iter.data_type);
    if (!dst) {
      return;
    }
    copy_attribute_using_map(GVArraySpan(*src), dst_to_src_map, dst.span);
    dst.finish();
  });
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
  Mesh *result = BKE_mesh_new_nomain(out_totvert, 0, out_faces_num, out_totloop);
  BKE_mesh_copy_parameters_for_eval(result, mim.joined_mesh);
  bke::MutableAttributeAccessor dst_attributes = result->attributes_for_write();

  static_assert(meshintersect::NO_INDEX == -1);

  /* Set the vertex coordinate values and other data. */
  MutableSpan<float3> positions = result->vert_positions_for_write();
  threading::parallel_for(im->vert_index_range(), 4096, [&](const IndexRange range) {
    for (const int vert : range) {
      const meshintersect::Vert *v = im->vert(vert);
      copy_v3fl_v3db(positions[vert], v->co);
    }
  });

  {
    Array<int> dst_to_src_vert(out_totvert);
    threading::parallel_for(positions.index_range(), 4096, [&](const IndexRange range) {
      for (const int vert : range) {
        const meshintersect::Vert *v = im->vert(vert);
        dst_to_src_vert[vert] = v->orig;
      }
    });
    gather_attributes_with_check(mim.joined_mesh->attributes(),
                                 bke::AttrDomain::Point,
                                 bke::AttrDomain::Point,
                                 bke::attribute_filter_from_skip_ref({"position"}),
                                 dst_to_src_vert,
                                 dst_attributes);
  }

  {
    OffsetIndices<int> dst_faces;
    if (out_faces_num != 0) {
      MutableSpan<int> face_offsets = result->face_offsets_for_write();
      threading::parallel_for(im->face_index_range(), 4096, [&](const IndexRange range) {
        for (const int face : range) {
          const meshintersect::Face *f = im->face(face);
          face_offsets[face] = f->size();
        }
      });
      dst_faces = offset_indices::accumulate_counts_to_offsets(face_offsets);
    }

    MutableSpan<int> dst_corner_verts = result->corner_verts_for_write();
    threading::parallel_for(im->face_index_range(), 4096, [&](const IndexRange range) {
      for (const int face : range) {
        const meshintersect::Face *f = im->face(face);
        MutableSpan<int> face_verts = dst_corner_verts.slice(dst_faces[face]);
        for (const int i : face_verts.index_range()) {
          face_verts[i] = im->lookup_vert(f->vert[i]);
        }
      }
    });

    Array<int> dst_to_src_face(out_faces_num);
    threading::parallel_for(im->face_index_range(), 4096, [&](const IndexRange range) {
      for (const int face : range) {
        const meshintersect::Face *f = im->face(face);
        dst_to_src_face[face] = f->orig;
      }
    });
    gather_attributes_with_check(mim.joined_mesh->attributes(),
                                 bke::AttrDomain::Face,
                                 bke::AttrDomain::Face,
                                 bke::attribute_filter_from_skip_ref({"material_index"}),
                                 dst_to_src_face,
                                 dst_attributes);

    if (mim.joined_mesh->attributes().contains("material_index")) {
      bke::SpanAttributeWriter dst_indices = dst_attributes.lookup_or_add_for_write_only_span<int>(
          "material_index", bke::AttrDomain::Face);
      if (mim.material_remaps.is_empty()) {
        const VArraySpan src = *mim.joined_mesh->attributes().lookup<int>("material_index");
        copy_attribute_using_map(src, dst_to_src_face, dst_indices.span);
      }
      else {
        set_material_from_map(
            dst_to_src_face, mim.material_remaps, mim.meshes, mim.mesh_offsets, dst_indices.span);
      }
      dst_indices.finish();
    }

    copy_or_interp_loop_attributes(im, result, dst_to_src_face, mim);
  }

  bke::mesh_calc_edges(*result, false, false);

  {
    Array<int> dst_to_src_edge(result->edges_num, -1);
    const OffsetIndices dst_polys = result->faces();
    const Span<int> dst_corner_edges = result->corner_edges();
    for (int fi : im->face_index_range()) {
      const meshintersect::Face *f = im->face(fi);
      const IndexRange face = dst_polys[fi];
      for (int j : f->index_range()) {
        if (f->edge_orig[j] != meshintersect::NO_INDEX) {
          dst_to_src_edge[dst_corner_edges[face[j]]] = f->edge_orig[j];
        }
      }
    }
    gather_attributes_with_check(mim.joined_mesh->attributes(),
                                 bke::AttrDomain::Edge,
                                 bke::AttrDomain::Edge,
                                 bke::attribute_filter_from_skip_ref({".edge_verts"}),
                                 dst_to_src_edge,
                                 dst_attributes);
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

  bke::GeometrySet joined_meshes_set = join_meshes_with_transforms(meshes, transforms);
  const Mesh *joined_mesh = joined_meshes_set.get_mesh();
  if (!joined_mesh) {
    return nullptr;
  }

  const int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "\nOLD_MESH_INTERSECT, nmeshes = " << meshes.size() << "\n";
  }
  MeshesToIMeshInfo mim(meshes);
  mim.joined_mesh = joined_mesh;
  meshintersect::IMeshArena arena;
  meshintersect::IMesh m_in = meshes_to_imesh(meshes, transforms, material_remaps, arena, &mim);
  const auto shape_fn = [&](int f) { return mesh_id_for_face(f, mim.mesh_offsets); };
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
#define BM_FACE_TAG BM_ELEM_SELECT_UV

/**
 * Function use to say what operand a face is part of, based on the `BM_FACE_TAG`
 * which is set in `bm_mesh_create`.
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
                             Span<Array<short>> material_remaps,
                             Array<std::array<BMLoop *, 3>> &r_looptris)
{
  const int meshes_num = meshes.size();
  BLI_assert(meshes_num >= 1);
  Array<bool> is_negative_transform(meshes_num);
  Array<bool> is_flip(meshes_num);
  const int tsize = transforms.size();
  for (const int i : IndexRange(meshes_num)) {
    if (tsize > i) {
      is_negative_transform[i] = math::is_negative(transforms[i]);
      is_flip[i] = is_negative_transform[i] != is_negative_transform[0];
    }
    else {
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
    copy_v3_v3(eve->co, math::transform_point(transforms[mesh_index], float3(eve->co)));
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
    copy_v3_v3(efa->no, math::transform_direction(transforms[mesh_index], float3(efa->no)));
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
    return BKE_mesh_copy_for_eval(*meshes[0]);
  }

  Array<std::array<BMLoop *, 3>> looptris;
  if (meshes.size() == 2) {
    BMesh *bm = mesh_bm_concat(meshes, transforms, material_remaps, looptris);
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
    BMesh *bm = mesh_bm_concat(two_meshes, two_transforms, two_remaps, looptris);
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

#ifdef BENCHMARK_TIME
/* Append benchmark time and other information for boolean operations to a /tmp file. */
static void write_boolean_benchmark_time(
    StringRef solver, StringRef op, const Mesh *mesh1, const Mesh *mesh2, float time_ms)
{
  StringRef mesh1_name = mesh1 ? mesh1->id.name + 2 : "";
  StringRef mesh2_name = mesh2 ? mesh2->id.name + 2 : "";
  const int num_faces_1 = mesh1 ? mesh1->faces_num : 0;
  const int num_faces_2 = mesh2 ? mesh2->faces_num : 0;
  const int num_tris_1 = mesh1 ? mesh1->corner_tris().size() : 0;
  const int num_tris_2 = mesh2 ? mesh2->corner_tris().size() : 0;
  const int threads = BLI_system_num_threads_override_get();

  /* Add header line if file doesn't exist yet. */
  bool first_time = false;
  if (!std::filesystem::exists(BENCHMARK_FILE)) {
    first_time = true;
  }
  /* Open the benchmark file in append mode. */
  std::ofstream outfile(BENCHMARK_FILE, std::ios_base::app);

  if (outfile.is_open()) {
    if (first_time) {
      outfile << "solver,op,mesh1,mesh2,face1,face2,tris1,tris2,time_in_ms,threads" << std::endl;
    }
    outfile << solver << "," << op << ",\"" << mesh1_name << "\",\"" << mesh2_name << "\","
            << num_faces_1 << "," << num_faces_2 << "," << num_tris_1 << "," << num_tris_2 << ","
            << time_ms << "," << threads << std::endl;
    outfile.close();
  }
  else {
    std::cerr << "Unable to open benchmark file: " << BENCHMARK_FILE << std::endl;
  }
}
#endif

/** \} */

Mesh *mesh_boolean(Span<const Mesh *> meshes,
                   Span<float4x4> transforms,
                   Span<Array<short>> material_remaps,
                   BooleanOpParameters op_params,
                   Solver solver,
                   Vector<int> *r_intersecting_edges,
                   BooleanError *r_error)
{
  Mesh *ans = nullptr;
#ifdef BENCHMARK_TIME
  const timeit::TimePoint start_time = timeit::Clock::now();
#endif
  switch (solver) {
    case Solver::Float:
      *r_error = BooleanError::NoError;
      ans = mesh_boolean_float(meshes,
                               transforms,
                               material_remaps,
                               operation_to_float_mode(op_params.boolean_mode),
                               r_intersecting_edges);
      break;
    case Solver::MeshArr:
#ifdef WITH_GMP
      *r_error = BooleanError::NoError;
      ans = mesh_boolean_mesh_arr(meshes,
                                  transforms,
                                  material_remaps,
                                  !op_params.no_self_intersections,
                                  !op_params.watertight,
                                  operation_to_mesh_arr_mode(op_params.boolean_mode),
                                  r_intersecting_edges);
#else
      *r_error = BooleanError::SolverNotAvailable;
#endif
      break;
    case Solver::Manifold:
#ifdef WITH_MANIFOLD
      ans = mesh_boolean_manifold(
          meshes, transforms, material_remaps, op_params, r_intersecting_edges, r_error);
#else
      *r_error = BooleanError::SolverNotAvailable;
#endif
      break;
    default:
      BLI_assert_unreachable();
  }
#ifdef BENCHMARK_TIME
  const timeit::TimePoint end_time = timeit::Clock::now();
  const timeit::Nanoseconds duration = end_time - start_time;
  float time_ms = duration.count() / 1.0e6f;
  Operation op = op_params.boolean_mode;
  const char *opstr = op == Operation::Intersect ?
                          "intersect" :
                          (op == Operation::Union ? "union" : "difference");
  const Mesh *mesh1 = meshes.size() > 0 ? meshes[0] : nullptr;
  const Mesh *mesh2 = meshes.size() > 0 ? meshes[1] : nullptr;
  const char *solverstr = solver == Solver::Float   ? "float" :
                          solver == Solver::MeshArr ? "mesharr" :
                                                      "manifold";
  write_boolean_benchmark_time(solverstr, opstr, mesh1, mesh2, time_ms);
#endif
  return ans;
}

}  // namespace blender::geometry::boolean
