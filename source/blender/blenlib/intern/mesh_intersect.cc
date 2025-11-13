/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

/* The #blender::meshintersect API needs GMP. */
#ifdef WITH_GMP

#  include <algorithm>
#  include <fstream>
#  include <functional>
#  include <iostream>
#  include <memory>
#  include <numeric>

#  include "BLI_array.hh"
#  include "BLI_assert.h"
#  include "BLI_delaunay_2d.hh"
#  include "BLI_kdopbvh.hh"
#  include "BLI_map.hh"
#  include "BLI_math_geom.h"
#  include "BLI_math_matrix.h"
#  include "BLI_math_mpq.hh"
#  include "BLI_math_vector.h"
#  include "BLI_math_vector_mpq_types.hh"
#  include "BLI_math_vector_types.hh"
#  include "BLI_mutex.hh"
#  include "BLI_polyfill_2d.h"
#  include "BLI_set.hh"
#  include "BLI_sort.hh"
#  include "BLI_span.hh"
#  include "BLI_task.h"
#  include "BLI_task.hh"
#  include "BLI_threads.h"
#  include "BLI_vector.hh"
#  include "BLI_vector_set.hh"

#  include "BLI_mesh_intersect.hh"

// #  define PERFDEBUG

#  ifdef _WIN_32
#    include "BLI_fileops.h"
#  endif

namespace blender::meshintersect {

#  ifdef PERFDEBUG
static void perfdata_init();
static void incperfcount(int countnum);
static void bumpperfcount(int countnum, int amt);
static void doperfmax(int maxnum, int val);
static void dump_perfdata();
#  endif

/** For debugging, can disable threading in intersect code with this static constant. */
static constexpr bool intersect_use_threading = true;

Vert::Vert(const mpq3 &mco, const double3 &dco, int id, int orig)
    : co_exact(mco), co(dco), id(id), orig(orig)
{
}

bool Vert::operator==(const Vert &other) const
{
  return this->co_exact == other.co_exact;
}

uint64_t Vert::hash() const
{
  uint64_t x = *reinterpret_cast<const uint64_t *>(&co.x);
  uint64_t y = *reinterpret_cast<const uint64_t *>(&co.y);
  uint64_t z = *reinterpret_cast<const uint64_t *>(&co.z);
  x = (x >> 56) ^ (x >> 46) ^ x;
  y = (y >> 55) ^ (y >> 45) ^ y;
  z = (z >> 54) ^ (z >> 44) ^ z;
  return x ^ y ^ z;
}

std::ostream &operator<<(std::ostream &os, const Vert *v)
{
  constexpr int dbg_level = 0;
  os << "v" << v->id;
  if (v->orig != NO_INDEX) {
    os << "o" << v->orig;
  }
  os << v->co;
  if (dbg_level > 0) {
    os << "=" << v->co_exact;
  }
  return os;
}

bool Plane::operator==(const Plane &other) const
{
  return norm_exact == other.norm_exact && d_exact == other.d_exact;
}

void Plane::make_canonical()
{
  if (norm_exact[0] != 0) {
    mpq_class den = norm_exact[0];
    norm_exact = mpq3(1, norm_exact[1] / den, norm_exact[2] / den);
    d_exact = d_exact / den;
  }
  else if (norm_exact[1] != 0) {
    mpq_class den = norm_exact[1];
    norm_exact = mpq3(0, 1, norm_exact[2] / den);
    d_exact = d_exact / den;
  }
  else {
    if (norm_exact[2] != 0) {
      mpq_class den = norm_exact[2];
      norm_exact = mpq3(0, 0, 1);
      d_exact = d_exact / den;
    }
    else {
      /* A degenerate plane. */
      d_exact = 0;
    }
  }
  norm = double3(norm_exact[0].get_d(), norm_exact[1].get_d(), norm_exact[2].get_d());
  d = d_exact.get_d();
}

Plane::Plane(const mpq3 &norm_exact, const mpq_class &d_exact)
    : norm_exact(norm_exact), d_exact(d_exact)
{
  norm = double3(norm_exact[0].get_d(), norm_exact[1].get_d(), norm_exact[2].get_d());
  d = d_exact.get_d();
}

Plane::Plane(const double3 &norm, const double d) : norm(norm), d(d)
{
  norm_exact = mpq3(0, 0, 0); /* Marks as "exact not yet populated". */
}

bool Plane::exact_populated() const
{
  return norm_exact[0] != 0 || norm_exact[1] != 0 || norm_exact[2] != 0;
}

uint64_t Plane::hash() const
{
  uint64_t x = *reinterpret_cast<const uint64_t *>(&this->norm.x);
  uint64_t y = *reinterpret_cast<const uint64_t *>(&this->norm.y);
  uint64_t z = *reinterpret_cast<const uint64_t *>(&this->norm.z);
  uint64_t d = *reinterpret_cast<const uint64_t *>(&this->d);
  x = (x >> 56) ^ (x >> 46) ^ x;
  y = (y >> 55) ^ (y >> 45) ^ y;
  z = (z >> 54) ^ (z >> 44) ^ z;
  d = (d >> 53) ^ (d >> 43) ^ d;
  return x ^ y ^ z ^ d;
}

std::ostream &operator<<(std::ostream &os, const Plane *plane)
{
  os << "[" << plane->norm << ";" << plane->d << "]";
  return os;
}

Face::Face(
    Span<const Vert *> verts, int id, int orig, Span<int> edge_origs, Span<bool> is_intersect)
    : vert(verts), edge_orig(edge_origs), is_intersect(is_intersect), id(id), orig(orig)
{
}

Face::Face(Span<const Vert *> verts, int id, int orig) : vert(verts), id(id), orig(orig) {}

void Face::populate_plane(bool need_exact)
{
  if (plane != nullptr) {
    if (!need_exact || plane->exact_populated()) {
      return;
    }
  }
  if (need_exact) {
    mpq3 normal_exact;
    if (vert.size() > 3) {
      Array<mpq3> co(vert.size());
      for (int i : index_range()) {
        co[i] = vert[i]->co_exact;
      }
      normal_exact = math::cross_poly(co.as_span());
    }
    else {
      mpq3 tr02 = vert[0]->co_exact - vert[2]->co_exact;
      mpq3 tr12 = vert[1]->co_exact - vert[2]->co_exact;
      normal_exact = math::cross(tr02, tr12);
    }
    mpq_class d_exact = -math::dot(normal_exact, vert[0]->co_exact);
    plane = new Plane(normal_exact, d_exact);
  }
  else {
    double3 normal;
    if (vert.size() > 3) {
      Array<double3> co(vert.size());
      for (int i : index_range()) {
        co[i] = vert[i]->co;
      }
      normal = math::cross_poly(co.as_span());
    }
    else {
      double3 tr02 = vert[0]->co - vert[2]->co;
      double3 tr12 = vert[1]->co - vert[2]->co;
      normal = math::cross(tr02, tr12);
    }
    double d = -math::dot(normal, vert[0]->co);
    plane = new Plane(normal, d);
  }
}

Face::~Face()
{
  delete plane;
}

bool Face::operator==(const Face &other) const
{
  if (this->size() != other.size()) {
    return false;
  }
  for (FacePos i : index_range()) {
    /* Can test pointer equality since we will have
     * unique vert pointers for unique co_equal's. */
    if (this->vert[i] != other.vert[i]) {
      return false;
    }
  }
  return true;
}

bool Face::cyclic_equal(const Face &other) const
{
  if (this->size() != other.size()) {
    return false;
  }
  int flen = this->size();
  for (FacePos start : index_range()) {
    for (FacePos start_other : index_range()) {
      bool ok = true;
      for (int i = 0; ok && i < flen; ++i) {
        FacePos p = (start + i) % flen;
        FacePos p_other = (start_other + i) % flen;
        if (this->vert[p] != other.vert[p_other]) {
          ok = false;
        }
      }
      if (ok) {
        return true;
      }
    }
  }
  return false;
}

std::ostream &operator<<(std::ostream &os, const Face *f)
{
  os << "f" << f->id << "o" << f->orig << "[";
  for (const Vert *v : *f) {
    os << v;
    if (v != f->vert[f->size() - 1]) {
      os << " ";
    }
  }
  os << "]";
  if (f->orig != NO_INDEX) {
    os << "o" << f->orig;
  }
  os << " e_orig[";
  for (int i : f->index_range()) {
    os << f->edge_orig[i];
    if (f->is_intersect[i]) {
      os << "#";
    }
    if (i != f->size() - 1) {
      os << " ";
    }
  }
  os << "]";
  return os;
}

/**
 * #IMeshArena is the owner of the Vert and Face resources used
 * during a run of one of the mesh-intersect main functions.
 * It also keeps has a hash table of all Verts created so that it can
 * ensure that only one instance of a Vert with a given co_exact will
 * exist. I.e., it de-duplicates the vertices.
 */
class IMeshArena::IMeshArenaImpl : NonCopyable, NonMovable {

  /**
   * Don't use Vert itself as key since resizing may move
   * pointers to the Vert around, and we need to have those pointers
   * stay the same throughout the lifetime of the #IMeshArena.
   */
  struct VSetKey {
    Vert *vert;

    VSetKey(Vert *p) : vert(p) {}

    uint64_t hash() const
    {
      return vert->hash();
    }

    bool operator==(const VSetKey &other) const
    {
      return *this->vert == *other.vert;
    }
  };

  Set<VSetKey> vset_;

  /**
   * Ownership of the Vert memory is here, so destroying this reclaims that memory.
   *
   * TODO: replace these with pooled allocation, and just destroy the pools at the end.
   */
  Vector<std::unique_ptr<Vert>> allocated_verts_;
  Vector<std::unique_ptr<Face>> allocated_faces_;

  /* Use these to allocate ids when Verts and Faces are allocated. */
  int next_vert_id_ = 0;
  int next_face_id_ = 0;

  /* Need a lock when multi-threading to protect allocation of new elements. */
  Mutex mutex_;

 public:
  void reserve(int vert_num_hint, int face_num_hint)
  {
    vset_.reserve(vert_num_hint);
    allocated_verts_.reserve(vert_num_hint);
    allocated_faces_.reserve(face_num_hint);
  }

  int tot_allocated_verts() const
  {
    return allocated_verts_.size();
  }

  int tot_allocated_faces() const
  {
    return allocated_faces_.size();
  }

  const Vert *add_or_find_vert(const mpq3 &co, int orig)
  {
    double3 dco(co[0].get_d(), co[1].get_d(), co[2].get_d());
    return add_or_find_vert(co, dco, orig);
  }

  const Vert *add_or_find_vert(const double3 &co, int orig)
  {
    mpq3 mco(co[0], co[1], co[2]);
    return add_or_find_vert(mco, co, orig);
  }

  const Vert *add_or_find_vert(Vert *vert)
  {
    return add_or_find_vert_(vert);
  }

  Face *add_face(Span<const Vert *> verts, int orig, Span<int> edge_origs, Span<bool> is_intersect)
  {
    Face *f = new Face(verts, next_face_id_++, orig, edge_origs, is_intersect);
    std::lock_guard lock(mutex_);
    allocated_faces_.append(std::unique_ptr<Face>(f));
    return f;
  }

  Face *add_face(Span<const Vert *> verts, int orig, Span<int> edge_origs)
  {
    Array<bool> is_intersect(verts.size(), false);
    return add_face(verts, orig, edge_origs, is_intersect);
  }

  Face *add_face(Span<const Vert *> verts, int orig)
  {
    Array<int> edge_origs(verts.size(), NO_INDEX);
    Array<bool> is_intersect(verts.size(), false);
    return add_face(verts, orig, edge_origs, is_intersect);
  }

  const Vert *find_vert(const mpq3 &co)
  {
    Vert vtry(co, double3(co[0].get_d(), co[1].get_d(), co[2].get_d()), NO_INDEX, NO_INDEX);
    VSetKey vskey(&vtry);
    std::lock_guard lock(mutex_);
    const VSetKey *lookup = vset_.lookup_key_ptr(vskey);
    if (!lookup) {
      return nullptr;
    }
    return lookup->vert;
  }

  /**
   * This is slow. Only used for unit tests right now.
   * Since it is only used for that purpose, access is not lock-protected.
   * The argument vs can be a cyclic shift of the actual stored Face.
   */
  const Face *find_face(Span<const Vert *> vs)
  {
    Array<int> eorig(vs.size(), NO_INDEX);
    Array<bool> is_intersect(vs.size(), false);
    Face ftry(vs, NO_INDEX, NO_INDEX, eorig, is_intersect);
    for (const int i : allocated_faces_.index_range()) {
      if (ftry.cyclic_equal(*allocated_faces_[i])) {
        return allocated_faces_[i].get();
      }
    }
    return nullptr;
  }

 private:
  const Vert *add_or_find_vert(const mpq3 &mco, const double3 &dco, int orig)
  {
    Vert *vtry = new Vert(mco, dco, NO_INDEX, NO_INDEX);
    const Vert *ans;
    VSetKey vskey(vtry);
    std::lock_guard lock(mutex_);
    const VSetKey *lookup = vset_.lookup_key_ptr(vskey);
    if (!lookup) {
      vtry->id = next_vert_id_++;
      vtry->orig = orig;
      vskey.vert = vtry;  // new Vert(mco, dco, next_vert_id_++, orig);
      vset_.add_new(vskey);
      allocated_verts_.append(std::unique_ptr<Vert>(vskey.vert));
      ans = vskey.vert;
    }
    else {
      /* It was a duplicate, so return the existing one.
       * Note that the returned Vert may have a different orig.
       * This is the intended semantics: if the Vert already
       * exists then we are merging verts and using the first-seen
       * one as the canonical one. */
      delete vtry;
      ans = lookup->vert;
    }
    return ans;
  };

  const Vert *add_or_find_vert_(Vert *vtry)
  {
    const Vert *ans;
    VSetKey vskey(vtry);
    std::lock_guard lock(mutex_);
    const VSetKey *lookup = vset_.lookup_key_ptr(vskey);
    if (!lookup) {
      vtry->id = next_vert_id_++;
      vskey.vert = vtry;  // new Vert(mco, dco, next_vert_id_++, orig);
      vset_.add_new(vskey);
      allocated_verts_.append(std::unique_ptr<Vert>(vskey.vert));
      ans = vskey.vert;
    }
    else {
      /* It was a duplicate, so return the existing one.
       * Note that the returned Vert may have a different orig.
       * This is the intended semantics: if the Vert already
       * exists then we are merging verts and using the first-seen
       * one as the canonical one. */
      delete vtry;
      ans = lookup->vert;
    }
    return ans;
  };
};

IMeshArena::IMeshArena()
{
  pimpl_ = std::make_unique<IMeshArenaImpl>();
}

IMeshArena::~IMeshArena() = default;

void IMeshArena::reserve(int vert_num_hint, int face_num_hint)
{
  pimpl_->reserve(vert_num_hint, face_num_hint);
}

int IMeshArena::tot_allocated_verts() const
{
  return pimpl_->tot_allocated_verts();
}

int IMeshArena::tot_allocated_faces() const
{
  return pimpl_->tot_allocated_faces();
}

const Vert *IMeshArena::add_or_find_vert(const mpq3 &co, int orig)
{
  return pimpl_->add_or_find_vert(co, orig);
}

const Vert *IMeshArena::add_or_find_vert(Vert *vert)
{
  return pimpl_->add_or_find_vert(vert);
}

Face *IMeshArena::add_face(Span<const Vert *> verts,
                           int orig,
                           Span<int> edge_origs,
                           Span<bool> is_intersect)
{
  return pimpl_->add_face(verts, orig, edge_origs, is_intersect);
}

Face *IMeshArena::add_face(Span<const Vert *> verts, int orig, Span<int> edge_origs)
{
  return pimpl_->add_face(verts, orig, edge_origs);
}

Face *IMeshArena::add_face(Span<const Vert *> verts, int orig)
{
  return pimpl_->add_face(verts, orig);
}

const Vert *IMeshArena::add_or_find_vert(const double3 &co, int orig)
{
  return pimpl_->add_or_find_vert(co, orig);
}

const Vert *IMeshArena::find_vert(const mpq3 &co) const
{
  return pimpl_->find_vert(co);
}

const Face *IMeshArena::find_face(Span<const Vert *> verts) const
{
  return pimpl_->find_face(verts);
}

void IMesh::set_faces(Span<Face *> faces)
{
  face_ = faces;
}

void IMesh::populate_vert()
{
  /* This is likely an overestimate, since verts are shared between
   * faces. It is ok if estimate is over or even under. */
  constexpr int ESTIMATE_VERTS_PER_FACE = 4;
  int estimate_verts_num = ESTIMATE_VERTS_PER_FACE * face_.size();
  populate_vert(estimate_verts_num);
}

void IMesh::populate_vert(int max_verts)
{
  if (vert_populated_) {
    return;
  }
  vert_to_index_.reserve(max_verts);
  int next_allocate_index = 0;
  for (const Face *f : face_) {
    for (const Vert *v : *f) {
      if (v->id == 1) {
      }
      int index = vert_to_index_.lookup_default(v, NO_INDEX);
      if (index == NO_INDEX) {
        BLI_assert(next_allocate_index < UINT_MAX - 2);
        vert_to_index_.add(v, next_allocate_index++);
      }
    }
  }
  int tot_v = next_allocate_index;
  vert_ = Array<const Vert *>(tot_v);
  for (auto item : vert_to_index_.items()) {
    int index = item.value;
    BLI_assert(index < tot_v);
    vert_[index] = item.key;
  }
  /* Easier debugging (at least when there are no merged input verts)
   * if output vert order is same as input, with new verts at the end.
   * TODO: when all debugged, set fix_order = false. */
  const bool fix_order = true;
  if (fix_order) {
    blender::parallel_sort(vert_.begin(), vert_.end(), [](const Vert *a, const Vert *b) {
      if (a->orig != NO_INDEX && b->orig != NO_INDEX) {
        return a->orig < b->orig;
      }
      if (a->orig != NO_INDEX) {
        return true;
      }
      if (b->orig != NO_INDEX) {
        return false;
      }
      return a->id < b->id;
    });
    for (int i : vert_.index_range()) {
      const Vert *v = vert_[i];
      vert_to_index_.add_overwrite(v, i);
    }
  }
  vert_populated_ = true;
}

bool IMesh::erase_face_positions(int f_index, Span<bool> face_pos_erase, IMeshArena *arena)
{
  const Face *cur_f = this->face(f_index);
  int cur_len = cur_f->size();
  int to_erase_num = 0;
  for (int i : cur_f->index_range()) {
    if (face_pos_erase[i]) {
      ++to_erase_num;
    }
  }
  if (to_erase_num == 0) {
    return false;
  }
  int new_len = cur_len - to_erase_num;
  if (new_len < 3) {
    /* This erase causes removal of whole face.
     * Because this may be called from a loop over the face array,
     * we don't want to compress that array right here; instead will
     * mark with null pointer and caller should call remove_null_faces().
     * the loop is done.
     */
    this->face_[f_index] = nullptr;
    return true;
  }
  Array<const Vert *> new_vert(new_len);
  Array<int> new_edge_orig(new_len);
  Array<bool> new_is_intersect(new_len);
  int new_index = 0;
  for (int i : cur_f->index_range()) {
    if (!face_pos_erase[i]) {
      new_vert[new_index] = (*cur_f)[i];
      new_edge_orig[new_index] = cur_f->edge_orig[i];
      new_is_intersect[new_index] = cur_f->is_intersect[i];
      ++new_index;
    }
  }
  BLI_assert(new_index == new_len);
  this->face_[f_index] = arena->add_face(new_vert, cur_f->orig, new_edge_orig, new_is_intersect);
  return false;
}

void IMesh::remove_null_faces()
{
  int64_t nullcount = 0;
  for (Face *f : this->face_) {
    if (f == nullptr) {
      ++nullcount;
    }
  }
  if (nullcount == 0) {
    return;
  }
  int64_t new_size = this->face_.size() - nullcount;
  int64_t copy_to_index = 0;
  int64_t copy_from_index = 0;
  Array<Face *> new_face(new_size);
  while (copy_from_index < face_.size()) {
    Face *f_from = face_[copy_from_index++];
    if (f_from) {
      new_face[copy_to_index++] = f_from;
    }
  }
  this->face_ = new_face;
}

std::ostream &operator<<(std::ostream &os, const IMesh &mesh)
{
  if (mesh.has_verts()) {
    os << "Verts:\n";
    int i = 0;
    for (const Vert *v : mesh.vertices()) {
      os << i << ": " << v << "\n";
      ++i;
    }
  }
  os << "\nFaces:\n";
  int i = 0;
  for (const Face *f : mesh.faces()) {
    os << i << ": " << f << "\n";
    if (f->plane != nullptr) {
      os << "    plane=" << f->plane << " eorig=[";
      for (Face::FacePos p = 0; p < f->size(); ++p) {
        os << f->edge_orig[p] << " ";
      }
      os << "]\n";
    }
    ++i;
  }
  return os;
}

bool bbs_might_intersect(const BoundingBox &bb_a, const BoundingBox &bb_b)
{
  return isect_aabb_aabb_v3(bb_a.min, bb_a.max, bb_b.min, bb_b.max);
}

/**
 * Data and functions to calculate bounding boxes and pad them, in parallel.
 * The bounding box calculation has the additional task of calculating the maximum
 * absolute value of any coordinate in the mesh, which will be used to calculate
 * the pad value.
 */
struct BBChunkData {
  double max_abs_val = 0.0;
};

struct BBCalcData {
  const IMesh &im;
  Array<BoundingBox> *face_bounding_box;

  BBCalcData(const IMesh &im, Array<BoundingBox> *fbb) : im(im), face_bounding_box(fbb) {};
};

static void calc_face_bb_range_func(void *__restrict userdata,
                                    const int iter,
                                    const TaskParallelTLS *__restrict tls)
{
  BBCalcData *bbdata = static_cast<BBCalcData *>(userdata);
  double max_abs = 0.0;
  const Face &face = *bbdata->im.face(iter);
  BoundingBox &bb = (*bbdata->face_bounding_box)[iter];
  for (const Vert *v : face) {
    bb.combine(v->co);
    for (int i = 0; i < 3; ++i) {
      max_abs = max_dd(max_abs, fabs(v->co[i]));
    }
  }
  BBChunkData *chunk = static_cast<BBChunkData *>(tls->userdata_chunk);
  chunk->max_abs_val = max_dd(max_abs, chunk->max_abs_val);
}

struct BBPadData {
  Array<BoundingBox> *face_bounding_box;
  double pad;

  BBPadData(Array<BoundingBox> *fbb, double pad) : face_bounding_box(fbb), pad(pad) {};
};

static void pad_face_bb_range_func(void *__restrict userdata,
                                   const int iter,
                                   const TaskParallelTLS *__restrict /*tls*/)
{
  BBPadData *pad_data = static_cast<BBPadData *>(userdata);
  (*pad_data->face_bounding_box)[iter].expand(pad_data->pad);
}

static void calc_face_bb_reduce(const void *__restrict /*userdata*/,
                                void *__restrict chunk_join,
                                void *__restrict chunk)
{
  BBChunkData *bbchunk_join = static_cast<BBChunkData *>(chunk_join);
  BBChunkData *bbchunk = static_cast<BBChunkData *>(chunk);
  bbchunk_join->max_abs_val = max_dd(bbchunk_join->max_abs_val, bbchunk->max_abs_val);
}

/**
 * We will expand the bounding boxes by an epsilon on all sides so that
 * the "less than" tests in isect_aabb_aabb_v3 are sufficient to detect
 * touching or overlap.
 */
static Array<BoundingBox> calc_face_bounding_boxes(const IMesh &m)
{
  int n = m.face_size();
  Array<BoundingBox> ans(n);
  TaskParallelSettings settings;
  BBCalcData data(m, &ans);
  BBChunkData chunk_data;
  BLI_parallel_range_settings_defaults(&settings);
  settings.userdata_chunk = &chunk_data;
  settings.userdata_chunk_size = sizeof(chunk_data);
  settings.func_reduce = calc_face_bb_reduce;
  settings.min_iter_per_thread = 1000;
  settings.use_threading = intersect_use_threading;
  BLI_task_parallel_range(0, n, &data, calc_face_bb_range_func, &settings);
  double max_abs_val = chunk_data.max_abs_val;
  constexpr float pad_factor = 10.0f;
  float pad = max_abs_val == 0.0f ? FLT_EPSILON : 2 * FLT_EPSILON * max_abs_val;
  pad *= pad_factor; /* For extra safety. */
  TaskParallelSettings pad_settings;
  BLI_parallel_range_settings_defaults(&pad_settings);
  settings.min_iter_per_thread = 1000;
  settings.use_threading = intersect_use_threading;
  BBPadData pad_data(&ans, pad);
  BLI_task_parallel_range(0, n, &pad_data, pad_face_bb_range_func, &pad_settings);
  return ans;
}

/**
 * A cluster of co-planar triangles, by index.
 * A pair of triangles T0 and T1 is said to "non-trivially co-planar-intersect"
 * if they are co-planar, intersect, and their intersection is not just existing
 * elements (verts, edges) of both triangles.
 * A co-planar cluster is said to be "nontrivial" if it has more than one triangle
 * and every triangle in it non-trivially co-planar-intersects with at least one other
 * triangle in the cluster.
 */
class CoplanarCluster {
  Vector<int> tris_;
  BoundingBox bb_;

 public:
  CoplanarCluster() = default;
  CoplanarCluster(int t, const BoundingBox &bb)
  {
    this->add_tri(t, bb);
  }

  /* Assume that caller knows this will not be a duplicate. */
  void add_tri(int t, const BoundingBox &bb)
  {
    tris_.append(t);
    bb_.combine(bb);
  }
  int tot_tri() const
  {
    return tris_.size();
  }
  int tri(int index) const
  {
    return tris_[index];
  }
  const int *begin() const
  {
    return tris_.begin();
  }
  const int *end() const
  {
    return tris_.end();
  }

  const BoundingBox &bounding_box() const
  {
    return bb_;
  }
};

/**
 * Maintains indexed set of #CoplanarCluster, with the added ability
 * to efficiently find the cluster index of any given triangle
 * (the max triangle index needs to be given in the initializer).
 * The #tri_cluster(t) function returns -1 if t is not part of any cluster.
 */
class CoplanarClusterInfo {
  Vector<CoplanarCluster> clusters_;
  Array<int> tri_cluster_;

 public:
  CoplanarClusterInfo() = default;
  explicit CoplanarClusterInfo(int numtri) : tri_cluster_(Array<int>(numtri))
  {
    tri_cluster_.fill(-1);
  }

  int tri_cluster(int t) const
  {
    BLI_assert(t < tri_cluster_.size());
    return tri_cluster_[t];
  }

  int add_cluster(const CoplanarCluster &cl)
  {
    int c_index = clusters_.append_and_get_index(cl);
    for (int t : cl) {
      BLI_assert(t < tri_cluster_.size());
      tri_cluster_[t] = c_index;
    }
    return c_index;
  }

  int tot_cluster() const
  {
    return clusters_.size();
  }

  const CoplanarCluster *begin()
  {
    return clusters_.begin();
  }

  const CoplanarCluster *end()
  {
    return clusters_.end();
  }

  IndexRange index_range() const
  {
    return clusters_.index_range();
  }

  const CoplanarCluster &cluster(int index) const
  {
    BLI_assert(index < clusters_.size());
    return clusters_[index];
  }
};

static std::ostream &operator<<(std::ostream &os, const CoplanarCluster &cl);

static std::ostream &operator<<(std::ostream &os, const CoplanarClusterInfo &clinfo);

enum ITT_value_kind { INONE, IPOINT, ISEGMENT, ICOPLANAR };

struct ITT_value {
  mpq3 p1;           /* Only relevant for IPOINT and ISEGMENT kind. */
  mpq3 p2;           /* Only relevant for ISEGMENT kind. */
  int t_source = -1; /* Index of the source triangle that intersected the target one. */
  enum ITT_value_kind kind = INONE;

  ITT_value() = default;
  explicit ITT_value(ITT_value_kind k) : kind(k) {}
  ITT_value(ITT_value_kind k, int tsrc) : t_source(tsrc), kind(k) {}
  ITT_value(ITT_value_kind k, const mpq3 &p1) : p1(p1), kind(k) {}
  ITT_value(ITT_value_kind k, const mpq3 &p1, const mpq3 &p2) : p1(p1), p2(p2), kind(k) {}
};

static std::ostream &operator<<(std::ostream &os, const ITT_value &itt);

/**
 * Project a 3d vert to a 2d one by eliding proj_axis. This does not create
 * degeneracies as long as the projection axis is one where the corresponding
 * component of the originating plane normal is non-zero.
 */
static mpq2 project_3d_to_2d(const mpq3 &p3d, int proj_axis)
{
  mpq2 p2d;
  switch (proj_axis) {
    case (0): {
      p2d[0] = p3d[1];
      p2d[1] = p3d[2];
      break;
    }
    case (1): {
      p2d[0] = p3d[0];
      p2d[1] = p3d[2];
      break;
    }
    case (2): {
      p2d[0] = p3d[0];
      p2d[1] = p3d[1];
      break;
    }
    default:
      BLI_assert(false);
  }
  return p2d;
}

/**
 * The sup and index functions are defined in the paper:
 * EXACT GEOMETRIC COMPUTATION USING CASCADING, by
 * Burnikel, Funke, and Seel. They are used to find absolute
 * bounds on the error due to doing a calculation in double
 * instead of exactly. For calculations involving only +, -, and *,
 * the supremum is the same function except using absolute values
 * on inputs and using + instead of -.
 * The index function follows these rules:
 *    index(x op y) = 1 + max(index(x), index(y)) for op + or -
 *    index(x * y)  = 1 + index(x) + index(y)
 *    index(x) = 0 if input x can be represented exactly as a double
 *    index(x) = 1 otherwise.
 *
 * With these rules in place, we know an absolute error bound:
 *
 *     |E_exact - E| <= supremum(E) * index(E) * DBL_EPSILON
 *
 * where E_exact is what would have been the exact value of the
 * expression and E is the one calculated with doubles.
 *
 * So the sign of E is the same as the sign of E_exact if
 *    |E| > supremum(E) * index(E) * DBL_EPSILON
 *
 * NOTE: a possible speedup would be to have a simple function
 * that calculates the error bound if one knows that all values
 * are less than some global maximum - most of the function would
 * be calculated ahead of time. The global max could be passed
 * from above.
 */
static double supremum_dot_cross(const double3 &a, const double3 &b)
{
  double3 abs_a = math::abs(a);
  double3 abs_b = math::abs(b);
  double3 c;
  /* This is dot(cross(a, b), cross(a,b)) but using absolute values for a and b
   * and always using + when operation is + or -. */
  c[0] = abs_a[1] * abs_b[2] + abs_a[2] * abs_b[1];
  c[1] = abs_a[2] * abs_b[0] + abs_a[0] * abs_b[2];
  c[2] = abs_a[0] * abs_b[1] + abs_a[1] * abs_b[0];
  return math::dot(c, c);
}

/* The index of dot when inputs are plane_coords with index 1 is much higher.
 * Plane coords have index 6.
 */
constexpr int index_dot_plane_coords = 15;

/**
 * Used with supremum to get error bound. See Burnikel et al paper.
 * index_plane_coord is the index of a plane coordinate calculated
 * for a triangle using the usual formula, assuming the input
 * coordinates have index 1.
 * index_cross is the index of each coordinate of the cross product.
 * It is actually 2 + 2 * (max index of input coords).
 * index_dot_cross is the index of the dot product of two cross products.
 * It is actually 7 + 4 * (max index of input coords)
 */
constexpr int index_dot_cross = 11;

/**
 * Return the approximate side of point p on a plane with normal plane_no and point plane_p.
 * The answer will be 1 if p is definitely above the plane, -1 if it is definitely below.
 * If the answer is 0, we are unsure about which side of the plane (or if it is on the plane).
 * In exact arithmetic, the answer is just `sgn(dot(p - plane_p, plane_no))`.
 *
 * The plane_no input is constructed, so has a higher index.
 */
constexpr int index_plane_side = 3 + 2 * index_dot_plane_coords;

static int filter_plane_side(const double3 &p,
                             const double3 &plane_p,
                             const double3 &plane_no,
                             const double3 &abs_p,
                             const double3 &abs_plane_p,
                             const double3 &abs_plane_no)
{
  double d = math::dot(p - plane_p, plane_no);
  if (d == 0.0) {
    return 0;
  }
  double supremum = math::dot(abs_p + abs_plane_p, abs_plane_no);
  double err_bound = supremum * index_plane_side * DBL_EPSILON;
  if (fabs(d) > err_bound) {
    return d > 0 ? 1 : -1;
  }
  return 0;
}

/*
 * #intersect_tri_tri and helper functions.
 * This code uses the algorithm of Guigue and Devillers, as described
 * in "Faster Triangle-Triangle Intersection Tests".
 * Adapted from code by Eric Haines:
 * https://github.com/erich666/jgt-code/tree/master/Volume_08/Number_1/Guigue2003
 */

/**
 * Return the point on ab where the plane with normal n containing point c intersects it.
 * Assumes ab is not perpendicular to n.
 * This works because the ratio of the projections of ab and ac onto n is the same as
 * the ratio along the line ab of the intersection point to the whole of ab.
 * The ab, ac, and dotbuf arguments are used as a temporaries; declaring them
 * in the caller can avoid many allocations and frees of mpq3 and mpq_class structures.
 */
static inline mpq3 tti_interp(
    const mpq3 &a, const mpq3 &b, const mpq3 &c, const mpq3 &n, mpq3 &ab, mpq3 &ac, mpq3 &dotbuf)
{
  ab = a;
  ab -= b;
  ac = a;
  ac -= c;
  mpq_class den = math::dot_with_buffer(ab, n, dotbuf);
  BLI_assert(den != 0);
  mpq_class alpha = math::dot_with_buffer(ac, n, dotbuf) / den;
  return a - alpha * ab;
}

/**
 * Return +1, 0, -1 as a + ad is above, on, or below the oriented plane containing a, b, c in CCW
 * order. This is the same as -oriented(a, b, c, a + ad), but uses fewer arithmetic operations.
 * TODO: change arguments to `const Vert *` and use floating filters.
 * The ba, ca, n, and dotbuf arguments are used as temporaries; declaring them
 * in the caller can avoid many allocations and frees of mpq3 and mpq_class structures.
 */
static inline int tti_above(const mpq3 &a,
                            const mpq3 &b,
                            const mpq3 &c,
                            const mpq3 &ad,
                            mpq3 &ba,
                            mpq3 &ca,
                            mpq3 &n,
                            mpq3 &dotbuf)
{
  ba = b;
  ba -= a;
  ca = c;
  ca -= a;

  n.x = ba.y * ca.z - ba.z * ca.y;
  n.y = ba.z * ca.x - ba.x * ca.z;
  n.z = ba.x * ca.y - ba.y * ca.x;

  return sgn(math::dot_with_buffer(ad, n, dotbuf));
}

/**
 * Given that triangles (p1, q1, r1) and (p2, q2, r2) are in canonical order,
 * use the classification chart in the Guigue and Devillers paper to find out
 * how the intervals [i,j] and [k,l] overlap, where [i,j] is where p1r1 and p1q1
 * intersect the plane-plane intersection line, L, and [k,l] is where p2q2 and p2r2
 * intersect L. By the canonicalization, those segments intersect L exactly once.
 * Canonicalization has made it so that for p1, q1, r1, either:
 * (a)) p1 is off the second triangle's plane and both q1 and r1 are either
 *   on the plane or on the other side of it from p1;  or
 * (b) p1 is on the plane both q1 and r1 are on the same side
 *   of the plane and at least one of q1 and r1 are off the plane.
 * Similarly for p2, q2, r2 with respect to the first triangle's plane.
 */
static ITT_value itt_canon2(const mpq3 &p1,
                            const mpq3 &q1,
                            const mpq3 &r1,
                            const mpq3 &p2,
                            const mpq3 &q2,
                            const mpq3 &r2,
                            const mpq3 &n1,
                            const mpq3 &n2)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "\ntri_tri_intersect_canon:\n";
    std::cout << "p1=" << p1 << " q1=" << q1 << " r1=" << r1 << "\n";
    std::cout << "p2=" << p2 << " q2=" << q2 << " r2=" << r2 << "\n";
    std::cout << "n1=" << n1 << " n2=" << n2 << "\n";
    std::cout << "approximate values:\n";
    std::cout << "p1=(" << p1[0].get_d() << "," << p1[1].get_d() << "," << p1[2].get_d() << ")\n";
    std::cout << "q1=(" << q1[0].get_d() << "," << q1[1].get_d() << "," << q1[2].get_d() << ")\n";
    std::cout << "r1=(" << r1[0].get_d() << "," << r1[1].get_d() << "," << r1[2].get_d() << ")\n";
    std::cout << "p2=(" << p2[0].get_d() << "," << p2[1].get_d() << "," << p2[2].get_d() << ")\n";
    std::cout << "q2=(" << q2[0].get_d() << "," << q2[1].get_d() << "," << q2[2].get_d() << ")\n";
    std::cout << "r2=(" << r2[0].get_d() << "," << r2[1].get_d() << "," << r2[2].get_d() << ")\n";
    std::cout << "n1=(" << n1[0].get_d() << "," << n1[1].get_d() << "," << n1[2].get_d() << ")\n";
    std::cout << "n2=(" << n2[0].get_d() << "," << n2[1].get_d() << "," << n2[2].get_d() << ")\n";
  }
  mpq3 p1p2 = p2 - p1;
  mpq3 intersect_1;
  mpq3 intersect_2;
  mpq3 buf[4];
  bool no_overlap = false;
  /* Top test in classification tree. */
  if (tti_above(p1, q1, r2, p1p2, buf[0], buf[1], buf[2], buf[3]) > 0) {
    /* Middle right test in classification tree. */
    if (tti_above(p1, r1, r2, p1p2, buf[0], buf[1], buf[2], buf[3]) <= 0) {
      /* Bottom right test in classification tree. */
      if (tti_above(p1, r1, q2, p1p2, buf[0], buf[1], buf[2], buf[3]) > 0) {
        /* Overlap is [k [i l] j]. */
        if (dbg_level > 0) {
          std::cout << "overlap [k [i l] j]\n";
        }
        /* i is intersect with p1r1. l is intersect with p2r2. */
        intersect_1 = tti_interp(p1, r1, p2, n2, buf[0], buf[1], buf[2]);
        intersect_2 = tti_interp(p2, r2, p1, n1, buf[0], buf[1], buf[2]);
      }
      else {
        /* Overlap is [i [k l] j]. */
        if (dbg_level > 0) {
          std::cout << "overlap [i [k l] j]\n";
        }
        /* k is intersect with p2q2. l is intersect is p2r2. */
        intersect_1 = tti_interp(p2, q2, p1, n1, buf[0], buf[1], buf[2]);
        intersect_2 = tti_interp(p2, r2, p1, n1, buf[0], buf[1], buf[2]);
      }
    }
    else {
      /* No overlap: [k l] [i j]. */
      if (dbg_level > 0) {
        std::cout << "no overlap: [k l] [i j]\n";
      }
      no_overlap = true;
    }
  }
  else {
    /* Middle left test in classification tree. */
    if (tti_above(p1, q1, q2, p1p2, buf[0], buf[1], buf[2], buf[3]) < 0) {
      /* No overlap: [i j] [k l]. */
      if (dbg_level > 0) {
        std::cout << "no overlap: [i j] [k l]\n";
      }
      no_overlap = true;
    }
    else {
      /* Bottom left test in classification tree. */
      if (tti_above(p1, r1, q2, p1p2, buf[0], buf[1], buf[2], buf[3]) >= 0) {
        /* Overlap is [k [i j] l]. */
        if (dbg_level > 0) {
          std::cout << "overlap [k [i j] l]\n";
        }
        /* i is intersect with p1r1. j is intersect with p1q1. */
        intersect_1 = tti_interp(p1, r1, p2, n2, buf[0], buf[1], buf[2]);
        intersect_2 = tti_interp(p1, q1, p2, n2, buf[0], buf[1], buf[2]);
      }
      else {
        /* Overlap is [i [k j] l]. */
        if (dbg_level > 0) {
          std::cout << "overlap [i [k j] l]\n";
        }
        /* k is intersect with p2q2. j is intersect with p1q1. */
        intersect_1 = tti_interp(p2, q2, p1, n1, buf[0], buf[1], buf[2]);
        intersect_2 = tti_interp(p1, q1, p2, n2, buf[0], buf[1], buf[2]);
      }
    }
  }
  if (no_overlap) {
    return ITT_value(INONE);
  }
  if (intersect_1 == intersect_2) {
    if (dbg_level > 0) {
      std::cout << "single intersect: " << intersect_1 << "\n";
    }
    return ITT_value(IPOINT, intersect_1);
  }
  if (dbg_level > 0) {
    std::cout << "intersect segment: " << intersect_1 << ", " << intersect_2 << "\n";
  }
  return ITT_value(ISEGMENT, intersect_1, intersect_2);
}

/* Helper function for intersect_tri_tri. Arguments have been canonicalized for triangle 1. */

static ITT_value itt_canon1(const mpq3 &p1,
                            const mpq3 &q1,
                            const mpq3 &r1,
                            const mpq3 &p2,
                            const mpq3 &q2,
                            const mpq3 &r2,
                            const mpq3 &n1,
                            const mpq3 &n2,
                            int sp2,
                            int sq2,
                            int sr2)
{
  constexpr int dbg_level = 0;
  if (sp2 > 0) {
    if (sq2 > 0) {
      return itt_canon2(p1, r1, q1, r2, p2, q2, n1, n2);
    }
    if (sr2 > 0) {
      return itt_canon2(p1, r1, q1, q2, r2, p2, n1, n2);
    }
    return itt_canon2(p1, q1, r1, p2, q2, r2, n1, n2);
  }
  if (sp2 < 0) {
    if (sq2 < 0) {
      return itt_canon2(p1, q1, r1, r2, p2, q2, n1, n2);
    }
    if (sr2 < 0) {
      return itt_canon2(p1, q1, r1, q2, r2, p2, n1, n2);
    }
    return itt_canon2(p1, r1, q1, p2, q2, r2, n1, n2);
  }
  if (sq2 < 0) {
    if (sr2 >= 0) {
      return itt_canon2(p1, r1, q1, q2, r2, p2, n1, n2);
    }
    return itt_canon2(p1, q1, r1, p2, q2, r2, n1, n2);
  }
  if (sq2 > 0) {
    if (sr2 > 0) {
      return itt_canon2(p1, r1, q1, p2, q2, r2, n1, n2);
    }
    return itt_canon2(p1, q1, r1, q2, r2, p2, n1, n2);
  }
  if (sr2 > 0) {
    return itt_canon2(p1, q1, r1, r2, p2, q2, n1, n2);
  }
  if (sr2 < 0) {
    return itt_canon2(p1, r1, q1, r2, p2, q2, n1, n2);
  }
  if (dbg_level > 0) {
    std::cout << "triangles are co-planar\n";
  }
  return ITT_value(ICOPLANAR);
}

static ITT_value intersect_tri_tri(const IMesh &tm, int t1, int t2)
{
  constexpr int dbg_level = 0;
#  ifdef PERFDEBUG
  incperfcount(1); /* Intersect_tri_tri calls. */
#  endif
  const Face &tri1 = *tm.face(t1);
  const Face &tri2 = *tm.face(t2);
  BLI_assert(tri1.plane_populated() && tri2.plane_populated());
  const Vert *vp1 = tri1[0];
  const Vert *vq1 = tri1[1];
  const Vert *vr1 = tri1[2];
  const Vert *vp2 = tri2[0];
  const Vert *vq2 = tri2[1];
  const Vert *vr2 = tri2[2];
  if (dbg_level > 0) {
    std::cout << "\nINTERSECT_TRI_TRI t1=" << t1 << ", t2=" << t2 << "\n";
    std::cout << "  p1 = " << vp1 << "\n";
    std::cout << "  q1 = " << vq1 << "\n";
    std::cout << "  r1 = " << vr1 << "\n";
    std::cout << "  p2 = " << vp2 << "\n";
    std::cout << "  q2 = " << vq2 << "\n";
    std::cout << "  r2 = " << vr2 << "\n";
  }

  /* Get signs of t1's vertices' distances to plane of t2 and vice versa. */

  /* Try first getting signs with double arithmetic, with error bounds.
   * If the signs calculated in this section are not 0, they are the same
   * as what they would be using exact arithmetic. */
  const double3 &d_p1 = vp1->co;
  const double3 &d_q1 = vq1->co;
  const double3 &d_r1 = vr1->co;
  const double3 &d_p2 = vp2->co;
  const double3 &d_q2 = vq2->co;
  const double3 &d_r2 = vr2->co;
  const double3 &d_n2 = tri2.plane->norm;

  const double3 &abs_d_p1 = math::abs(d_p1);
  const double3 &abs_d_q1 = math::abs(d_q1);
  const double3 &abs_d_r1 = math::abs(d_r1);
  const double3 &abs_d_r2 = math::abs(d_r2);
  const double3 &abs_d_n2 = math::abs(d_n2);

  int sp1 = filter_plane_side(d_p1, d_r2, d_n2, abs_d_p1, abs_d_r2, abs_d_n2);
  int sq1 = filter_plane_side(d_q1, d_r2, d_n2, abs_d_q1, abs_d_r2, abs_d_n2);
  int sr1 = filter_plane_side(d_r1, d_r2, d_n2, abs_d_r1, abs_d_r2, abs_d_n2);
  if ((sp1 > 0 && sq1 > 0 && sr1 > 0) || (sp1 < 0 && sq1 < 0 && sr1 < 0)) {
#  ifdef PERFDEBUG
    incperfcount(2); /* Triangle-triangle intersects decided by filter plane tests. */
#  endif
    if (dbg_level > 0) {
      std::cout << "no intersection, all t1's verts above or below t2\n";
    }
    return ITT_value(INONE);
  }

  const double3 &d_n1 = tri1.plane->norm;
  const double3 &abs_d_p2 = math::abs(d_p2);
  const double3 &abs_d_q2 = math::abs(d_q2);
  const double3 &abs_d_n1 = math::abs(d_n1);

  int sp2 = filter_plane_side(d_p2, d_r1, d_n1, abs_d_p2, abs_d_r1, abs_d_n1);
  int sq2 = filter_plane_side(d_q2, d_r1, d_n1, abs_d_q2, abs_d_r1, abs_d_n1);
  int sr2 = filter_plane_side(d_r2, d_r1, d_n1, abs_d_r2, abs_d_r1, abs_d_n1);
  if ((sp2 > 0 && sq2 > 0 && sr2 > 0) || (sp2 < 0 && sq2 < 0 && sr2 < 0)) {
#  ifdef PERFDEBUG
    incperfcount(2); /* Triangle-triangle intersects decided by filter plane tests. */
#  endif
    if (dbg_level > 0) {
      std::cout << "no intersection, all t2's verts above or below t1\n";
    }
    return ITT_value(INONE);
  }

  mpq3 buf[2];
  const mpq3 &p1 = vp1->co_exact;
  const mpq3 &q1 = vq1->co_exact;
  const mpq3 &r1 = vr1->co_exact;
  const mpq3 &p2 = vp2->co_exact;
  const mpq3 &q2 = vq2->co_exact;
  const mpq3 &r2 = vr2->co_exact;

  const mpq3 &n2 = tri2.plane->norm_exact;
  if (sp1 == 0) {
    buf[0] = p1;
    buf[0] -= r2;
    sp1 = sgn(math::dot_with_buffer(buf[0], n2, buf[1]));
  }
  if (sq1 == 0) {
    buf[0] = q1;
    buf[0] -= r2;
    sq1 = sgn(math::dot_with_buffer(buf[0], n2, buf[1]));
  }
  if (sr1 == 0) {
    buf[0] = r1;
    buf[0] -= r2;
    sr1 = sgn(math::dot_with_buffer(buf[0], n2, buf[1]));
  }

  if (dbg_level > 1) {
    std::cout << "  sp1=" << sp1 << " sq1=" << sq1 << " sr1=" << sr1 << "\n";
  }

  if ((sp1 * sq1 > 0) && (sp1 * sr1 > 0)) {
    if (dbg_level > 0) {
      std::cout << "no intersection, all t1's verts above or below t2 (exact)\n";
    }
#  ifdef PERFDEBUG
    incperfcount(3); /* Triangle-triangle intersects decided by exact plane tests. */
#  endif
    return ITT_value(INONE);
  }

  /* Repeat for signs of t2's vertices with respect to plane of t1. */
  const mpq3 &n1 = tri1.plane->norm_exact;
  if (sp2 == 0) {
    buf[0] = p2;
    buf[0] -= r1;
    sp2 = sgn(math::dot_with_buffer(buf[0], n1, buf[1]));
  }
  if (sq2 == 0) {
    buf[0] = q2;
    buf[0] -= r1;
    sq2 = sgn(math::dot_with_buffer(buf[0], n1, buf[1]));
  }
  if (sr2 == 0) {
    buf[0] = r2;
    buf[0] -= r1;
    sr2 = sgn(math::dot_with_buffer(buf[0], n1, buf[1]));
  }

  if (dbg_level > 1) {
    std::cout << "  sp2=" << sp2 << " sq2=" << sq2 << " sr2=" << sr2 << "\n";
  }

  if ((sp2 * sq2 > 0) && (sp2 * sr2 > 0)) {
    if (dbg_level > 0) {
      std::cout << "no intersection, all t2's verts above or below t1 (exact)\n";
    }
#  ifdef PERFDEBUG
    incperfcount(3); /* Triangle-triangle intersects decided by exact plane tests. */
#  endif
    return ITT_value(INONE);
  }

  /* Do rest of the work with vertices in a canonical order, where p1 is on
   * positive side of plane and q1, r1 are not, or p1 is on the plane and
   * q1 and r1 are off the plane on the same side. */
  ITT_value ans;
  if (sp1 > 0) {
    if (sq1 > 0) {
      ans = itt_canon1(r1, p1, q1, p2, r2, q2, n1, n2, sp2, sr2, sq2);
    }
    else if (sr1 > 0) {
      ans = itt_canon1(q1, r1, p1, p2, r2, q2, n1, n2, sp2, sr2, sq2);
    }
    else {
      ans = itt_canon1(p1, q1, r1, p2, q2, r2, n1, n2, sp2, sq2, sr2);
    }
  }
  else if (sp1 < 0) {
    if (sq1 < 0) {
      ans = itt_canon1(r1, p1, q1, p2, q2, r2, n1, n2, sp2, sq2, sr2);
    }
    else if (sr1 < 0) {
      ans = itt_canon1(q1, r1, p1, p2, q2, r2, n1, n2, sp2, sq2, sr2);
    }
    else {
      ans = itt_canon1(p1, q1, r1, p2, r2, q2, n1, n2, sp2, sr2, sq2);
    }
  }
  else {
    if (sq1 < 0) {
      if (sr1 >= 0) {
        ans = itt_canon1(q1, r1, p1, p2, r2, q2, n1, n2, sp2, sr2, sq2);
      }
      else {
        ans = itt_canon1(p1, q1, r1, p2, q2, r2, n1, n2, sp2, sq2, sr2);
      }
    }
    else if (sq1 > 0) {
      if (sr1 > 0) {
        ans = itt_canon1(p1, q1, r1, p2, r2, q2, n1, n2, sp2, sr2, sq2);
      }
      else {
        ans = itt_canon1(q1, r1, p1, p2, q2, r2, n1, n2, sp2, sq2, sr2);
      }
    }
    else {
      if (sr1 > 0) {
        ans = itt_canon1(r1, p1, q1, p2, q2, r2, n1, n2, sp2, sq2, sr2);
      }
      else if (sr1 < 0) {
        ans = itt_canon1(r1, p1, q1, p2, r2, q2, n1, n2, sp2, sr2, sq2);
      }
      else {
        if (dbg_level > 0) {
          std::cout << "triangles are co-planar\n";
        }
        ans = ITT_value(ICOPLANAR);
      }
    }
  }
  if (ans.kind == ICOPLANAR) {
    ans.t_source = t2;
  }

#  ifdef PERFDEBUG
  if (ans.kind != INONE) {
    incperfcount(4);
  }
#  endif
  return ans;
}

struct CDT_data {
  const Plane *t_plane;
  Vector<mpq2> vert;
  Vector<std::pair<int, int>> edge;
  Vector<Vector<int>> face;
  /** Parallels face, gives id from input #IMesh of input face. */
  Vector<int> input_face;
  /** Parallels face, says if input face orientation is opposite. */
  Vector<bool> is_reversed;
  /** Result of running CDT on input with (vert, edge, face). */
  CDT_result<mpq_class> cdt_out;
  /**
   * To speed up get_cdt_edge_orig, sometimes populate this map from vertex pair to output edge.
   */
  Map<std::pair<int, int>, int> verts_to_edge;
  int proj_axis;
};

/**
 * We could de-duplicate verts here, but CDT routine will do that anyway.
 */
static int prepare_need_vert(CDT_data &cd, const mpq3 &p3d)
{
  mpq2 p2d = project_3d_to_2d(p3d, cd.proj_axis);
  int v = cd.vert.append_and_get_index(p2d);
  return v;
}

/**
 * To un-project a 2d vert that was projected along cd.proj_axis, we copy the coordinates
 * from the two axes not involved in the projection, and use the plane equation of the
 * originating 3d plane, cd.t_plane, to derive the coordinate of the projected axis.
 * The plane equation says a point p is on the plane if dot(p, plane.n()) + plane.d() == 0.
 * Assume that the projection axis is such that plane.n()[proj_axis] != 0.
 */
static mpq3 unproject_cdt_vert(const CDT_data &cd, const mpq2 &p2d)
{
  mpq3 p3d;
  BLI_assert(cd.t_plane->exact_populated());
  BLI_assert(cd.t_plane->norm_exact[cd.proj_axis] != 0);
  const mpq3 &n = cd.t_plane->norm_exact;
  const mpq_class &d = cd.t_plane->d_exact;
  switch (cd.proj_axis) {
    case (0): {
      mpq_class num = n[1] * p2d[0] + n[2] * p2d[1] + d;
      num = -num;
      p3d[0] = num / n[0];
      p3d[1] = p2d[0];
      p3d[2] = p2d[1];
      break;
    }
    case (1): {
      p3d[0] = p2d[0];
      mpq_class num = n[0] * p2d[0] + n[2] * p2d[1] + d;
      num = -num;
      p3d[1] = num / n[1];
      p3d[2] = p2d[1];
      break;
    }
    case (2): {
      p3d[0] = p2d[0];
      p3d[1] = p2d[1];
      mpq_class num = n[0] * p2d[0] + n[1] * p2d[1] + d;
      num = -num;
      p3d[2] = num / n[2];
      break;
    }
    default:
      BLI_assert(false);
  }
  return p3d;
}

static void prepare_need_edge(CDT_data &cd, const mpq3 &p1, const mpq3 &p2)
{
  int v1 = prepare_need_vert(cd, p1);
  int v2 = prepare_need_vert(cd, p2);
  cd.edge.append(std::pair<int, int>(v1, v2));
}

static void prepare_need_tri(CDT_data &cd, const IMesh &tm, int t)
{
  const Face &tri = *tm.face(t);
  int v0 = prepare_need_vert(cd, tri[0]->co_exact);
  int v1 = prepare_need_vert(cd, tri[1]->co_exact);
  int v2 = prepare_need_vert(cd, tri[2]->co_exact);
  bool rev;
  /* How to get CCW orientation of projected triangle? Note that when look down y axis
   * as opposed to x or z, the orientation of the other two axes is not right-and-up. */
  BLI_assert(cd.t_plane->exact_populated());
  if (tri.plane->norm_exact[cd.proj_axis] >= 0) {
    rev = cd.proj_axis == 1;
  }
  else {
    rev = cd.proj_axis != 1;
  }
  int cd_t = cd.face.append_and_get_index(Vector<int>());
  cd.face[cd_t].append(v0);
  if (rev) {
    cd.face[cd_t].append(v2);
    cd.face[cd_t].append(v1);
  }
  else {
    cd.face[cd_t].append(v1);
    cd.face[cd_t].append(v2);
  }
  cd.input_face.append(t);
  cd.is_reversed.append(rev);
}

static CDT_data prepare_cdt_input(const IMesh &tm, int t, const Span<ITT_value> itts)
{
  CDT_data ans;
  BLI_assert(tm.face(t)->plane_populated());
  ans.t_plane = tm.face(t)->plane;
  BLI_assert(ans.t_plane->exact_populated());
  ans.proj_axis = math::dominant_axis(ans.t_plane->norm_exact);
  prepare_need_tri(ans, tm, t);
  for (const ITT_value &itt : itts) {
    switch (itt.kind) {
      case INONE:
        break;
      case IPOINT: {
        prepare_need_vert(ans, itt.p1);
        break;
      }
      case ISEGMENT: {
        prepare_need_edge(ans, itt.p1, itt.p2);
        break;
      }
      case ICOPLANAR: {
        prepare_need_tri(ans, tm, itt.t_source);
        break;
      }
    }
  }
  return ans;
}

static CDT_data prepare_cdt_input_for_cluster(const IMesh &tm,
                                              const CoplanarClusterInfo &clinfo,
                                              int c,
                                              const Span<ITT_value> itts)
{
  CDT_data ans;
  BLI_assert(c < clinfo.tot_cluster());
  const CoplanarCluster &cl = clinfo.cluster(c);
  BLI_assert(cl.tot_tri() > 0);
  int t0 = cl.tri(0);
  BLI_assert(tm.face(t0)->plane_populated());
  ans.t_plane = tm.face(t0)->plane;
  BLI_assert(ans.t_plane->exact_populated());
  ans.proj_axis = math::dominant_axis(ans.t_plane->norm_exact);
  for (const int t : cl) {
    prepare_need_tri(ans, tm, t);
  }
  for (const ITT_value &itt : itts) {
    switch (itt.kind) {
      case IPOINT: {
        prepare_need_vert(ans, itt.p1);
        break;
      }
      case ISEGMENT: {
        prepare_need_edge(ans, itt.p1, itt.p2);
        break;
      }
      default:
        break;
    }
  }
  return ans;
}

/* Return a copy of the argument with the integers ordered in ascending order. */
static inline std::pair<int, int> sorted_int_pair(std::pair<int, int> pair)
{
  if (pair.first <= pair.second) {
    return pair;
  }
  return std::pair<int, int>(pair.second, pair.first);
}

/**
 * Build cd.verts_to_edge to map from a pair of cdt output indices to an index in cd.cdt_out.edge.
 * Order the vertex indices so that the smaller one is first in the pair.
 */
static void populate_cdt_edge_map(Map<std::pair<int, int>, int> &verts_to_edge,
                                  const CDT_result<mpq_class> &cdt_out)
{
  verts_to_edge.reserve(cdt_out.edge.size());
  for (int e : cdt_out.edge.index_range()) {
    std::pair<int, int> vpair = sorted_int_pair(cdt_out.edge[e]);
    /* There should be only one edge for each vertex pair. */
    verts_to_edge.add(vpair, e);
  }
}

/**
 * Fills in cd.cdt_out with result of doing the cdt calculation on (vert, edge, face).
 */
static void do_cdt(CDT_data &cd)
{
  constexpr int dbg_level = 0;
  CDT_input<mpq_class> cdt_in;
  cdt_in.vert = Span<mpq2>(cd.vert);
  cdt_in.edge = Span<std::pair<int, int>>(cd.edge);
  cdt_in.face = Span<Vector<int>>(cd.face);
  if (dbg_level > 0) {
    std::cout << "CDT input\nVerts:\n";
    for (int i : cdt_in.vert.index_range()) {
      std::cout << "v" << i << ": " << cdt_in.vert[i] << "=(" << cdt_in.vert[i][0].get_d() << ","
                << cdt_in.vert[i][1].get_d() << ")\n";
    }
    std::cout << "Edges:\n";
    for (int i : cdt_in.edge.index_range()) {
      std::cout << "e" << i << ": (" << cdt_in.edge[i].first << ", " << cdt_in.edge[i].second
                << ")\n";
    }
    std::cout << "Tris\n";
    for (int f : cdt_in.face.index_range()) {
      std::cout << "f" << f << ": ";
      for (int j : cdt_in.face[f].index_range()) {
        std::cout << cdt_in.face[f][j] << " ";
      }
      std::cout << "\n";
    }
  }
  cdt_in.epsilon = 0; /* TODO: needs attention for non-exact T. */
  cd.cdt_out = blender::meshintersect::delaunay_2d_calc(cdt_in, CDT_INSIDE);
  constexpr int make_edge_map_threshold = 15;
  if (cd.cdt_out.edge.size() >= make_edge_map_threshold) {
    populate_cdt_edge_map(cd.verts_to_edge, cd.cdt_out);
  }
  if (dbg_level > 0) {
    std::cout << "\nCDT result\nVerts:\n";
    for (int i : cd.cdt_out.vert.index_range()) {
      std::cout << "v" << i << ": " << cd.cdt_out.vert[i] << "=(" << cd.cdt_out.vert[i][0].get_d()
                << "," << cd.cdt_out.vert[i][1].get_d() << "\n";
    }
    std::cout << "Tris\n";
    for (int f : cd.cdt_out.face.index_range()) {
      std::cout << "f" << f << ": ";
      for (int j : cd.cdt_out.face[f].index_range()) {
        std::cout << cd.cdt_out.face[f][j] << " ";
      }
      std::cout << "orig: ";
      for (int j : cd.cdt_out.face_orig[f].index_range()) {
        std::cout << cd.cdt_out.face_orig[f][j] << " ";
      }
      std::cout << "\n";
    }
    std::cout << "Edges\n";
    for (int e : cd.cdt_out.edge.index_range()) {
      std::cout << "e" << e << ": (" << cd.cdt_out.edge[e].first << ", "
                << cd.cdt_out.edge[e].second << ") ";
      std::cout << "orig: ";
      for (int j : cd.cdt_out.edge_orig[e].index_range()) {
        std::cout << cd.cdt_out.edge_orig[e][j] << " ";
      }
      std::cout << "\n";
    }
  }
}

/* Find an original edge index that goes with the edge that the CDT output edge
 * that goes between verts i0 and i1 (in the CDT output vert indexing scheme).
 * There may be more than one: if so, prefer one that was originally a face edge.
 * The input to CDT for a triangle with some intersecting segments from other triangles
 * will have both edges and a face constraint for the main triangle (this is redundant
 * but allows us to discover which face edge goes with which output edges).
 * If there is any face edge, return one of those as the original.
 * If there is no face edge but there is another edge in the input problem, then that
 * edge must have come from intersection with another triangle, so set *r_is_intersect
 * to true in that case. */
static int get_cdt_edge_orig(
    int i0, int i1, const CDT_data &cd, const IMesh &in_tm, bool *r_is_intersect)
{
  int foff = cd.cdt_out.face_edge_offset;
  *r_is_intersect = false;
  int e = NO_INDEX;
  if (cd.verts_to_edge.size() > 0) {
    /* Use the populated map to find the edge, if any, between vertices i0 and i1. */
    std::pair<int, int> vpair = sorted_int_pair(std::pair<int, int>(i0, i1));
    e = cd.verts_to_edge.lookup_default(vpair, NO_INDEX);
  }
  else {
    for (int ee : cd.cdt_out.edge.index_range()) {
      std::pair<int, int> edge = cd.cdt_out.edge[ee];
      if ((edge.first == i0 && edge.second == i1) || (edge.first == i1 && edge.second == i0)) {
        e = ee;
        break;
      }
    }
  }
  if (e == NO_INDEX) {
    return NO_INDEX;
  }

  /* Pick an arbitrary orig, but not one equal to NO_INDEX, if we can help it. */
  /* TODO: if edge has origs from more than one part of the nary input,
   * then want to set *r_is_intersect to true. */
  int face_eorig = NO_INDEX;
  bool have_non_face_eorig = false;
  for (int orig_index : cd.cdt_out.edge_orig[e]) {
    /* orig_index encodes the triangle and pos within the triangle of the input edge. */
    if (orig_index >= foff) {
      if (face_eorig == NO_INDEX) {
        int in_face_index = (orig_index / foff) - 1;
        int pos = orig_index % foff;
        /* We need to retrieve the edge orig field from the Face used to populate the
         * in_face_index'th face of the CDT, at the pos'th position of the face. */
        int in_tm_face_index = cd.input_face[in_face_index];
        BLI_assert(in_tm_face_index < in_tm.face_size());
        const Face *facep = in_tm.face(in_tm_face_index);
        BLI_assert(pos < facep->size());
        bool is_rev = cd.is_reversed[in_face_index];
        int eorig = is_rev ? facep->edge_orig[2 - pos] : facep->edge_orig[pos];
        if (eorig != NO_INDEX) {
          face_eorig = eorig;
        }
      }
    }
    else {
      if (!have_non_face_eorig) {
        have_non_face_eorig = true;
      }
      if (face_eorig != NO_INDEX && have_non_face_eorig) {
        /* Only need at most one orig for each type. */
        break;
      }
    }
  }
  if (face_eorig != NO_INDEX) {
    return face_eorig;
  }
  if (have_non_face_eorig) {
    /* This must have been an input to the CDT problem that was an intersection edge. */
    /* TODO: maybe there is an orig index:
     * This happens if an input edge was formed by an input face having
     * an edge that is co-planar with the cluster, while the face as a whole is not. */
    *r_is_intersect = true;
    return NO_INDEX;
  }
  return NO_INDEX;
}

/**
 * Make a Face from the CDT output triangle cdt_out_t, which has corresponding input triangle
 * cdt_in_t. The triangle indices that are in cd.input_face are from IMesh tm.
 * Return a pointer to the newly constructed Face.
 */
static Face *cdt_tri_as_imesh_face(
    int cdt_out_t, int cdt_in_t, const CDT_data &cd, const IMesh &tm, IMeshArena *arena)
{
  const CDT_result<mpq_class> &cdt_out = cd.cdt_out;
  int t_orig = tm.face(cd.input_face[cdt_in_t])->orig;
  BLI_assert(cdt_out.face[cdt_out_t].size() == 3);
  int i0 = cdt_out.face[cdt_out_t][0];
  int i1 = cdt_out.face[cdt_out_t][1];
  int i2 = cdt_out.face[cdt_out_t][2];
  mpq3 v0co = unproject_cdt_vert(cd, cdt_out.vert[i0]);
  mpq3 v1co = unproject_cdt_vert(cd, cdt_out.vert[i1]);
  mpq3 v2co = unproject_cdt_vert(cd, cdt_out.vert[i2]);
  /* No need to provide an original index: if coord matches
   * an original one, then it will already be in the arena
   * with the correct orig field. */
  const Vert *v0 = arena->add_or_find_vert(v0co, NO_INDEX);
  const Vert *v1 = arena->add_or_find_vert(v1co, NO_INDEX);
  const Vert *v2 = arena->add_or_find_vert(v2co, NO_INDEX);
  Face *facep;
  bool is_isect0;
  bool is_isect1;
  bool is_isect2;
  if (cd.is_reversed[cdt_in_t]) {
    int oe0 = get_cdt_edge_orig(i0, i2, cd, tm, &is_isect0);
    int oe1 = get_cdt_edge_orig(i2, i1, cd, tm, &is_isect1);
    int oe2 = get_cdt_edge_orig(i1, i0, cd, tm, &is_isect2);
    facep = arena->add_face(
        {v0, v2, v1}, t_orig, {oe0, oe1, oe2}, {is_isect0, is_isect1, is_isect2});
  }
  else {
    int oe0 = get_cdt_edge_orig(i0, i1, cd, tm, &is_isect0);
    int oe1 = get_cdt_edge_orig(i1, i2, cd, tm, &is_isect1);
    int oe2 = get_cdt_edge_orig(i2, i0, cd, tm, &is_isect2);
    facep = arena->add_face(
        {v0, v1, v2}, t_orig, {oe0, oe1, oe2}, {is_isect0, is_isect1, is_isect2});
  }
  facep->populate_plane(false);
  return facep;
}

/* Like BLI_math's is_quad_flip_v3_first_third_fast, with const double3's. */
static bool is_quad_flip_first_third(const double3 &v1,
                                     const double3 &v2,
                                     const double3 &v3,
                                     const double3 &v4)
{
  const double3 d_12 = v2 - v1;
  const double3 d_13 = v3 - v1;
  const double3 d_14 = v4 - v1;

  const double3 cross_a = math::cross(d_12, d_13);
  const double3 cross_b = math::cross(d_14, d_13);
  return math::dot(cross_a, cross_b) > 0.0f;
}

/**
 * Tessellate face f into triangles and return an array of `const Face *`
 * giving that triangulation. Intended to be used when f has => 4 vertices.
 * Care is taken so that the original edge index associated with
 * each edge in the output triangles either matches the original edge
 * for the (identical) edge of f, or else is -1. So diagonals added
 * for triangulation can later be identified by having #NO_INDEX for original.
 *
 * This code uses Blenlib's BLI_polyfill_calc to do triangulation, and is therefore quite fast.
 * Unfortunately, it can product degenerate triangles that mesh_intersect will remove, leaving
 * the mesh non-PWN.
 */
static Array<Face *> polyfill_triangulate_poly(Face *f, IMeshArena *arena)
{
  /* Similar to loop body in #BM_mesh_calc_tessellation. */
  int flen = f->size();
  BLI_assert(flen >= 4);
  if (!f->plane_populated()) {
    f->populate_plane(false);
  }
  const double3 &poly_normal = f->plane->norm;
  float no[3] = {float(poly_normal[0]), float(poly_normal[1]), float(poly_normal[2])};
  normalize_v3(no);
  if (flen == 4) {
    const Vert *v0 = (*f)[0];
    const Vert *v1 = (*f)[1];
    const Vert *v2 = (*f)[2];
    const Vert *v3 = (*f)[3];
    int eo_01 = f->edge_orig[0];
    int eo_12 = f->edge_orig[1];
    int eo_23 = f->edge_orig[2];
    int eo_30 = f->edge_orig[3];
    Face *f0, *f1;
    if (UNLIKELY(is_quad_flip_first_third(v0->co, v1->co, v2->co, v3->co))) {
      f0 = arena->add_face({v0, v1, v3}, f->orig, {eo_01, -1, eo_30}, {false, false, false});
      f1 = arena->add_face({v1, v2, v3}, f->orig, {eo_12, eo_23, -1}, {false, false, false});
    }
    else {
      f0 = arena->add_face({v0, v1, v2}, f->orig, {eo_01, eo_12, -1}, {false, false, false});
      f1 = arena->add_face({v0, v2, v3}, f->orig, {-1, eo_23, eo_30}, {false, false, false});
    }
    return Array<Face *>{f0, f1};
  }
  /* Project along negative face normal so (x,y) can be used in 2d. */
  float axis_mat[3][3];
  float (*projverts)[2];
  uint(*tris)[3];
  const int totfilltri = flen - 2;
  /* Prepare projected vertices and array to receive triangles in tessellation. */
  tris = MEM_malloc_arrayN<uint[3]>(size_t(totfilltri), __func__);
  projverts = MEM_malloc_arrayN<float[2]>(size_t(flen), __func__);
  axis_dominant_v3_to_m3_negate(axis_mat, no);
  for (int j = 0; j < flen; ++j) {
    const double3 &dco = (*f)[j]->co;
    float co[3] = {float(dco[0]), float(dco[1]), float(dco[2])};
    mul_v2_m3v3(projverts[j], axis_mat, co);
  }
  BLI_polyfill_calc(projverts, flen, 1, tris);
  /* Put tessellation triangles into Face form. Record original edges where they exist. */
  Array<Face *> ans(totfilltri);
  for (int t = 0; t < totfilltri; ++t) {
    uint *tri = tris[t];
    int eo[3];
    const Vert *v[3];
    for (int k = 0; k < 3; k++) {
      BLI_assert(tri[k] < flen);
      v[k] = (*f)[tri[k]];
      /* If tri edge goes between two successive indices in
       * the original face, then it is an original edge. */
      if ((tri[k] + 1) % flen == tri[(k + 1) % 3]) {
        eo[k] = f->edge_orig[tri[k]];
      }
      else {
        eo[k] = NO_INDEX;
      }
      ans[t] = arena->add_face(
          {v[0], v[1], v[2]}, f->orig, {eo[0], eo[1], eo[2]}, {false, false, false});
    }
  }

  MEM_freeN(tris);
  MEM_freeN(projverts);

  return ans;
}

/**
 * Tessellate face f into triangles and return an array of `const Face *`
 * giving that triangulation, using an exact triangulation method.
 *
 * The method used is to use the CDT triangulation. Usually that triangulation
 * will only use the existing vertices. However, if the face self-intersects
 * then the CDT triangulation will include the intersection points.
 * If this happens, we use the polyfill triangulator instead. We don't
 * use the polyfill triangulator by default because it can create degenerate
 * triangles (which we can handle but they'll create non-manifold meshes).
 *
 * While it is tempting to handle quadrilaterals specially, since that
 * is by far the usual case, we need to know if the quad is convex when
 * projected before doing so, and that takes a fair amount of computation by itself.
 */
static Array<Face *> exact_triangulate_poly(Face *f, IMeshArena *arena)
{
  int flen = f->size();
  Array<mpq2> in_verts(flen);
  Array<Vector<int>> faces(1);
  faces.first().resize(flen);
  std::iota(faces.first().begin(), faces.first().end(), 0);

  /* Project poly along dominant axis of normal to get 2d coords. */
  if (!f->plane_populated()) {
    f->populate_plane(false);
  }
  const double3 &poly_normal = f->plane->norm;
  int axis = math::dominant_axis(poly_normal);
  /* If project down y axis as opposed to x or z, the orientation
   * of the face will be reversed.
   * Yet another reversal happens if the poly normal in the dominant
   * direction is opposite that of the positive dominant axis. */
  bool rev1 = (axis == 1);
  bool rev2 = poly_normal[axis] < 0;
  bool rev = rev1 ^ rev2;
  for (int i = 0; i < flen; ++i) {
    int ii = rev ? flen - i - 1 : i;
    mpq2 &p2d = in_verts[ii];
    int k = 0;
    for (int j = 0; j < 3; ++j) {
      if (j != axis) {
        p2d[k++] = (*f)[ii]->co_exact[j];
      }
    }
  }

  CDT_input<mpq_class> cdt_in;
  cdt_in.vert = std::move(in_verts);
  cdt_in.face = std::move(faces);

  CDT_result<mpq_class> cdt_out = delaunay_2d_calc(cdt_in, CDT_INSIDE);
  int n_tris = cdt_out.face.size();
  Array<Face *> ans(n_tris);
  for (int t = 0; t < n_tris; ++t) {
    int i_v_out[3];
    const Vert *v[3];
    int eo[3];
    bool needs_steiner = false;
    for (int i = 0; i < 3; ++i) {
      i_v_out[i] = cdt_out.face[t][i];
      if (cdt_out.vert_orig[i_v_out[i]].is_empty()) {
        needs_steiner = true;
        break;
      }
      v[i] = (*f)[cdt_out.vert_orig[i_v_out[i]][0]];
    }
    if (needs_steiner) {
      /* Fall back on the polyfill triangulator. */
      return polyfill_triangulate_poly(f, arena);
    }
    Map<std::pair<int, int>, int> verts_to_edge;
    populate_cdt_edge_map(verts_to_edge, cdt_out);
    int foff = cdt_out.face_edge_offset;
    for (int i = 0; i < 3; ++i) {
      std::pair<int, int> vpair(i_v_out[i], i_v_out[(i + 1) % 3]);
      std::pair<int, int> vpair_canon = sorted_int_pair(vpair);
      int e_out = verts_to_edge.lookup_default(vpair_canon, NO_INDEX);
      BLI_assert(e_out != NO_INDEX);
      eo[i] = NO_INDEX;
      for (int orig : cdt_out.edge_orig[e_out]) {
        if (orig >= foff) {
          int pos = orig % foff;
          BLI_assert(pos < f->size());
          eo[i] = f->edge_orig[pos];
          break;
        }
      }
    }
    if (rev) {
      ans[t] = arena->add_face(
          {v[0], v[2], v[1]}, f->orig, {eo[2], eo[1], eo[0]}, {false, false, false});
    }
    else {
      ans[t] = arena->add_face(
          {v[0], v[1], v[2]}, f->orig, {eo[0], eo[1], eo[2]}, {false, false, false});
    }
  }
  return ans;
}

static bool face_is_degenerate(const Face *f)
{
  const Face &face = *f;
  const Vert *v0 = face[0];
  const Vert *v1 = face[1];
  const Vert *v2 = face[2];
  if (v0 == v1 || v0 == v2 || v1 == v2) {
    return true;
  }
  double3 da = v2->co - v0->co;
  double3 db = v2->co - v1->co;
  double3 dab = math::cross(da, db);
  double dab_length_squared = math::length_squared(dab);
  double err_bound = supremum_dot_cross(dab, dab) * index_dot_cross * DBL_EPSILON;
  if (dab_length_squared > err_bound) {
    return false;
  }
  mpq3 a = v2->co_exact - v0->co_exact;
  mpq3 b = v2->co_exact - v1->co_exact;
  mpq3 ab = math::cross(a, b);
  if (ab.x == 0 && ab.y == 0 && ab.z == 0) {
    return true;
  }

  return false;
}

/** Fast check for degenerate tris. It is OK if it returns true for nearly degenerate triangles. */
static bool any_degenerate_tris_fast(const Array<Face *> &triangulation)
{
  for (const Face *f : triangulation) {
    const Vert *v0 = (*f)[0];
    const Vert *v1 = (*f)[1];
    const Vert *v2 = (*f)[2];
    if (v0 == v1 || v0 == v2 || v1 == v2) {
      return true;
    }
    double3 da = v2->co - v0->co;
    double3 db = v2->co - v1->co;
    double da_length_squared = math::length_squared(da);
    double db_length_squared = math::length_squared(db);
    if (da_length_squared == 0.0 || db_length_squared == 0.0) {
      return true;
    }
    /* |da x db| = |da| |db| sin t, where t is angle between them.
     * The triangle is almost degenerate if sin t is almost 0.
     * sin^2 t = |da x db|^2 / (|da|^2 |db|^2)
     */
    double3 dab = math::cross(da, db);
    double dab_length_squared = math::length_squared(dab);
    double sin_squared_t = dab_length_squared / (da_length_squared * db_length_squared);
    if (sin_squared_t < 1e-8) {
      return true;
    }
  }
  return false;
}

/**
 * Tessellate face f into triangles and return an array of `const Face *`
 * giving that triangulation.
 * Care is taken so that the original edge index associated with
 * each edge in the output triangles either matches the original edge
 * for the (identical) edge of f, or else is -1. So diagonals added
 * for triangulation can later be identified by having #NO_INDEX for original.
 */
static Array<Face *> triangulate_poly(Face *f, IMeshArena *arena)
{
  /* Try the much faster method using Blender's BLI_polyfill_calc. */
  Array<Face *> ans = polyfill_triangulate_poly(f, arena);

  /* This may create degenerate triangles. If so, try the exact CDT-based triangulator. */
  if (any_degenerate_tris_fast(ans)) {
    return exact_triangulate_poly(f, arena);
  }
  return ans;
}

IMesh triangulate_polymesh(IMesh &imesh, IMeshArena *arena)
{
  Vector<Face *> face_tris;
  constexpr int estimated_tris_per_face = 3;
  face_tris.reserve(estimated_tris_per_face * imesh.face_size());
  threading::parallel_for(imesh.face_index_range(), 2048, [&](IndexRange range) {
    for (int i : range) {
      Face *f = imesh.face(i);
      if (!f->plane_populated() && f->size() >= 4) {
        f->populate_plane(false);
      }
    }
  });
  for (Face *f : imesh.faces()) {
    /* Tessellate face f, following plan similar to #BM_face_calc_tessellation. */
    int flen = f->size();
    if (flen == 3) {
      face_tris.append(f);
    }
    else {
      Array<Face *> tris = triangulate_poly(f, arena);
      for (Face *tri : tris) {
        face_tris.append(tri);
      }
    }
  }
  return IMesh(face_tris);
}

/**
 * Using the result of CDT in cd.cdt_out, extract an #IMesh representing the subdivision
 * of input triangle t, which should be an element of cd.input_face.
 */
static IMesh extract_subdivided_tri(const CDT_data &cd,
                                    const IMesh &in_tm,
                                    int t,
                                    IMeshArena *arena)
{
  const CDT_result<mpq_class> &cdt_out = cd.cdt_out;
  int t_in_cdt = -1;
  for (int i = 0; i < cd.input_face.size(); ++i) {
    if (cd.input_face[i] == t) {
      t_in_cdt = i;
    }
  }
  if (t_in_cdt == -1) {
    std::cout << "Could not find " << t << " in cdt input tris\n";
    BLI_assert(false);
    return IMesh();
  }
  constexpr int inline_buf_size = 20;
  Vector<Face *, inline_buf_size> faces;
  for (int f : cdt_out.face.index_range()) {
    if (cdt_out.face_orig[f].contains(t_in_cdt)) {
      Face *facep = cdt_tri_as_imesh_face(f, t_in_cdt, cd, in_tm, arena);
      faces.append(facep);
    }
  }
  return IMesh(faces);
}

static bool bvhtreeverlap_cmp(const BVHTreeOverlap &a, const BVHTreeOverlap &b)
{
  if (a.indexA < b.indexA) {
    return true;
  }
  if ((a.indexA == b.indexA) && (a.indexB < b.indexB)) {
    return true;
  }
  return false;
}
class TriOverlaps {
  BVHTree *tree_{nullptr};
  BVHTree *tree_b_{nullptr};
  BVHTreeOverlap *overlap_{nullptr};
  Array<int> first_overlap_;
  uint overlap_num_{0};

  struct CBData {
    const IMesh &tm;
    std::function<int(int)> shape_fn;
    int nshapes;
    bool use_self;
  };

 public:
  TriOverlaps(const IMesh &tm,
              const Span<BoundingBox> tri_bb,
              int nshapes,
              std::function<int(int)> shape_fn,
              bool use_self)
  {
    constexpr int dbg_level = 0;
    if (dbg_level > 0) {
      std::cout << "TriOverlaps construction\n";
    }
    /* Tree type is 8 => octree; axis = 6 => using XYZ axes only. */
    tree_ = BLI_bvhtree_new(tm.face_size(), FLT_EPSILON, 8, 6);
    /* In the common case of a binary boolean and no self intersection in
     * each shape, we will use two trees and simple bounding box overlap. */
    bool two_trees_no_self = nshapes == 2 && !use_self;
    if (two_trees_no_self) {
      tree_b_ = BLI_bvhtree_new(tm.face_size(), FLT_EPSILON, 8, 6);
    }

    /* Create a Vector containing face shape. */
    Vector<int> shapes;
    shapes.resize(tm.face_size());
    threading::parallel_for(tm.face_index_range(), 2048, [&](IndexRange range) {
      for (int t : range) {
        shapes[t] = shape_fn(tm.face(t)->orig);
      }
    });

    float bbpts[6];
    for (int t : tm.face_index_range()) {
      const BoundingBox &bb = tri_bb[t];
      copy_v3_v3(bbpts, bb.min);
      copy_v3_v3(bbpts + 3, bb.max);
      int shape = shapes[t];
      if (two_trees_no_self) {
        if (shape == 0) {
          BLI_bvhtree_insert(tree_, t, bbpts, 2);
        }
        else if (shape == 1) {
          BLI_bvhtree_insert(tree_b_, t, bbpts, 2);
        }
      }
      else {
        if (shape != -1) {
          BLI_bvhtree_insert(tree_, t, bbpts, 2);
        }
      }
    }
    BLI_bvhtree_balance(tree_);
    if (two_trees_no_self) {
      BLI_bvhtree_balance(tree_b_);
      /* Don't expect a lot of trivial intersects in this case. */
      overlap_ = BLI_bvhtree_overlap(tree_, tree_b_, &overlap_num_, nullptr, nullptr);
    }
    else {
      CBData cbdata{tm, shape_fn, nshapes, use_self};
      if (nshapes == 1) {
        overlap_ = BLI_bvhtree_overlap(tree_, tree_, &overlap_num_, nullptr, nullptr);
      }
      else {
        overlap_ = BLI_bvhtree_overlap(
            tree_, tree_, &overlap_num_, only_different_shapes, &cbdata);
      }
    }
    /* The rest of the code is simpler and easier to parallelize if, in the two-trees case,
     * we repeat the overlaps with indexA and indexB reversed. It is important that
     * in the repeated part, sorting will then bring things with indexB together. */
    if (two_trees_no_self) {
      overlap_ = static_cast<BVHTreeOverlap *>(
          MEM_reallocN(overlap_, 2 * overlap_num_ * sizeof(overlap_[0])));
      for (uint i = 0; i < overlap_num_; ++i) {
        overlap_[overlap_num_ + i].indexA = overlap_[i].indexB;
        overlap_[overlap_num_ + i].indexB = overlap_[i].indexA;
      }
      overlap_num_ += overlap_num_;
    }
    /* Sort the overlaps to bring all the intersects with a given indexA together. */
    std::sort(overlap_, overlap_ + overlap_num_, bvhtreeverlap_cmp);
    if (dbg_level > 0) {
      std::cout << overlap_num_ << " overlaps found:\n";
      for (BVHTreeOverlap ov : overlap()) {
        std::cout << "A: " << ov.indexA << ", B: " << ov.indexB << "\n";
      }
    }
    first_overlap_ = Array<int>(tm.face_size(), -1);
    for (int i = 0; i < int(overlap_num_); ++i) {
      int t = overlap_[i].indexA;
      if (first_overlap_[t] == -1) {
        first_overlap_[t] = i;
      }
    }
  }

  ~TriOverlaps()
  {
    if (tree_) {
      BLI_bvhtree_free(tree_);
    }
    if (tree_b_) {
      BLI_bvhtree_free(tree_b_);
    }
    if (overlap_) {
      MEM_freeN(overlap_);
    }
  }

  Span<BVHTreeOverlap> overlap() const
  {
    return Span<BVHTreeOverlap>(overlap_, overlap_num_);
  }

  int first_overlap_index(int t) const
  {
    return first_overlap_[t];
  }

 private:
  static bool only_different_shapes(void *userdata, int index_a, int index_b, int /*thread*/)
  {
    CBData *cbdata = static_cast<CBData *>(userdata);
    return cbdata->tm.face(index_a)->orig != cbdata->tm.face(index_b)->orig;
  }
};

/**
 * Data needed for parallelization of #calc_overlap_itts.
 */
struct OverlapIttsData {
  Vector<std::pair<int, int>> intersect_pairs;
  Map<std::pair<int, int>, ITT_value> &itt_map;
  const IMesh &tm;
  IMeshArena *arena;

  OverlapIttsData(Map<std::pair<int, int>, ITT_value> &itt_map, const IMesh &tm, IMeshArena *arena)
      : itt_map(itt_map), tm(tm), arena(arena)
  {
  }
};

/**
 * Return a std::pair containing a and b in canonical order:
 * With a <= b.
 */
static std::pair<int, int> canon_int_pair(int a, int b)
{
  if (a > b) {
    std::swap(a, b);
  }
  return std::pair<int, int>(a, b);
}

static void calc_overlap_itts_range_func(void *__restrict userdata,
                                         const int iter,
                                         const TaskParallelTLS *__restrict /*tls*/)
{
  constexpr int dbg_level = 0;
  OverlapIttsData *data = static_cast<OverlapIttsData *>(userdata);
  std::pair<int, int> tri_pair = data->intersect_pairs[iter];
  int a = tri_pair.first;
  int b = tri_pair.second;
  if (dbg_level > 0) {
    std::cout << "calc_overlap_itts_range_func a=" << a << ", b=" << b << "\n";
  }
  ITT_value itt = intersect_tri_tri(data->tm, a, b);
  if (dbg_level > 0) {
    std::cout << "result of intersecting " << a << " and " << b << " = " << itt << "\n";
  }
  BLI_assert(data->itt_map.contains(tri_pair));
  data->itt_map.add_overwrite(tri_pair, itt);
}

/**
 * Fill in itt_map with the vector of ITT_values that result from intersecting the triangles in
 * ov. Use a canonical order for triangles: (a,b) where `a < b`.
 */
static void calc_overlap_itts(Map<std::pair<int, int>, ITT_value> &itt_map,
                              const IMesh &tm,
                              const TriOverlaps &ov,
                              IMeshArena *arena)
{
  OverlapIttsData data(itt_map, tm, arena);
  /* Put dummy values in `itt_map` initially,
   * so map entries will exist when doing the range function.
   * This means we won't have to protect the `itt_map.add_overwrite` function with a lock. */
  for (const BVHTreeOverlap &olap : ov.overlap()) {
    std::pair<int, int> key = canon_int_pair(olap.indexA, olap.indexB);
    if (!itt_map.contains(key)) {
      itt_map.add_new(key, ITT_value());
      data.intersect_pairs.append(key);
    }
  }
  int tot_intersect_pairs = data.intersect_pairs.size();
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.min_iter_per_thread = 1000;
  settings.use_threading = intersect_use_threading;
  BLI_task_parallel_range(0, tot_intersect_pairs, &data, calc_overlap_itts_range_func, &settings);
}

/**
 * For each triangle in tm, fill in the corresponding slot in
 * r_tri_subdivided with the result of intersecting it with
 * all the other triangles in the mesh, if it intersects any others.
 * But don't do this for triangles that are part of a cluster.
 */
static void calc_subdivided_non_cluster_tris(Array<IMesh> &r_tri_subdivided,
                                             const IMesh &tm,
                                             const Map<std::pair<int, int>, ITT_value> &itt_map,
                                             const CoplanarClusterInfo &clinfo,
                                             const TriOverlaps &ov,
                                             IMeshArena *arena)
{
  const int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "\nCALC_SUBDIVIDED_TRIS\n\n";
  }
  Span<BVHTreeOverlap> overlap = ov.overlap();
  struct OverlapTriRange {
    int tri_index;
    int overlap_start;
    int len;
  };
  Vector<OverlapTriRange> overlap_tri_range;
  int overlap_num = overlap.size();
  overlap_tri_range.reserve(overlap_num);
  int overlap_index = 0;
  while (overlap_index < overlap_num) {
    int t = overlap[overlap_index].indexA;
    int i = overlap_index;
    while (i + 1 < overlap_num && overlap[i + 1].indexA == t) {
      ++i;
    }
    /* Now overlap[overlap_index] to overlap[i] have indexA == t.
     * We only record ranges for triangles that are not in clusters,
     * because the ones in clusters are handled separately. */
    if (clinfo.tri_cluster(t) == NO_INDEX) {
      int len = i - overlap_index + 1;
      if (!(len == 1 && overlap[overlap_index].indexB == t)) {
        OverlapTriRange range = {t, overlap_index, len};
        overlap_tri_range.append(range);
#  ifdef PERFDEBUG
        bumpperfcount(0, len); /* Non-cluster overlaps. */
#  endif
      }
    }
    overlap_index = i + 1;
  }
  int overlap_tri_range_num = overlap_tri_range.size();
  Array<CDT_data> cd_data(overlap_tri_range_num);
  int grain_size = 64;
  threading::parallel_for(overlap_tri_range.index_range(), grain_size, [&](IndexRange range) {
    for (int otr_index : range) {
      OverlapTriRange &otr = overlap_tri_range[otr_index];
      int t = otr.tri_index;
      if (dbg_level > 0) {
        std::cout << "handling overlap range\nt=" << t << " start=" << otr.overlap_start
                  << " len=" << otr.len << "\n";
      }
      constexpr int inline_capacity = 100;
      Vector<ITT_value, inline_capacity> itts(otr.len);
      for (int j = otr.overlap_start; j < otr.overlap_start + otr.len; ++j) {
        int t_other = overlap[j].indexB;
        std::pair<int, int> key = canon_int_pair(t, t_other);
        ITT_value itt;
        if (itt_map.contains(key)) {
          itt = itt_map.lookup(key);
        }
        if (itt.kind != INONE) {
          itts.append(itt);
        }
        if (dbg_level > 0) {
          std::cout << "  tri t" << t_other << "; result = " << itt << "\n";
        }
      }
      if (itts.size() > 0) {
        cd_data[otr_index] = prepare_cdt_input(tm, t, itts);
        do_cdt(cd_data[otr_index]);
      }
    }
  });
  /* Extract the new faces serially, so that Boolean is repeatable regardless of parallelism. */
  for (int otr_index : overlap_tri_range.index_range()) {
    CDT_data &cdd = cd_data[otr_index];
    if (cdd.vert.size() > 0) {
      int t = overlap_tri_range[otr_index].tri_index;
      r_tri_subdivided[t] = extract_subdivided_tri(cdd, tm, t, arena);
      if (dbg_level > 1) {
        std::cout << "subdivide output for tri " << t << " = " << r_tri_subdivided[t];
      }
    }
  }
  /* Now have to put in the triangles that are the same as the input ones, and not in clusters.
   */
  threading::parallel_for(tm.face_index_range(), 2048, [&](IndexRange range) {
    for (int t : range) {
      if (r_tri_subdivided[t].face_size() == 0 && clinfo.tri_cluster(t) == NO_INDEX) {
        r_tri_subdivided[t] = IMesh({tm.face(t)});
      }
    }
  });
}

/**
 * For each cluster in clinfo, extract the triangles from the cluster
 * that correspond to each original triangle t that is part of the cluster,
 * and put the resulting triangles into an IMesh in tri_subdivided[t].
 * We have already done the CDT for the triangles in the cluster, whose
 * result is in cluster_subdivided[c] for each cluster c.
 */
static void calc_cluster_tris(Array<IMesh> &tri_subdivided,
                              const IMesh &tm,
                              const CoplanarClusterInfo &clinfo,
                              const Span<CDT_data> cluster_subdivided,
                              IMeshArena *arena)
{
  for (int c : clinfo.index_range()) {
    const CoplanarCluster &cl = clinfo.cluster(c);
    const CDT_data &cd = cluster_subdivided[c];
    /* Each triangle in cluster c should be an input triangle in cd.input_faces.
     * (See prepare_cdt_input_for_cluster.)
     * So accumulate a Vector of Face* for each input face by going through the
     * output faces and making a Face for each input face that it is part of.
     * (The Boolean algorithm wants duplicates if a given output triangle is part
     * of more than one input triangle.)
     */
    int n_cluster_tris = cl.tot_tri();
    const CDT_result<mpq_class> &cdt_out = cd.cdt_out;
    BLI_assert(cd.input_face.size() == n_cluster_tris);
    Array<Vector<Face *>> face_vec(n_cluster_tris);
    for (int cdt_out_t : cdt_out.face.index_range()) {
      for (int cdt_in_t : cdt_out.face_orig[cdt_out_t]) {
        Face *f = cdt_tri_as_imesh_face(cdt_out_t, cdt_in_t, cd, tm, arena);
        face_vec[cdt_in_t].append(f);
      }
    }
    for (int cdt_in_t : cd.input_face.index_range()) {
      int tm_t = cd.input_face[cdt_in_t];
      BLI_assert(tri_subdivided[tm_t].face_size() == 0);
      tri_subdivided[tm_t] = IMesh(face_vec[cdt_in_t]);
    }
  }
}

static CDT_data calc_cluster_subdivided(const CoplanarClusterInfo &clinfo,
                                        int c,
                                        const IMesh &tm,
                                        const TriOverlaps &ov,
                                        const Map<std::pair<int, int>, ITT_value> &itt_map,
                                        IMeshArena * /*arena*/)
{
  constexpr int dbg_level = 0;
  BLI_assert(c < clinfo.tot_cluster());
  const CoplanarCluster &cl = clinfo.cluster(c);
  /* Make a CDT input with triangles from C and intersects from other triangles in tm. */
  if (dbg_level > 0) {
    std::cout << "CALC_CLUSTER_SUBDIVIDED for cluster " << c << " = " << cl << "\n";
  }
  /* Get vector itts of all intersections of a triangle of cl with any triangle of tm not
   * in cl and not co-planar with it (for that latter, if there were an intersection,
   * it should already be in cluster cl). */
  Vector<ITT_value> itts;
  Span<BVHTreeOverlap> ovspan = ov.overlap();
  for (int t : cl) {
    if (dbg_level > 0) {
      std::cout << "find intersects with triangle " << t << " of cluster\n";
    }
    int first_i = ov.first_overlap_index(t);
    if (first_i == -1) {
      continue;
    }
    for (int i = first_i; i < ovspan.size() && ovspan[i].indexA == t; ++i) {
      int t_other = ovspan[i].indexB;
      if (clinfo.tri_cluster(t_other) != c) {
        if (dbg_level > 0) {
          std::cout << "use intersect(" << t << "," << t_other << "\n";
        }
        std::pair<int, int> key = canon_int_pair(t, t_other);
        if (itt_map.contains(key)) {
          ITT_value itt = itt_map.lookup(key);
          if (!ELEM(itt.kind, INONE, ICOPLANAR)) {
            itts.append(itt);
            if (dbg_level > 0) {
              std::cout << "  itt = " << itt << "\n";
            }
          }
        }
      }
    }
  }
  /* Use CDT to subdivide the cluster triangles and the points and segments in itts. */
  CDT_data cd_data = prepare_cdt_input_for_cluster(tm, clinfo, c, itts);
  do_cdt(cd_data);
  return cd_data;
}

static IMesh union_tri_subdivides(const blender::Array<IMesh> &tri_subdivided)
{
  int tot_tri = 0;
  for (const IMesh &m : tri_subdivided) {
    tot_tri += m.face_size();
  }
  Array<Face *> faces(tot_tri);
  int face_index = 0;
  for (const IMesh &m : tri_subdivided) {
    for (Face *f : m.faces()) {
      faces[face_index++] = f;
    }
  }
  return IMesh(faces);
}

static CoplanarClusterInfo find_clusters(const IMesh &tm,
                                         const Array<BoundingBox> &tri_bb,
                                         const Map<std::pair<int, int>, ITT_value> &itt_map)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "FIND_CLUSTERS\n";
  }
  CoplanarClusterInfo ans(tm.face_size());
  /* Use a VectorSet to get stable order from run to run. */
  VectorSet<int> maybe_coplanar_tris;
  maybe_coplanar_tris.reserve(2 * itt_map.size());
  for (auto item : itt_map.items()) {
    if (item.value.kind == ICOPLANAR) {
      int t1 = item.key.first;
      int t2 = item.key.second;
      maybe_coplanar_tris.add_multiple({t1, t2});
    }
  }
  if (dbg_level > 0) {
    std::cout << "found " << maybe_coplanar_tris.size() << " possible coplanar tris\n";
  }
  if (maybe_coplanar_tris.is_empty()) {
    if (dbg_level > 0) {
      std::cout << "No possible coplanar tris, so no clusters\n";
    }
    return ans;
  }
  /* There can be more than one #CoplanarCluster per plane. Accumulate them in
   * a Vector. We will have to merge some elements of the Vector as we discover
   * triangles that form intersection bridges between two or more clusters. */
  Map<Plane, Vector<CoplanarCluster>> plane_cls;
  plane_cls.reserve(maybe_coplanar_tris.size());
  for (int t : maybe_coplanar_tris) {
    /* Use a canonical version of the plane for map index.
     * We can't just store the canonical version in the face
     * since canonicalizing loses the orientation of the normal. */
    Plane tplane = *tm.face(t)->plane;
    BLI_assert(tplane.exact_populated());
    tplane.make_canonical();
    if (dbg_level > 0) {
      std::cout << "plane for tri " << t << " = " << &tplane << "\n";
    }
    /* Assume all planes are in canonical from (see canon_plane()). */
    if (plane_cls.contains(tplane)) {
      Vector<CoplanarCluster> &curcls = plane_cls.lookup(tplane);
      if (dbg_level > 0) {
        std::cout << "already has " << curcls.size() << " clusters\n";
      }
      /* Partition `curcls` into those that intersect t non-trivially, and those that don't. */
      Vector<CoplanarCluster *> int_cls;
      Vector<CoplanarCluster *> no_int_cls;
      for (CoplanarCluster &cl : curcls) {
        if (dbg_level > 1) {
          std::cout << "consider intersecting with cluster " << cl << "\n";
        }
        if (bbs_might_intersect(tri_bb[t], cl.bounding_box())) {
          if (dbg_level > 1) {
            std::cout << "append to int_cls\n";
          }
          int_cls.append(&cl);
        }
        else {
          if (dbg_level > 1) {
            std::cout << "append to no_int_cls\n";
          }
          no_int_cls.append(&cl);
        }
      }
      if (int_cls.is_empty()) {
        /* t doesn't intersect any existing cluster in its plane, so make one just for it. */
        if (dbg_level > 1) {
          std::cout << "no intersecting clusters for t, make a new one\n";
        }
        curcls.append(CoplanarCluster(t, tri_bb[t]));
      }
      else if (int_cls.size() == 1) {
        /* t intersects exactly one existing cluster, so can add t to that cluster. */
        if (dbg_level > 1) {
          std::cout << "exactly one existing cluster, " << int_cls[0] << ", adding to it\n";
        }
        int_cls[0]->add_tri(t, tri_bb[t]);
      }
      else {
        /* t intersections 2 or more existing clusters: need to merge them and replace all the
         * originals with the merged one in `curcls`. */
        if (dbg_level > 1) {
          std::cout << "merging\n";
        }
        CoplanarCluster mergecl;
        mergecl.add_tri(t, tri_bb[t]);
        for (CoplanarCluster *cl : int_cls) {
          for (int t : *cl) {
            mergecl.add_tri(t, tri_bb[t]);
          }
        }
        Vector<CoplanarCluster> newvec;
        newvec.append(mergecl);
        for (CoplanarCluster *cl_no_int : no_int_cls) {
          newvec.append(*cl_no_int);
        }
        plane_cls.add_overwrite(tplane, newvec);
      }
    }
    else {
      if (dbg_level > 0) {
        std::cout << "first cluster for its plane\n";
      }
      plane_cls.add_new(tplane, Vector<CoplanarCluster>{CoplanarCluster(t, tri_bb[t])});
    }
  }
  /* Does this give deterministic order for cluster ids? I think so, since
   * hash for planes is on their values, not their addresses. */
  for (auto item : plane_cls.items()) {
    for (const CoplanarCluster &cl : item.value) {
      if (cl.tot_tri() > 1) {
        ans.add_cluster(cl);
      }
    }
  }

  return ans;
}

/* Data and functions to test triangle degeneracy in parallel. */
struct DegenData {
  const IMesh &tm;
};

struct DegenChunkData {
  bool has_degenerate_tri = false;
};

static void degenerate_range_func(void *__restrict userdata,
                                  const int iter,
                                  const TaskParallelTLS *__restrict tls)
{
  DegenData *data = static_cast<DegenData *>(userdata);
  DegenChunkData *chunk_data = static_cast<DegenChunkData *>(tls->userdata_chunk);
  const Face *f = data->tm.face(iter);
  bool is_degenerate = face_is_degenerate(f);
  chunk_data->has_degenerate_tri |= is_degenerate;
}

static void degenerate_reduce(const void *__restrict /*userdata*/,
                              void *__restrict chunk_join,
                              void *__restrict chunk)
{
  DegenChunkData *degen_chunk_join = static_cast<DegenChunkData *>(chunk_join);
  DegenChunkData *degen_chunk = static_cast<DegenChunkData *>(chunk);
  degen_chunk_join->has_degenerate_tri |= degen_chunk->has_degenerate_tri;
}

/* Does triangle #IMesh tm have any triangles with zero area? */
static bool has_degenerate_tris(const IMesh &tm)
{
  DegenData degen_data = {tm};
  DegenChunkData degen_chunk_data;
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.userdata_chunk = &degen_chunk_data;
  settings.userdata_chunk_size = sizeof(degen_chunk_data);
  settings.func_reduce = degenerate_reduce;
  settings.min_iter_per_thread = 1000;
  settings.use_threading = intersect_use_threading;
  BLI_task_parallel_range(0, tm.face_size(), &degen_data, degenerate_range_func, &settings);
  return degen_chunk_data.has_degenerate_tri;
}

static IMesh remove_degenerate_tris(const IMesh &tm_in)
{
  IMesh ans;
  Vector<Face *> new_faces;
  new_faces.reserve(tm_in.face_size());
  for (Face *f : tm_in.faces()) {
    if (!face_is_degenerate(f)) {
      new_faces.append(f);
    }
  }
  ans.set_faces(new_faces);
  return ans;
}

IMesh trimesh_self_intersect(const IMesh &tm_in, IMeshArena *arena)
{
  return trimesh_nary_intersect(tm_in, 1, [](int /*t*/) { return 0; }, true, arena);
}

IMesh trimesh_nary_intersect(const IMesh &tm_in,
                             int nshapes,
                             const FunctionRef<int(int)> shape_fn,
                             bool use_self,
                             IMeshArena *arena)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "\nTRIMESH_NARY_INTERSECT nshapes=" << nshapes << " use_self=" << use_self
              << "\n";
    for (const Face *f : tm_in.faces()) {
      BLI_assert(f->is_tri());
      UNUSED_VARS_NDEBUG(f);
    }
    if (dbg_level > 1) {
      std::cout << "input mesh:\n" << tm_in;
      for (int t : tm_in.face_index_range()) {
        std::cout << "shape(" << t << ") = " << shape_fn(tm_in.face(t)->orig) << "\n";
      }
      write_obj_mesh(const_cast<IMesh &>(tm_in), "trimesh_input");
    }
  }
#  ifdef PERFDEBUG
  perfdata_init();
  double start_time = BLI_time_now_seconds();
  std::cout << "trimesh_nary_intersect start\n";
#  endif
  /* Usually can use tm_in but if it has degenerate or illegal triangles,
   * then need to work on a copy of it without those triangles. */
  const IMesh *tm_clean = &tm_in;
  IMesh tm_cleaned;
  if (has_degenerate_tris(tm_in)) {
    if (dbg_level > 0) {
      std::cout << "cleaning degenerate triangles\n";
    }
    tm_cleaned = remove_degenerate_tris(tm_in);
    tm_clean = &tm_cleaned;
    if (dbg_level > 1) {
      std::cout << "cleaned input mesh:\n" << tm_cleaned;
    }
  }
#  ifdef PERFDEBUG
  double clean_time = BLI_time_now_seconds();
  std::cout << "cleaned, time = " << clean_time - start_time << "\n";
#  endif
  Array<BoundingBox> tri_bb = calc_face_bounding_boxes(*tm_clean);
#  ifdef PERFDEBUG
  double bb_calc_time = BLI_time_now_seconds();
  std::cout << "bbs calculated, time = " << bb_calc_time - clean_time << "\n";
#  endif
  TriOverlaps tri_ov(*tm_clean, tri_bb, nshapes, shape_fn, use_self);
#  ifdef PERFDEBUG
  double overlap_time = BLI_time_now_seconds();
  std::cout << "intersect overlaps calculated, time = " << overlap_time - bb_calc_time << "\n";
#  endif
  Array<IMesh> tri_subdivided(tm_clean->face_size(), NoInitialization());
  threading::parallel_for(tm_clean->face_index_range(), 1024, [&](IndexRange range) {
    for (int t : range) {
      if (tri_ov.first_overlap_index(t) != -1) {
        tm_clean->face(t)->populate_plane(true);
      }
      new (static_cast<void *>(&tri_subdivided[t])) IMesh;
    }
  });
#  ifdef PERFDEBUG
  double plane_populate = BLI_time_now_seconds();
  std::cout << "planes populated, time = " << plane_populate - overlap_time << "\n";
#  endif
  /* itt_map((a,b)) will hold the intersection value resulting from intersecting
   * triangles with indices a and b, where a < b. */
  Map<std::pair<int, int>, ITT_value> itt_map;
  itt_map.reserve(tri_ov.overlap().size());
  calc_overlap_itts(itt_map, *tm_clean, tri_ov, arena);
#  ifdef PERFDEBUG
  double itt_time = BLI_time_now_seconds();
  std::cout << "itts found, time = " << itt_time - plane_populate << "\n";
#  endif
  CoplanarClusterInfo clinfo = find_clusters(*tm_clean, tri_bb, itt_map);
  if (dbg_level > 1) {
    std::cout << clinfo;
  }
#  ifdef PERFDEBUG
  double find_cluster_time = BLI_time_now_seconds();
  std::cout << "clusters found, time = " << find_cluster_time - itt_time << "\n";
  doperfmax(0, tm_in.face_size());
  doperfmax(1, clinfo.tot_cluster());
  doperfmax(2, tri_ov.overlap().size());
#  endif
  calc_subdivided_non_cluster_tris(tri_subdivided, *tm_clean, itt_map, clinfo, tri_ov, arena);
#  ifdef PERFDEBUG
  double subdivided_tris_time = BLI_time_now_seconds();
  std::cout << "subdivided non-cluster tris found, time = " << subdivided_tris_time - itt_time
            << "\n";
#  endif
  Array<CDT_data> cluster_subdivided(clinfo.tot_cluster());
  for (int c : clinfo.index_range()) {
    cluster_subdivided[c] = calc_cluster_subdivided(clinfo, c, *tm_clean, tri_ov, itt_map, arena);
  }
#  ifdef PERFDEBUG
  double cluster_subdivide_time = BLI_time_now_seconds();
  std::cout << "subdivided clusters found, time = "
            << cluster_subdivide_time - subdivided_tris_time << "\n";
#  endif
  calc_cluster_tris(tri_subdivided, *tm_clean, clinfo, cluster_subdivided, arena);
#  ifdef PERFDEBUG
  double extract_time = BLI_time_now_seconds();
  std::cout << "subdivided cluster tris found, time = " << extract_time - cluster_subdivide_time
            << "\n";
#  endif
  IMesh combined = union_tri_subdivides(tri_subdivided);
  if (dbg_level > 1) {
    std::cout << "TRIMESH_NARY_INTERSECT answer:\n";
    std::cout << combined;
  }
#  ifdef PERFDEBUG
  double end_time = BLI_time_now_seconds();
  std::cout << "triangles combined, time = " << end_time - extract_time << "\n";
  std::cout << "trimesh_nary_intersect done, total time = " << end_time - start_time << "\n";
  dump_perfdata();
#  endif
  return combined;
}

static std::ostream &operator<<(std::ostream &os, const CoplanarCluster &cl)
{
  os << "cl(";
  bool first = true;
  for (const int t : cl) {
    if (first) {
      first = false;
    }
    else {
      os << ",";
    }
    os << t;
  }
  os << ")";
  return os;
}

static std::ostream &operator<<(std::ostream &os, const CoplanarClusterInfo &clinfo)
{
  os << "Coplanar Cluster Info:\n";
  for (int c : clinfo.index_range()) {
    os << c << ": " << clinfo.cluster(c) << "\n";
  }
  return os;
}

static std::ostream &operator<<(std::ostream &os, const ITT_value &itt)
{
  switch (itt.kind) {
    case INONE:
      os << "none";
      break;
    case IPOINT:
      os << "point " << itt.p1;
      break;
    case ISEGMENT:
      os << "segment " << itt.p1 << " " << itt.p2;
      break;
    case ICOPLANAR:
      os << "co-planar t" << itt.t_source;
      break;
  }
  return os;
}

void write_obj_mesh(IMesh &m, const std::string &objname)
{
  /* Would like to use #BKE_tempdir_base() here, but that brings in dependence on kernel library.
   * This is just for developer debugging anyway,
   * and should never be called in production Blender. */
#  ifdef _WIN_32
  const char *objdir = BLI_dir_home();
  if (objdir == nullptr) {
    std::cout << "Could not access home directory\n";
    return;
  }
#  else
  const char *objdir = "/tmp/";
#  endif

  if (m.face_size() == 0) {
    return;
  }

  std::string fname = std::string(objdir) + objname + std::string(".obj");
  std::ofstream f;
  f.open(fname);
  if (!f) {
    std::cout << "Could not open file " << fname << "\n";
    return;
  }

  if (!m.has_verts()) {
    m.populate_vert();
  }
  for (const Vert *v : m.vertices()) {
    const double3 dv = v->co;
    f << "v " << dv[0] << " " << dv[1] << " " << dv[2] << "\n";
  }
  for (const Face *face : m.faces()) {
    /* OBJ files use 1-indexing for vertices. */
    f << "f ";
    for (const Vert *v : *face) {
      int i = m.lookup_vert(v);
      BLI_assert(i != NO_INDEX);
      /* OBJ files use 1-indexing for vertices. */
      f << i + 1 << " ";
    }
    f << "\n";
  }
  f.close();
}

#  ifdef PERFDEBUG
struct PerfCounts {
  Vector<int> count;
  Vector<const char *> count_name;
  Vector<int> max;
  Vector<const char *> max_name;
};

static PerfCounts *perfdata = nullptr;

static void perfdata_init()
{
  perfdata = new PerfCounts;

  /* count 0. */
  perfdata->count.append(0);
  perfdata->count_name.append("Non-cluster overlaps");

  /* count 1. */
  perfdata->count.append(0);
  perfdata->count_name.append("intersect_tri_tri calls");

  /* count 2. */
  perfdata->count.append(0);
  perfdata->count_name.append("tri tri intersects decided by filter plane tests");

  /* count 3. */
  perfdata->count.append(0);
  perfdata->count_name.append("tri tri intersects decided by exact plane tests");

  /* count 4. */
  perfdata->count.append(0);
  perfdata->count_name.append("final non-NONE intersects");

  /* max 0. */
  perfdata->max.append(0);
  perfdata->max_name.append("total faces");

  /* max 1. */
  perfdata->max.append(0);
  perfdata->max_name.append("total clusters");

  /* max 2. */
  perfdata->max.append(0);
  perfdata->max_name.append("total overlaps");
}

static void incperfcount(int countnum)
{
  perfdata->count[countnum]++;
}

static void bumpperfcount(int countnum, int amt)
{
  perfdata->count[countnum] += amt;
}

static void doperfmax(int maxnum, int val)
{
  perfdata->max[maxnum] = max_ii(perfdata->max[maxnum], val);
}

static void dump_perfdata()
{
  std::cout << "\nPERFDATA\n";
  for (int i : perfdata->count.index_range()) {
    std::cout << perfdata->count_name[i] << " = " << perfdata->count[i] << "\n";
  }
  for (int i : perfdata->max.index_range()) {
    std::cout << perfdata->max_name[i] << " = " << perfdata->max[i] << "\n";
  }
  delete perfdata;
}
#  endif

}  // namespace blender::meshintersect

#endif  // WITH_GMP
