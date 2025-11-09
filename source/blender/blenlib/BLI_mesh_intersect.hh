/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * Mesh intersection library functions.
 * Uses exact arithmetic, so need GMP.
 */

#ifdef WITH_GMP

#  include <iosfwd>

#  include "BLI_array.hh"
#  include "BLI_function_ref.hh"
#  include "BLI_index_range.hh"
#  include "BLI_map.hh"
#  include "BLI_math_mpq.hh"
#  include "BLI_math_vector_mpq_types.hh"
#  include "BLI_math_vector_types.hh"
#  include "BLI_span.hh"
#  include "BLI_utility_mixins.hh"

namespace blender::meshintersect {

constexpr int NO_INDEX = -1;

/**
 * Vertex coordinates are stored both as #double3 and #mpq3, which should agree.
 * Most calculations are done in exact arithmetic, using the mpq3 version,
 * but some predicates can be sped up by operating on doubles and using error analysis
 * to find the cases where that is good enough.
 * Vertices also carry along an id, created on allocation. The id
 * is useful for making algorithms that don't depend on pointers.
 * Also, they are easier to read while debugging.
 * They also carry an orig index, which can be used to tie them back to
 * vertices that the caller may have in a different way (e.g., #BMVert).
 * An orig index can be #NO_INDEX, indicating the Vert was created by
 * the algorithm and doesn't match an original Vert.
 * Vertices can be reliably compared for equality,
 * and hashed (on their co_exact field).
 */
struct Vert {
  mpq3 co_exact;
  double3 co;
  int id = NO_INDEX;
  int orig = NO_INDEX;

  Vert() = default;
  Vert(const mpq3 &mco, const double3 &dco, int id, int orig);
  ~Vert() = default;

  /** Test equality on the co_exact field. */
  bool operator==(const Vert &other) const;

  /** Hash on the co_exact field. */
  uint64_t hash() const;
};

std::ostream &operator<<(std::ostream &os, const Vert *v);

/**
 * A Plane whose equation is `dot(norm, p) + d = 0`.
 * The norm and d fields are always present, but the norm_exact
 * and d_exact fields may be lazily populated. Since we don't
 * store degenerate planes, we can tell if the exact versions
 * are not populated yet by having `norm_exact == 0`.
 */
struct Plane {
  mpq3 norm_exact;
  mpq_class d_exact;
  double3 norm;
  double d;

  Plane() = default;
  Plane(const mpq3 &norm_exact, const mpq_class &d_exact);
  Plane(const double3 &norm, double d);

  /** Test equality on the exact fields. */
  bool operator==(const Plane &other) const;

  /** Hash on the exact fields. */
  uint64_t hash() const;

  void make_canonical();
  /**
   * This is wrong for degenerate planes, but we don't expect to call it on those.
   */
  bool exact_populated() const;
  void populate_exact();
};

std::ostream &operator<<(std::ostream &os, const Plane *plane);

/**
 * A #Face has a sequence of Verts that for a CCW ordering around them.
 * Faces carry an index, created at allocation time, useful for making
 * pointer-independent algorithms, and for debugging.
 * They also carry an original index, meaningful to the caller.
 * And they carry original edge indices too: each is a number meaningful
 * to the caller for the edge starting from the corresponding face position.
 * A "face position" is the index of a vertex around a face.
 * Faces don't own the memory pointed at by the vert array.
 * Also indexed by face position, the is_intersect array says
 * for each edge whether or not it is the result of intersecting
 * with another face in the intersect algorithm.
 * Since the intersect algorithm needs the plane for each face,
 * a #Face also stores the Plane of the face, but this is only
 * populate later because not all faces will be intersected.
 */
struct Face : NonCopyable {
  Array<const Vert *> vert;
  Array<int> edge_orig;
  Array<bool> is_intersect;
  Plane *plane = nullptr;
  int id = NO_INDEX;
  int orig = NO_INDEX;

  using FacePos = int;

  Face() = default;
  Face(Span<const Vert *> verts, int id, int orig, Span<int> edge_origs, Span<bool> is_intersect);
  Face(Span<const Vert *> verts, int id, int orig);
  ~Face();

  bool is_tri() const
  {
    return vert.size() == 3;
  }

  /* Test equality of verts, in same positions. */
  bool operator==(const Face &other) const;

  /* Test equality faces allowing cyclic shifts. */
  bool cyclic_equal(const Face &other) const;

  FacePos next_pos(FacePos p) const
  {
    return (p + 1) % vert.size();
  }

  FacePos prev_pos(FacePos p) const
  {
    return (p + vert.size() - 1) % vert.size();
  }

  const Vert *const &operator[](int index) const
  {
    return vert[index];
  }

  int size() const
  {
    return vert.size();
  }

  const Vert *const *begin() const
  {
    return vert.begin();
  }

  const Vert *const *end() const
  {
    return vert.end();
  }

  IndexRange index_range() const
  {
    return IndexRange(vert.size());
  }

  void populate_plane(bool need_exact);

  bool plane_populated() const
  {
    return plane != nullptr;
  }
};

std::ostream &operator<<(std::ostream &os, const Face *f);

/**
 * #IMeshArena is the owner of the Vert and Face resources used
 * during a run of one of the mesh-intersect main functions.
 * It also keeps has a hash table of all Verts created so that it can
 * ensure that only one instance of a Vert with a given co_exact will
 * exist. I.e., it de-duplicates the vertices.
 */
class IMeshArena : NonCopyable, NonMovable {
  class IMeshArenaImpl;
  std::unique_ptr<IMeshArenaImpl> pimpl_;

 public:
  IMeshArena();
  ~IMeshArena();

  /**
   * Provide hints to number of expected Verts and Faces expected
   * to be allocated.
   */
  void reserve(int vert_num_hint, int face_num_hint);

  int tot_allocated_verts() const;
  int tot_allocated_faces() const;

  /**
   * These add routines find and return an existing Vert with the same
   * co_exact, if it exists (the orig argument is ignored in this case),
   * or else allocates and returns a new one. The index field of a
   * newly allocated Vert will be the index in creation order.
   */
  const Vert *add_or_find_vert(const mpq3 &co, int orig);
  const Vert *add_or_find_vert(const double3 &co, int orig);
  const Vert *add_or_find_vert(Vert *vert);

  Face *add_face(Span<const Vert *> verts,
                 int orig,
                 Span<int> edge_origs,
                 Span<bool> is_intersect);
  Face *add_face(Span<const Vert *> verts, int orig, Span<int> edge_origs);
  Face *add_face(Span<const Vert *> verts, int orig);

  /** The following return #nullptr if not found. */
  const Vert *find_vert(const mpq3 &co) const;
  const Face *find_face(Span<const Vert *> verts) const;
};

/**
 * A #blender::meshintersect::IMesh is a self-contained mesh structure
 * that can be used in `blenlib` without depending on the rest of Blender.
 * The Vert and #Face resources used in the #IMesh should be owned by
 * some #IMeshArena.
 * The Verts used by a #IMesh can be recovered from the Faces, so
 * are usually not stored, but on request, the #IMesh can populate
 * internal structures for indexing exactly the set of needed Verts,
 * and also going from a Vert pointer to the index in that system.
 */
class IMesh {
  Array<Face *> face_;                   /* Not `const` so can lazily populate planes. */
  Array<const Vert *> vert_;             /* Only valid if vert_populated_. */
  Map<const Vert *, int> vert_to_index_; /* Only valid if vert_populated_. */
  bool vert_populated_ = false;

 public:
  IMesh() = default;
  IMesh(Span<Face *> faces) : face_(faces) {}

  void set_faces(Span<Face *> faces);
  Face *face(int index) const
  {
    return face_[index];
  }

  int face_size() const
  {
    return face_.size();
  }

  int vert_size() const
  {
    return vert_.size();
  }

  bool has_verts() const
  {
    return vert_populated_;
  }

  void set_dirty_verts()
  {
    vert_populated_ = false;
    vert_to_index_.clear();
    vert_ = Array<const Vert *>();
  }

  /* Pass `max_verts` if there is a good bound estimate on the maximum number of verts. */
  void populate_vert();
  void populate_vert(int max_verts);

  const Vert *vert(int index) const
  {
    BLI_assert(vert_populated_);
    return vert_[index];
  }

  /** Returns index in vert_ where v is, or #NO_INDEX. */
  int lookup_vert(const Vert *v) const
  {
    BLI_assert(vert_populated_);
    return vert_to_index_.lookup_default(v, NO_INDEX);
  }

  IndexRange vert_index_range() const
  {
    BLI_assert(vert_populated_);
    return IndexRange(vert_.size());
  }

  IndexRange face_index_range() const
  {
    return IndexRange(face_.size());
  }

  Span<const Vert *> vertices() const
  {
    BLI_assert(vert_populated_);
    return Span<const Vert *>(vert_);
  }

  Span<Face *> faces() const
  {
    return Span<Face *>(face_);
  }

  /**
   * Replace face at given index with one that elides the
   * vertices at the positions in face_pos_erase that are true.
   * Use arena to allocate the new face in.
   * This may end up setting the face at f_index to NULL.
   * Return true if that is so, else return false.
   * The caller may want to use remove_null_faces if any face
   * was removed, to avoid the need to check for null faces later.
   */
  bool erase_face_positions(int f_index, Span<bool> face_pos_erase, IMeshArena *arena);

  void remove_null_faces();
};

std::ostream &operator<<(std::ostream &os, const IMesh &mesh);

/**
 * A Bounding Box using floats, and a routine to detect possible
 * intersection.
 */
struct BoundingBox {
  float3 min{FLT_MAX, FLT_MAX, FLT_MAX};
  float3 max{-FLT_MAX, -FLT_MAX, -FLT_MAX};

  BoundingBox() = default;
  BoundingBox(const float3 &min, const float3 &max) : min(min), max(max) {}

  void combine(const float3 &p)
  {
    math::min_max(p, this->min, this->max);
  }

  void combine(const double3 &p)
  {
    math::min_max(float3(p), this->min, this->max);
  }

  void combine(const BoundingBox &bb)
  {
    min = math::min(this->min, bb.min);
    max = math::max(this->max, bb.max);
  }

  void expand(float pad)
  {
    min -= pad;
    max += pad;
  }
};

/**
 * Assume bounding boxes have been expanded by a sufficient epsilon on all sides
 * so that the comparisons against the bb bounds are sufficient to guarantee that
 * if an overlap or even touching could happen, this will return true.
 */
bool bbs_might_intersect(const BoundingBox &bb_a, const BoundingBox &bb_b);

/**
 * This is the main routine for calculating the self_intersection of a triangle mesh.
 *
 * The output will have duplicate vertices merged and degenerate triangles ignored.
 * If the input has overlapping co-planar triangles, then there will be
 * as many duplicates as there are overlaps in each overlapping triangular region.
 * The orig field of each #IndexedTriangle will give the orig index in the input #IMesh
 * that the output triangle was a part of (input can have -1 for that field and then
 * the index in `tri[]` will be used as the original index).
 * The orig structure of the output #IMesh gives the originals for vertices and edges.
 * \note if the input tm_in has a non-empty orig structure, then it is ignored.
 */
IMesh trimesh_self_intersect(const IMesh &tm_in, IMeshArena *arena);

IMesh trimesh_nary_intersect(const IMesh &tm_in,
                             int nshapes,
                             FunctionRef<int(int)> shape_fn,
                             bool use_self,
                             IMeshArena *arena);

/**
 * Return an #IMesh that is a triangulation of a mesh with general
 * polygonal faces, #IMesh.
 * Added diagonals will be distinguishable by having edge original
 * indices of #NO_INDEX.
 */
IMesh triangulate_polymesh(IMesh &imesh, IMeshArena *arena);

/**
 * Writing the obj_mesh has the side effect of populating verts in the #IMesh.
 */
void write_obj_mesh(IMesh &m, const std::string &objname);

} /* namespace blender::meshintersect */

#endif /* WITH_GMP */
