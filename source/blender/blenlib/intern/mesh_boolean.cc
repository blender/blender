/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#ifdef WITH_GMP

#  include <algorithm>
#  include <atomic>
#  include <fstream>
#  include <iostream>

#  include "BLI_array.hh"
#  include "BLI_assert.h"
#  include "BLI_hash.hh"
#  include "BLI_kdopbvh.hh"
#  include "BLI_map.hh"
#  include "BLI_math_boolean.hh"
#  include "BLI_math_geom.h"
#  include "BLI_math_mpq.hh"
#  include "BLI_math_vector.hh"
#  include "BLI_math_vector_mpq_types.hh"
#  include "BLI_mesh_intersect.hh"
#  include "BLI_set.hh"
#  include "BLI_span.hh"
#  include "BLI_stack.hh"
#  include "BLI_task.hh"
#  include "BLI_vector.hh"

#  include "BLI_mesh_boolean.hh"

#  ifdef WITH_TBB
#    include <tbb/parallel_reduce.h>
#    include <tbb/spin_mutex.h>
#  endif

#  ifdef _WIN_32
#    include "BLI_fileops.h"
#  endif

// #  define PERFDEBUG

namespace blender::meshintersect {

/**
 * Edge as two `const` Vert *'s, in a canonical order (lower vert id first).
 * We use the Vert id field for hashing to get algorithms
 * that yield predictable results from run-to-run and machine-to-machine.
 */
class Edge {
  const Vert *v_[2] = {nullptr, nullptr};

 public:
  Edge() = default;
  Edge(const Vert *v0, const Vert *v1)
  {
    if (v0->id <= v1->id) {
      v_[0] = v0;
      v_[1] = v1;
    }
    else {
      v_[0] = v1;
      v_[1] = v0;
    }
  }

  const Vert *v0() const
  {
    return v_[0];
  }

  const Vert *v1() const
  {
    return v_[1];
  }

  const Vert *operator[](int i) const
  {
    return v_[i];
  }

  bool operator==(Edge other) const
  {
    return v_[0]->id == other.v_[0]->id && v_[1]->id == other.v_[1]->id;
  }

  uint64_t hash() const
  {
    return get_default_hash(v_[0]->id, v_[1]->id);
  }
};

static std::ostream &operator<<(std::ostream &os, const Edge &e)
{
  if (e.v0() == nullptr) {
    BLI_assert(e.v1() == nullptr);
    os << "(null,null)";
  }
  else {
    os << "(" << e.v0() << "," << e.v1() << ")";
  }
  return os;
}

static std::ostream &operator<<(std::ostream &os, const Span<int> a)
{
  for (int i : a.index_range()) {
    os << a[i];
    if (i != a.size() - 1) {
      os << " ";
    }
  }
  return os;
}

static std::ostream &operator<<(std::ostream &os, const Array<int> &iarr)
{
  os << Span<int>(iarr);
  return os;
}

/** Holds information about topology of an #IMesh that is all triangles. */
class TriMeshTopology : NonCopyable {
  /** Triangles that contain a given Edge (either order). */
  Map<Edge, Vector<int> *> edge_tri_;
  /** Edges incident on each vertex. */
  Map<const Vert *, Vector<Edge>> vert_edges_;

 public:
  TriMeshTopology(const IMesh &tm);
  ~TriMeshTopology();

  /* If e is manifold, return index of the other triangle (not t) that has it.
   * Else return NO_INDEX. */
  int other_tri_if_manifold(Edge e, int t) const
  {
    const auto *p = edge_tri_.lookup_ptr(e);
    if (p != nullptr && (*p)->size() == 2) {
      return ((**p)[0] == t) ? (**p)[1] : (**p)[0];
    }
    return NO_INDEX;
  }

  /* Which triangles share edge e (in either orientation)? */
  const Vector<int> *edge_tris(Edge e) const
  {
    return edge_tri_.lookup_default(e, nullptr);
  }

  /* Which edges are incident on the given vertex?
   * We assume v has some incident edges. */
  Span<Edge> vert_edges(const Vert *v) const
  {
    return vert_edges_.lookup(v);
  }

  Map<Edge, Vector<int> *>::ItemIterator edge_tri_map_items() const
  {
    return edge_tri_.items();
  }
};

TriMeshTopology::TriMeshTopology(const IMesh &tm)
{
  const int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "TRIMESHTOPOLOGY CONSTRUCTION\n";
  }
  /* If everything were manifold, `F+V-E=2` and `E=3F/2`.
   * So an likely overestimate, allowing for non-manifoldness, is `E=2F` and `V=F`. */
  const int estimate_num_edges = 2 * tm.face_size();
  const int estimate_verts_num = tm.face_size();
  edge_tri_.reserve(estimate_num_edges);
  vert_edges_.reserve(estimate_verts_num);
  for (int t : tm.face_index_range()) {
    const Face &tri = *tm.face(t);
    BLI_assert(tri.is_tri());
    for (int i = 0; i < 3; ++i) {
      const Vert *v = tri[i];
      const Vert *vnext = tri[(i + 1) % 3];
      Edge e(v, vnext);
      Vector<Edge> *edges = vert_edges_.lookup_ptr(v);
      if (edges == nullptr) {
        vert_edges_.add_new(v, Vector<Edge>());
        edges = vert_edges_.lookup_ptr(v);
        BLI_assert(edges != nullptr);
      }
      edges->append_non_duplicates(e);

      auto *p = edge_tri_.lookup_ptr(Edge(v, vnext));
      if (p == nullptr) {
        edge_tri_.add_new(e, new Vector<int>{t});
      }
      else {
        (*p)->append_non_duplicates(t);
      }
    }
  }
  /* Debugging. */
  if (dbg_level > 0) {
    std::cout << "After TriMeshTopology construction\n";
    for (auto item : edge_tri_.items()) {
      std::cout << "tris for edge " << item.key << ": " << *item.value << "\n";
      constexpr bool print_stats = false;
      if (print_stats) {
        edge_tri_.print_stats("");
      }
    }
    for (auto item : vert_edges_.items()) {
      std::cout << "edges for vert " << item.key << ":\n";
      for (const Edge &e : item.value) {
        std::cout << "  " << e << "\n";
      }
      std::cout << "\n";
    }
  }
}

TriMeshTopology::~TriMeshTopology()
{
  Vector<Vector<int> *> values;

  /* Deconstructing is faster in parallel, so it is worth building an array of things to delete. */
  for (auto *item : edge_tri_.values()) {
    values.append(item);
  }

  threading::parallel_for(values.index_range(), 256, [&](IndexRange range) {
    for (int i : range) {
      delete values[i];
    }
  });
}

/** A Patch is a maximal set of triangles that share manifold edges only. */
class Patch {
  Vector<int> tri_; /* Indices of triangles in the Patch. */

 public:
  Patch() = default;

  void add_tri(int t)
  {
    tri_.append(t);
  }

  int tot_tri() const
  {
    return tri_.size();
  }

  int tri(int i) const
  {
    return tri_[i];
  }

  IndexRange tri_range() const
  {
    return IndexRange(tri_.size());
  }

  Span<int> tris() const
  {
    return Span<int>(tri_);
  }

  int cell_above{NO_INDEX};
  int cell_below{NO_INDEX};
  int component{NO_INDEX};
};

static std::ostream &operator<<(std::ostream &os, const Patch &patch)
{
  os << "Patch " << patch.tris();
  if (patch.cell_above != NO_INDEX) {
    os << " cell_above=" << patch.cell_above;
  }
  else {
    os << " cell_above not set";
  }
  if (patch.cell_below != NO_INDEX) {
    os << " cell_below=" << patch.cell_below;
  }
  else {
    os << " cell_below not set";
  }
  return os;
}

class PatchesInfo {
  /** All of the Patches for a #IMesh. */
  Vector<Patch> patch_;
  /** Patch index for corresponding triangle. */
  Array<int> tri_patch_;
  /** Shared edge for incident patches; (-1, -1) if none. */
  Map<std::pair<int, int>, Edge> pp_edge_;

 public:
  explicit PatchesInfo(int ntri)
  {
    constexpr int max_expected_patch_patch_incidences = 100;
    tri_patch_ = Array<int>(ntri, NO_INDEX);
    pp_edge_.reserve(max_expected_patch_patch_incidences);
  }

  int tri_patch(int t) const
  {
    return tri_patch_[t];
  }

  int add_patch()
  {
    int patch_index = patch_.append_and_get_index(Patch());
    return patch_index;
  }

  void grow_patch(int patch_index, int t)
  {
    tri_patch_[t] = patch_index;
    patch_[patch_index].add_tri(t);
  }

  bool tri_is_assigned(int t) const
  {
    return tri_patch_[t] != NO_INDEX;
  }

  const Patch &patch(int patch_index) const
  {
    return patch_[patch_index];
  }

  Patch &patch(int patch_index)
  {
    return patch_[patch_index];
  }

  int tot_patch() const
  {
    return patch_.size();
  }

  IndexRange index_range() const
  {
    return IndexRange(patch_.size());
  }

  const Patch *begin() const
  {
    return patch_.begin();
  }

  const Patch *end() const
  {
    return patch_.end();
  }

  Patch *begin()
  {
    return patch_.begin();
  }

  Patch *end()
  {
    return patch_.end();
  }

  void add_new_patch_patch_edge(int p1, int p2, Edge e)
  {
    pp_edge_.add_new(std::pair<int, int>(p1, p2), e);
    pp_edge_.add_new(std::pair<int, int>(p2, p1), e);
  }

  Edge patch_patch_edge(int p1, int p2)
  {
    return pp_edge_.lookup_default(std::pair<int, int>(p1, p2), Edge());
  }

  const Map<std::pair<int, int>, Edge> &patch_patch_edge_map()
  {
    return pp_edge_;
  }
};

static bool apply_bool_op(BoolOpType bool_optype, const Array<int> &winding);

/**
 * A Cell is a volume of 3-space, surrounded by patches.
 * We will partition all 3-space into Cells.
 * One cell, the Ambient cell, contains all other cells.
 */
class Cell {
  Set<int> patches_;
  Array<int> winding_;
  int merged_to_{NO_INDEX};
  bool winding_assigned_{false};
  /* in_output_volume_ will be true when this cell should be in the output volume. */
  bool in_output_volume_{false};
  /* zero_volume_ will be true when this is a zero-volume cell (inside a stack of identical
   * triangles). */
  bool zero_volume_{false};

 public:
  Cell() = default;

  void add_patch(int p)
  {
    patches_.add(p);
    zero_volume_ = false; /* If it was true before, it no longer is. */
  }

  const Set<int> &patches() const
  {
    return patches_;
  }

  /** In a set of 2, which is patch that is not p? */
  int patch_other(int p) const
  {
    if (patches_.size() != 2) {
      return NO_INDEX;
    }
    for (int pother : patches_) {
      if (pother != p) {
        return pother;
      }
    }
    return NO_INDEX;
  }

  Span<int> winding() const
  {
    return Span<int>(winding_);
  }

  void init_winding(int winding_len)
  {
    winding_ = Array<int>(winding_len);
  }

  void seed_ambient_winding()
  {
    winding_.fill(0);
    winding_assigned_ = true;
  }

  void set_winding_and_in_output_volume(const Cell &from_cell,
                                        int shape,
                                        int delta,
                                        BoolOpType bool_optype)
  {
    std::copy(from_cell.winding().begin(), from_cell.winding().end(), winding_.begin());
    if (shape >= 0) {
      winding_[shape] += delta;
    }
    winding_assigned_ = true;
    in_output_volume_ = apply_bool_op(bool_optype, winding_);
  }

  bool in_output_volume() const
  {
    return in_output_volume_;
  }

  bool winding_assigned() const
  {
    return winding_assigned_;
  }

  bool zero_volume() const
  {
    return zero_volume_;
  }

  int merged_to() const
  {
    return merged_to_;
  }

  void set_merged_to(int c)
  {
    merged_to_ = c;
  }

  /**
   * Call this when it is possible that this Cell has zero volume,
   * and if it does, set zero_volume_ to true.
   */
  void check_for_zero_volume(const PatchesInfo &pinfo, const IMesh &mesh);
};

static std::ostream &operator<<(std::ostream &os, const Cell &cell)
{
  os << "Cell patches";
  for (int p : cell.patches()) {
    std::cout << " " << p;
  }
  if (cell.winding().size() > 0) {
    os << " winding=" << cell.winding();
    os << " in_output_volume=" << cell.in_output_volume();
  }
  os << " zv=" << cell.zero_volume();
  std::cout << "\n";
  return os;
}

static bool tris_have_same_verts(const IMesh &mesh, int t1, int t2)
{
  const Face &tri1 = *mesh.face(t1);
  const Face &tri2 = *mesh.face(t2);
  BLI_assert(tri1.size() == 3 && tri2.size() == 3);
  if (tri1.vert[0] == tri2.vert[0]) {
    return ((tri1.vert[1] == tri2.vert[1] && tri1.vert[2] == tri2.vert[2]) ||
            (tri1.vert[1] == tri2.vert[2] && tri1.vert[2] == tri2.vert[1]));
  }
  if (tri1.vert[0] == tri2.vert[1]) {
    return ((tri1.vert[1] == tri2.vert[0] && tri1.vert[2] == tri2.vert[2]) ||
            (tri1.vert[1] == tri2.vert[2] && tri1.vert[2] == tri2.vert[0]));
  }
  if (tri1.vert[0] == tri2.vert[2]) {
    return ((tri1.vert[1] == tri2.vert[0] && tri1.vert[2] == tri2.vert[1]) ||
            (tri1.vert[1] == tri2.vert[1] && tri1.vert[2] == tri2.vert[0]));
  }
  return false;
}

/**
 * A Cell will have zero volume if it is bounded by exactly two patches and those
 * patches are geometrically identical triangles (perhaps flipped versions of each other).
 * If this Cell has zero volume, set its zero_volume_ member to true.
 */
void Cell::check_for_zero_volume(const PatchesInfo &pinfo, const IMesh &mesh)
{
  if (patches_.size() == 2) {
    int p1_index = NO_INDEX;
    int p2_index = NO_INDEX;
    for (int p : patches_) {
      if (p1_index == NO_INDEX) {
        p1_index = p;
      }
      else {
        p2_index = p;
      }
    }
    BLI_assert(p1_index != NO_INDEX && p2_index != NO_INDEX);
    const Patch &p1 = pinfo.patch(p1_index);
    const Patch &p2 = pinfo.patch(p2_index);
    if (p1.tot_tri() == 1 && p2.tot_tri() == 1) {
      if (tris_have_same_verts(mesh, p1.tri(0), p2.tri(0))) {
        zero_volume_ = true;
      }
    }
  }
}

/* Information about all the Cells. */
class CellsInfo {
  Vector<Cell> cell_;

 public:
  CellsInfo() = default;

  int add_cell()
  {
    int index = cell_.append_and_get_index(Cell());
    return index;
  }

  Cell &cell(int c)
  {
    return cell_[c];
  }

  const Cell &cell(int c) const
  {
    return cell_[c];
  }

  int tot_cell() const
  {
    return cell_.size();
  }

  IndexRange index_range() const
  {
    return cell_.index_range();
  }

  const Cell *begin() const
  {
    return cell_.begin();
  }

  const Cell *end() const
  {
    return cell_.end();
  }

  Cell *begin()
  {
    return cell_.begin();
  }

  Cell *end()
  {
    return cell_.end();
  }

  void init_windings(int winding_len)
  {
    for (Cell &cell : cell_) {
      cell.init_winding(winding_len);
    }
  }
};

/**
 * For Debugging: write an `.obj` file showing the patch/cell structure or just the cells.
 */
static void write_obj_cell_patch(const IMesh &m,
                                 const CellsInfo &cinfo,
                                 const PatchesInfo &pinfo,
                                 bool cells_only,
                                 const std::string &name)
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

  std::string fname = std::string(objdir) + name + std::string("_cellpatch.obj");
  std::ofstream f;
  f.open(fname);
  if (!f) {
    std::cout << "Could not open file " << fname << "\n";
    return;
  }

  /* Copy IMesh so can populate verts. */
  IMesh mm = m;
  mm.populate_vert();
  f << "o cellpatch\n";
  for (const Vert *v : mm.vertices()) {
    const double3 dv = v->co;
    f << "v " << dv[0] << " " << dv[1] << " " << dv[2] << "\n";
  }
  if (!cells_only) {
    for (int p : pinfo.index_range()) {
      f << "g patch" << p << "\n";
      const Patch &patch = pinfo.patch(p);
      for (int t : patch.tris()) {
        const Face &tri = *mm.face(t);
        f << "f ";
        for (const Vert *v : tri) {
          f << mm.lookup_vert(v) + 1 << " ";
        }
        f << "\n";
      }
    }
  }
  for (int c : cinfo.index_range()) {
    f << "g cell" << c << "\n";
    const Cell &cell = cinfo.cell(c);
    for (int p : cell.patches()) {
      const Patch &patch = pinfo.patch(p);
      for (int t : patch.tris()) {
        const Face &tri = *mm.face(t);
        f << "f ";
        for (const Vert *v : tri) {
          f << mm.lookup_vert(v) + 1 << " ";
        }
        f << "\n";
      }
    }
  }
  f.close();
}

static void merge_cells(int merge_to, int merge_from, CellsInfo &cinfo, PatchesInfo &pinfo)
{
  if (merge_to == merge_from) {
    return;
  }
  Cell &merge_from_cell = cinfo.cell(merge_from);
  Cell &merge_to_cell = cinfo.cell(merge_to);
  int final_merge_to = merge_to;
  while (merge_to_cell.merged_to() != NO_INDEX) {
    final_merge_to = merge_to_cell.merged_to();
    merge_to_cell = cinfo.cell(final_merge_to);
  }
  for (int cell_p : merge_from_cell.patches()) {
    merge_to_cell.add_patch(cell_p);
    Patch &patch = pinfo.patch(cell_p);
    if (patch.cell_above == merge_from) {
      patch.cell_above = merge_to;
    }
    if (patch.cell_below == merge_from) {
      patch.cell_below = merge_to;
    }
  }
  merge_from_cell.set_merged_to(final_merge_to);
}

/**
 * Partition the triangles of \a tm into Patches.
 */
static PatchesInfo find_patches(const IMesh &tm, const TriMeshTopology &tmtopo)
{
  const int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "\nFIND_PATCHES\n";
  }
  int ntri = tm.face_size();
  PatchesInfo pinfo(ntri);
  /* Algorithm: Grow patches across manifold edges as long as there are unassigned triangles. */
  Stack<int> cur_patch_grow;

  /* Create an Array containing indices of adjacent faces. */
  Array<std::array<int, 3>> t_others(tm.face_size());
  threading::parallel_for(tm.face_index_range(), 2048, [&](IndexRange range) {
    for (int t : range) {
      const Face &tri = *tm.face(t);
      for (int i = 0; i < 3; ++i) {
        Edge e(tri[i], tri[(i + 1) % 3]);
        t_others[t][i] = tmtopo.other_tri_if_manifold(e, t);
      }
    }
  });
  for (int t : tm.face_index_range()) {
    if (pinfo.tri_patch(t) == -1) {
      cur_patch_grow.push(t);
      int cur_patch_index = pinfo.add_patch();
      while (!cur_patch_grow.is_empty()) {
        int tcand = cur_patch_grow.pop();
        if (dbg_level > 1) {
          std::cout << "pop tcand = " << tcand << "; assigned = " << pinfo.tri_is_assigned(tcand)
                    << "\n";
        }
        if (pinfo.tri_is_assigned(tcand)) {
          continue;
        }
        if (dbg_level > 1) {
          std::cout << "grow patch from seed tcand=" << tcand << "\n";
        }
        pinfo.grow_patch(cur_patch_index, tcand);
        const Face &tri = *tm.face(tcand);
        for (int i = 0; i < 3; ++i) {
          Edge e(tri[i], tri[(i + 1) % 3]);
          int t_other = t_others[tcand][i];
          if (dbg_level > 1) {
            std::cout << "  edge " << e << " generates t_other=" << t_other << "\n";
          }
          if (t_other != NO_INDEX) {
            if (!pinfo.tri_is_assigned(t_other)) {
              if (dbg_level > 1) {
                std::cout << "    push t_other = " << t_other << "\n";
              }
              cur_patch_grow.push(t_other);
            }
          }
          else {
            /* e is non-manifold. Set any patch-patch incidences we can. */
            if (dbg_level > 1) {
              std::cout << "    e non-manifold case\n";
            }
            const Vector<int> *etris = tmtopo.edge_tris(e);
            if (etris != nullptr) {
              for (int i : etris->index_range()) {
                int t_other = (*etris)[i];
                if (t_other != tcand && pinfo.tri_is_assigned(t_other)) {
                  int p_other = pinfo.tri_patch(t_other);
                  if (p_other == cur_patch_index) {
                    continue;
                  }
                  if (pinfo.patch_patch_edge(cur_patch_index, p_other).v0() == nullptr) {
                    pinfo.add_new_patch_patch_edge(cur_patch_index, p_other, e);
                    if (dbg_level > 1) {
                      std::cout << "added patch_patch_edge (" << cur_patch_index << "," << p_other
                                << ") = " << e << "\n";
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  if (dbg_level > 0) {
    std::cout << "\nafter FIND_PATCHES: found " << pinfo.tot_patch() << " patches\n";
    for (int p : pinfo.index_range()) {
      std::cout << p << ": " << pinfo.patch(p) << "\n";
    }
    if (dbg_level > 1) {
      std::cout << "\ntriangle map\n";
      for (int t : tm.face_index_range()) {
        std::cout << t << ": " << tm.face(t) << " patch " << pinfo.tri_patch(t) << "\n";
      }
    }
    std::cout << "\npatch-patch incidences\n";
    for (int p1 : pinfo.index_range()) {
      for (int p2 : pinfo.index_range()) {
        Edge e = pinfo.patch_patch_edge(p1, p2);
        if (e.v0() != nullptr) {
          std::cout << "p" << p1 << " and p" << p2 << " share edge " << e << "\n";
        }
      }
    }
  }
  return pinfo;
}

/**
 * If e is an edge in tri, return the vertex that isn't part of tri,
 * the "flap" vertex, or nullptr if e is not part of tri.
 * Also, e may be reversed in tri.
 * Set *r_rev to true if it is reversed, else false.
 */
static const Vert *find_flap_vert(const Face &tri, const Edge e, bool *r_rev)
{
  *r_rev = false;
  const Vert *flapv;
  if (tri[0] == e.v0()) {
    if (tri[1] == e.v1()) {
      *r_rev = false;
      flapv = tri[2];
    }
    else {
      if (tri[2] != e.v1()) {
        return nullptr;
      }
      *r_rev = true;
      flapv = tri[1];
    }
  }
  else if (tri[1] == e.v0()) {
    if (tri[2] == e.v1()) {
      *r_rev = false;
      flapv = tri[0];
    }
    else {
      if (tri[0] != e.v1()) {
        return nullptr;
      }
      *r_rev = true;
      flapv = tri[2];
    }
  }
  else {
    if (tri[2] != e.v0()) {
      return nullptr;
    }
    if (tri[0] == e.v1()) {
      *r_rev = false;
      flapv = tri[1];
    }
    else {
      if (tri[1] != e.v1()) {
        return nullptr;
      }
      *r_rev = true;
      flapv = tri[0];
    }
  }
  return flapv;
}

/**
 * Triangle \a tri and tri0 share edge e.
 * Classify \a tri with respect to tri0 as described in
 * sort_tris_around_edge, and return 1, 2, 3, or 4 as \a tri is:
 * (1) co-planar with tri0 and on same side of e
 * (2) co-planar with tri0 and on opposite side of e
 * (3) below plane of tri0
 * (4) above plane of tri0
 * For "above" and "below", we use the orientation of non-reversed
 * orientation of tri0.
 * Because of the way the intersect mesh was made, we can assume
 * that if a triangle is in class 1 then it is has the same flap vert
 * as tri0.
 */
static int sort_tris_class(const Face &tri, const Face &tri0, const Edge e)
{
  const int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "classify  e = " << e << "\n";
  }
  mpq3 a0 = tri0[0]->co_exact;
  mpq3 a1 = tri0[1]->co_exact;
  mpq3 a2 = tri0[2]->co_exact;
  bool rev;
  bool rev0;
  const Vert *flapv0 = find_flap_vert(tri0, e, &rev0);
  const Vert *flapv = find_flap_vert(tri, e, &rev);
  if (dbg_level > 0) {
    std::cout << " t0 = " << tri0[0] << " " << tri0[1] << " " << tri0[2];
    std::cout << " rev0 = " << rev0 << " flapv0 = " << flapv0 << "\n";
    std::cout << " t = " << tri[0] << " " << tri[1] << " " << tri[2];
    std::cout << " rev = " << rev << " flapv = " << flapv << "\n";
  }
  BLI_assert(flapv != nullptr && flapv0 != nullptr);
  const mpq3 flap = flapv->co_exact;
  /* orient will be positive if flap is below oriented plane of a0,a1,a2. */
  int orient = orient3d(a0, a1, a2, flap);
  int ans;
  if (orient > 0) {
    ans = rev0 ? 4 : 3;
  }
  else if (orient < 0) {
    ans = rev0 ? 3 : 4;
  }
  else {
    ans = flapv == flapv0 ? 1 : 2;
  }
  if (dbg_level > 0) {
    std::cout << " orient = " << orient << " ans = " << ans << "\n";
  }
  return ans;
}

constexpr int EXTRA_TRI_INDEX = INT_MAX;

/**
 * To ensure consistent ordering of co-planar triangles if they happen to be sorted around
 * more than one edge, sort the triangle indices in g (in place) by their index -- but also apply
 * a sign to the index: positive if the triangle has edge e in the same orientation,
 * otherwise negative.
 */
static void sort_by_signed_triangle_index(Vector<int> &g,
                                          const Edge e,
                                          const IMesh &tm,
                                          const Face *extra_tri)
{
  Array<int> signed_g(g.size());
  for (int i : g.index_range()) {
    const Face &tri = g[i] == EXTRA_TRI_INDEX ? *extra_tri : *tm.face(g[i]);
    bool rev;
    find_flap_vert(tri, e, &rev);
    signed_g[i] = rev ? -g[i] : g[i];
  }
  std::sort(signed_g.begin(), signed_g.end());

  for (int i : g.index_range()) {
    g[i] = abs(signed_g[i]);
  }
}

/**
 * Sort the triangles \a tris, which all share edge e, as they appear
 * geometrically clockwise when looking down edge e.
 * Triangle t0 is the first triangle in the top-level call
 * to this recursive routine. The merge step below differs
 * for the top level call and all the rest, so this distinguishes those cases.
 * Care is taken in the case of duplicate triangles to have
 * an ordering that is consistent with that which would happen
 * if another edge of the triangle were sorted around.
 *
 * We sometimes need to do this with an extra triangle that is not part of tm.
 * To accommodate this:
 * If extra_tri is non-null, then an index of EXTRA_TRI_INDEX should use it for the triangle.
 */
static Array<int> sort_tris_around_edge(
    const IMesh &tm, const Edge e, const Span<int> tris, const int t0, const Face *extra_tri)
{
  /* Divide and conquer, quick-sort-like sort.
   * Pick a triangle t0, then partition into groups:
   * (1) co-planar with t0 and on same side of e
   * (2) co-planar with t0 and on opposite side of e
   * (3) below plane of t0
   * (4) above plane of t0
   * Each group is sorted and then the sorts are merged to give the answer.
   * We don't expect the input array to be very large - should typically
   * be only 3 or 4 - so OK to make copies of arrays instead of swapping
   * around in a single array. */
  const int dbg_level = 0;
  if (tris.is_empty()) {
    return Array<int>();
  }
  if (dbg_level > 0) {
    if (t0 == tris[0]) {
      std::cout << "\n";
    }
    std::cout << "sort_tris_around_edge " << e << "\n";
    std::cout << "tris = " << tris << "\n";
    std::cout << "t0 = " << t0 << "\n";
  }
  Vector<int> g1{tris[0]};
  Vector<int> g2;
  Vector<int> g3;
  Vector<int> g4;
  std::array<Vector<int> *, 4> groups = {&g1, &g2, &g3, &g4};
  const Face &triref = *tm.face(tris[0]);
  for (int i : tris.index_range()) {
    if (i == 0) {
      continue;
    }
    int t = tris[i];
    BLI_assert(t < tm.face_size() || (t == EXTRA_TRI_INDEX && extra_tri != nullptr));
    const Face &tri = (t == EXTRA_TRI_INDEX) ? *extra_tri : *tm.face(t);
    if (dbg_level > 2) {
      std::cout << "classifying tri " << t << " with respect to " << tris[0] << "\n";
    }
    int group_num = sort_tris_class(tri, triref, e);
    if (dbg_level > 2) {
      std::cout << "  classify result : " << group_num << "\n";
    }
    groups[group_num - 1]->append(t);
  }
  if (dbg_level > 1) {
    std::cout << "g1 = " << g1 << "\n";
    std::cout << "g2 = " << g2 << "\n";
    std::cout << "g3 = " << g3 << "\n";
    std::cout << "g4 = " << g4 << "\n";
  }
  if (g1.size() > 1) {
    sort_by_signed_triangle_index(g1, e, tm, extra_tri);
    if (dbg_level > 1) {
      std::cout << "g1 sorted: " << g1 << "\n";
    }
  }
  if (g2.size() > 1) {
    sort_by_signed_triangle_index(g2, e, tm, extra_tri);
    if (dbg_level > 1) {
      std::cout << "g2 sorted: " << g2 << "\n";
    }
  }
  if (g3.size() > 1) {
    Array<int> g3sorted = sort_tris_around_edge(tm, e, g3, t0, extra_tri);
    std::copy(g3sorted.begin(), g3sorted.end(), g3.begin());
    if (dbg_level > 1) {
      std::cout << "g3 sorted: " << g3 << "\n";
    }
  }
  if (g4.size() > 1) {
    Array<int> g4sorted = sort_tris_around_edge(tm, e, g4, t0, extra_tri);
    std::copy(g4sorted.begin(), g4sorted.end(), g4.begin());
    if (dbg_level > 1) {
      std::cout << "g4 sorted: " << g4 << "\n";
    }
  }
  int group_tot_size = g1.size() + g2.size() + g3.size() + g4.size();
  Array<int> ans(group_tot_size);
  int *p = ans.begin();
  if (tris[0] == t0) {
    p = std::copy(g1.begin(), g1.end(), p);
    p = std::copy(g4.begin(), g4.end(), p);
    p = std::copy(g2.begin(), g2.end(), p);
    std::copy(g3.begin(), g3.end(), p);
  }
  else {
    p = std::copy(g3.begin(), g3.end(), p);
    p = std::copy(g1.begin(), g1.end(), p);
    p = std::copy(g4.begin(), g4.end(), p);
    std::copy(g2.begin(), g2.end(), p);
  }
  if (dbg_level > 0) {
    std::cout << "sorted tris = " << ans << "\n";
  }
  return ans;
}

/**
 * Find the Cells around edge e.
 * This possibly makes new cells in \a cinfo, and sets up the
 * bipartite graph edges between cells and patches.
 * Will modify \a pinfo and \a cinfo and the patches and cells they contain.
 */
static void find_cells_from_edge(const IMesh &tm,
                                 const TriMeshTopology &tmtopo,
                                 PatchesInfo &pinfo,
                                 CellsInfo &cinfo,
                                 const Edge e)
{
  const int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "FIND_CELLS_FROM_EDGE " << e << "\n";
  }
  const Vector<int> *edge_tris = tmtopo.edge_tris(e);
  BLI_assert(edge_tris != nullptr);
  Array<int> sorted_tris = sort_tris_around_edge(
      tm, e, Span<int>(*edge_tris), (*edge_tris)[0], nullptr);

  int n_edge_tris = edge_tris->size();
  Array<int> edge_patches(n_edge_tris);
  for (int i = 0; i < n_edge_tris; ++i) {
    edge_patches[i] = pinfo.tri_patch(sorted_tris[i]);
    if (dbg_level > 1) {
      std::cout << "edge_patches[" << i << "] = " << edge_patches[i] << "\n";
    }
  }
  for (int i = 0; i < n_edge_tris; ++i) {
    int inext = (i + 1) % n_edge_tris;
    int r_index = edge_patches[i];
    int rnext_index = edge_patches[inext];
    Patch &r = pinfo.patch(r_index);
    Patch &rnext = pinfo.patch(rnext_index);
    bool r_flipped;
    bool rnext_flipped;
    find_flap_vert(*tm.face(sorted_tris[i]), e, &r_flipped);
    find_flap_vert(*tm.face(sorted_tris[inext]), e, &rnext_flipped);
    int *r_follow_cell = r_flipped ? &r.cell_below : &r.cell_above;
    int *rnext_prev_cell = rnext_flipped ? &rnext.cell_above : &rnext.cell_below;
    if (dbg_level > 0) {
      std::cout << "process patch pair " << r_index << " " << rnext_index << "\n";
      std::cout << "  r_flipped = " << r_flipped << " rnext_flipped = " << rnext_flipped << "\n";
      std::cout << "  r_follow_cell (" << (r_flipped ? "below" : "above")
                << ") = " << *r_follow_cell << "\n";
      std::cout << "  rnext_prev_cell (" << (rnext_flipped ? "above" : "below")
                << ") = " << *rnext_prev_cell << "\n";
    }
    if (*r_follow_cell == NO_INDEX && *rnext_prev_cell == NO_INDEX) {
      /* Neither is assigned: make a new cell. */
      int c = cinfo.add_cell();
      *r_follow_cell = c;
      *rnext_prev_cell = c;
      Cell &cell = cinfo.cell(c);
      cell.add_patch(r_index);
      cell.add_patch(rnext_index);
      cell.check_for_zero_volume(pinfo, tm);
      if (dbg_level > 0) {
        std::cout << "  made new cell " << c << "\n";
        std::cout << "  p" << r_index << "." << (r_flipped ? "cell_below" : "cell_above") << " = c"
                  << c << "\n";
        std::cout << "  p" << rnext_index << "." << (rnext_flipped ? "cell_above" : "cell_below")
                  << " = c" << c << "\n";
      }
    }
    else if (*r_follow_cell != NO_INDEX && *rnext_prev_cell == NO_INDEX) {
      int c = *r_follow_cell;
      *rnext_prev_cell = c;
      Cell &cell = cinfo.cell(c);
      cell.add_patch(rnext_index);
      cell.check_for_zero_volume(pinfo, tm);
      if (dbg_level > 0) {
        std::cout << "  reuse r_follow: p" << rnext_index << "."
                  << (rnext_flipped ? "cell_above" : "cell_below") << " = c" << c << "\n";
      }
    }
    else if (*r_follow_cell == NO_INDEX && *rnext_prev_cell != NO_INDEX) {
      int c = *rnext_prev_cell;
      *r_follow_cell = c;
      Cell &cell = cinfo.cell(c);
      cell.add_patch(r_index);
      cell.check_for_zero_volume(pinfo, tm);
      if (dbg_level > 0) {
        std::cout << "  reuse rnext prev: rprev_p" << r_index << "."
                  << (r_flipped ? "cell_below" : "cell_above") << " = c" << c << "\n";
      }
    }
    else {
      if (*r_follow_cell != *rnext_prev_cell) {
        int follow_cell_num_patches = cinfo.cell(*r_follow_cell).patches().size();
        int prev_cell_num_patches = cinfo.cell(*rnext_prev_cell).patches().size();
        if (follow_cell_num_patches >= prev_cell_num_patches) {
          if (dbg_level > 0) {
            std::cout << " merge cell " << *rnext_prev_cell << " into cell " << *r_follow_cell
                      << "\n";
          }
          merge_cells(*r_follow_cell, *rnext_prev_cell, cinfo, pinfo);
        }
      }
      else {
        if (dbg_level > 0) {
          std::cout << " merge cell " << *r_follow_cell << " into cell " << *rnext_prev_cell
                    << "\n";
        }
        merge_cells(*rnext_prev_cell, *r_follow_cell, cinfo, pinfo);
      }
    }
  }
}

/**
 * Find the partition of 3-space into Cells.
 * This assigns the cell_above and cell_below for each Patch.
 */
static CellsInfo find_cells(const IMesh &tm, const TriMeshTopology &tmtopo, PatchesInfo &pinfo)
{
  const int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "\nFIND_CELLS\n";
  }
  CellsInfo cinfo;
  /* For each unique edge shared between patch pairs, process it. */
  Set<Edge> processed_edges;
  for (const auto item : pinfo.patch_patch_edge_map().items()) {
    int p = item.key.first;
    int q = item.key.second;
    if (p < q) {
      const Edge &e = item.value;
      if (!processed_edges.contains(e)) {
        processed_edges.add_new(e);
        find_cells_from_edge(tm, tmtopo, pinfo, cinfo, e);
      }
    }
  }
  /* Some patches may have no cells at this point. These are either:
   * (a) a closed manifold patch only incident on itself (sphere, torus, klein bottle, etc.).
   * (b) an open manifold patch only incident on itself (has non-manifold boundaries).
   * Make above and below cells for these patches. This will create a disconnected patch-cell
   * bipartite graph, which will have to be fixed later. */
  for (int p : pinfo.index_range()) {
    Patch &patch = pinfo.patch(p);
    if (patch.cell_above == NO_INDEX) {
      int c = cinfo.add_cell();
      patch.cell_above = c;
      Cell &cell = cinfo.cell(c);
      cell.add_patch(p);
    }
    if (patch.cell_below == NO_INDEX) {
      int c = cinfo.add_cell();
      patch.cell_below = c;
      Cell &cell = cinfo.cell(c);
      cell.add_patch(p);
    }
  }
  if (dbg_level > 0) {
    std::cout << "\nFIND_CELLS found " << cinfo.tot_cell() << " cells\nCells\n";
    for (int i : cinfo.index_range()) {
      std::cout << i << ": " << cinfo.cell(i) << "\n";
    }
    std::cout << "Patches\n";
    for (int i : pinfo.index_range()) {
      std::cout << i << ": " << pinfo.patch(i) << "\n";
    }
    if (dbg_level > 1) {
      write_obj_cell_patch(tm, cinfo, pinfo, false, "postfindcells");
    }
  }
  return cinfo;
}

/**
 * Find the connected patch components (connects are via intermediate cells), and put
 * component numbers in each patch.
 * Return a Vector of components - each a Vector of the patch ids in the component.
 */
static Vector<Vector<int>> find_patch_components(const CellsInfo &cinfo, PatchesInfo &pinfo)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "FIND_PATCH_COMPONENTS\n";
  }
  if (pinfo.tot_patch() == 0) {
    return Vector<Vector<int>>();
  }
  int current_component = 0;
  Array<bool> cell_processed(cinfo.tot_cell(), false);
  Stack<int> stack; /* Patch indices to visit. */
  Vector<Vector<int>> ans;
  for (int pstart : pinfo.index_range()) {
    Patch &patch_pstart = pinfo.patch(pstart);
    if (patch_pstart.component != NO_INDEX) {
      continue;
    }
    ans.append(Vector<int>());
    ans[current_component].append(pstart);
    stack.push(pstart);
    patch_pstart.component = current_component;
    while (!stack.is_empty()) {
      int p = stack.pop();
      Patch &patch = pinfo.patch(p);
      BLI_assert(patch.component == current_component);
      for (int c : {patch.cell_above, patch.cell_below}) {
        if (cell_processed[c]) {
          continue;
        }
        cell_processed[c] = true;
        for (int pn : cinfo.cell(c).patches()) {
          Patch &patch_neighbor = pinfo.patch(pn);
          if (patch_neighbor.component == NO_INDEX) {
            patch_neighbor.component = current_component;
            stack.push(pn);
            ans[current_component].append(pn);
          }
        }
      }
    }
    ++current_component;
  }
  if (dbg_level > 0) {
    std::cout << "found " << ans.size() << " components\n";
    for (int comp : ans.index_range()) {
      std::cout << comp << ": " << ans[comp] << "\n";
    }
  }
  return ans;
}

/**
 * Do all patches have cell_above and cell_below set?
 * Is the bipartite graph connected?
 */
static bool patch_cell_graph_ok(const CellsInfo &cinfo, const PatchesInfo &pinfo)
{
  for (int c : cinfo.index_range()) {
    const Cell &cell = cinfo.cell(c);
    if (cell.merged_to() != NO_INDEX) {
      continue;
    }
    if (cell.patches().is_empty()) {
      std::cout << "Patch/Cell graph disconnected at Cell " << c << " with no patches\n";
      return false;
    }
    for (int p : cell.patches()) {
      if (p >= pinfo.tot_patch()) {
        std::cout << "Patch/Cell graph has bad patch index at Cell " << c << "\n";
        return false;
      }
    }
  }
  for (int p : pinfo.index_range()) {
    const Patch &patch = pinfo.patch(p);
    if (patch.cell_above == NO_INDEX || patch.cell_below == NO_INDEX) {
      std::cout << "Patch/Cell graph disconnected at Patch " << p
                << " with one or two missing cells\n";
      return false;
    }
    if (patch.cell_above >= cinfo.tot_cell() || patch.cell_below >= cinfo.tot_cell()) {
      std::cout << "Patch/Cell graph has bad cell index at Patch " << p << "\n";
      return false;
    }
  }
  return true;
}

/**
 * Is trimesh tm PWN ("Piece-wise constant Winding Number")?
 * See Zhou et al. paper for exact definition, but roughly
 * means that the faces connect so as to form closed volumes.
 * The actual definition says that if you calculate the
 * generalized winding number of every point not exactly on
 * the mesh, it will always be an integer.
 * Necessary (but not sufficient) conditions that a mesh be PWN:
 *    No edges with a non-zero sum of incident face directions.
 * I think that cases like Klein bottles are likely to satisfy
 * this without being PWN. So this routine will be only
 * approximately right.
 */
static bool is_pwn(const IMesh &tm, const TriMeshTopology &tmtopo)
{
  constexpr int dbg_level = 0;
  std::atomic<bool> is_pwn = true;
  Vector<std::pair<Edge, Vector<int> *>> tris;

  for (auto item : tmtopo.edge_tri_map_items()) {
    tris.append(std::pair<Edge, Vector<int> *>(item.key, item.value));
  }

  threading::parallel_for(tris.index_range(), 2048, [&](IndexRange range) {
    if (!is_pwn.load()) {
      /* Early out if mesh is already determined to be non-pwn. */
      return;
    }

    for (int j : range) {
      const Edge &edge = tris[j].first;
      int tot_orient = 0;
      /* For each face t attached to edge, add +1 if the edge
       * is positively in t, and -1 if negatively in t. */
      for (int t : *tris[j].second) {
        const Face &face = *tm.face(t);
        BLI_assert(face.size() == 3);
        for (int i : face.index_range()) {
          if (face[i] == edge.v0()) {
            if (face[(i + 1) % 3] == edge.v1()) {
              ++tot_orient;
            }
            else {
              BLI_assert(face[(i + 3 - 1) % 3] == edge.v1());
              --tot_orient;
            }
          }
        }
      }
      if (tot_orient != 0) {
        if (dbg_level > 0) {
          std::cout << "edge causing non-pwn: " << edge << "\n";
        }
        is_pwn = false;
        break;
      }
    }
  });
  return is_pwn.load();
}

/**
 * Find which of the cells around edge e contains point p.
 * Do this by inserting a dummy triangle containing v and sorting the
 * triangles around the edge to find out where in the sort order
 * the dummy triangle lies, then finding which cell is between
 * the two triangles on either side of the dummy.
 */
static int find_cell_for_point_near_edge(const mpq3 &p,
                                         const Edge &e,
                                         const IMesh &tm,
                                         const TriMeshTopology &tmtopo,
                                         const PatchesInfo &pinfo,
                                         IMeshArena *arena)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "FIND_CELL_FOR_POINT_NEAR_EDGE, p=" << p << " e=" << e << "\n";
  }
  const Vector<int> *etris = tmtopo.edge_tris(e);
  const Vert *dummy_vert = arena->add_or_find_vert(p, NO_INDEX);
  const Face *dummy_tri = arena->add_face({e.v0(), e.v1(), dummy_vert},
                                          NO_INDEX,
                                          {NO_INDEX, NO_INDEX, NO_INDEX},
                                          {false, false, false});
  BLI_assert(etris != nullptr);
  Array<int> edge_tris(etris->size() + 1);
  std::copy(etris->begin(), etris->end(), edge_tris.begin());
  edge_tris[edge_tris.size() - 1] = EXTRA_TRI_INDEX;
  Array<int> sorted_tris = sort_tris_around_edge(tm, e, edge_tris, edge_tris[0], dummy_tri);
  if (dbg_level > 0) {
    std::cout << "sorted tris = " << sorted_tris << "\n";
  }
  int *p_sorted_dummy = std::find(sorted_tris.begin(), sorted_tris.end(), EXTRA_TRI_INDEX);
  BLI_assert(p_sorted_dummy != sorted_tris.end());
  int dummy_index = p_sorted_dummy - sorted_tris.begin();
  int prev_tri = (dummy_index == 0) ? sorted_tris[sorted_tris.size() - 1] :
                                      sorted_tris[dummy_index - 1];
  if (dbg_level > 0) {
    int next_tri = (dummy_index == sorted_tris.size() - 1) ? sorted_tris[0] :
                                                             sorted_tris[dummy_index + 1];
    std::cout << "prev tri to dummy = " << prev_tri << ";  next tri to dummy = " << next_tri
              << "\n";
  }
  const Patch &prev_patch = pinfo.patch(pinfo.tri_patch(prev_tri));
  if (dbg_level > 0) {
    std::cout << "prev_patch = " << prev_patch << "\n";
  }
  bool prev_flipped;
  find_flap_vert(*tm.face(prev_tri), e, &prev_flipped);
  int c = prev_flipped ? prev_patch.cell_below : prev_patch.cell_above;
  if (dbg_level > 0) {
    std::cout << "find_cell_for_point_near_edge returns " << c << "\n";
  }
  return c;
}

/**
 * Find the ambient cell -- that is, the cell that is outside
 * all other cells.
 * If component_patches != nullptr, restrict consideration to patches
 * in that vector.
 *
 * The method is to find an edge known to be on the convex hull
 * of the mesh, then insert a dummy triangle that has that edge
 * and a point known to be outside the whole mesh. Then sorting
 * the triangles around the edge will reveal where the dummy triangle
 * fits in that sorting order, and hence, the two adjacent patches
 * to the dummy triangle - thus revealing the cell that the point
 * known to be outside the whole mesh is in.
 */
static int find_ambient_cell(const IMesh &tm,
                             const Vector<int> *component_patches,
                             const TriMeshTopology &tmtopo,
                             const PatchesInfo &pinfo,
                             IMeshArena *arena)
{
  int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "FIND_AMBIENT_CELL\n";
  }
  /* First find a vertex with the maximum x value. */
  /* Prefer not to populate the verts in the #IMesh just for this. */
  const Vert *v_extreme;
  auto max_x_vert = [](const Vert *a, const Vert *b) {
    return (a->co_exact.x > b->co_exact.x) ? a : b;
  };
  if (component_patches == nullptr) {
    v_extreme = threading::parallel_reduce(
        tm.face_index_range(),
        2048,
        (*tm.face(0))[0],
        [&](IndexRange range, const Vert *init) {
          const Vert *ans = init;
          for (int i : range) {
            const Face *f = tm.face(i);
            for (const Vert *v : *f) {
              if (v->co_exact.x > ans->co_exact.x) {
                ans = v;
              }
            }
          }
          return ans;
        },
        max_x_vert);
  }
  else {
    if (dbg_level > 0) {
      std::cout << "restrict to patches " << *component_patches << "\n";
    }
    int p0 = (*component_patches)[0];
    v_extreme = threading::parallel_reduce(
        component_patches->index_range(),
        2048,
        (*tm.face(pinfo.patch(p0).tri(0)))[0],
        [&](IndexRange range, const Vert *init) {
          const Vert *ans = init;
          for (int pi : range) {
            int p = (*component_patches)[pi];
            const Vert *tris_ans = threading::parallel_reduce(
                IndexRange(pinfo.patch(p).tot_tri()),
                2048,
                init,
                [&](IndexRange tris_range, const Vert *t_init) {
                  const Vert *v_ans = t_init;
                  for (int i : tris_range) {
                    int t = pinfo.patch(p).tri(i);
                    const Face *f = tm.face(t);
                    for (const Vert *v : *f) {
                      if (v->co_exact.x > v_ans->co_exact.x) {
                        v_ans = v;
                      }
                    }
                  }
                  return v_ans;
                },
                max_x_vert);
            if (tris_ans->co_exact.x > ans->co_exact.x) {
              ans = tris_ans;
            }
          }
          return ans;
        },
        max_x_vert);
  }
  if (dbg_level > 0) {
    std::cout << "v_extreme = " << v_extreme << "\n";
  }
  /* Find edge attached to v_extreme with max absolute slope
   * when projected onto the XY plane. That edge is guaranteed to
   * be on the convex hull of the mesh. */
  const Span<Edge> edges = tmtopo.vert_edges(v_extreme);
  const mpq_class &extreme_x = v_extreme->co_exact.x;
  const mpq_class &extreme_y = v_extreme->co_exact.y;
  Edge ehull;
  mpq_class max_abs_slope = -1;
  for (Edge e : edges) {
    const Vert *v_other = (e.v0() == v_extreme) ? e.v1() : e.v0();
    const mpq3 &co_other = v_other->co_exact;
    mpq_class delta_x = co_other.x - extreme_x;
    if (delta_x == 0) {
      /* Vertical slope. */
      ehull = e;
      break;
    }
    mpq_class abs_slope = abs((co_other.y - extreme_y) / delta_x);
    if (abs_slope > max_abs_slope) {
      ehull = e;
      max_abs_slope = abs_slope;
    }
  }
  if (dbg_level > 0) {
    std::cout << "ehull = " << ehull << " slope = " << max_abs_slope << "\n";
  }
  /* Sort triangles around ehull, including a dummy triangle that include a known point in
   * ambient cell. */
  mpq3 p_in_ambient = v_extreme->co_exact;
  p_in_ambient.x += 1;
  int c_ambient = find_cell_for_point_near_edge(p_in_ambient, ehull, tm, tmtopo, pinfo, arena);
  if (dbg_level > 0) {
    std::cout << "FIND_AMBIENT_CELL returns " << c_ambient << "\n";
  }
  return c_ambient;
}

/**
 * We need an edge on the convex hull of the edges incident on \a closestp
 * in order to sort around, including a dummy triangle that has \a testp and
 * the sorting edge vertices. So we don't want an edge that is co-linear
 * with the line through \a testp and \a closestp.
 * The method is to project onto a plane that contains `testp-closestp`,
 * and then choose the edge that, when projected, has the maximum absolute
 * slope (regarding the line `testp-closestp` as the x-axis for slope computation).
 */
static Edge find_good_sorting_edge(const Vert *testp,
                                   const Vert *closestp,
                                   const TriMeshTopology &tmtopo)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "FIND_GOOD_SORTING_EDGE testp = " << testp << ", closestp = " << closestp << "\n";
  }
  /* We want to project the edges incident to closestp onto a plane
   * whose ordinate direction will be regarded as going from closestp to testp,
   * and whose abscissa direction is some perpendicular to that.
   * A perpendicular direction can be found by swapping two coordinates
   * and negating one, and zeroing out the third, being careful that one
   * of the swapped vertices is non-zero. */
  const mpq3 &co_closest = closestp->co_exact;
  const mpq3 &co_test = testp->co_exact;
  BLI_assert(co_test != co_closest);
  mpq3 abscissa = co_test - co_closest;
  /* Find a non-zero-component axis of abscissa. */
  int axis;
  for (axis = 0; axis < 3; ++axis) {
    if (abscissa[axis] != 0) {
      break;
    }
  }
  BLI_assert(axis < 3);
  int axis_next = (axis + 1) % 3;
  int axis_next_next = (axis_next + 1) % 3;
  mpq3 ordinate;
  ordinate[axis] = abscissa[axis_next];
  ordinate[axis_next] = -abscissa[axis];
  ordinate[axis_next_next] = 0;
  /* By construction, dot(abscissa, ordinate) == 0, so they are perpendicular. */
  mpq3 normal = math::cross(abscissa, ordinate);
  if (dbg_level > 0) {
    std::cout << "abscissa = " << abscissa << "\n";
    std::cout << "ordinate = " << ordinate << "\n";
    std::cout << "normal = " << normal << "\n";
  }
  mpq_class nlen2 = math::length_squared(normal);
  mpq_class max_abs_slope = -1;
  Edge esort;
  const Span<Edge> edges = tmtopo.vert_edges(closestp);
  for (Edge e : edges) {
    const Vert *v_other = (e.v0() == closestp) ? e.v1() : e.v0();
    const mpq3 &co_other = v_other->co_exact;
    mpq3 evec = co_other - co_closest;
    /* Get projection of evec onto plane of abscissa and ordinate. */
    mpq3 proj_evec = evec - (math::dot(evec, normal) / nlen2) * normal;
    /* The projection calculations along the abscissa and ordinate should
     * be scaled by 1/abscissa and 1/ordinate respectively,
     * but we can skip: it won't affect which `evec` has the maximum slope. */
    mpq_class evec_a = math::dot(proj_evec, abscissa);
    mpq_class evec_o = math::dot(proj_evec, ordinate);
    if (dbg_level > 0) {
      std::cout << "e = " << e << "\n";
      std::cout << "v_other = " << v_other << "\n";
      std::cout << "evec = " << evec << ", proj_evec = " << proj_evec << "\n";
      std::cout << "evec_a = " << evec_a << ", evec_o = " << evec_o << "\n";
    }
    if (evec_a == 0) {
      /* evec is perpendicular to abscissa. */
      esort = e;
      if (dbg_level > 0) {
        std::cout << "perpendicular esort is " << esort << "\n";
      }
      break;
    }
    mpq_class abs_slope = abs(evec_o / evec_a);
    if (abs_slope > max_abs_slope) {
      esort = e;
      max_abs_slope = abs_slope;
      if (dbg_level > 0) {
        std::cout << "with abs_slope " << abs_slope << " new esort is " << esort << "\n";
      }
    }
  }
  return esort;
}

/**
 * Find the cell that contains v. Consider the cells adjacent to triangle t.
 * The close_edge and close_vert values are what were returned by
 * #closest_on_tri_to_point when determining that v was close to t.
 * They will indicate whether the point of closest approach to t is to
 * an edge of t, a vertex of t, or somewhere inside t.
 *
 * The algorithm is similar to the one for find_ambient_cell, except that
 * instead of an arbitrary point known to be outside the whole mesh, we
 * have a particular point (v) and we just want to determine the patches
 * that point is between in sorting-around-an-edge order.
 */
static int find_containing_cell(const Vert *v,
                                int t,
                                int close_edge,
                                int close_vert,
                                const PatchesInfo &pinfo,
                                const IMesh &tm,
                                const TriMeshTopology &tmtopo,
                                IMeshArena *arena)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "FIND_CONTAINING_CELL v=" << v << ", t=" << t << "\n";
  }
  const Face &tri = *tm.face(t);
  Edge etest;
  if (close_edge == -1 && close_vert == -1) {
    /* Choose any edge if closest point is inside the triangle. */
    close_edge = 0;
  }
  if (close_edge != -1) {
    const Vert *v0 = tri[close_edge];
    const Vert *v1 = tri[(close_edge + 1) % 3];
    const Span<Edge> edges = tmtopo.vert_edges(v0);
    if (dbg_level > 0) {
      std::cout << "look for edge containing " << v0 << " and " << v1 << "\n";
      std::cout << "  in edges: ";
      for (Edge e : edges) {
        std::cout << e << " ";
      }
      std::cout << "\n";
    }
    for (Edge e : edges) {
      if ((e.v0() == v0 && e.v1() == v1) || (e.v0() == v1 && e.v1() == v0)) {
        etest = e;
        break;
      }
    }
  }
  else {
    int cv = close_vert;
    const Vert *vert_cv = tri[cv];
    if (vert_cv == v) {
      /* Need to use another one to find sorting edge. */
      vert_cv = tri[(cv + 1) % 3];
      BLI_assert(vert_cv != v);
    }
    etest = find_good_sorting_edge(v, vert_cv, tmtopo);
  }
  BLI_assert(etest.v0() != nullptr);
  if (dbg_level > 0) {
    std::cout << "etest = " << etest << "\n";
  }
  int c = find_cell_for_point_near_edge(v->co_exact, etest, tm, tmtopo, pinfo, arena);
  if (dbg_level > 0) {
    std::cout << "find_containing_cell returns " << c << "\n";
  }
  return c;
}

/**
 * Find the closest point in triangle (a, b, c) to point p.
 * Return the distance squared to that point.
 * Also, if the closest point in the triangle is on a vertex,
 * return 0, 1, or 2 for a, b, c in *r_vert; else -1.
 * If the closest point is on an edge, return 0, 1, or 2
 * for edges ab, bc, or ca in *r_edge; else -1.
 * (Adapted from #closest_on_tri_to_point_v3()).
 * The arguments ab, ac, ..., r are used as temporaries
 * in this routine. Passing them in from the caller can
 * avoid many allocations and frees of temporary mpq3 values
 * and the mpq_class values within them.
 */
static mpq_class closest_on_tri_to_point(const mpq3 &p,
                                         const mpq3 &a,
                                         const mpq3 &b,
                                         const mpq3 &c,
                                         mpq3 &ab,
                                         mpq3 &ac,
                                         mpq3 &ap,
                                         mpq3 &bp,
                                         mpq3 &cp,
                                         mpq3 &m,
                                         mpq3 &r,
                                         int *r_edge,
                                         int *r_vert)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "CLOSEST_ON_TRI_TO_POINT p = " << p << "\n";
    std::cout << " a = " << a << ", b = " << b << ", c = " << c << "\n";
  }
  /* Check if p in vertex region outside a. */
  ab = b;
  ab -= a;
  ac = c;
  ac -= a;
  ap = p;
  ap -= a;

  mpq_class d1 = math::dot_with_buffer(ab, ap, m);
  mpq_class d2 = math::dot_with_buffer(ac, ap, m);
  if (d1 <= 0 && d2 <= 0) {
    /* Barycentric coordinates (1,0,0). */
    *r_edge = -1;
    *r_vert = 0;
    if (dbg_level > 0) {
      std::cout << "  answer = a\n";
    }
    return math::distance_squared_with_buffer(p, a, m);
  }
  /* Check if p in vertex region outside b. */
  bp = p;
  bp -= b;
  mpq_class d3 = math::dot_with_buffer(ab, bp, m);
  mpq_class d4 = math::dot_with_buffer(ac, bp, m);
  if (d3 >= 0 && d4 <= d3) {
    /* Barycentric coordinates (0,1,0). */
    *r_edge = -1;
    *r_vert = 1;
    if (dbg_level > 0) {
      std::cout << "  answer = b\n";
    }
    return math::distance_squared_with_buffer(p, b, m);
  }
  /* Check if p in region of ab. */
  mpq_class vc = d1 * d4 - d3 * d2;
  if (vc <= 0 && d1 >= 0 && d3 <= 0) {
    mpq_class v = d1 / (d1 - d3);
    /* Barycentric coordinates (1-v,v,0). */
    r = ab;
    r *= v;
    r += a;
    *r_vert = -1;
    *r_edge = 0;
    if (dbg_level > 0) {
      std::cout << "  answer = on ab at " << r << "\n";
    }
    return math::distance_squared_with_buffer(p, r, m);
  }
  /* Check if p in vertex region outside c. */
  cp = p;
  cp -= c;
  mpq_class d5 = math::dot_with_buffer(ab, cp, m);
  mpq_class d6 = math::dot_with_buffer(ac, cp, m);
  if (d6 >= 0 && d5 <= d6) {
    /* Barycentric coordinates (0,0,1). */
    *r_edge = -1;
    *r_vert = 2;
    if (dbg_level > 0) {
      std::cout << "  answer = c\n";
    }
    return math::distance_squared_with_buffer(p, c, m);
  }
  /* Check if p in edge region of ac. */
  mpq_class vb = d5 * d2 - d1 * d6;
  if (vb <= 0 && d2 >= 0 && d6 <= 0) {
    mpq_class w = d2 / (d2 - d6);
    /* Barycentric coordinates (1-w,0,w). */
    r = ac;
    r *= w;
    r += a;
    *r_vert = -1;
    *r_edge = 2;
    if (dbg_level > 0) {
      std::cout << "  answer = on ac at " << r << "\n";
    }
    return math::distance_squared_with_buffer(p, r, m);
  }
  /* Check if p in edge region of bc. */
  mpq_class va = d3 * d6 - d5 * d4;
  if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) {
    mpq_class w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
    /* Barycentric coordinates (0,1-w,w). */
    r = c;
    r -= b;
    r *= w;
    r += b;
    *r_vert = -1;
    *r_edge = 1;
    if (dbg_level > 0) {
      std::cout << "  answer = on bc at " << r << "\n";
    }
    return math::distance_squared_with_buffer(p, r, m);
  }
  /* p inside face region. Compute barycentric coordinates (u,v,w). */
  mpq_class denom = 1 / (va + vb + vc);
  mpq_class v = vb * denom;
  mpq_class w = vc * denom;
  ac *= w;
  r = ab;
  r *= v;
  r += a;
  r += ac;
  *r_vert = -1;
  *r_edge = -1;
  if (dbg_level > 0) {
    std::cout << "  answer = inside at " << r << "\n";
  }
  return math::distance_squared_with_buffer(p, r, m);
}

static float closest_on_tri_to_point_float_dist_squared(const float3 &p,
                                                        const double3 &a,
                                                        const double3 &b,
                                                        const double3 &c)
{
  float3 fa, fb, fc, closest;
  copy_v3fl_v3db(fa, a);
  copy_v3fl_v3db(fb, b);
  copy_v3fl_v3db(fc, c);
  closest_on_tri_to_point_v3(closest, p, fa, fb, fc);
  return len_squared_v3v3(p, closest);
}

struct ComponentContainer {
  int containing_component{NO_INDEX};
  int nearest_cell{NO_INDEX};
  mpq_class dist_to_cell;

  ComponentContainer(int cc, int cell, mpq_class d)
      : containing_component(cc), nearest_cell(cell), dist_to_cell(d)
  {
  }
};

/**
 * Find out all the components, not equal to comp, that contain a point
 * in comp in a non-ambient cell of those components.
 * In other words, find the components that comp is nested inside
 * (maybe not directly nested, which is why there can be more than one).
 */
static Vector<ComponentContainer> find_component_containers(int comp,
                                                            const Span<Vector<int>> components,
                                                            const Span<int> ambient_cell,
                                                            const IMesh &tm,
                                                            const PatchesInfo &pinfo,
                                                            const TriMeshTopology &tmtopo,
                                                            Array<BoundingBox> &comp_bb,
                                                            IMeshArena *arena)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "FIND_COMPONENT_CONTAINERS for comp " << comp << "\n";
  }
  Vector<ComponentContainer> ans;
  int test_p = components[comp][0];
  int test_t = pinfo.patch(test_p).tri(0);
  const Vert *test_v = tm.face(test_t)[0].vert[0];
  if (dbg_level > 0) {
    std::cout << "test vertex in comp: " << test_v << "\n";
  }
  const double3 &test_v_d = test_v->co;
  float3 test_v_f(test_v_d[0], test_v_d[1], test_v_d[2]);

  mpq3 buf[7];

  for (int comp_other : components.index_range()) {
    if (comp == comp_other) {
      continue;
    }
    if (dbg_level > 0) {
      std::cout << "comp_other = " << comp_other << "\n";
    }
    if (!bbs_might_intersect(comp_bb[comp], comp_bb[comp_other])) {
      if (dbg_level > 0) {
        std::cout << "bounding boxes don't overlap\n";
      }
      continue;
    }
    int nearest_tri = NO_INDEX;
    int nearest_tri_close_vert = -1;
    int nearest_tri_close_edge = -1;
    mpq_class nearest_tri_dist_squared;
    float nearest_tri_dist_squared_float = FLT_MAX;
    for (int p : components[comp_other]) {
      const Patch &patch = pinfo.patch(p);
      for (int t : patch.tris()) {
        const Face &tri = *tm.face(t);
        if (dbg_level > 1) {
          std::cout << "tri " << t << " = " << &tri << "\n";
        }
        int close_vert;
        int close_edge;
        /* Try a cheap float test first. */
        float d2_f = closest_on_tri_to_point_float_dist_squared(
            test_v_f, tri[0]->co, tri[1]->co, tri[2]->co);
        if (d2_f - FLT_EPSILON > nearest_tri_dist_squared_float) {
          continue;
        }
        mpq_class d2 = closest_on_tri_to_point(test_v->co_exact,
                                               tri[0]->co_exact,
                                               tri[1]->co_exact,
                                               tri[2]->co_exact,
                                               buf[0],
                                               buf[1],
                                               buf[2],
                                               buf[3],
                                               buf[4],
                                               buf[5],
                                               buf[6],
                                               &close_edge,
                                               &close_vert);
        if (dbg_level > 1) {
          std::cout << "  close_edge=" << close_edge << " close_vert=" << close_vert
                    << "  dsquared=" << d2.get_d() << "\n";
        }
        if (nearest_tri == NO_INDEX || d2 < nearest_tri_dist_squared) {
          nearest_tri = t;
          nearest_tri_close_edge = close_edge;
          nearest_tri_close_vert = close_vert;
          nearest_tri_dist_squared = d2;
          nearest_tri_dist_squared_float = d2_f;
        }
      }
    }
    if (dbg_level > 0) {
      std::cout << "closest tri to comp=" << comp << " in comp_other=" << comp_other << " is t"
                << nearest_tri << "\n";
      const Face *tn = tm.face(nearest_tri);
      std::cout << "tri = " << tn << "\n";
      std::cout << "  (" << tn->vert[0]->co << "," << tn->vert[1]->co << "," << tn->vert[2]->co
                << ")\n";
    }
    int containing_cell = find_containing_cell(test_v,
                                               nearest_tri,
                                               nearest_tri_close_edge,
                                               nearest_tri_close_vert,

                                               pinfo,
                                               tm,
                                               tmtopo,
                                               arena);
    if (dbg_level > 0) {
      std::cout << "containing cell = " << containing_cell << "\n";
    }
    if (containing_cell != ambient_cell[comp_other]) {
      ans.append(ComponentContainer(comp_other, containing_cell, nearest_tri_dist_squared));
    }
  }
  return ans;
}

/**
 * Populate the per-component bounding boxes, expanding them
 * by an appropriate epsilon so that we conservatively will say
 * that components could intersect if the BBs overlap.
 */
static void populate_comp_bbs(const Span<Vector<int>> components,
                              const PatchesInfo &pinfo,
                              const IMesh &im,
                              Array<BoundingBox> &comp_bb)
{
  const int comp_grainsize = 16;
  /* To get a good expansion epsilon, we need to find the maximum
   * absolute value of any coordinate. Do it first per component,
   * then get the overall max. */
  Array<double> max_abs(components.size(), 0.0);
  threading::parallel_for(components.index_range(), comp_grainsize, [&](IndexRange comp_range) {
    for (int c : comp_range) {
      BoundingBox &bb = comp_bb[c];
      double &maxa = max_abs[c];
      for (int p : components[c]) {
        const Patch &patch = pinfo.patch(p);
        for (int t : patch.tris()) {
          const Face &tri = *im.face(t);
          for (const Vert *v : tri) {
            bb.combine(v->co);
            for (int i = 0; i < 3; ++i) {
              maxa = max_dd(maxa, fabs(v->co[i]));
            }
          }
        }
      }
    }
  });
  double all_max_abs = 0.0;
  for (int c : components.index_range()) {
    all_max_abs = max_dd(all_max_abs, max_abs[c]);
  }
  constexpr float pad_factor = 10.0f;
  float pad = all_max_abs == 0.0 ? FLT_EPSILON : 2 * FLT_EPSILON * all_max_abs;
  pad *= pad_factor;
  for (int c : components.index_range()) {
    comp_bb[c].expand(pad);
  }
}

/**
 * The cells and patches are supposed to form a bipartite graph.
 * The graph may be disconnected (if parts of meshes are nested or side-by-side
 * without intersection with other each other).
 * Connect the bipartite graph. This involves discovering the connected components
 * of the patches, then the nesting structure of those components.
 */
static void finish_patch_cell_graph(const IMesh &tm,
                                    CellsInfo &cinfo,
                                    PatchesInfo &pinfo,
                                    const TriMeshTopology &tmtopo,
                                    IMeshArena *arena)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "FINISH_PATCH_CELL_GRAPH\n";
  }
  Vector<Vector<int>> components = find_patch_components(cinfo, pinfo);
  if (components.size() <= 1) {
    if (dbg_level > 0) {
      std::cout << "one component so finish_patch_cell_graph does no work\n";
    }
    return;
  }
  if (dbg_level > 0) {
    std::cout << "components:\n";
    for (int comp : components.index_range()) {
      std::cout << comp << ": " << components[comp] << "\n";
    }
  }
  Array<int> ambient_cell(components.size());
  for (int comp : components.index_range()) {
    ambient_cell[comp] = find_ambient_cell(tm, &components[comp], tmtopo, pinfo, arena);
  }
  if (dbg_level > 0) {
    std::cout << "ambient cells:\n";
    for (int comp : ambient_cell.index_range()) {
      std::cout << comp << ": " << ambient_cell[comp] << "\n";
    }
  }
  int tot_components = components.size();
  Array<Vector<ComponentContainer>> comp_cont(tot_components);
  if (tot_components > 1) {
    Array<BoundingBox> comp_bb(tot_components);
    populate_comp_bbs(components, pinfo, tm, comp_bb);
    for (int comp : components.index_range()) {
      comp_cont[comp] = find_component_containers(
          comp, components, ambient_cell, tm, pinfo, tmtopo, comp_bb, arena);
    }
    if (dbg_level > 0) {
      std::cout << "component containers:\n";
      for (int comp : comp_cont.index_range()) {
        std::cout << comp << ": ";
        for (const ComponentContainer &cc : comp_cont[comp]) {
          std::cout << "[containing_comp=" << cc.containing_component
                    << ", nearest_cell=" << cc.nearest_cell << ", d2=" << cc.dist_to_cell << "] ";
        }
        std::cout << "\n";
      }
    }
  }
  if (dbg_level > 1) {
    write_obj_cell_patch(tm, cinfo, pinfo, false, "beforemerge");
  }
  /* For nested components, merge their ambient cell with the nearest containing cell. */
  Vector<int> outer_components;
  for (int comp : comp_cont.index_range()) {
    if (comp_cont[comp].is_empty()) {
      outer_components.append(comp);
    }
    else {
      ComponentContainer &closest = comp_cont[comp][0];
      for (int i = 1; i < comp_cont[comp].size(); ++i) {
        if (comp_cont[comp][i].dist_to_cell < closest.dist_to_cell) {
          closest = comp_cont[comp][i];
        }
      }
      int comp_ambient = ambient_cell[comp];
      int cont_cell = closest.nearest_cell;
      if (dbg_level > 0) {
        std::cout << "merge comp " << comp << "'s ambient cell=" << comp_ambient << " to cell "
                  << cont_cell << "\n";
      }
      merge_cells(cont_cell, comp_ambient, cinfo, pinfo);
    }
  }
  /* For outer components (not nested in any other component), merge their ambient cells. */
  if (outer_components.size() > 1) {
    int merged_ambient = ambient_cell[outer_components[0]];
    for (int i = 1; i < outer_components.size(); ++i) {
      if (dbg_level > 0) {
        std::cout << "merge comp " << outer_components[i]
                  << "'s ambient cell=" << ambient_cell[outer_components[i]] << " to cell "
                  << merged_ambient << "\n";
      }
      merge_cells(merged_ambient, ambient_cell[outer_components[i]], cinfo, pinfo);
    }
  }
  if (dbg_level > 0) {
    std::cout << "after FINISH_PATCH_CELL_GRAPH\nCells\n";
    for (int i : cinfo.index_range()) {
      if (cinfo.cell(i).merged_to() == NO_INDEX) {
        std::cout << i << ": " << cinfo.cell(i) << "\n";
      }
    }
    std::cout << "Patches\n";
    for (int i : pinfo.index_range()) {
      std::cout << i << ": " << pinfo.patch(i) << "\n";
    }
    if (dbg_level > 1) {
      write_obj_cell_patch(tm, cinfo, pinfo, false, "finished");
    }
  }
}

/**
 * Starting with ambient cell c_ambient, with all zeros for winding numbers,
 * propagate winding numbers to all the other cells.
 * There will be a vector of \a nshapes winding numbers in each cell, one per
 * input shape.
 * As one crosses a patch into a new cell, the original shape (mesh part)
 * that patch was part of dictates which winding number changes.
 * The shape_fn(triangle_number) function should return the shape that the
 * triangle is part of.
 * Also, as soon as the winding numbers for a cell are set, use bool_optype
 * to decide whether that cell is included or excluded from the boolean output.
 * If included, the cell's in_output_volume will be set to true.
 */
static void propagate_windings_and_in_output_volume(PatchesInfo &pinfo,
                                                    CellsInfo &cinfo,
                                                    int c_ambient,
                                                    BoolOpType op,
                                                    int nshapes,
                                                    FunctionRef<int(int)> shape_fn)
{
  int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "PROPAGATE_WINDINGS, ambient cell = " << c_ambient << "\n";
  }
  Cell &cell_ambient = cinfo.cell(c_ambient);
  cell_ambient.seed_ambient_winding();
  /* Use a vector as a queue. It can't grow bigger than number of cells. */
  Vector<int> queue;
  queue.reserve(cinfo.tot_cell());
  int queue_head = 0;
  queue.append(c_ambient);
  while (queue_head < queue.size()) {
    int c = queue[queue_head++];
    if (dbg_level > 1) {
      std::cout << "process cell " << c << "\n";
    }
    Cell &cell = cinfo.cell(c);
    for (int p : cell.patches()) {
      Patch &patch = pinfo.patch(p);
      bool p_above_c = patch.cell_below == c;
      int c_neighbor = p_above_c ? patch.cell_above : patch.cell_below;
      if (dbg_level > 1) {
        std::cout << "  patch " << p << " p_above_c = " << p_above_c << "\n";
        std::cout << "    c_neighbor = " << c_neighbor << "\n";
      }
      Cell &cell_neighbor = cinfo.cell(c_neighbor);
      if (!cell_neighbor.winding_assigned()) {
        int winding_delta = p_above_c ? -1 : 1;
        int t = patch.tri(0);
        int shape = shape_fn(t);
        BLI_assert(shape < nshapes);
        UNUSED_VARS_NDEBUG(nshapes);
        if (dbg_level > 1) {
          std::cout << "    representative tri " << t << ": in shape " << shape << "\n";
        }
        cell_neighbor.set_winding_and_in_output_volume(cell, shape, winding_delta, op);
        if (dbg_level > 1) {
          std::cout << "    now cell_neighbor = " << cell_neighbor << "\n";
        }
        queue.append(c_neighbor);
        BLI_assert(queue.size() <= cinfo.tot_cell());
      }
    }
  }
  if (dbg_level > 0) {
    std::cout << "\nPROPAGATE_WINDINGS result\n";
    for (int i = 0; i < cinfo.tot_cell(); ++i) {
      std::cout << i << ": " << cinfo.cell(i) << "\n";
    }
  }
}

/**
 * Given an array of winding numbers, where the `i-th` entry is a cell's winding
 * number with respect to input shape (mesh part) i, return true if the
 * cell should be included in the output of the boolean operation.
 *   Intersection: all the winding numbers must be nonzero.
 *   Union: at least one winding number must be nonzero.
 *   Difference (first shape minus the rest): first winding number must be nonzero
 *      and the rest must have at least one zero winding number.
 */
static bool apply_bool_op(BoolOpType bool_optype, const Array<int> &winding)
{
  int nw = winding.size();
  BLI_assert(nw > 0);
  switch (bool_optype) {
    case BoolOpType::Intersect: {
      for (int i = 0; i < nw; ++i) {
        if (winding[i] == 0) {
          return false;
        }
      }
      return true;
    }
    case BoolOpType::Union: {
      for (int i = 0; i < nw; ++i) {
        if (winding[i] != 0) {
          return true;
        }
      }
      return false;
    }
    case BoolOpType::Difference: {
      /* if nw > 2, make it shape 0 minus the union of the rest. */
      if (winding[0] == 0) {
        return false;
      }
      if (nw == 1) {
        return true;
      }
      for (int i = 1; i < nw; ++i) {
        if (winding[i] >= 1) {
          return false;
        }
      }
      return true;
    }
    default:
      return false;
  }
}

/**
 * Special processing for extract_from_in_output_volume_diffs to handle
 * triangles that are part of stacks of geometrically identical
 * triangles enclosing zero volume cells.
 */
static void extract_zero_volume_cell_tris(Vector<Face *> &r_tris,
                                          const IMesh &tm_subdivided,
                                          const PatchesInfo &pinfo,
                                          const CellsInfo &cinfo,
                                          IMeshArena *arena)
{
  int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "extract_zero_volume_cell_tris\n";
  }
  /* Find patches that are adjacent to zero-volume cells. */
  Array<bool> adj_to_zv(pinfo.tot_patch());
  for (int p : pinfo.index_range()) {
    const Patch &patch = pinfo.patch(p);
    if (cinfo.cell(patch.cell_above).zero_volume() || cinfo.cell(patch.cell_below).zero_volume()) {
      adj_to_zv[p] = true;
    }
    else {
      adj_to_zv[p] = false;
    }
  }
  /* Partition the adj_to_zv patches into stacks. */
  Vector<Vector<int>> patch_stacks;
  Array<bool> allocated_to_stack(pinfo.tot_patch(), false);
  for (int p : pinfo.index_range()) {
    if (!adj_to_zv[p] || allocated_to_stack[p]) {
      continue;
    }
    int stack_index = patch_stacks.size();
    patch_stacks.append(Vector<int>{p});
    Vector<int> &stack = patch_stacks[stack_index];
    Vector<bool> flipped{false};
    allocated_to_stack[p] = true;
    /* We arbitrarily choose p's above and below directions as above and below for whole stack.
     * Triangles in the stack that don't follow that convention are marked with flipped = true.
     * The non-zero-volume cell above the whole stack, following this convention, is
     * above_stack_cell. The non-zero-volume cell below the whole stack is #below_stack_cell. */
    /* First, walk in the above_cell direction from p. */
    int pwalk = p;
    const Patch *pwalk_patch = &pinfo.patch(pwalk);
    int c = pwalk_patch->cell_above;
    const Cell *cell = &cinfo.cell(c);
    while (cell->zero_volume()) {
      /* In zero-volume cells, the cell should have exactly two patches. */
      BLI_assert(cell->patches().size() == 2);
      int pother = cell->patch_other(pwalk);
      bool flip = pinfo.patch(pother).cell_above == c;
      flipped.append(flip);
      stack.append(pother);
      allocated_to_stack[pother] = true;
      pwalk = pother;
      pwalk_patch = &pinfo.patch(pwalk);
      c = flip ? pwalk_patch->cell_below : pwalk_patch->cell_above;
      cell = &cinfo.cell(c);
    }
    const Cell *above_stack_cell = cell;
    /* Now walk in the below_cell direction from p. */
    pwalk = p;
    pwalk_patch = &pinfo.patch(pwalk);
    c = pwalk_patch->cell_below;
    cell = &cinfo.cell(c);
    while (cell->zero_volume()) {
      BLI_assert(cell->patches().size() == 2);
      int pother = cell->patch_other(pwalk);
      bool flip = pinfo.patch(pother).cell_below == c;
      flipped.append(flip);
      stack.append(pother);
      allocated_to_stack[pother] = true;
      pwalk = pother;
      pwalk_patch = &pinfo.patch(pwalk);
      c = flip ? pwalk_patch->cell_above : pwalk_patch->cell_below;
      cell = &cinfo.cell(c);
    }
    const Cell *below_stack_cell = cell;
    if (dbg_level > 0) {
      std::cout << "process zero-volume patch stack " << stack << "\n";
      std::cout << "flipped = ";
      for (bool b : flipped) {
        std::cout << b << " ";
      }
      std::cout << "\n";
    }
    if (above_stack_cell->in_output_volume() ^ below_stack_cell->in_output_volume()) {
      bool need_flipped_tri = above_stack_cell->in_output_volume();
      if (dbg_level > 0) {
        std::cout << "need tri " << (need_flipped_tri ? "flipped" : "") << "\n";
      }
      int t_to_add = NO_INDEX;
      for (int i : stack.index_range()) {
        if (flipped[i] == need_flipped_tri) {
          t_to_add = pinfo.patch(stack[i]).tri(0);
          if (dbg_level > 0) {
            std::cout << "using tri " << t_to_add << "\n";
          }
          r_tris.append(tm_subdivided.face(t_to_add));
          break;
        }
      }
      if (t_to_add == NO_INDEX) {
        const Face *f = tm_subdivided.face(pinfo.patch(p).tri(0));
        const Face &tri = *f;
        /* We need flipped version or else we would have found it above. */
        std::array<const Vert *, 3> flipped_vs = {tri[0], tri[2], tri[1]};
        std::array<int, 3> flipped_e_origs = {
            tri.edge_orig[2], tri.edge_orig[1], tri.edge_orig[0]};
        std::array<bool, 3> flipped_is_intersect = {
            tri.is_intersect[2], tri.is_intersect[1], tri.is_intersect[0]};
        Face *flipped_f = arena->add_face(
            flipped_vs, f->orig, flipped_e_origs, flipped_is_intersect);
        r_tris.append(flipped_f);
      }
    }
  }
}

/**
 * Extract the output mesh from tm_subdivided and return it as a new mesh.
 * The cells in \a cinfo must have cells-to-be-retained with in_output_volume set.
 * We keep only triangles between those in the output volume and those not in.
 * We flip the normals of any triangle that has an in_output_volume cell above
 * and a not-in_output_volume cell below.
 * For all stacks of exact duplicate co-planar triangles, we want to
 * include either one version of the triangle or none, depending on
 * whether the in_output_volume in_output_volumes on either side of the stack are
 * different or the same.
 */
static IMesh extract_from_in_output_volume_diffs(const IMesh &tm_subdivided,
                                                 const PatchesInfo &pinfo,
                                                 const CellsInfo &cinfo,
                                                 IMeshArena *arena)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "\nEXTRACT_FROM_FLAG_DIFFS\n";
  }
  Vector<Face *> out_tris;
  out_tris.reserve(tm_subdivided.face_size());
  bool any_zero_volume_cell = false;
  for (int t : tm_subdivided.face_index_range()) {
    int p = pinfo.tri_patch(t);
    const Patch &patch = pinfo.patch(p);
    const Cell &cell_above = cinfo.cell(patch.cell_above);
    const Cell &cell_below = cinfo.cell(patch.cell_below);
    if (dbg_level > 0) {
      std::cout << "tri " << t << ": cell_above=" << patch.cell_above
                << " cell_below=" << patch.cell_below << "\n";
      std::cout << " in_output_volume_above=" << cell_above.in_output_volume()
                << " in_output_volume_below=" << cell_below.in_output_volume() << "\n";
    }
    bool adjacent_zero_volume_cell = cell_above.zero_volume() || cell_below.zero_volume();
    any_zero_volume_cell |= adjacent_zero_volume_cell;
    if (cell_above.in_output_volume() ^ cell_below.in_output_volume() &&
        !adjacent_zero_volume_cell)
    {
      bool flip = cell_above.in_output_volume();
      if (dbg_level > 0) {
        std::cout << "need tri " << t << " flip=" << flip << "\n";
      }
      Face *f = tm_subdivided.face(t);
      if (flip) {
        Face &tri = *f;
        std::array<const Vert *, 3> flipped_vs = {tri[0], tri[2], tri[1]};
        std::array<int, 3> flipped_e_origs = {
            tri.edge_orig[2], tri.edge_orig[1], tri.edge_orig[0]};
        std::array<bool, 3> flipped_is_intersect = {
            tri.is_intersect[2], tri.is_intersect[1], tri.is_intersect[0]};
        Face *flipped_f = arena->add_face(
            flipped_vs, f->orig, flipped_e_origs, flipped_is_intersect);
        out_tris.append(flipped_f);
      }
      else {
        out_tris.append(f);
      }
    }
  }
  if (any_zero_volume_cell) {
    extract_zero_volume_cell_tris(out_tris, tm_subdivided, pinfo, cinfo, arena);
  }
  return IMesh(out_tris);
}

static const char *bool_optype_name(BoolOpType op)
{
  switch (op) {
    case BoolOpType::None:
      return "none";
    case BoolOpType::Intersect:
      return "intersect";
    case BoolOpType::Union:
      return "union";
    case BoolOpType::Difference:
      return "difference";
    default:
      return "<unknown>";
  }
}

static double3 calc_point_inside_tri_db(const Face &tri)
{
  const Vert *v0 = tri.vert[0];
  const Vert *v1 = tri.vert[1];
  const Vert *v2 = tri.vert[2];
  double3 ans = v0->co / 3 + v1->co / 3 + v2->co / 3;
  return ans;
}
class InsideShapeTestData {
 public:
  const IMesh &tm;
  FunctionRef<int(int)> shape_fn;
  int nshapes;
  /* A per-shape vector of parity of hits of that shape. */
  Array<int> hit_parity;

  InsideShapeTestData(const IMesh &tm, FunctionRef<int(int)> shape_fn, int nshapes)
      : tm(tm), shape_fn(shape_fn), nshapes(nshapes)
  {
  }
};

static void inside_shape_callback(void *userdata,
                                  int index,
                                  const BVHTreeRay *ray,
                                  BVHTreeRayHit * /*hit*/)
{
  const int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "inside_shape_callback, index = " << index << "\n";
  }
  InsideShapeTestData *data = static_cast<InsideShapeTestData *>(userdata);
  const Face &tri = *data->tm.face(index);
  int shape = data->shape_fn(tri.orig);
  if (shape == -1) {
    return;
  }
  float dist;
  float fv0[3];
  float fv1[3];
  float fv2[3];
  for (int i = 0; i < 3; ++i) {
    fv0[i] = float(tri.vert[0]->co[i]);
    fv1[i] = float(tri.vert[1]->co[i]);
    fv2[i] = float(tri.vert[2]->co[i]);
  }
  if (dbg_level > 0) {
    std::cout << "  fv0=(" << fv0[0] << "," << fv0[1] << "," << fv0[2] << ")\n";
    std::cout << "  fv1=(" << fv1[0] << "," << fv1[1] << "," << fv1[2] << ")\n";
    std::cout << "  fv2=(" << fv2[0] << "," << fv2[1] << "," << fv2[2] << ")\n";
  }
  if (isect_ray_tri_epsilon_v3(
          ray->origin, ray->direction, fv0, fv1, fv2, &dist, nullptr, FLT_EPSILON))
  {
    /* Count parity as +1 if ray is in the same direction as triangle's normal,
     * and -1 if the directions are opposite. */
    double3 o_db{double(ray->origin[0]), double(ray->origin[1]), double(ray->origin[2])};
    int parity = orient3d(tri.vert[0]->co, tri.vert[1]->co, tri.vert[2]->co, o_db);
    if (dbg_level > 0) {
      std::cout << "origin at " << o_db << ", parity = " << parity << "\n";
    }
    data->hit_parity[shape] += parity;
  }
}

/**
 * Test the triangle with index \a t_index to see which shapes it is inside,
 * and fill in \a in_shape with a confidence value between 0 and 1 that says
 * how likely we think it is that it is inside.
 * This is done by casting some rays from just on the positive side of a test
 * face in various directions and summing the parity of crossing faces of each face.
 *
 * \param tree: Contains all the triangles of \a tm and can be used for fast ray-casting.
 */
static void test_tri_inside_shapes(const IMesh &tm,
                                   FunctionRef<int(int)> shape_fn,
                                   int nshapes,
                                   int test_t_index,
                                   BVHTree *tree,
                                   Array<float> &in_shape)
{
  const int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "test_point_inside_shapes, t_index = " << test_t_index << "\n";
  }
  Face &tri_test = *tm.face(test_t_index);
  int shape = shape_fn(tri_test.orig);
  if (shape == -1) {
    in_shape.fill(0.0f);
    return;
  }
  double3 test_point = calc_point_inside_tri_db(tri_test);
  /* Offset the test point a tiny bit in the tri_test normal direction. */
  tri_test.populate_plane(false);
  double3 norm = math::normalize(tri_test.plane->norm);
  const double offset_amount = 1e-5;
  double3 offset_test_point = test_point + offset_amount * norm;
  if (dbg_level > 0) {
    std::cout << "test tri is in shape " << shape << "\n";
    std::cout << "test point = " << test_point << "\n";
    std::cout << "offset_test_point = " << offset_test_point << "\n";
  }
  /* Try six test rays almost along orthogonal axes.
   * Perturb their directions slightly to make it less likely to hit a seam.
   * Ray-cast assumes they have unit length, so use r1 near 1 and
   * ra near 0.5, and rb near .01, but normalized so `sqrt(r1^2 + ra^2 + rb^2) == 1`. */
  constexpr int rays_num = 6;
  constexpr float r1 = 0.9987025295199663f;
  constexpr float ra = 0.04993512647599832f;
  constexpr float rb = 0.009987025295199663f;
  const float test_rays[rays_num][3] = {
      {r1, ra, rb}, {-r1, -ra, -rb}, {rb, r1, ra}, {-rb, -r1, -ra}, {ra, rb, r1}, {-ra, -rb, -r1}};
  InsideShapeTestData data(tm, shape_fn, nshapes);
  data.hit_parity = Array<int>(nshapes, 0);
  Array<int> count_insides(nshapes, 0);
  const float co[3] = {
      float(offset_test_point[0]), float(offset_test_point[1]), float(offset_test_point[2])};
  for (int i = 0; i < rays_num; ++i) {
    if (dbg_level > 0) {
      std::cout << "shoot ray " << i << "(" << test_rays[i][0] << "," << test_rays[i][1] << ","
                << test_rays[i][2] << ")\n";
    }
    BLI_bvhtree_ray_cast_all(tree, co, test_rays[i], 0.0f, FLT_MAX, inside_shape_callback, &data);
    if (dbg_level > 0) {
      std::cout << "ray " << i << " result:";
      for (int j = 0; j < nshapes; ++j) {
        std::cout << " " << data.hit_parity[j];
      }
      std::cout << "\n";
    }
    for (int j = 0; j < nshapes; ++j) {
      if (j != shape && data.hit_parity[j] > 0) {
        ++count_insides[j];
      }
    }
    data.hit_parity.fill(0);
  }
  for (int j = 0; j < nshapes; ++j) {
    if (j == shape) {
      in_shape[j] = 1.0f; /* Let's say a shape is always inside itself. */
    }
    else {
      in_shape[j] = float(count_insides[j]) / float(rays_num);
    }
    if (dbg_level > 0) {
      std::cout << "shape " << j << " inside = " << in_shape[j] << "\n";
    }
  }
}

/**
 * Return a BVH Tree that contains all of the triangles of \a tm.
 * The caller must free it.
 * (We could possible reuse the BVH tree(s) built in TriOverlaps,
 * in the mesh intersect function. A future TODO.)
 */
static BVHTree *raycast_tree(const IMesh &tm)
{
  BVHTree *tree = BLI_bvhtree_new(tm.face_size(), FLT_EPSILON, 4, 6);
  for (int i : tm.face_index_range()) {
    const Face *f = tm.face(i);
    float t_cos[9];
    for (int j = 0; j < 3; ++j) {
      const Vert *v = f->vert[j];
      for (int k = 0; k < 3; ++k) {
        t_cos[3 * j + k] = float(v->co[k]);
      }
    }
    BLI_bvhtree_insert(tree, i, t_cos, 3);
  }
  BLI_bvhtree_balance(tree);
  return tree;
}

/**
 * Should a face with given shape and given winding array be removed for given boolean op?
 * Also return true in *r_do_flip if it retained by normals need to be flipped.
 */
static bool raycast_test_remove(BoolOpType op, Array<int> &winding, int shape, bool *r_do_flip)
{
  constexpr int dbg_level = 0;
  /* Find out the "in the output volume" flag for each of the cases of winding[shape] == 0
   * and winding[shape] == 1. If the flags are different, this patch should be in the output.
   * Also, if this is a Difference and the shape isn't the first one, need to flip the normals.
   */
  winding[shape] = 0;
  bool in_output_volume_0 = apply_bool_op(op, winding);
  winding[shape] = 1;
  bool in_output_volume_1 = apply_bool_op(op, winding);
  bool do_remove = in_output_volume_0 == in_output_volume_1;
  bool do_flip = !do_remove && op == BoolOpType::Difference && shape != 0;
  if (dbg_level > 0) {
    std::cout << "winding = ";
    for (int i = 0; i < winding.size(); ++i) {
      std::cout << winding[i] << " ";
    }
    std::cout << "\niv0=" << in_output_volume_0 << ", iv1=" << in_output_volume_1 << "\n";
    std::cout << " remove=" << do_remove << ", flip=" << do_flip << "\n";
  }
  *r_do_flip = do_flip;
  return do_remove;
}

/** Add triangle a flipped version of tri to out_faces. */
static void raycast_add_flipped(Vector<Face *> &out_faces, const Face &tri, IMeshArena *arena)
{

  Array<const Vert *> flipped_vs = {tri[0], tri[2], tri[1]};
  Array<int> flipped_e_origs = {tri.edge_orig[2], tri.edge_orig[1], tri.edge_orig[0]};
  Array<bool> flipped_is_intersect = {
      tri.is_intersect[2], tri.is_intersect[1], tri.is_intersect[0]};
  Face *flipped_f = arena->add_face(flipped_vs, tri.orig, flipped_e_origs, flipped_is_intersect);
  out_faces.append(flipped_f);
}

/**
 * Use the RayCast method for deciding if a triangle of the
 * mesh is supposed to be included or excluded in the boolean result,
 * and return the mesh that is the boolean result.
 * The reason this is done on a triangle-by-triangle basis is that
 * when the input is not PWN, some patches can be both inside and outside
 * some shapes (e.g., a plane cutting through Suzanne's open eyes).
 */
static IMesh raycast_tris_boolean(
    const IMesh &tm, BoolOpType op, int nshapes, FunctionRef<int(int)> shape_fn, IMeshArena *arena)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "RAYCAST_TRIS_BOOLEAN\n";
  }
  IMesh ans;
  BVHTree *tree = raycast_tree(tm);
  Vector<Face *> out_faces;
  out_faces.reserve(tm.face_size());
#  ifdef WITH_TBB
  tbb::spin_mutex mtx;
#  endif
  const int grainsize = 256;
  threading::parallel_for(IndexRange(tm.face_size()), grainsize, [&](IndexRange range) {
    Array<float> in_shape(nshapes, 0);
    Array<int> winding(nshapes, 0);
    for (int t : range) {
      Face &tri = *tm.face(t);
      int shape = shape_fn(tri.orig);
      if (dbg_level > 0) {
        std::cout << "process triangle " << t << " = " << &tri << "\n";
        std::cout << "shape = " << shape << "\n";
      }
      test_tri_inside_shapes(tm, shape_fn, nshapes, t, tree, in_shape);
      for (int other_shape = 0; other_shape < nshapes; ++other_shape) {
        if (other_shape == shape) {
          continue;
        }
        /* The in_shape array has a confidence value for "insideness".
         * For most operations, even a hint of being inside
         * gives good results, but when shape is a cutter in a Difference
         * operation, we want to be pretty sure that the point is inside other_shape.
         * E.g., #75827.
         * Also, when the operation is intersection, we also want high confidence.
         */
        bool need_high_confidence = (op == BoolOpType::Difference && shape != 0) ||
                                    op == BoolOpType::Intersect;
        bool inside = in_shape[other_shape] >= (need_high_confidence ? 0.5f : 0.1f);
        if (dbg_level > 0) {
          std::cout << "test point is " << (inside ? "inside" : "outside") << " other_shape "
                    << other_shape << " val = " << in_shape[other_shape] << "\n";
        }
        winding[other_shape] = inside;
      }
      bool do_flip;
      bool do_remove = raycast_test_remove(op, winding, shape, &do_flip);
      {
#  ifdef WITH_TBB
        tbb::spin_mutex::scoped_lock lock(mtx);
#  endif
        if (!do_remove) {
          if (!do_flip) {
            out_faces.append(&tri);
          }
          else {
            raycast_add_flipped(out_faces, tri, arena);
          }
        }
      }
    }
  });
  BLI_bvhtree_free(tree);
  ans.set_faces(out_faces);
  return ans;
}

/* This is (sometimes much faster) version of raycast boolean
 * that does it per patch rather than per triangle.
 * It may fail in cases where raycast_tri_boolean will succeed,
 * but the latter can be very slow on huge meshes. */
static IMesh raycast_patches_boolean(const IMesh &tm,
                                     BoolOpType op,
                                     int nshapes,
                                     FunctionRef<int(int)> shape_fn,
                                     const PatchesInfo &pinfo,
                                     IMeshArena *arena)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "RAYCAST_PATCHES_BOOLEAN\n";
  }
  IMesh ans;
  BVHTree *tree = raycast_tree(tm);
  Vector<Face *> out_faces;
  out_faces.reserve(tm.face_size());
  Array<float> in_shape(nshapes, 0);
  Array<int> winding(nshapes, 0);
  for (int p : pinfo.index_range()) {
    const Patch &patch = pinfo.patch(p);
    /* For test triangle, choose one in the middle of patch list
     * as the ones near the beginning may be very near other patches. */
    int test_t_index = patch.tri(patch.tot_tri() / 2);
    Face &tri_test = *tm.face(test_t_index);
    /* Assume all triangles in a patch are in the same shape. */
    int shape = shape_fn(tri_test.orig);
    if (dbg_level > 0) {
      std::cout << "process patch " << p << " = " << patch << "\n";
      std::cout << "test tri = " << test_t_index << " = " << &tri_test << "\n";
      std::cout << "shape = " << shape << "\n";
    }
    if (shape == -1) {
      continue;
    }
    test_tri_inside_shapes(tm, shape_fn, nshapes, test_t_index, tree, in_shape);
    for (int other_shape = 0; other_shape < nshapes; ++other_shape) {
      if (other_shape == shape) {
        continue;
      }
      bool need_high_confidence = (op == BoolOpType::Difference && shape != 0) ||
                                  op == BoolOpType::Intersect;
      bool inside = in_shape[other_shape] >= (need_high_confidence ? 0.5f : 0.1f);
      if (dbg_level > 0) {
        std::cout << "test point is " << (inside ? "inside" : "outside") << " other_shape "
                  << other_shape << " val = " << in_shape[other_shape] << "\n";
      }
      winding[other_shape] = inside;
    }
    bool do_flip;
    bool do_remove = raycast_test_remove(op, winding, shape, &do_flip);
    if (!do_remove) {
      for (int t : patch.tris()) {
        Face *f = tm.face(t);
        if (!do_flip) {
          out_faces.append(f);
        }
        else {
          raycast_add_flipped(out_faces, *f, arena);
        }
      }
    }
  }
  BLI_bvhtree_free(tree);

  ans.set_faces(out_faces);
  return ans;
}
/**
 * If \a tri1 and \a tri2 have a common edge (in opposite orientation),
 * return the indices into \a tri1 and \a tri2 where that common edge starts. Else return
 * (-1,-1).
 */
static std::pair<int, int> find_tris_common_edge(const Face &tri1, const Face &tri2)
{
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      if (tri1[(i + 1) % 3] == tri2[j] && tri1[i] == tri2[(j + 1) % 3]) {
        return std::pair<int, int>(i, j);
      }
    }
  }
  return std::pair<int, int>(-1, -1);
}

struct MergeEdge {
  /** Length (squared) of the edge, used for sorting. */
  double len_squared = 0.0;
  /* v1 and v2 are the ends of the edge, ordered so that `v1->id < v2->id` */
  const Vert *v1 = nullptr;
  const Vert *v2 = nullptr;
  /* left_face and right_face are indices into #FaceMergeState.face. */
  int left_face = -1;
  int right_face = -1;
  int orig = -1; /* An edge orig index that can be used for this edge. */
  /** Is it allowed to dissolve this edge? */
  bool dissolvable = false;
  /** Is this an intersect edge? */
  bool is_intersect = false;

  MergeEdge() = default;

  MergeEdge(const Vert *va, const Vert *vb)
  {
    if (va->id < vb->id) {
      this->v1 = va;
      this->v2 = vb;
    }
    else {
      this->v1 = vb;
      this->v2 = va;
    }
  };
};

struct MergeFace {
  /** The current sequence of Verts forming this face. */
  Vector<const Vert *> vert;
  /** For each position in face, what is index in #FaceMergeState of edge for that position? */
  Vector<int> edge;
  /** If not -1, merge_to gives a face index in #FaceMergeState that this is merged to. */
  int merge_to = -1;
  /** A face->orig that can be used for the merged face. */
  int orig = -1;
};
struct FaceMergeState {
  /**
   * The faces being considered for merging. Some will already have been merge (merge_to != -1).
   */
  Vector<MergeFace> face;
  /**
   * The edges that are part of the faces in face[], together with current topological
   * information (their left and right faces) and whether or not they are dissolvable.
   */
  Vector<MergeEdge> edge;
  /**
   * `edge_map` maps a pair of `const Vert *` ids (in canonical order: smaller id first)
   * to the index in the above edge vector in which to find the corresponding #MergeEdge.
   */
  Map<std::pair<int, int>, int> edge_map;
};

static std::ostream &operator<<(std::ostream &os, const FaceMergeState &fms)
{
  os << "faces:\n";
  for (int f : fms.face.index_range()) {
    const MergeFace &mf = fms.face[f];
    std::cout << f << ": orig=" << mf.orig << " verts ";
    for (const Vert *v : mf.vert) {
      std::cout << v << " ";
    }
    std::cout << "\n";
    std::cout << "    edges " << mf.edge << "\n";
    std::cout << "    merge_to = " << mf.merge_to << "\n";
  }
  os << "\nedges:\n";
  for (int e : fms.edge.index_range()) {
    const MergeEdge &me = fms.edge[e];
    std::cout << e << ": (" << me.v1 << "," << me.v2 << ") left=" << me.left_face
              << " right=" << me.right_face << " dis=" << me.dissolvable << " orig=" << me.orig
              << " is_int=" << me.is_intersect << "\n";
  }
  return os;
}

/**
 * \a tris all have the same original face.
 * Find the 2d edge/triangle topology for these triangles, but only the ones facing in the
 * norm direction, and whether each edge is dissolvable or not.
 * If we did the initial triangulation properly, and any Delaunay triangulations of intersections
 * properly, then each triangle edge should have at most one neighbor.
 * However, there can be anomalies. For example, if an input face is self-intersecting, we fall
 * back on the floating point poly-fill triangulation, which, after which all bets are off.
 * Hence, try to be tolerant of such unexpected topology.
 */
static void init_face_merge_state(FaceMergeState *fms,
                                  const Span<int> tris,
                                  const IMesh &tm,
                                  const double3 &norm)
{
  constexpr int dbg_level = 0;
  /* Reserve enough faces and edges so that neither will have to resize. */
  fms->face.reserve(tris.size() + 1);
  fms->edge.reserve(3 * tris.size());
  fms->edge_map.reserve(3 * tris.size());
  if (dbg_level > 0) {
    std::cout << "\nINIT_FACE_MERGE_STATE\n";
  }
  for (int t : tris.index_range()) {
    MergeFace mf;
    const Face &tri = *tm.face(tris[t]);
    if (dbg_level > 0) {
      std::cout << "process tri = " << &tri << "\n";
    }
    BLI_assert(tri.plane_populated());
    if (math::dot(norm, tri.plane->norm) <= 0.0) {
      if (dbg_level > 0) {
        std::cout << "triangle has wrong orientation, skipping\n";
      }
      continue;
    }
    mf.vert.append(tri[0]);
    mf.vert.append(tri[1]);
    mf.vert.append(tri[2]);
    mf.orig = tri.orig;
    int f = fms->face.append_and_get_index(mf);
    if (dbg_level > 1) {
      std::cout << "appended MergeFace for tri at f = " << f << "\n";
    }
    for (int i = 0; i < 3; ++i) {
      int inext = (i + 1) % 3;
      MergeEdge new_me(mf.vert[i], mf.vert[inext]);
      std::pair<int, int> canon_vs(new_me.v1->id, new_me.v2->id);
      int me_index = fms->edge_map.lookup_default(canon_vs, -1);
      if (dbg_level > 1) {
        std::cout << "new_me = canon_vs = " << new_me.v1 << ", " << new_me.v2 << "\n";
        std::cout << "me_index lookup = " << me_index << "\n";
      }
      if (me_index == -1) {
        double3 vec = new_me.v2->co - new_me.v1->co;
        new_me.len_squared = math::length_squared(vec);
        new_me.orig = tri.edge_orig[i];
        new_me.is_intersect = tri.is_intersect[i];
        new_me.dissolvable = (new_me.orig == NO_INDEX && !new_me.is_intersect);
        fms->edge.append(new_me);
        me_index = fms->edge.size() - 1;
        fms->edge_map.add_new(canon_vs, me_index);
        if (dbg_level > 1) {
          std::cout << "added new me with me_index = " << me_index << "\n";
          std::cout << "  len_squared = " << new_me.len_squared << "  orig = " << new_me.orig
                    << ", is_intersect" << new_me.is_intersect
                    << ", dissolvable = " << new_me.dissolvable << "\n";
        }
      }
      MergeEdge &me = fms->edge[me_index];
      if (dbg_level > 1) {
        std::cout << "retrieved me at index " << me_index << ":\n";
        std::cout << "  v1 = " << me.v1 << " v2 = " << me.v2 << "\n";
        std::cout << "  dis = " << me.dissolvable << " int = " << me.is_intersect << "\n";
        std::cout << "  left_face = " << me.left_face << " right_face = " << me.right_face << "\n";
      }
      if (me.dissolvable && tri.edge_orig[i] != NO_INDEX) {
        if (dbg_level > 1) {
          std::cout << "reassigning orig to " << tri.edge_orig[i] << ", dissolvable = false\n";
        }
        me.dissolvable = false;
        me.orig = tri.edge_orig[i];
      }
      if (me.dissolvable && tri.is_intersect[i]) {
        if (dbg_level > 1) {
          std::cout << "reassigning dissolvable = false, is_intersect = true\n";
        }
        me.dissolvable = false;
        me.is_intersect = true;
      }
      /* This face is left or right depending on orientation of edge. */
      if (me.v1 == mf.vert[i]) {
        if (dbg_level > 1) {
          std::cout << "me.v1 == mf.vert[i] so set edge[" << me_index << "].left_face = " << f
                    << "\n";
        }
        if (me.left_face != -1) {
          /* Unexpected in the normal case: this means more than one triangle shares this
           * edge in the same orientation. But be tolerant of this case. By making this
           * edge not dissolvable, we'll avoid future problems due to this non-manifold topology.
           */
          if (dbg_level > 1) {
            std::cout << "me.left_face was already occupied, so triangulation wasn't good\n";
          }
          me.dissolvable = false;
        }
        else {
          fms->edge[me_index].left_face = f;
        }
      }
      else {
        if (dbg_level > 1) {
          std::cout << "me.v1 != mf.vert[i] so set edge[" << me_index << "].right_face = " << f
                    << "\n";
        }
        if (me.right_face != -1) {
          /* Unexpected, analogous to the me.left_face != -1 case above. */
          if (dbg_level > 1) {
            std::cout << "me.right_face was already occupied, so triangulation wasn't good\n";
          }
          me.dissolvable = false;
        }
        else {
          fms->edge[me_index].right_face = f;
        }
      }
      fms->face[f].edge.append(me_index);
    }
  }
  if (dbg_level > 0) {
    std::cout << *fms;
  }
}

/**
 * To have a valid #BMesh, there are constraints on what edges can be removed.
 * We cannot remove an edge if (a) it would create two disconnected boundary parts
 * (which will happen if there's another edge sharing the same two faces);
 * or (b) it would create a face with a repeated vertex.
 */
static bool dissolve_leaves_valid_bmesh(FaceMergeState *fms,
                                        const MergeEdge &me,
                                        int me_index,
                                        const MergeFace &mf_left,
                                        const MergeFace &mf_right)
{
  int a_edge_start = mf_left.edge.first_index_of_try(me_index);
  BLI_assert(a_edge_start != -1);
  int alen = mf_left.vert.size();
  int blen = mf_right.vert.size();
  int b_left_face = me.right_face;
  bool ok = true;
  /* Is there another edge, not me, in A's face, whose right face is B's left? */
  for (int a_e_index = (a_edge_start + 1) % alen; ok && a_e_index != a_edge_start;
       a_e_index = (a_e_index + 1) % alen)
  {
    const MergeEdge &a_me_cur = fms->edge[mf_left.edge[a_e_index]];
    if (a_me_cur.right_face == b_left_face) {
      ok = false;
    }
  }
  /* Is there a vert in A, not me.v1 or me.v2, that is also in B?
   * One could avoid this O(n^2) algorithm if had a structure
   * saying which faces a vertex touches. */
  for (int a_v_index = 0; ok && a_v_index < alen; ++a_v_index) {
    const Vert *a_v = mf_left.vert[a_v_index];
    if (!ELEM(a_v, me.v1, me.v2)) {
      for (int b_v_index = 0; b_v_index < blen; ++b_v_index) {
        const Vert *b_v = mf_right.vert[b_v_index];
        if (a_v == b_v) {
          ok = false;
        }
      }
    }
  }
  return ok;
}

/**
 * `mf_left` and `mf_right` should share a #MergeEdge `me`, having index `me_index`.
 * We change `mf_left` to remove edge `me` and insert the appropriate edges of
 * `mf_right` in between the start and end vertices of that edge.
 * We change the left face of the spliced-in edges to be `mf_left`'s index.
 * We mark the `merge_to` property of `mf_right`, which is now in essence deleted.
 */
static void splice_faces(
    FaceMergeState *fms, MergeEdge &me, int me_index, MergeFace &mf_left, MergeFace &mf_right)
{
  int a_edge_start = mf_left.edge.first_index_of_try(me_index);
  int b_edge_start = mf_right.edge.first_index_of_try(me_index);
  BLI_assert(a_edge_start != -1 && b_edge_start != -1);
  int alen = mf_left.vert.size();
  int blen = mf_right.vert.size();
  Vector<const Vert *> splice_vert;
  Vector<int> splice_edge;
  splice_vert.reserve(alen + blen - 2);
  splice_edge.reserve(alen + blen - 2);
  int ai = 0;
  while (ai < a_edge_start) {
    splice_vert.append(mf_left.vert[ai]);
    splice_edge.append(mf_left.edge[ai]);
    ++ai;
  }
  int bi = b_edge_start + 1;
  while (bi != b_edge_start) {
    if (bi >= blen) {
      bi = 0;
      if (bi == b_edge_start) {
        break;
      }
    }
    splice_vert.append(mf_right.vert[bi]);
    splice_edge.append(mf_right.edge[bi]);
    if (mf_right.vert[bi] == fms->edge[mf_right.edge[bi]].v1) {
      fms->edge[mf_right.edge[bi]].left_face = me.left_face;
    }
    else {
      fms->edge[mf_right.edge[bi]].right_face = me.left_face;
    }
    ++bi;
  }
  ai = a_edge_start + 1;
  while (ai < alen) {
    splice_vert.append(mf_left.vert[ai]);
    splice_edge.append(mf_left.edge[ai]);
    ++ai;
  }
  mf_right.merge_to = me.left_face;
  mf_left.vert = splice_vert;
  mf_left.edge = splice_edge;
  me.left_face = -1;
  me.right_face = -1;
}

/**
 * Given that fms has been properly initialized to contain a set of faces that
 * together form a face or part of a face of the original #IMesh, and that
 * it has properly recorded with faces are dissolvable, dissolve as many edges as possible.
 * We try to dissolve in decreasing order of edge length, so that it is more likely
 * that the final output doesn't have awkward looking long edges with extreme angles.
 */
static void do_dissolve(FaceMergeState *fms)
{
  const int dbg_level = 0;
  if (dbg_level > 1) {
    std::cout << "\nDO_DISSOLVE\n";
  }
  Vector<int> dissolve_edges;
  for (int e : fms->edge.index_range()) {
    if (fms->edge[e].dissolvable) {
      dissolve_edges.append(e);
    }
  }
  if (dissolve_edges.is_empty()) {
    return;
  }
  /* Things look nicer if we dissolve the longer edges first. */
  std::sort(
      dissolve_edges.begin(), dissolve_edges.end(), [fms](const int &a, const int &b) -> bool {
        return (fms->edge[a].len_squared > fms->edge[b].len_squared);
      });
  if (dbg_level > 0) {
    std::cout << "Sorted dissolvable edges: " << dissolve_edges << "\n";
  }
  for (int me_index : dissolve_edges) {
    MergeEdge &me = fms->edge[me_index];
    if (me.left_face == -1 || me.right_face == -1) {
      continue;
    }
    MergeFace &mf_left = fms->face[me.left_face];
    MergeFace &mf_right = fms->face[me.right_face];
    if (!dissolve_leaves_valid_bmesh(fms, me, me_index, mf_left, mf_right)) {
      continue;
    }
    if (dbg_level > 0) {
      std::cout << "Removing edge " << me_index << "\n";
    }
    splice_faces(fms, me, me_index, mf_left, mf_right);
    if (dbg_level > 1) {
      std::cout << "state after removal:\n";
      std::cout << *fms;
    }
  }
}

/**
 * Given that \a tris form a triangulation of a face or part of a face that was in \a imesh_in,
 * merge as many of the triangles together as possible, by dissolving the edges between them.
 * We can only dissolve triangulation edges that don't overlap real input edges, and we
 * can only dissolve them if doing so leaves the remaining faces able to create valid #BMesh.
 * We can tell edges that don't overlap real input edges because they will have an
 * "original edge" that is different from #NO_INDEX.
 * \note it is possible that some of the triangles in \a tris have reversed orientation
 * to the rest, so we have to handle the two cases separately.
 */
static Vector<Face *> merge_tris_for_face(const Vector<int> &tris,
                                          const IMesh &tm,
                                          const IMesh &imesh_in,
                                          IMeshArena *arena)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "merge_tris_for_face\n";
    std::cout << "tris: " << tris << "\n";
  }
  Vector<Face *> ans;
  if (tris.size() <= 1) {
    if (tris.size() == 1) {
      ans.append(tm.face(tris[0]));
    }
    return ans;
  }
  bool done = false;
  double3 first_tri_normal = tm.face(tris[0])->plane->norm;
  double3 second_tri_normal = tm.face(tris[1])->plane->norm;
  if (tris.size() == 2 && math::dot(first_tri_normal, second_tri_normal) > 0.0) {
    /* Is this a case where quad with one diagonal remained unchanged?
     * Worth special handling because this case will be very common. */
    Face &tri1 = *tm.face(tris[0]);
    Face &tri2 = *tm.face(tris[1]);
    Face *in_face = imesh_in.face(tri1.orig);
    if (in_face->size() == 4) {
      std::pair<int, int> estarts = find_tris_common_edge(tri1, tri2);
      if (estarts.first != -1 && tri1.edge_orig[estarts.first] == NO_INDEX) {
        if (dbg_level > 0) {
          std::cout << "try recovering orig quad case\n";
          std::cout << "tri1 = " << &tri1 << "\n";
          std::cout << "tri1 = " << &tri2 << "\n";
        }
        int i0 = estarts.first;
        int i1 = (i0 + 1) % 3;
        int i2 = (i0 + 2) % 3;
        int j2 = (estarts.second + 2) % 3;
        Face tryface({tri1[i1], tri1[i2], tri1[i0], tri2[j2]}, -1, -1, {}, {});
        if (tryface.cyclic_equal(*in_face)) {
          if (dbg_level > 0) {
            std::cout << "inface = " << in_face << "\n";
            std::cout << "quad recovery worked\n";
          }
          ans.append(in_face);
          done = true;
        }
      }
    }
  }
  if (done) {
    return ans;
  }

  double3 first_tri_normal_rev = -first_tri_normal;
  for (const double3 &norm : {first_tri_normal, first_tri_normal_rev}) {
    FaceMergeState fms;
    init_face_merge_state(&fms, tris, tm, norm);
    do_dissolve(&fms);
    if (dbg_level > 0) {
      std::cout << "faces in merged result:\n";
    }
    for (const MergeFace &mf : fms.face) {
      if (mf.merge_to == -1) {
        Array<int> e_orig(mf.edge.size());
        Array<bool> is_intersect(mf.edge.size());
        for (int i : mf.edge.index_range()) {
          e_orig[i] = fms.edge[mf.edge[i]].orig;
          is_intersect[i] = fms.edge[mf.edge[i]].is_intersect;
        }
        Face *facep = arena->add_face(mf.vert, mf.orig, e_orig, is_intersect);
        ans.append(facep);
        if (dbg_level > 0) {
          std::cout << "  " << facep << "\n";
        }
      }
    }
  }
  return ans;
}

static bool approx_in_line(const double3 &a, const double3 &b, const double3 &c)
{
  double3 vec1 = b - a;
  double3 vec2 = c - b;
  double cos_ang = math::dot(math::normalize(vec1), math::normalize(vec2));
  return fabs(cos_ang - 1.0) < 1e-4;
}

/**
 * Return an array, paralleling imesh_out.vert, saying which vertices can be dissolved.
 * A vertex v can be dissolved if (a) it is not an input vertex; (b) it has valence 2;
 * and (c) if v's two neighboring vertices are u and w, then (u,v,w) forms a straight line.
 * Return the number of dissolvable vertices in r_count_dissolve.
 */
static Array<bool> find_dissolve_verts(IMesh &imesh_out, int *r_count_dissolve)
{
  imesh_out.populate_vert();
  /* dissolve[i] will say whether imesh_out.vert(i) can be dissolved. */
  Array<bool> dissolve(imesh_out.vert_size());
  for (int v_index : imesh_out.vert_index_range()) {
    const Vert &vert = *imesh_out.vert(v_index);
    dissolve[v_index] = (vert.orig == NO_INDEX);
  }
  /* neighbors[i] will be a pair giving the up-to-two neighboring vertices
   * of the vertex v in position i of imesh_out.vert.
   * If we encounter a third, then v will not be dissolvable. */
  Array<std::pair<const Vert *, const Vert *>> neighbors(
      imesh_out.vert_size(), std::pair<const Vert *, const Vert *>(nullptr, nullptr));
  for (int f : imesh_out.face_index_range()) {
    const Face &face = *imesh_out.face(f);
    for (int i : face.index_range()) {
      const Vert *v = face[i];
      int v_index = imesh_out.lookup_vert(v);
      BLI_assert(v_index != NO_INDEX);
      if (dissolve[v_index]) {
        const Vert *n1 = face[face.next_pos(i)];
        const Vert *n2 = face[face.prev_pos(i)];
        const Vert *f_n1 = neighbors[v_index].first;
        const Vert *f_n2 = neighbors[v_index].second;
        if (f_n1 != nullptr) {
          /* Already has a neighbor in another face; can't dissolve unless they are the same. */
          if (!((n1 == f_n2 && n2 == f_n1) || (n1 == f_n1 && n2 == f_n2))) {
            /* Different neighbors, so can't dissolve. */
            dissolve[v_index] = false;
          }
        }
        else {
          /* These are the first-seen neighbors. */
          neighbors[v_index] = std::pair<const Vert *, const Vert *>(n1, n2);
        }
      }
    }
  }
  int count = 0;
  for (int v_out : imesh_out.vert_index_range()) {
    if (dissolve[v_out]) {
      dissolve[v_out] = false; /* Will set back to true if final condition is satisfied. */
      const std::pair<const Vert *, const Vert *> &nbrs = neighbors[v_out];
      if (nbrs.first != nullptr) {
        BLI_assert(nbrs.second != nullptr);
        const Vert *v_v_out = imesh_out.vert(v_out);
        if (approx_in_line(nbrs.first->co, v_v_out->co, nbrs.second->co)) {
          dissolve[v_out] = true;
          ++count;
        }
      }
    }
  }
  if (r_count_dissolve != nullptr) {
    *r_count_dissolve = count;
  }
  return dissolve;
}

/**
 * The dissolve array parallels the `imesh.vert` array. Wherever it is true,
 * remove the corresponding vertex from the vertices in the faces of
 * `imesh.faces` to account for the close-up of the gaps in `imesh.vert`.
 */
static void dissolve_verts(IMesh *imesh, const Array<bool> dissolve, IMeshArena *arena)
{
  constexpr int inline_face_size = 100;
  Vector<bool, inline_face_size> face_pos_erase;
  bool any_faces_erased = false;
  for (int f : imesh->face_index_range()) {
    const Face &face = *imesh->face(f);
    face_pos_erase.clear();
    int erase_num = 0;
    for (const Vert *v : face) {
      int v_index = imesh->lookup_vert(v);
      BLI_assert(v_index != NO_INDEX);
      if (dissolve[v_index]) {
        face_pos_erase.append(true);
        ++erase_num;
      }
      else {
        face_pos_erase.append(false);
      }
    }
    if (erase_num > 0) {
      any_faces_erased |= imesh->erase_face_positions(f, face_pos_erase, arena);
    }
  }
  imesh->set_dirty_verts();
  if (any_faces_erased) {
    imesh->remove_null_faces();
  }
}

/**
 * The main boolean function operates on a triangle #IMesh and produces a
 * Triangle #IMesh as output.
 * This function converts back into a general polygonal mesh by removing
 * any possible triangulation edges (which can be identified because they
 * will have an original edge that is NO_INDEX.
 * Not all triangulation edges can be removed: if they ended up non-trivially overlapping a real
 * input edge, then we need to keep it. Also, some are necessary to make the output satisfy
 * the "valid #BMesh" property: we can't produce output faces that have repeated vertices in
 * them, or have several disconnected boundaries (e.g., faces with holes).
 */
static IMesh polymesh_from_trimesh_with_dissolve(const IMesh &tm_out,
                                                 const IMesh &imesh_in,
                                                 IMeshArena *arena)
{
  const int dbg_level = 0;
  if (dbg_level > 1) {
    std::cout << "\nPOLYMESH_FROM_TRIMESH_WITH_DISSOLVE\n";
  }
  /* For now: need plane normals for all triangles. */
  const int grainsize = 1024;
  threading::parallel_for(tm_out.face_index_range(), grainsize, [&](IndexRange range) {
    for (int i : range) {
      Face *tri = tm_out.face(i);
      tri->populate_plane(false);
    }
  });
  /* Gather all output triangles that are part of each input face.
   * face_output_tris[f] will be indices of triangles in tm_out
   * that have f as their original face. */
  int tot_in_face = imesh_in.face_size();
  Array<Vector<int>> face_output_tris(tot_in_face);
  for (int t : tm_out.face_index_range()) {
    const Face &tri = *tm_out.face(t);
    int in_face = tri.orig;
    face_output_tris[in_face].append(t);
  }
  if (dbg_level > 1) {
    std::cout << "face_output_tris:\n";
    for (int f : face_output_tris.index_range()) {
      std::cout << f << ": " << face_output_tris[f] << "\n";
    }
  }

  /* Merge triangles that we can from face_output_tri to make faces for output.
   * face_output_face[f] will be new original const Face *'s that
   * make up whatever part of the boolean output remains of input face f. */
  Array<Vector<Face *>> face_output_face(tot_in_face);
  int tot_out_face = 0;
  for (int in_f : imesh_in.face_index_range()) {
    if (dbg_level > 1) {
      std::cout << "merge tris for face " << in_f << "\n";
    }
    int out_tris_for_face_num = face_output_tris.size();
    if (out_tris_for_face_num == 0) {
      continue;
    }
    face_output_face[in_f] = merge_tris_for_face(face_output_tris[in_f], tm_out, imesh_in, arena);
    tot_out_face += face_output_face[in_f].size();
  }
  Array<Face *> face(tot_out_face);
  int out_f_index = 0;
  for (int in_f : imesh_in.face_index_range()) {
    const Span<Face *> f_faces = face_output_face[in_f];
    if (f_faces.size() > 0) {
      std::copy(f_faces.begin(), f_faces.end(), &face[out_f_index]);
      out_f_index += f_faces.size();
    }
  }
  IMesh imesh_out(face);
  /* Dissolve vertices that were (a) not original; and (b) now have valence 2 and
   * are between two other vertices that are exactly in line with them.
   * These were created because of triangulation edges that have been dissolved. */
  int count_dissolve;
  Array<bool> v_dissolve = find_dissolve_verts(imesh_out, &count_dissolve);
  if (count_dissolve > 0) {
    dissolve_verts(&imesh_out, v_dissolve, arena);
  }
  if (dbg_level > 1) {
    write_obj_mesh(imesh_out, "boolean_post_dissolve");
  }

  return imesh_out;
}

IMesh boolean_trimesh(IMesh &tm_in,
                      BoolOpType op,
                      int nshapes,
                      FunctionRef<int(int)> shape_fn,
                      bool use_self,
                      bool hole_tolerant,
                      IMeshArena *arena)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "BOOLEAN of " << nshapes << " operand" << (nshapes == 1 ? "" : "s")
              << " op=" << bool_optype_name(op) << "\n";
    if (dbg_level > 1) {
      tm_in.populate_vert();
      std::cout << "boolean_trimesh input:\n" << tm_in;
      write_obj_mesh(tm_in, "boolean_in");
    }
  }
  if (tm_in.face_size() == 0) {
    return IMesh(tm_in);
  }
#  ifdef PERFDEBUG
  double start_time = BLI_time_now_seconds();
  std::cout << "  boolean_trimesh, timing begins\n";
#  endif

  IMesh tm_si = trimesh_nary_intersect(tm_in, nshapes, shape_fn, use_self, arena);
  if (dbg_level > 1) {
    write_obj_mesh(tm_si, "boolean_tm_si");
    std::cout << "\nboolean_tm_input after intersection:\n" << tm_si;
  }
#  ifdef PERFDEBUG
  double intersect_time = BLI_time_now_seconds();
  std::cout << "  intersected, time = " << intersect_time - start_time << "\n";
#  endif

  /* It is possible for tm_si to be empty if all the input triangles are bogus/degenerate. */
  if (tm_si.face_size() == 0 || op == BoolOpType::None) {
    return tm_si;
  }
  auto si_shape_fn = [shape_fn, tm_si](int t) { return shape_fn(tm_si.face(t)->orig); };
  TriMeshTopology tm_si_topo(tm_si);
#  ifdef PERFDEBUG
  double topo_time = BLI_time_now_seconds();
  std::cout << "  topology built, time = " << topo_time - intersect_time << "\n";
#  endif
  bool pwn = is_pwn(tm_si, tm_si_topo);
#  ifdef PERFDEBUG
  double pwn_time = BLI_time_now_seconds();
  std::cout << "  pwn checked, time = " << pwn_time - topo_time << "\n";
#  endif
  IMesh tm_out;
  if (!pwn) {
    if (dbg_level > 0) {
      std::cout << "Input is not PWN, using raycast method\n";
    }
    if (hole_tolerant) {
      tm_out = raycast_tris_boolean(tm_si, op, nshapes, shape_fn, arena);
    }
    else {
      PatchesInfo pinfo = find_patches(tm_si, tm_si_topo);
      tm_out = raycast_patches_boolean(tm_si, op, nshapes, shape_fn, pinfo, arena);
    }
#  ifdef PERFDEBUG
    double raycast_time = BLI_time_now_seconds();
    std::cout << "  raycast_boolean done, time = " << raycast_time - pwn_time << "\n";
#  endif
  }
  else {
    PatchesInfo pinfo = find_patches(tm_si, tm_si_topo);
#  ifdef PERFDEBUG
    double patch_time = BLI_time_now_seconds();
    std::cout << "  patches found, time = " << patch_time - pwn_time << "\n";
#  endif
    CellsInfo cinfo = find_cells(tm_si, tm_si_topo, pinfo);
    if (dbg_level > 0) {
      std::cout << "Input is PWN\n";
    }
#  ifdef PERFDEBUG
    double cell_time = BLI_time_now_seconds();
    std::cout << "  cells found, time = " << cell_time - pwn_time << "\n";
#  endif
    finish_patch_cell_graph(tm_si, cinfo, pinfo, tm_si_topo, arena);
#  ifdef PERFDEBUG
    double finish_pc_time = BLI_time_now_seconds();
    std::cout << "  finished patch-cell graph, time = " << finish_pc_time - cell_time << "\n";
#  endif
    bool pc_ok = patch_cell_graph_ok(cinfo, pinfo);
    if (!pc_ok) {
      /* TODO: if bad input can lead to this, diagnose the problem. */
      std::cout << "Something funny about input or a bug in boolean\n";
      return IMesh(tm_in);
    }
    cinfo.init_windings(nshapes);
    int c_ambient = find_ambient_cell(tm_si, nullptr, tm_si_topo, pinfo, arena);
#  ifdef PERFDEBUG
    double amb_time = BLI_time_now_seconds();
    std::cout << "  ambient cell found, time = " << amb_time - finish_pc_time << "\n";
#  endif
    if (c_ambient == NO_INDEX) {
      /* TODO: find a way to propagate this error to user properly. */
      std::cout << "Could not find an ambient cell; input not valid?\n";
      return IMesh(tm_si);
    }
    propagate_windings_and_in_output_volume(pinfo, cinfo, c_ambient, op, nshapes, si_shape_fn);
#  ifdef PERFDEBUG
    double propagate_time = BLI_time_now_seconds();
    std::cout << "  windings propagated, time = " << propagate_time - amb_time << "\n";
#  endif
    tm_out = extract_from_in_output_volume_diffs(tm_si, pinfo, cinfo, arena);
#  ifdef PERFDEBUG
    double extract_time = BLI_time_now_seconds();
    std::cout << "  extracted, time = " << extract_time - propagate_time << "\n";
#  endif
    if (dbg_level > 0) {
      /* Check if output is PWN. */
      TriMeshTopology tm_out_topo(tm_out);
      if (!is_pwn(tm_out, tm_out_topo)) {
        std::cout << "OUTPUT IS NOT PWN!\n";
      }
    }
  }
  if (dbg_level > 1) {
    write_obj_mesh(tm_out, "boolean_tm_output");
    std::cout << "boolean tm output:\n" << tm_out;
  }
#  ifdef PERFDEBUG
  double end_time = BLI_time_now_seconds();
  std::cout << "  boolean_trimesh done, total time = " << end_time - start_time << "\n";
#  endif
  return tm_out;
}

static void dump_test_spec(IMesh &imesh)
{
  std::cout << "test spec = " << imesh.vert_size() << " " << imesh.face_size() << "\n";
  for (const Vert *v : imesh.vertices()) {
    std::cout << v->co_exact[0] << " " << v->co_exact[1] << " " << v->co_exact[2] << " # "
              << v->co[0] << " " << v->co[1] << " " << v->co[2] << "\n";
  }
  for (const Face *f : imesh.faces()) {
    for (const Vert *fv : *f) {
      std::cout << imesh.lookup_vert(fv) << " ";
    }
    std::cout << "\n";
  }
}

IMesh boolean_mesh(IMesh &imesh,
                   BoolOpType op,
                   int nshapes,
                   FunctionRef<int(int)> shape_fn,
                   bool use_self,
                   bool hole_tolerant,
                   IMesh *imesh_triangulated,
                   IMeshArena *arena)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "\nBOOLEAN_MESH\n"
              << nshapes << " operand" << (nshapes == 1 ? "" : "s")
              << " op=" << bool_optype_name(op) << "\n";
    if (dbg_level > 1) {
      write_obj_mesh(imesh, "boolean_mesh_in");
      std::cout << imesh;
      if (dbg_level > 2) {
        dump_test_spec(imesh);
      }
    }
  }
  IMesh *tm_in = imesh_triangulated;
  IMesh our_triangulation;
#  ifdef PERFDEBUG
  double start_time = BLI_time_now_seconds();
  std::cout << "boolean_mesh, timing begins\n";
#  endif
  if (tm_in == nullptr) {
    our_triangulation = triangulate_polymesh(imesh, arena);
    tm_in = &our_triangulation;
  }
#  ifdef PERFDEBUG
  double tri_time = BLI_time_now_seconds();
  std::cout << "triangulated, time = " << tri_time - start_time << "\n";
#  endif
  if (dbg_level > 1) {
    write_obj_mesh(*tm_in, "boolean_tm_in");
  }
  IMesh tm_out = boolean_trimesh(*tm_in, op, nshapes, shape_fn, use_self, hole_tolerant, arena);
#  ifdef PERFDEBUG
  double bool_tri_time = BLI_time_now_seconds();
  std::cout << "boolean_trimesh done, time = " << bool_tri_time - tri_time << "\n";
#  endif
  if (dbg_level > 1) {
    std::cout << "bool_trimesh_output:\n" << tm_out;
    write_obj_mesh(tm_out, "bool_trimesh_output");
  }
  IMesh ans = polymesh_from_trimesh_with_dissolve(tm_out, imesh, arena);
#  ifdef PERFDEBUG
  double dissolve_time = BLI_time_now_seconds();
  std::cout << "polymesh from dissolving, time = " << dissolve_time - bool_tri_time << "\n";
#  endif
  if (dbg_level > 0) {
    std::cout << "boolean_mesh output:\n" << ans;
    if (dbg_level > 2) {
      ans.populate_vert();
      dump_test_spec(ans);
    }
  }
#  ifdef PERFDEBUG
  double end_time = BLI_time_now_seconds();
  std::cout << "boolean_mesh done, total time = " << end_time - start_time << "\n";
#  endif
  return ans;
}

}  // namespace blender::meshintersect

#endif  // WITH_GMP
