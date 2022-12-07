/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <fstream>
#include <iostream>
#include <limits>
#include <queue>

#include "BLI_array.hh"
#include "BLI_heap.h"
#include "BLI_map.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_memarena.h"
#include "BLI_mesh_inset.hh"
#include "BLI_polyfill_2d.h"
#include "BLI_polyfill_2d_beautify.h"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "BLI_mesh_inset.hh"

namespace blender::meshinset {

class Vert;
class Edge;
class Triangle;

/** The predecessor index to \a i in a triangle. */
static inline int pred_index(const int i)
{
  return (i + 2) % 3;
}

/** The successor index to \a i in a triangle. */
static inline int succ_index(const int i)
{
  return (i + 1) % 3;
}

/** The ith edge of triangle tri. Not shared with the adjacent triangle. */
class Edge {
  /* Assume Triangles are allocated on at least 4 byte boundaries.
   * Then, use the lower two bits of the pointer as the index (0,1,2)
   * saying which edge of the triangle we refer to.
   */

  /** A pointer with lower two bits used for index. */
  Triangle *tri_and_index_;

 public:
  Edge() : tri_and_index_(nullptr)
  {
  }
  Edge(const Triangle *tri, int tri_edge_index)
  {
    static_assert(sizeof(Triangle *) % 4 == 0);
    uintptr_t tri_x = reinterpret_cast<uintptr_t>(tri);
    BLI_assert(0 <= tri_edge_index && tri_edge_index < 3);
    tri_x |= tri_edge_index;
    tri_and_index_ = reinterpret_cast<Triangle *>(tri_x);
  }

  /** The triangle containing this edge. */
  Triangle *tri() const
  {
    uintptr_t tri_x = reinterpret_cast<uintptr_t>(tri_and_index_);
    tri_x &= ~3;
    return reinterpret_cast<Triangle *>(tri_x);
  }

  /** Which edge of the triangle is it? 0, 1, or 2? */
  int tri_edge_index() const
  {
    uintptr_t tri_x = reinterpret_cast<uintptr_t>(tri_and_index_);
    return tri_x & 3;
  }

  bool is_null() const
  {
    return tri_and_index_ == nullptr;
  }

  /** Return the edge next around this one's triangle. */
  Edge triangle_succ() const
  {
    return Edge(this->tri(), succ_index(this->tri_edge_index()));
  }

  /** Return the edge before this in this one's triangle. */
  Edge triangle_pred() const
  {
    return Edge(this->tri(), pred_index(this->tri_edge_index()));
  }

  uint64_t hash() const
  {
    /* Like the default hash for pointers but without the right-shift of 4 bits. */
    uintptr_t ptr = reinterpret_cast<uintptr_t>(this->tri_and_index_);
    uint64_t hash = static_cast<uint64_t>(ptr);
    return hash;
  }

  friend std::ostream &operator<<(std::ostream &os, const Edge e);

 protected:
  friend bool operator==(const Edge &a, const Edge &b)
  {
    return a.tri_and_index_ == b.tri_and_index_;
  }
  friend bool operator!=(const Edge &a, const Edge &b)
  {
    return a.tri_and_index_ != b.tri_and_index_;
  }
};

static Edge null_edge = Edge();

enum VertFlags { VDELETED = 1 };

class Vert {
 public:
  float3 co;
  /** Any Edge leaving this vertesx. */
  Edge e;
  int id{0};
  uint8_t flags{0};

  Vert(const float3 co, const Edge e) : co(co), e(e)
  {
  }
  Vert(const float3 co) : co(co), e(Edge(nullptr, 0))
  {
  }

  void mark_deleted()
  {
    this->flags |= VDELETED;
  }

  bool is_deleted() const
  {
    return (this->flags & VDELETED) != 0;
  }

  friend std::ostream &operator<<(std::ostream &os, const Vert &v);
  friend std::ostream &operator<<(std::ostream &os, const Vert *v);
};

class Triangle {

  enum TriangleFlags {
    /* TDELETED means the triangle is no longer part of its TriangleMesh. */
    TDELETED = 1,
    /* TNORMAL_VALID means the normal_ member is the normal using current coordinates. */
    TNORMAL_VALID = 1 << 1,
    /* TREGION means the triangle is part of the region still being inset. */
    TREGION = 1 << 2,
    /* TSPOKEi means the ith edge is a spoke in the Straight Skeleton construction. */
    TSPOKE0 = 1 << 3,
    TSPOKE1 = 1 << 4,
    TSPOKE2 = 1 << 5,
    /* TORIGi means the ith edge is an edge that was in the incoming mesh (before triangulation).
     */
    TORIG0 = 1 << 6,
    TORIG1 = 1 << 7,
    TORIG2 = 1 << 8,
  };
  Edge neighbor_[3];
  Vert *vert_[3];
  float3 normal_;
  int id_{0};
  uint16_t flags_{0};

 public:
  Triangle(Vert *v0, Vert *v1, Vert *v2)
  {
    neighbor_[0] = Edge();
    neighbor_[1] = Edge();
    neighbor_[2] = Edge();
    vert_[0] = v0;
    vert_[1] = v1;
    vert_[2] = v2;
    if (v0 != nullptr && v0->e.is_null()) {
      v0->e = Edge(this, 0);
    }
    if (v1 != nullptr && v1->e.is_null()) {
      v1->e = Edge(this, 1);
    }

    if (v2 != nullptr && v2->e.is_null()) {
      v2->e = Edge(this, 2);
    }
  };

  /** This triangle's ith vertex. */
  Vert *vert(int i) const
  {
    return vert_[i];
  }

  /** The neighbor edge corresponding to this triangle's ith edge. */
  Edge neighbor(int i) const
  {
    return neighbor_[i];
  }

  /** This Triangle's ith edge. */
  Edge edge(int i) const
  {
    return Edge(this, i);
  }

  /** An id for this triangle. Should have been set using set_id(). */
  int id() const
  {
    return id_;
  }

  /** Set the id of this triangle (should be done just once). */
  void set_id(int id)
  {
    id_ = id;
  }

  /** Set the ith vertex of this triangle to \a v .*/
  void set_vert(int i, Vert *v)
  {
    vert_[i] = v;
    flags_ &= ~TNORMAL_VALID;
  }

  /** A "ghost" triangle has a nullptr for vert[1]. */
  bool is_ghost() const
  {
    return vert_[1] == nullptr;
  }

  /** Return the triangle normal. Assumes calculate_normal() has been called. */
  float3 normal() const
  {
    BLI_assert(flags_ & TNORMAL_VALID);
    return normal_;
  }

  void calculate_normal();

  void mark_deleted()
  {
    flags_ |= TDELETED;
  }

  bool is_deleted() const
  {
    return (flags_ & TDELETED) != 0;
  }

  /* Our algorithm cares which triangles are "in the region" or not. */
  void mark_in_region()
  {
    flags_ |= TREGION;
  }

  void clear_in_region()
  {
    flags_ &= ~TREGION;
  }

  bool in_region() const
  {
    return (flags_ & TREGION) != 0;
  }

  /* Our algorithm cares which edges are "spokes".
   * As a convenience, always mark the spoke of the neibhgor edge too. */
  void mark_spoke(int pos)
  {
    flags_ |= (TSPOKE0 << pos);
    Edge en = neighbor_[pos];
    Triangle *tn = en.tri();
    tn->flags_ |= (TSPOKE0 << en.tri_edge_index());
  }

  void clear_spoke(int pos)
  {
    flags_ &= ~(TSPOKE0 << pos);
    Edge en = neighbor_[pos];
    Triangle *tn = en.tri();
    tn->flags_ &= ~(TSPOKE0 << en.tri_edge_index());
  }

  bool is_spoke(int pos) const
  {
    return (flags_ & (TSPOKE0 << pos)) != 0;
  }

  /* Our algorithm cares about which edges are original edges
   * (i.e., not triangulation edges). */
  void mark_orig(int pos)
  {
    flags_ |= (TORIG0 << pos);
    Edge en = neighbor_[pos];
    if (!en.is_null()) {
      Triangle *tn = en.tri();
      tn->flags_ |= (TORIG0 << en.tri_edge_index());
    }
  }

  bool is_orig(int pos) const
  {
    return (flags_ & (TORIG0 << pos)) != 0;
  }

  friend void set_mutual_neighbors(Triangle *t1, int pos1, Triangle *t2, int pos2);
  friend void set_mutual_neighbors(Triangle *t1, int pos1, Edge e2);

  friend std::ostream &operator<<(std::ostream &os, const Triangle &tri);
};

void Triangle::calculate_normal()
{
  BLI_assert(!this->is_ghost() && !this->is_deleted());
  float3 v0v1 = vert_[1]->co - vert_[0]->co;
  float3 v0v2 = vert_[2]->co - vert_[0]->co;
  normal_ = math::normalize(math::cross_high_precision(v0v1, v0v2));
  flags_ |= TNORMAL_VALID;
}

/* For use when we may not have calculated tri->normal_ (mostly for debugging). */
static float3 triangle_normal(const Triangle *tri)
{
  if (tri->is_ghost() || tri->is_deleted()) {
    return float3(0.0f, 0.0f, 0.0f);
  }
  BLI_assert(!tri->is_ghost());
  float3 v0v1 = tri->vert(1)->co - tri->vert(0)->co;
  float3 v0v2 = tri->vert(2)->co - tri->vert(0)->co;
  return math::normalize(math::cross_high_precision(v0v1, v0v2));
}

/** Mark triangles \a t1 and \a t2 as neighbors, where \a pos1 and \a pos2
 * are the positions in triangles t1 and t2 respectively where the mutual edges occur. */
void set_mutual_neighbors(Triangle *t1, int pos1, Triangle *t2, int pos2)
{
  BLI_assert(t1 != nullptr && t2 != nullptr);
  t1->neighbor_[pos1] = Edge(t2, pos2);
  t2->neighbor_[pos2] = Edge(t1, pos1);
}

/** Like above but with an Edge instead of t2, pos2. */
void set_mutual_neighbors(Triangle *t1, int pos1, Edge e2)
{
  BLI_assert(t1 != nullptr);
  t1->neighbor_[pos1] = e2;
  Triangle *t2 = e2.tri();
  t2->neighbor_[e2.tri_edge_index()] = Edge(t1, pos1);
}

/** Return the vertex at the source end of \a e. */
static Vert *v_src(Edge e)
{
  return e.tri()->vert(e.tri_edge_index());
}

/** Return the vertex at the destination end of \a e. */
static Vert *v_dst(Edge e)
{
  return e.tri()->vert(succ_index(e.tri_edge_index()));
}

/** Return the edge paired with \a e in the neighbor triangle. */
static Edge neighbor_edge(Edge e)
{
  const Triangle *t = e.tri();
  if (t != nullptr) {
    return t->neighbor(e.tri_edge_index());
  }
  return null_edge;
}

/** Return the edge that is the CCW rotation from \a e around its source.
 * Assume the source is not the "infinite" vertex of a ghost triangle.
 */
static Edge rot_ccw(Edge e)
{
  BLI_assert(v_src(e) != nullptr);
  Edge ans = neighbor_edge(e.triangle_pred());
  return ans;
}

/** Return the edge that is the CW rotation from \a e around its source.
 * Assume the source is not the "infinite" vertex of a ghost triangle.
 */
static Edge rot_cw(Edge e)
{
  BLI_assert(v_src(e) != nullptr);
  Edge ans = neighbor_edge(e).triangle_succ();
  return ans;
}

/** Return the edge from \a v1 to \a v2 if it exists, else null_edge */
static Edge edge_between(const Vert *v1, const Vert *v2)
{
  Edge e = v1->e;
  if (e.is_null()) {
    return null_edge;
  }
  while (v_dst(e) != v2) {
    e = rot_ccw(e);
    if (e == v1->e) {
      return null_edge;
    }
  }
  return e;
}

/* Don't need this right now, but may want it later. */
#if 0
/** Return a Vector of the triangles with \a v as src. Includes any ghost triangles. */
static Vector<Triangle *> triangles_of_vert(const Vert *v)
{
  Vector<Triangle *> ans;
  Edge e = v->e;
  if (!e.is_null()) {
    do {
      Triangle *t = e.tri();
      if (t != nullptr) {
        ans.append(t);
      }
      e = rot_ccw(e);
    } while (e != v->e);
  }
  return ans;
}
#endif

/** Calculate the vertex normal, assuming its triangles have had normals calculated.
 * In Blender, the vertex normal is the angle-weighted combination of the adjacent
 * face normals.
 */
static float3 vertex_normal(const Vert *vert)
{
  BLI_assert(!vert->is_deleted());
  float3 ans{0.0f, 0.0f, 0.0f};
  Edge e0 = vert->e;
  BLI_assert(!e0.is_null());
  Edge ecur = e0;
  do {
    Triangle *tri = ecur.tri();
    BLI_assert(!tri->is_deleted());
    if (!tri->is_ghost()) {
      Edge eprev = ecur.triangle_pred();
      float3 din = math::normalize(vert->co - v_src(eprev)->co);
      float3 dout = math::normalize(v_dst(ecur)->co - vert->co);
      float fac = saacos(-math::dot(din, dout));
      ans = ans + fac * tri->normal();
    }
    ecur = rot_ccw(ecur);
  } while (ecur != e0);
  ans = math::normalize(ans);
  return ans;
}

/** Analog of BM_vert_calc_shell_factor. */
static float vertex_shell_factor(Vert *vert)
{
  float accum_shell = 0.0f;
  float accum_angle = 0.0f;
  Edge e = vert->e;
  float3 vnorm = vertex_normal(vert);
  do {
    if (!e.tri()->is_ghost()) {
      Edge eprev = e.triangle_pred();
      float face_angle = angle_v3v3v3(v_src(eprev)->co, v_src(e)->co, v_dst(e)->co);
      accum_shell += shell_v3v3_normalized_to_dist(vnorm, e.tri()->normal()) * face_angle;
      accum_angle += face_angle;
    }
    e = rot_ccw(e);
  } while (e != vert->e);
  if (accum_angle != 0.0f) {
    return accum_shell / accum_angle;
  }
  return 1.0f;
}

class TriangleMesh {
  Vector<Triangle *> triangles_;
  Vector<Vert *> verts_;

 public:
  ~TriangleMesh()
  {
    for (Triangle *t : triangles_) {
      delete t;
    }
    for (Vert *v : verts_) {
      delete v;
    }
  }

  Vert *add_vert(const float3 co)
  {
    Vert *vert = new Vert(co);
    int v = int(verts_.append_and_get_index(vert));
    vert->id = v;
    return vert;
  }

  Vert *get_vert_by_index(const int index) const
  {
    return verts_[index];
  }

  Triangle *add_triangle(Vert *v0, Vert *v1, Vert *v2)
  {
    Triangle *tri = new Triangle(v0, v1, v2);
    int t = int(triangles_.append_and_get_index(tri));
    tri->set_id(t);
    return tri;
  }

  /** Add pointer to already allocated Triangle \a tri. Takes ownership of memory.*/
  void add_allocated_triangle(Triangle *tri)
  {
    int t = int(triangles_.append_and_get_index(tri));
    tri->set_id(t);
  }

  /** Split vertex \a v with edges \a e1 and \a e2 (which must attach to v)
   * attached to the new vertex and the rest attached to the old, with a
   * zero-length edge between the new and old. Return the new vertex.
   */
  Vert *split_vert(Vert *v, Edge e1, Edge e2);

  /** Collapse the edge \a e to the single vertex at its source end.
   * This will delete the trriangle that \a e is part of, and also the vertex
   * at the destination end of \a e, and will fix the affected neighbor relations.
   * Return the edge that is the collapse of the other two edges of the original
   * triangle, oriented away from the vertex that is the result of collapsing \a e.
   */
  Edge collapse_edge(Edge e);

  /** Collapse the triangle \a tri to a single vertex (the one at \a pos).
   * This will delete the triangle and its three adjacent ones, delete two vertices,
   * and fix appropriate neighbor relations.
   * Return the vertex that it all collapses to.
   */
  Vert *collapse_triangle(Triangle *tri, int pos);

  /** Delete \a tri, which should have a repeated vertex and therefore is degenerate.
   * This means merging the two non-degenerate sides, which means properly setting
   * the neighbor relations across the new single edge.
   * Also, if any vertex was using an edge in \a tri for its representative, then a
   * new representaative must be found.
   */
  Edge delete_degenerate_triangle(Triangle *tri);

  Span<Vert *> all_verts() const
  {
    return verts_.as_span();
  }

  Span<Triangle *> all_tris() const
  {

    return triangles_.as_span();
  }

  void calculate_all_tri_normals();

  void validate();

  friend std::ostream &operator<<(std::ostream &os, const TriangleMesh &trimesh);
};

/** Some debugging output routines. */

std::ostream &operator<<(std::ostream &os, const Edge e)
{
  if (e.is_null()) {
    os << "enull";
  }
  else {
    os << "e(t" << e.tri()->id() << "," << e.tri_edge_index() << ")";
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, const Vert &v)
{
  os << "v" << v.id << " co" << v.co << " " << v.e;
  return os;
}

std::ostream &operator<<(std::ostream &os, const Vert *v)
{
  if (v == nullptr) {
    os << "vnull";
  }
  else {
    os << *v;
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, const Triangle &tri)
{
  os << "t" << tri.id() << "(";
  for (int i = 0; i < 3; ++i) {
    if (tri.vert(i) == nullptr) {
      os << "vnull";
    }
    else {
      os << "v" << tri.vert(i)->id;
    }
    if (i < 2) {
      os << ",";
    }
  }
  os << ") nbr(";
  for (int i = 0; i < 3; ++i) {
    os << tri.neighbor(i);
    if (i < 2) {
      os << ",";
    }
  }
  os << ")";
  if (tri.in_region()) {
    os << " r";
  }
  for (int i = 0; i < 3; ++i) {
    if (tri.is_spoke(i)) {
      os << " s" << i;
    }
    if (tri.is_orig(i)) {
      os << " o" << i;
    }
  }
  if (tri.is_deleted()) {
    os << " deleted";
  }
  return os;
}

constexpr bool debug_ghost_triangles = false;

std::ostream &operator<<(std::ostream &os, const TriangleMesh &trimesh)
{
  os << "\nTriangleMesh\nVERTS\n";
  for (const Vert *v : trimesh.all_verts()) {
    if (!v->is_deleted()) {
      os << v << "\n";
    }
  }
  os << "\nTRIS\n";
  for (const Triangle *t : trimesh.all_tris()) {
    if (!t->is_deleted() && (debug_ghost_triangles || !t->is_ghost())) {
      os << *t << "\n";
    }
  }
  return os;
}

static void trimesh_draw(const std::string &label, const TriangleMesh &trimesh)
{
  static bool append = false; /* Will be set to true after first call. */

/* Would like to use #BKE_tempdir_base() here, but that brings in dependence on kernel library.
 * This is just for developer debugging anyway, and should never be called in production Blender.
 */
#ifdef _WIN32
  const char *drawfile = "./skel_debug_draw.html";
#else
  const char *drawfile = "/tmp/skel_debug_draw.html";
#endif

  constexpr int max_draw_width = 1800;
  constexpr int max_draw_height = 1600;
  constexpr double margin_expand = 0.05;
  constexpr int vert_radius = 3;
  constexpr bool draw_vert_labels = true;
  constexpr bool draw_face_labels = true;
  constexpr bool draw_ghost_labels = debug_ghost_triangles;

  Span<Vert *> verts = trimesh.all_verts();
  Span<Triangle *> tris = trimesh.all_tris();

  /* Get the best projection axis. */
  float3 avg_normal(0.0f, 0.0f, 0.0f);
  for (const Triangle *tri : tris) {
    avg_normal = avg_normal + triangle_normal(tri);
  }
  avg_normal = math::normalize(avg_normal);
  float axis_mat[3][3];
  axis_dominant_v3_to_m3(axis_mat, avg_normal);

  Array<float2> proj_vertco(verts.size());
  for (int64_t i : verts.index_range()) {
    mul_v2_m3v3(proj_vertco[i], axis_mat, verts[i]->co);
  }

  float2 vmin(FLT_MAX, FLT_MAX);
  float2 vmax(-FLT_MAX, -FLT_MAX);
  for (const float2 &pv : proj_vertco) {
    vmin[0] = math::min(vmin[0], pv[0]);
    vmin[1] = math::min(vmin[1], pv[1]);
    vmax[0] = math::max(vmax[0], pv[0]);
    vmax[1] = math::max(vmax[1], pv[1]);
  }
  double draw_margin = ((vmax.x - vmin.x) + (vmax.y - vmin.y)) * margin_expand;
  double minx = vmin.x - draw_margin;
  double maxx = vmax.x + draw_margin;
  double miny = vmin.y - draw_margin;
  double maxy = vmax.y + draw_margin;

  double width = maxx - minx;
  double height = maxy - miny;
  double aspect = height / width;
  int view_width = max_draw_width;
  int view_height = static_cast<int>(view_width * aspect);
  if (view_height > max_draw_height) {
    view_height = max_draw_height;
    view_width = static_cast<int>(view_height / aspect);
  }
  double scale = view_width / width;

  auto mapxy = [&](float2 z) -> float2 {
    return float2((z[0] - minx) * scale, (maxy - z[1]) * scale);
  };

  std::ofstream f;
  if (append) {
    f.open(drawfile, std::ios_base::app);
  }
  else {
    f.open(drawfile);
  }
  if (!f) {
    std::cout << "Could not open file " << drawfile << "\n";
    return;
  }

  f << "<div>" << label << "</div>\n<div>\n"
    << "<svg version=\"1.1\" "
       "xmlns=\"http://www.w3.org/2000/svg\" "
       "xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
       "xml:space=\"preserve\"\n"
    << "width=\"" << view_width << "\" height=\"" << view_height << "\">\n";

  for (const Triangle *tri : tris) {
    if (tri->is_deleted()) {
      continue;
    }
    if (tri->is_ghost()) {
      if (draw_ghost_labels) {
        const float2 &uco = proj_vertco[tri->vert(0)->id];
        const float2 &vco = proj_vertco[tri->vert(2)->id];
        float2 p = mapxy(0.5f * (uco + vco));
        f << "<text x=\"" << p[0] << "\" y=\"" << p[1] << "\" font-size=\"small\">g" << tri->id()
          << "</text>\n";
      }
    }
    else {
      float2 center(0.0f, 0.0f);
      for (int i = 0; i < 3; ++i) {
        float2 p0 = mapxy(proj_vertco[tri->vert(i)->id]);
        float2 p1 = mapxy(proj_vertco[tri->vert(succ_index(i))->id]);
        /* Make spokes and boundary between in-region and out-region bolder. */
        bool in_r = tri->in_region();
        bool other_in_r = neighbor_edge(Edge(tri, i)).tri()->in_region();
        bool spoke = tri->is_spoke(i);
        int width = spoke ? 4 : (in_r != other_in_r ? 3 : 1);
        f << "<line fill=\"none\" stroke=\"black\" stroke-width=\"" << width << "\" x1=\"" << p0[0]
          << "\" y1=\"" << p0[1] << "\" x2=\"" << p1[0] << "\" y2=\"" << p1[1] << "\"/>\n";
        center = center + p0;
      }
      if (draw_face_labels) {
        center = center / 3.0f;
        f << "<text x=\"" << center[0] << "\" y=\"" << center[1] << "\" font-size=\"small\">"
          << tri->id() << "</text>\n";
        /* Show first vertex with a dotted line from center to it. */
        float2 p0 = mapxy(proj_vertco[tri->vert(0)->id]);
        f << "<line fill=\"none\" stroke=\"grey\" stroke-width=\"1\" stroke-dasharray=\"2, 5\" "
             "x1=\""
          << center[0] << "\" y1=\"" << center[1] << "\" x2=\"" << p0[0] << "\" y2=\"" << p0[1]
          << "\"/>\n";
      }
    }
  }

  for (const Vert *vert : verts) {
    if (vert->is_deleted()) {
      continue;
    }
    float2 p = mapxy(proj_vertco[vert->id]);
    f << R"(<circle fill="black" cx=")" << p[0] << "\" cy=\"" << p[1] << "\" r=\"" << vert_radius
      << "\">\n";
    f << "  <title>[" << vert->id << "]" << vert->co << "</title>\n";
    f << "</circle>\n";
    if (draw_vert_labels) {
      f << "<text x=\"" << p[0] + vert_radius << "\" y=\"" << p[1] - vert_radius
        << R"(" font-size="small">v)" << vert->id << "</text>\n";
    }
  }

  f << "</svg>\n";
  append = true;
}

void TriangleMesh::calculate_all_tri_normals()
{
  for (Triangle *tri : triangles_) {
    if (!tri->is_ghost() && !tri->is_deleted()) {
      tri->calculate_normal();
    }
  }
}

/* Split vertex v with edges e1 and e2 (which must attach to v)
 * attached to the new vertex and the rest attached to the old, with a
 * zero-length edge between the new and old. Return the new vertex.
 *
 * We want to transform the diagram on the left to the one on the
 * right below.
 *
 *  --------------------        ---------------------------
 * |\                /|        |-                       -|
 * | \              / |        | \-                    / |
 * |  \      t3    /  |        |  \\-                 /  |
 * |   \          /   |        |   \ \       t3      /   |
 * |    \        /    |        |    \ \-            /    |
 * |  e2 \      /     |        |     \  \          /     |
 * |      \    /      |        |      \  \-       /      |
 * |       \  /       |        |       \tl \-    /       |
 * |        \/        |        |        \    \  /        |
 * |  t0   v/\    t2  |  ----> | t0   v -------/ v_new   |
 * |       /  \       |        |       /      /\         |
 * |      /    \      |        |      / tf  /-  \     t2 |
 * |     /      \     |        |     /    /-     \       |
 * |    /   t1   \    |        |    /   /-        -\     |
 * |   /          \   |        |   /  /-            \    |
 * |  / e1         \  |        |  / /-        t1     \   |
 * | /              \ |        | //-                  \  |
 * |------------------|        |/-                     - |
 *                              --------------------------
 *
 (diagram created using textik.com)
 */
Vert *TriangleMesh::split_vert(Vert *v, Edge e1, Edge e2)
{
  constexpr bool dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "split_vert v" << v->id << " " << e1 << " " << e2 << "\n";
  }
  BLI_assert(v_src(e1) == v && v_src(e2) == v);
  /* Gather the edges CCW around v from e1. */
  Vector<Edge> fan;
  Edge ecur = e1;
  do {
    fan.append(ecur);
    if (dbg_level > 0) {
      std::cout << " fan append " << ecur << "\n";
    }
    ecur = rot_ccw(ecur);
  } while (ecur != e1);
  bool e1_is_spoke = e1.tri()->is_spoke(e1.tri_edge_index());
  bool e2_is_spoke = e2.tri()->is_spoke(e2.tri_edge_index());

  /* Now make a new vertex, v_new, at the same position as v.
   * Every triangle between e1 and e2 (ccw) gets v_new replacing v.
   * Two new triangles then fill in the gaps bewteen the sides e1 and e2
   * and v_new.
   */
  Vert *v_new = this->add_vert(v->co);
  /* The representative edge of v needs to change if it is currently an
   * edge in the fan except those in t0. Easy just to always reassign it
   * via the newly made triangles.
   */
  v->e = null_edge;
  Triangle *tri_new_first = nullptr;
  Triangle *tri_new_last = nullptr;
  for (int64_t i : fan.index_range()) {
    ecur = fan[i];
    if (ecur == e2) {
      break;
    }
    Triangle *tri = ecur.tri();
    int pos = ecur.tri_edge_index();
    BLI_assert(tri->vert(pos) == v);
    tri->set_vert(pos, v_new);
    if (ecur == e1) {
      /* Make the extra triangle containing e1 and v_new. */
      Edge prev_tri_edge = tri->neighbor(pos);
      tri_new_first = this->add_triangle(v, v_dst(e1), v_new);
      set_mutual_neighbors(tri_new_first, 0, prev_tri_edge);
      set_mutual_neighbors(tri_new_first, 1, tri, pos);
      /* Neighbor for pos 2 of tri_new_first will be set when make tri_new_last. */
      if (e1_is_spoke) {
        tri_new_first->clear_spoke(0);
        tri_new_first->mark_spoke(1);
      }
      if (dbg_level > 0) {
        std::cout << "tri_new_first = " << *tri_new_first << "\n";
      }
    }
    Edge ecur_pred = ecur.triangle_pred();
    if (neighbor_edge(ecur_pred) == e2) {
      int pred_pos = pred_index(pos);
      /* Make the extra triangle containing neighbor_edge(e2) and v_new. */
      Edge next_tri_edge = tri->neighbor(pred_pos);
      tri_new_last = this->add_triangle(v, v_new, v_src(ecur_pred));
      BLI_assert(tri_new_first != nullptr);
      set_mutual_neighbors(tri_new_last, 0, tri_new_first, 2);
      set_mutual_neighbors(tri_new_last, 1, tri, pred_pos);
      set_mutual_neighbors(tri_new_last, 2, next_tri_edge);
      if (e2_is_spoke) {
        tri_new_last->mark_spoke(1);
        tri_new_last->clear_spoke(2);
      }
      if (dbg_level > 0) {
        std::cout << "tri_new_last = " << *tri_new_last << "\n";
      }
    }
  }
  return v_new;
}

/** Find and set a `v->e` that is not part of Triangle `tri`. */
static void set_rep_excluding(Vert *v, const Triangle *tri)
{
  Edge e0 = v->e;
  Edge ecur = e0;
  do {
    /* It is possible that, triangles around v may be deleted as we are in the process of deleting
     * v. */
    if (ecur.tri() != tri && !ecur.tri()->is_deleted()) {
      v->e = ecur;
      return;
    }
    ecur = rot_ccw(ecur);
  } while (ecur != e0);
  BLI_assert_unreachable();
}

/** Delete \a tri, which should have a repeated vertex and therefore is degenerate.
 * This means merging the two non-degenerate sides, which means properly setting
 * the neighbor relations across the new single edge.
 * Also, if any vertex was using an edge in \a tri for its representative, then a
 * new representaative must be found.
 * Return one of the edges that represents the merged non-degenerate edges.
 */
Edge TriangleMesh::delete_degenerate_triangle(Triangle *tri)
{
  /* Find positions of non-degenerate edges. */
  Vector<int> good_edges;
  for (int i = 0; i < 3; ++i) {
    if (tri->vert(i) != tri->vert(succ_index(i))) {
      good_edges.append(i);
    }
  }
  BLI_assert(good_edges.size() == 2);
  const int p0 = good_edges[0];
  const int p1 = good_edges[1];
  Edge en_0 = tri->neighbor(p0);
  Edge en_1 = tri->neighbor(p1);
  if (tri->is_spoke(p0) || tri->is_spoke(p1)) {
    en_0.tri()->mark_spoke(en_0.tri_edge_index());
    en_1.tri()->mark_spoke(en_1.tri_edge_index());
  }
  BLI_assert(en_0.tri() != en_1.tri());
  set_mutual_neighbors(en_0.tri(), en_0.tri_edge_index(), en_1.tri(), en_1.tri_edge_index());
  Vert *v0 = tri->vert(p0);
  Vert *v1 = tri->vert(p1);
  if (v0->e.tri() == tri) {
    set_rep_excluding(v0, tri);
  }
  if (v1->e.tri() == tri) {
    set_rep_excluding(v1, tri);
  }
  tri->mark_deleted();
  return en_0;
}

/* Collapse the edge \a e to the single vertex at its source end.
 * This will delete the trriangle that \a e is part of, and also the vertex
 * at the destination end of \a e, and will fix the affected neighbor relations.
 * Return the edge that is the collapse of the other two edges of the original
 * triangle, oriented away from the vertex that is the result of collapsing \a e.
 *
 * The general setup looks like this:
 *
 *                   /-            -
 *                  /- -\         -/ -\
 *                /-     -\     -/     -\
 *              /-  t2a    -\ -/   t1b   -\
 *            ------------------------------|
 *           /|              / \           /-\
 *         /-  \            / v2-\         |  -\
 *       /-    |    t2    /-      \   t1  /     -\
 *     /-  t2b  \        /         \      | t1a   -\
 *    --         \      /     t     -\   /         --
 *      \---      \   /-              \  |     ---/
 *          \---  |  /v0    e        v1-/  ---/
 *              \--\- ----------- ------|-/
 *                 /\                -/\
 *                /  -\     t0      /   -\
 *              /-     -\         -/      -\
 *             /  t0a    -\      /   t0b    -\
 *            /            -\v3-/             -\
 *           --------------- -/---------------- -
 *
 * It is possible that either or both of the "a" and "b" triangles may be missing
 * (that is, e.g., t0 and t1 might be neighbors, or only have one triangle betewen them).
 * The general result expected looks like this, and we return e'.
 *
 *                    -            -
 *                  -- \-        -/ -\
 *                -/     \-    -/     -\
 *              -/  t2a    \  /  t1b    -
 *              ------------ |------------
 *               \           |          /|
 *              | -\    t2   |  t1    /- |
 *              |   -\       |      /-   |
 *              |     -\   e'|    /-     |
 *              |  t2b  -\   |  /-  t1a  |
 *              |         -\v0/-         |
 *              |---------- --------------
 *                     --/   |  \--
 *                   -/      |     \-
 *                --/    t0a |  t0b  \--
 *              -/           |          \-
 *             --------------|-------------
 *
 * It is even possible that t0 == t1 or t0 == t2 but not both. If t0 == t1, we the result is:
 *
 *                    -            -
 *                  -- \-        -/ -\
 *                -/     \-    -/     -\
 *              -/  t2a    \  /  t0b    -
 *              ------------ |------------
 *               \           |          /|
 *              | -\    t2   |  t0a    /-
 *              |   -\       |      /-
 *              |     -\   e'|    /-
 *              |  t2b  -\   |  /-
 *              |         -\v0/-
 *              |-------------
 *
 * and if t0 == t2 it is like this:
 *
 *                                 -
 *                               -/ -\
 *                             -/     -\
 *                            /  t1b    -
 *              ------------ |------------
 *               \           |          /|
 *                -\    t2a  |  t1    /- |
 *                  -\       |      /-   |
 *                    -\   e'|    /-     |
 *                      -\   |  /-  t1a  |
 *                        -\v0/-         |
 *                          --------------
 *                     --/   |  \--
 *                   -/      |     \-
 *                --/    t0a |  t0b  \--
 *              -/           |          \-
 *             --------------|-------------
 *
 */
Edge TriangleMesh::collapse_edge(Edge e)
{
  Triangle *t_a = e.tri();
  Vert *v0 = v_src(e);
  Vert *v1 = v_dst(e);
  /* Gather triangles around `v1` that will get `v1` changed to `v0`. */
  Vector<Triangle *> v1_tris;
  Edge ecur = v1->e;
  do {
    Triangle *t = ecur.tri();
    BLI_assert(!t->is_ghost() && !t->is_deleted());
    v1_tris.append(t);
    ecur = rot_ccw(ecur);
  } while (ecur != v1->e);
  /* For each triangle in `v1_tris`, replace `v1` by `v0` then eliminate if now there are two
   * `v0`s. */
  Edge e_ans = null_edge;
  for (Triangle *t : v1_tris) {
    int v0_count = 0;
    for (int i = 0; i < 3; ++i) {
      Vert *v = t->vert(i);
      if (v == v1) {
        t->set_vert(i, v0);
        v0_count++;
      }
      else if (v == v0) {
        v0_count++;
      }
    }
    if (v0_count > 1) {
      Edge enew = delete_degenerate_triangle(t);
      if (t == t_a) {
        e_ans = enew;
      }
    }
  }
  v1->mark_deleted();
  BLI_assert(e_ans != null_edge);
  if (v_src(e_ans) != v0) {
    e_ans = e_ans.tri()->neighbor(e_ans.tri_edge_index());
    BLI_assert(v_src(e_ans) == v0);
  }
  return e_ans;
}

/** Collapse the triangle \a tri to a single vertex (the one at \pos)).
 * This will delete the triangle and its three adjacent ones, delete two vertices,
 * and fix appropriate neighbor relations.
 * Return the vertex that it all collapses to.
 *
 * See the diagram before collapse_edge for the general setup.
 * If we also collapse edge e' after doing the first collapse, we get
 * the result desired here.
 */
Vert *TriangleMesh::collapse_triangle(Triangle *tri, int pos)
{
  BLI_assert(!tri->is_ghost());
  Edge e = tri->edge(pos);
  Vert *v = v_src(e);
  Edge e_prime = collapse_edge(e);
  collapse_edge(e_prime);
  return v;
}

/** Check that we have a valid Triangle mesh, asserting false if not. */
void TriangleMesh::validate()
{
  for (const Vert *v : verts_) {
    if (!v->is_deleted()) {
      const Edge e = v->e;
      const Triangle *t = e.tri();
      const int index = e.tri_edge_index();
      BLI_assert(t != nullptr);
      BLI_assert(!t->is_deleted());
      BLI_assert(0 <= index && index <= 2);
      int count_edges = 0;
      Edge eloop = e;
      do {
        eloop = rot_ccw(eloop);
        BLI_assert(!eloop.tri()->is_deleted());
        count_edges++;
        BLI_assert(count_edges < 1000000);
      } while (eloop != e);
    }
  }
  for (const Triangle *t : triangles_) {
    if (!t->is_deleted()) {
      if (t->is_ghost()) {
        BLI_assert(!t->is_deleted());
        BLI_assert(t->vert(0) != nullptr && !t->vert(0)->is_deleted());
        BLI_assert(t->vert(1) == nullptr);
        BLI_assert(t->vert(2) != nullptr && !t->vert(2)->is_deleted());
      }
      else {
        for (int i = 0; i < 2; ++i) {
          const Edge e = t->edge(i);
          const Edge en = t->neighbor(i);
          BLI_assert(!en.is_null());
          const Triangle *tn = en.tri();
          const int in = en.tri_edge_index();
          BLI_assert(tn->neighbor(in) == e);
        }
      }
    }
  }
}

/** Make the initial contours inset for \a contours
 * The contour must be a sequence of vertices in \a trimesh such that there is an edge
 * between each successive pair, and between the last and the first.
 * Initially there will be zero distance between the inset edge and the one it is inset from.
 * The return value is the vector of Edges paralleling \a cojntours but giving
 * the Egdes corresponding to the inset-by-zero original contours.
 * The Edges that connect the original contour vertices to their inset versions
 * need to be marked as "spokes".
 */
static Vector<Vector<Edge>> init_contour_inset(TriangleMesh &trimesh, Span<Vector<int>> contours)
{
  Vector<Vector<Edge>> ans;
  ans.reserve(contours.size());
  for (const Vector<int> &cont : contours) {
    /* Find the edges that make up the contour. */
    int n = int(cont.size());
    Array<Edge> cont_edges(n);
    for (int i = 0; i < n; ++i) {
      int v_index = cont[i];
      int v_next_index = cont[(i + 1) % n];
      Vert *v = trimesh.get_vert_by_index(v_index);
      Vert *v_next = trimesh.get_vert_by_index(v_next_index);
      BLI_assert(v != nullptr && v_next != nullptr);
      Edge e = edge_between(v, v_next);
      BLI_assert(!e.is_null());
      cont_edges[i] = e;
    }
    /* Split each vertex in the contour with split edges being contour edges. */

    Vector<Vert *> split_verts;
    split_verts.reserve(n);
    for (int i = 0; i < n; ++i) {
      Vert *v = trimesh.get_vert_by_index(cont[i]);
      Edge e = cont_edges[i];
      Edge e_prev_reverse = neighbor_edge(cont_edges[(i + n - 1) % n]);
      Vert *v_split = trimesh.split_vert(v, e, e_prev_reverse);
      split_verts.append(v_split);
      Edge e_spoke = edge_between(v, v_split);
      BLI_assert(!e_spoke.is_null());
      e_spoke.tri()->mark_spoke(e_spoke.tri_edge_index());
    }
    ans.append(Vector<Edge>());
    Vector<Edge> &contour_edges = ans.last();
    contour_edges.reserve(n);
    for (int i = 0; i < int(split_verts.size()); ++i) {
      Vert *v0 = split_verts[i];
      Vert *v1 = split_verts[(i + 1) % n];
      Edge e = edge_between(v0, v1);
      BLI_assert(!e.is_null());
      contour_edges.append(e);
    }
  }
  return ans;
}

static float det(const float3 &v1, const float3 &v2, const float3 &n)
{
  return math::dot(math::cross_high_precision(v1, v2), n);
}

/** Solve a*x*x + b*x + c = 0 and return the number of real roots,
 * and set *r_root1 to the first root and *r_root2 to the second, as each of those exist.
 */
static int solve_quadratic(float a, float b, float c, float *r_root1, float *r_root2)
{
  if (a == 0.0f) {
    if (b == 0.0f) {
      return 0;
    }
    *r_root1 = -c / b;
    return 1;
  }
  float p = -b / a / 2;
  float q = c / a;
  float discr = p * p - q;
  if (fabsf(discr) < 2e-7 * abs(q)) {
    /* Duplicate zero. */
    *r_root1 = p;
    return 1;
  }
  if (discr < 0.0f) {
    return 0;
  }
  /* Numerically stable solution to the quadratic eq. */
  float x1 = p + copysignf(sqrtf(discr), p);
  if (x1 == 0.0f) {
    *r_root1 = x1;
    return 1;
  }
  float x2 = q / x1;
  *r_root1 = x1;
  *r_root2 = x2;
  return 2;
}

static std::pair<float3, float> calc_velo(const float3 &delta_prev,
                                          const float3 &delta_next,
                                          const float3 &normal)
{
  float3 r1 = delta_next - delta_prev;
  r1 = r1 - math::cross_high_precision(math::cross_high_precision(r1, normal), normal);
  float3 velo;
  /* Get the best precision bisector. */
  if (math::length_squared(r1) > 1e-12f * math::length_squared(delta_next)) {
    velo = r1;
  }
  else {
    float3 r2 = delta_next + delta_prev;
    velo = math::cross(r2, normal);
  }
  float dhdl = det(velo, delta_next, normal);
  if (dhdl < 0) {
    return std::pair<float3, float>(-velo, -dhdl);
  }
  else {
    return std::pair<float3, float>(velo, dhdl);
  }
}

class SkeletonVertex {
  float3 position_;
  float3 delta_prev_;
  float3 delta_next_;
  float3 normal_;
  float3 velo_;
  float dhdl_;
  float height_;

 public:
  SkeletonVertex(
      float3 position, float height, float3 *delta_prev, float3 *delta_next, float3 *normal);

  float3 position() const
  {
    return position_;
  }

  float3 delta_prev() const
  {
    return delta_prev_;
  }

  float3 delta_next() const
  {
    return delta_next_;
  }

  float3 normal() const
  {
    return normal_;
  }

  float3 velo() const
  {
    return velo_;
  }

  float dhdl() const
  {
    return dhdl_;
  }

  float height() const
  {
    return height_;
  }

  friend std::ostream &operator<<(std::ostream &os, const SkeletonVertex &skv);
};

SkeletonVertex::SkeletonVertex(
    float3 position, float height, float3 *delta_prev, float3 *delta_next, float3 *normal)
    : position_(position), height_(height)
{
  if (delta_prev != nullptr && delta_next != nullptr) {
    /* This is a moving SkeletonVertex. */
    BLI_assert(normal != nullptr);
    delta_prev_ = *delta_prev;
    delta_next_ = *delta_next;
    normal_ = *normal;
    std::pair<float3, float> velo_dhdl = calc_velo(*delta_prev, *delta_next, *normal);
    velo_ = velo_dhdl.first;
    dhdl_ = velo_dhdl.second;
  }
  else {
    /* This is a stationary inner vertex. */
    delta_prev_ = float3(0.0, 0.0, 0.0);
    delta_next_ = float3(0.0, 0.0, 0.0);
    if (normal != nullptr) {
      normal_ = *normal;
    }
    else {
      normal_ = float3(0.0, 0.0, 0.0);
    }
    velo_ = float3(0.0, 0.0, 0.0);
    dhdl_ = 1.0;  // Zero will cause trouble.
  }
}

static constexpr float inf = std::numeric_limits<float>::infinity();

std::ostream &operator<<(std::ostream &os, const SkeletonVertex &skv)
{
  os << "skv(" << skv.position_ << ", dp=" << skv.delta_prev_ << ", dn=" << skv.delta_next_
     << ", n=" << skv.normal_ << ", velo=" << skv.velo_ << ", dhdl=" << skv.dhdl_
     << ", h=" << skv.height_ << ")";
  return os;
}

class SkeletonEvent {
  Edge edge_;
  float height_{inf};
  float3 final_pos_{0.0f, 0.0f, 0.0f};
  int epoch_{0};
  bool split_event_{false};

 public:
  SkeletonEvent(Edge edge) : edge_(edge)
  {
  }

  SkeletonEvent(Edge edge, float height, float3 final_pos, bool split_event, int epoch)
      : edge_(edge),
        height_(height),
        final_pos_(final_pos),
        epoch_(epoch),
        split_event_(split_event)
  {
  }

  Edge edge() const
  {
    return edge_;
  }

  float height() const
  {
    return height_;
  }

  float3 final_pos() const
  {
    return final_pos_;
  }

  bool split_event() const
  {
    return split_event_;
  }

  bool is_valid() const
  {
    Triangle *t = edge_.tri();
    return height_ != inf && !t->is_deleted() && t->in_region();
  }

  int epoch() const
  {
    return epoch_;
  }

  friend std::ostream &operator<<(std::ostream &os, const SkeletonEvent &ev);
};

std::ostream &operator<<(std::ostream &os, const SkeletonEvent &ev)
{
  if (ev.is_valid()) {
    os << "ev(h=" << ev.height_ << ", edge=" << ev.edge_ << ", fpos=" << ev.final_pos_
       << ", split=" << ev.split_event_ << ", epoch=" << ev.epoch_ << ")";
  }
  else {
    os << "<invalid event>";
  }
  return os;
}

class SkeletonEventCompare {
 public:
  /* Since we want lowest height first, have to implement "greater"
   * because priority queue will make the greatest element the one
   * that is "top" and popped next. */
  bool operator()(const SkeletonEvent &ev1, const SkeletonEvent &ev2)
  {
    /* We want the later-added event if the triangles are the same. */
    if (ev1.edge().tri() == ev2.edge().tri()) {
      return ev1.epoch() < ev2.epoch();
    }
    if (ev1.height() > ev2.height()) {
      return true;
    }
    else if (ev1.height() < ev2.height()) {
      return false;
    }
    else {
      int s1 = ev1.split_event();
      int s2 = ev2.split_event();
      if (s1 > s2) {
        return true;
      }
      else if (s1 < s2) {
        return false;
      }
      else {
        /* Arbitrary tie breaker that is deterministic. */
        return v_src(ev1.edge())->id > v_src(ev2.edge())->id;
      }
    }
  }
};

class StraightSkeleton {
  /** Calculation argument parameters. . */
  TriangleMesh &trimesh_;
  Span<Vector<int>> contours_;
  float target_height_;

  /** Contoiur and Region data. */
  Vector<Vector<Edge>> contour_edges_;
  Set<Edge> contour_edge_set_;
  int tot_region_triangles_{0};

  /** Algorithm data structiures. */
  Vector<SkeletonVertex *> skel_vertices_;
  Map<int, SkeletonVertex *> skel_vertex_map_;
  std::priority_queue<SkeletonEvent, std::vector<SkeletonEvent>, SkeletonEventCompare>
      event_queue_;
  int tot_flip_events_{0};
  int epoch_{0};

 public:
  /** Algorithm output data structures. */
  Map<int, float> vertex_height_map;
  Set<Triangle *> remaining_triangles_set;

  StraightSkeleton(TriangleMesh &trimesh, Span<Vector<int>> contours, float target_height);
  ~StraightSkeleton();

  void compute();

 private:
  SkeletonVertex *add_skeleton_vertex(
      float3 position, float height, float3 *delta_prev, float3 *delta_next, float3 *normal)
  {
    /* Store the allocated SkeletonVertex so can free at end.
     * TODO: allocate these from a pool.
     */
    skel_vertices_.append(new SkeletonVertex(position, height, delta_prev, delta_next, normal));
    return skel_vertices_.last();
  }

  /** Set up data needed about contours and arrays, assumine contour_edges_ is set. */
  void calculate_contour_and_region_data();

  /** Record \a skv as the SkeletonVertex for the vertex with index \a vert_index. */
  void set_skel_vertex_map(int vert_index, SkeletonVertex *skv)
  {
    skel_vertex_map_.add_overwrite(vert_index, skv);
  }

  /** Remove \a skv from the map. */
  void remove_from_skel_vertex_map(int vert_index)
  {
    skel_vertex_map_.remove(vert_index);
  }

  SkeletonVertex *skel_vertex_map(int vert_index) const
  {
    return skel_vertex_map_.lookup_default(vert_index, nullptr);
  }

  bool skel_vertex_map_has_id(int vert_index) const
  {
    return skel_vertex_map_.contains(vert_index);
  }

  SkeletonVertex *get_skel_vertex(Edge e) const
  {
    return skel_vertex_map_.lookup_default(v_src(e)->id, nullptr);
  }

  /** Add events. */
  void add_events(Vert *vert, float min_height, bool instant);

  /** Add the triangle with edge \a edge to the event queue. */
  void add_triangle(Edge edge, float min_height);

  /** Handle vertex event (edge collapse). Return the new spoke edge. */
  Edge handle_vertex_event(Edge edge);

  /** Handle split event where \a edge starts at a reflex vertex.. */
  std::tuple<Vert *, Vert *, Vert *> handle_split_event(Edge edge);

  /** Handle a flip event. */
  std::pair<Edge, Edge> handle_flip_event(Edge edge);

  /** Handle a closing event. */
  void handle_closing_event(Edge edge);

  /** For debugging . */
  void dump_event_queue() const;
  void dump_state() const;
};

StraightSkeleton::StraightSkeleton(TriangleMesh &trimesh,
                                   Span<Vector<int>> contours,
                                   float target_height)
    : trimesh_(trimesh), contours_(contours), target_height_(target_height)
{
  /* Build sets of the contour edges and region triangles,
   * to enable fast tests for being a contour edge or region triangle. */
}

StraightSkeleton::~StraightSkeleton()
{
  /* Free the SkeletonVertex memory. */
  for (SkeletonVertex *skv : skel_vertices_) {
    delete skv;
  }
}

void StraightSkeleton::dump_event_queue() const
{
  std::cout << "Event Queue\n";
  /* Copy it, so can empty the copy while printing. */
  std::priority_queue<SkeletonEvent, std::vector<SkeletonEvent>, SkeletonEventCompare> q =
      event_queue_;
  while (!q.empty()) {
    std::cout << q.top() << "\n";
    q.pop();
  }
}

void StraightSkeleton::dump_state() const
{
  std::cout << "State\n";
  dump_event_queue();
  std::cout << trimesh_;
  trimesh_draw("dump_state", trimesh_);
  int num_t = int(trimesh_.all_tris().size());
  for (int i = 0; i < num_t; ++i) {
    SkeletonVertex *skv = skel_vertex_map_.lookup_default(i, nullptr);
    if (skv != nullptr) {
      std::cout << "skv[" << i << "] = " << *skv << "\n";
    }
  }
}

/** Calculate contour_edge_set_ and region_triangles_set_ . */
void StraightSkeleton::calculate_contour_and_region_data()
{
  /* Get a Set containing all the contour edges. */
  for (const Vector<Edge> &contour : contour_edges_) {
    for (Edge e : contour) {
      contour_edge_set_.add(e);
    }
  }
  /* Find all the triangles interior to (left of) contours.
   * This is the closure of triangles reachable by triangle
   * neighbors, but not crossing any contour edge.
   * Mark each by setting their region flags,
   * and also count how many there are.
   */
  for (const Vector<Edge> &contour : contour_edges_) {
    if (contour.size() < 3) {
      /* There cannot be any contained triangles. */
      continue;
    }
    Triangle *seed_tri = contour[0].tri();
    seed_tri->mark_in_region();
    tot_region_triangles_++;
    Vector<Triangle *> stack;
    stack.append(seed_tri);
    while (!stack.is_empty()) {
      Triangle *tri = stack.pop_last();
      for (int i = 0; i < 3; ++i) {
        Edge e = tri->edge(i);
        /* Cannot cross over contour edges. */
        if (!contour_edge_set_.contains(e)) {
          Edge e_nbr = neighbor_edge(e);
          BLI_assert(!e_nbr.is_null());
          Triangle *t_nbr = e_nbr.tri();
          if (t_nbr && !t_nbr->is_ghost()) {
            if (!t_nbr->in_region()) {
              t_nbr->mark_in_region();
              tot_region_triangles_++;
              stack.append(t_nbr);
            }
          }
        }
      }
    }
  }
}

/** Wavefront edges are between region triangles and non-region triangles. */
static bool is_wavefront_edge(Edge e)
{
  if (e.is_null()) {
    return false;
  }
  bool r1 = e.tri()->in_region();
  bool r2 = neighbor_edge(e).tri()->in_region();
  return r1 != r2;
}

/** Return the first edge ccw of e that is a wavefront edge, or null_edge if none. */
static Edge find_ccw_wavefront_edge(Edge e)
{
  Edge ans = rot_ccw(e);
  while (!is_wavefront_edge(ans)) {
    ans = rot_ccw(ans);
    if (ans == e) {
      return null_edge;
    }
  }
  return ans;
}

/** Return the first edge cw of e that is a wavefront edge, or null_edge if none. */
static Edge find_cw_wavefront_edge(Edge e)
{
  Edge ans = rot_cw(e);
  while (!is_wavefront_edge(ans)) {
    ans = rot_cw(ans);
    if (ans == e) {
      return null_edge;
    }
  }
  return ans;
}

/** Return the first edge cw from \a edge that is a spoke or wavefront edge. */
static Edge find_cw_spoke_or_wavefront_edge(Edge edge)
{
  Edge e = rot_cw(edge);
  do {
    if (e.tri()->is_spoke(e.tri_edge_index()) || is_wavefront_edge(e)) {
      return e;
    }
    e = rot_cw(e);
  } while (e != edge);
  return null_edge;
}

static Edge find_cw_wavefront_or_orig_edge(Edge edge)
{
  Edge e = rot_cw(edge);
  do {
    if (is_wavefront_edge(e) || e.tri()->is_orig(e.tri_edge_index())) {
      return e;
    }
    e = rot_cw(e);
  } while (e != edge);
  return null_edge;
}

static constexpr float dhdl_epsilon = 1e-5f;
static constexpr float collision_epsilon = 1e-5;

void StraightSkeleton::add_events(Vert *vert, float min_height, bool instant)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "add_events " << *vert << " min_height=" << min_height << " instant=" << instant
              << "\n";
  }
  if (!instant) {
    Edge e = vert->e;
    do {
      Triangle *tri = e.tri();
      if (tri->in_region()) {
        add_triangle(e, min_height);
      }
      e = rot_ccw(e);
    } while (e != vert->e);
  }
  else {
    float best_sqr = inf;
    Edge best = null_edge;
    float3 ref_pos = vert->co;
    float3 best_pos = ref_pos;
    Edge e = vert->e;
    do {
      if (e.tri()->in_region()) {
        Edge face_edge = e.triangle_succ().triangle_succ();
        bool out1 = is_wavefront_edge(face_edge);
        bool out2 = is_wavefront_edge(face_edge.triangle_succ());
        SkeletonVertex *skv1 = get_skel_vertex(face_edge);
        SkeletonVertex *skv2 = get_skel_vertex(face_edge.triangle_succ().triangle_succ());
        if (out1 && skv1->dhdl() != 0.0f) {
          float3 pos = skv1->position() +
                       skv1->velo() * (min_height - skv1->height()) / skv1->dhdl();
          float d = math::distance_squared(pos, ref_pos);
          if (d < best_sqr) {
            best_sqr = d;
            best = face_edge;
            best_pos = pos;
          }
        }
        if (out2 && skv2->dhdl() != 0.0f) {
          float3 pos = skv2->position() +
                       skv2->velo() * (min_height - skv2->height()) / skv2->dhdl();
          float d = math::distance_squared(pos, ref_pos);
          if (d < best_sqr) {
            best_sqr = d;
            best = face_edge.triangle_succ();
            best_pos = pos;
          }
        }
      }
      e = rot_ccw(e);
    } while (e != vert->e);
    if (dbg_level > 0) {
      std::cout << "instant case: pushing event for edge " << best << " at height " << min_height
                << "\n";
    }
    event_queue_.push(SkeletonEvent(best, min_height, best_pos, false, epoch_));
    e = vert->e;
    do {
      Edge face_edge = e.triangle_succ().triangle_succ();
      if (face_edge.tri()->in_region() && face_edge != best && face_edge.triangle_succ() != best) {
        if (dbg_level > 0) {
          std::cout << "instant case: pushing dummy event for edge " << face_edge << "\n";
        }
        event_queue_.push(SkeletonEvent(face_edge));
      }
      e = rot_ccw(e);
    } while (e != vert->e);
  }
}

void StraightSkeleton::add_triangle(Edge edge, float min_height)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "add_triangle(" << edge << ", " << min_height << ")\n";
  }
  bool out1 = is_wavefront_edge(edge);
  bool out2 = is_wavefront_edge(edge.triangle_succ());
  bool out3 = is_wavefront_edge(edge.triangle_pred());
  int out_num = int(out1) + int(out2) + int(out3);
  SkeletonVertex *skv1 = get_skel_vertex(edge);
  SkeletonVertex *skv2 = get_skel_vertex(edge.triangle_succ());
  SkeletonVertex *skv3 = get_skel_vertex(edge.triangle_pred());
  if (dbg_level > 1) {
    std::cout << "out1=" << out1 << ", out2=" << out2 << ", out3=" << out3 << "\n";
    std::cout << " skv1=" << *skv1 << "\n skv2=" << *skv2 << "\n skv3=" << *skv3 << "\n";
  }
  /* This special case handling is needed, it seems. */
  if (fabsf(skv1->dhdl()) < dhdl_epsilon) {
    if (skv2->dhdl() == 0.0f || skv3->dhdl() == 0.0f) {
      event_queue_.push(SkeletonEvent(edge));
      return;
    }
    float best_sqr = inf;
    Edge best = null_edge;
    float3 ref_pos = skv1->position();
    float3 best_pos = ref_pos;
    if (out1) {
      float3 pos = skv2->position() + skv2->velo() * (min_height - skv2->height()) / skv2->dhdl();
      float d = math::distance_squared(pos, ref_pos);
      if (d < best_sqr) {
        best_sqr = d;
        best = edge;
        best_pos = pos;
      }
    }
    if (out3) {
      float3 pos = skv3->position() + skv3->velo() * (min_height - skv3->height()) / skv3->dhdl();
      float d = math::distance_squared(pos, ref_pos);
      if (d < best_sqr) {
        best_sqr = d;
        best = edge.triangle_succ().triangle_succ();
        best_pos = pos;
      }
    }
    if (best.is_null()) {
      event_queue_.push(SkeletonEvent(edge));
    }
    else {
      event_queue_.push(SkeletonEvent(best, min_height, best_pos, false, epoch_));
    }
    return;
  }
  if (fabsf(skv2->dhdl()) <= dhdl_epsilon) {
    if (skv1->dhdl() == 0.0f || skv3->dhdl() == 0.0f) {
      return;
    }
    float best_sqr = inf;
    Edge best = null_edge;
    float3 ref_pos = skv2->position();
    float3 best_pos = ref_pos;
    if (out1) {
      float3 pos = skv1->position() + skv1->velo() * (min_height - skv1->height()) / skv1->dhdl();
      float d = math::distance_squared(pos, ref_pos);
      if (d < best_sqr) {
        best_sqr = d;
        best = edge;
        best_pos = pos;
      }
    }
    if (out2) { /* CHECK: Should this test be if out3? */
      float3 pos = skv3->position() + skv3->velo() * (min_height - skv3->height()) / skv3->dhdl();
      float d = math::distance_squared(pos, ref_pos);
      if (d < best_sqr) {
        best_sqr = d;
        best = edge.triangle_succ(); /* CHECK another succ? */
        best_pos = pos;
      }
    }
    if (best.is_null()) {
      event_queue_.push(SkeletonEvent(edge));
    }
    else {
      event_queue_.push(SkeletonEvent(best, min_height, best_pos, false, epoch_));
    }
    return;
  }
  if (fabsf(skv3->dhdl()) <= dhdl_epsilon) {
    if (skv1->dhdl() == 0.0f || skv2->dhdl() == 0.0f) {
      event_queue_.push(SkeletonEvent(edge));
      return;
    }
    float best_sqr = inf;
    Edge best = null_edge;
    float3 ref_pos = skv3->position();
    float3 best_pos = ref_pos;
    if (out2) {
      float3 pos = skv2->position() + skv2->velo() * (min_height - skv2->height()) / skv2->dhdl();
      float d = math::distance_squared(pos, ref_pos);
      if (d < best_sqr) {
        best_sqr = d;
        best = edge.triangle_succ();
        best_pos = pos;
      }
    }
    if (out3) {
      float3 pos = skv1->position() + skv1->velo() * (min_height - skv1->height()) / skv1->dhdl();
      float d = math::distance_squared(pos, ref_pos);
      if (d < best_sqr) {
        best_sqr = d;
        best = edge.triangle_succ().triangle_succ();
        best_pos = pos;
      }
    }
    if (best.is_null()) {
      event_queue_.push(SkeletonEvent(edge));
    }
    else {
      event_queue_.push(SkeletonEvent(best, min_height, best_pos, false, epoch_));
    }
    return;
  }
  float3 x1 = v_src(edge)->co;
  float3 x2 = v_src(edge.triangle_succ())->co;
  float3 x3 = v_src(edge.triangle_succ().triangle_succ())->co;
  float3 normal = math::normalize(skv1->normal() + skv2->normal() + skv3->normal());
  float3 v1 = skv1->velo() / skv1->dhdl();
  float3 v2 = skv2->velo() / skv2->dhdl();
  float3 v3 = skv3->velo() / skv3->dhdl();
  x1 = x1 + v1 * (min_height - skv1->height());
  x2 = x2 + v2 * (min_height - skv2->height());
  x3 = x3 + v3 * (min_height - skv3->height());
  float3 dx1 = x1 - x2;
  float3 dx2 = x2 - x3;
  float3 dx3 = x1 - x3;
  float3 dv1 = v1 - v2;
  float3 dv2 = v2 - v3;
  float3 dv3 = v1 - v3;
  float c = det(dx1, dx2, normal);
  bool positive = c > 0.0f;
  if (out_num == 3) {
    /* Closing event. */
    float a = det(dv1, dv2, normal);
    float b = det(dx1, dv2, normal) + det(dv1, dx2, normal);
    /* Solve 2*a*x+b = 0. */
    float height = -0.5f * b / a;
    event_queue_.push(SkeletonEvent(edge, min_height + height, x1 + v1 * height, false, epoch_));
    if (dbg_level > 1) {
      std::cout << "added closing event " << event_queue_.top() << "\n";
    }
    return;
  }

  auto div_zero = [&](float a, float b) -> float {
    if (fabsf(b) > 1e-7) {
      return a / b;
    }
    if (!positive) {
      return 1e-16;
    }
    return std::numeric_limits<double>::quiet_NaN();
  };

  /* Use collapsing edges as indicator => solve simple linear equation. */
  float dv1_sqr = math::length_squared(dv1);
  float dv2_sqr = math::length_squared(dv2);
  float dv3_sqr = math::length_squared(dv3);
  if (dv1_sqr == 0.0f && dv2_sqr == 0.0f && dv3_sqr == 0.0f) {
    event_queue_.push(SkeletonEvent(edge));
    return;
  }
  float lin_height1 = div_zero(-math::dot(dx1, dv1), dv1_sqr);
  float lin_height2 = div_zero(-math::dot(dx2, dv2), dv2_sqr);
  float lin_height3 = div_zero(-math::dot(dx3, dv3), dv3_sqr);

  if (out_num == 2) {
    /* Vertex Event. */
    if (dbg_level > 1) {
      std::cout << "out_num==2 vertex event case\n";
    }
    float epsilon = positive ? 0.0f : fabsf(c) + 1e-6;  // TODO: remove epsilon?
    if (!out1) {
      if ((lin_height2 < lin_height3 || !(lin_height3 >= -epsilon)) && lin_height2 >= -epsilon) {
        event_queue_.push(SkeletonEvent(
            edge.triangle_succ(), lin_height2 + min_height, x2 + v2 * lin_height2, false, epoch_));
      }
      else if (lin_height3 >= -epsilon) {
        event_queue_.push(SkeletonEvent(edge.triangle_succ().triangle_succ(),
                                        lin_height3 + min_height,
                                        x3 + v3 * lin_height3,
                                        false,
                                        epoch_));
      }
      else {
        event_queue_.push(SkeletonEvent(edge));
      }
    }
    else if (!out2) {
      if ((lin_height1 < lin_height3 || !(lin_height3 >= -epsilon)) && lin_height1 >= -epsilon) {
        event_queue_.push(
            SkeletonEvent(edge, lin_height1 + min_height, x1 + v1 * lin_height1, false, epoch_));
      }
      else if (lin_height3 >= -epsilon) {
        event_queue_.push(SkeletonEvent(edge.triangle_succ().triangle_succ(),
                                        lin_height3 + min_height,
                                        x3 + v3 * lin_height3,
                                        false,
                                        epoch_));
      }
      else {
        event_queue_.push(SkeletonEvent(edge));
      }
    }
    else if (!out3) {
      if ((lin_height2 < lin_height1 || !(lin_height1 >= -epsilon)) && lin_height2 >= -epsilon) {
        event_queue_.push(SkeletonEvent(
            edge.triangle_succ(), lin_height2 + min_height, x2 + v2 * lin_height2, false, epoch_));
      }
      else if (lin_height1 >= -epsilon) {
        event_queue_.push(
            SkeletonEvent(edge, lin_height1 + min_height, x1 + v1 * lin_height1, false, epoch_));
      }
      else {
        event_queue_.push(SkeletonEvent(edge));
      }
    }
    return;
  }

  /* General case -> solve quadratic equation a*x*x + b*x + c = 0 , */
  float a = det(dv1, dv2, normal);
  float b = det(dx1, dv2, normal) + det(dv1, dx2, normal);
  float res1, res2;
  int nroots = solve_quadratic(a, b, c, &res1, &res2);
  float height = inf;
  for (int i = 0; i < nroots; ++i) {
    float h = i == 1 ? res1 : res2;
    if (0.0f <= h && h < height) {
      height = h;
    }
  }
  /* Replace the quadratic solution if the triangle is zero and doesn't change size.
   * TODO: change the constants for what is appropriate for floats. */
  if (fabsf(a) < 1e-10f && fabsf(b) < 1e-4f && fabsf(c) < 1e-4f) {
    nroots = 1;
    res1 = 0.0f;
  }
  if (dbg_level > 1) {
    std::cout << "general case, nroots=" << nroots << " res1=" << res1 << " res2=" << res2 << "\n";
  }
  /* Check if edge collapse times were missed. */
  float lin_height1_ = div_zero(-math::length_squared(dx1), math::dot(dx1, dv1));
  float lin_height2_ = div_zero(-math::length_squared(dx2), math::dot(dx2, dv2));
  float lin_height3_ = div_zero(-math::length_squared(dx3), math::dot(dx3, dv3));
  /* Note: for floats, changed 1e-7 to 1e-5 below. */
  if (lin_height1 >= 0 && !(lin_height1 >= height) && fabsf(lin_height1 - lin_height1_) < 1e-5) {
    height = lin_height1;
  }
  if (lin_height2 >= 0 && !(lin_height2 >= height) && fabsf(lin_height2 - lin_height2_) < 1e-5) {
    height = lin_height2;
  }
  if (lin_height3 >= 0 && !(lin_height3 >= height) && fabsf(lin_height3 - lin_height3_) < 1e-5) {
    height = lin_height3;
  }
  /* Abort if there is no event. */
  if (positive) {
    if (height == inf || !(height >= 0.0f)) {
      if (dbg_level > 1) {
        std::cout << "no event so force abort\n";
      }
      event_queue_.push(SkeletonEvent(edge));
      return;
    }
  }
  else {
    /* Note: changes 1e-8 to 1e-5 for float. */
    if (nroots == 0 || !(res1 >= -fabsf(c) - 1e-8f)) {
      if (dbg_level > 1) {
        std::cout << "no event2 so force abort\n";
      }
      event_queue_.push(SkeletonEvent(edge));
      return;
    }
    height = res1;
  }
  float len1 = math::length_squared(dx1 + dv1 * height);
  float len2 = math::length_squared(dx2 + dv2 * height);
  float len3 = math::length_squared(dx3 + dv3 * height);
  if (dbg_level > 1) {
    std::cout << "len1=" << len1 << " len2=" << len2 << " len3=" << len3 << "\n";
  }
  /* If no length is 0, then this is a split event and the special
   * edge is the longest one.
   * Add the edge to eh queue which has the split vertex as its origin. */
  if (len1 > len2 && len1 > len3) {
    /* Vertex event. */
    if (len2 <= len3 && out2 && !out1) {
      event_queue_.push(SkeletonEvent(
          edge.triangle_succ(), height + min_height, x2 + v2 * height, false, epoch_));
    }
    else if (len2 >= len3 && out3 && !out1) {
      event_queue_.push(SkeletonEvent(edge.triangle_succ().triangle_succ(),
                                      height + min_height,
                                      x3 + v3 * height,
                                      false,
                                      epoch_));
    }
    else {
      /* Split/flip event. */
      event_queue_.push(SkeletonEvent(edge.triangle_succ().triangle_succ(),
                                      height + min_height,
                                      x3 + v3 * height,
                                      true,
                                      epoch_));
    }
  }
  else if (len2 > len3) {
    /* Vertex event. */
    if (len1 <= len3 && out1 && !out2) {
      event_queue_.push(SkeletonEvent(edge, height + min_height, x1 + v1 * height, false, epoch_));
    }
    else if (len1 >= len3 && out3 && !out2) {
      event_queue_.push(SkeletonEvent(edge.triangle_succ().triangle_succ(),
                                      height + min_height,
                                      x3 + v3 * height,
                                      false,
                                      epoch_));
    }
    else {
      /* Split/flip event. */
      event_queue_.push(SkeletonEvent(edge, height + min_height, x1 + v1 * height, true, epoch_));
    }
  }
  else {
    /* Vertex event. */
    if (len2 <= len1 && out2 && !out3) {
      event_queue_.push(SkeletonEvent(
          edge.triangle_succ(), height + min_height, x2 + v2 * height, false, epoch_));
    }
    else if (len2 >= len1 && out1 && !out3) {
      event_queue_.push(SkeletonEvent(edge, height + min_height, x1 + v1 * height, false, epoch_));
    }
    else {
      /* Split/flip event. */
      event_queue_.push(SkeletonEvent(
          edge.triangle_succ(), height + min_height, x2 + v2 * height, true, epoch_));
    }
  }
}

/* Handle a vertex event (some papers call this an edge event),
 * where the triangle X in the following diagram collapses,
 * and the labeled edge is part of the wavefront (one but not
 * both of the other edges of the triangle may be part of the
 * wavefront too). Suppose we have the following, where e
 * is the argument edge, and ep--e--en are part of the wavefront,
 * and s and sn are spokes.
 * We need to collapse edge e (deleting triangles X and C and vertex vn).
 * Then we need to split the remaining vertex v, splitting off edges
 * en to ep CCW. Note: it is possible that en is a side of X, so be
 * careful about that after collapsing e.
 * The newly formed edge from the split will be a spoke, and that
 * spoke is the return value.
 * The split has to happen so that v is the "inner" one after
 * the split -- i.e., the destination end of the new spoke.
 *
 *           /-- --------\               -----|------- /---\
 *     /-----    \        -------- -----/     |       /     ------\
 * ----           \              -/ \         |      /             ----
 *  |--\       I   \     H      /    -\   G   |    /-   F        ---/
 *  \   --\ ep      |         -/       \      |   /       en  --/   |
 *   |     --\      \       -/          -\    |  /        ---/      |
 *   |        --\    \     /      X       \   | /     ---/          |
 *   \           --\  \  -/                -\ |-   --/       E      |
 *    |             --\|/v        e       vn - ---/                 |
 *    |    A          /-----------------------/----------------------
 *    \             /-  -\                    |                  -/
 *     |           /      --\         C       \        D      --/
 *     \         /-          ---\              |sn          -/
 *      |     s /                --\           \         --/
 *      |      /       B            --\         |     --/
 *      \    /-                        ---\     \   -/
 *       |  /                              --\   |-/
 *       |/-                ----------------  --
 *       - ----------------/
 *
 */
Edge StraightSkeleton::handle_vertex_event(Edge edge)
{
  BLI_assert(is_wavefront_edge(edge));
  Vert *v = v_src(edge);
  Edge ep = find_ccw_wavefront_edge(edge);
  Edge en = find_ccw_wavefront_edge(neighbor_edge(edge));
  BLI_assert(!ep.is_null() && !en.is_null());
  Edge en_neighbor = neighbor_edge(en);
  trimesh_.collapse_edge(edge);
  en = neighbor_edge(en_neighbor); /* May have changed from old en. */
  /* We want v to be inside after the split. */
  Vert *vnew = trimesh_.split_vert(v, rot_ccw(ep), rot_cw(en));
  Edge new_spoke = edge_between(vnew, v);
  BLI_assert(!new_spoke.is_null());
  /* Mark new_spoke as a spoke. */
  Triangle *tspoke = new_spoke.tri();
  tspoke->mark_spoke(new_spoke.tri_edge_index());
  return new_spoke;
}

/* Handle a split event. The initial situation looks something like this:
 *
 * |\--\
 * \ |\ -----\ /---/|
 *  ||\ \-\    ----\                                                                             /-----
 * /-//
 *   \ \ -\---\     ----\                                                                  /-----
 * /- / | | \  \   ---\      -----\                                                      /----- /-
 * / / \  |  \      ---\        ----\                                          /------ /-   /  | |
 * \   \         ---\         ----\                               /-----                  /-    / /
 *    \  \   \            ---\          -----\                   /-----                      /- s
 * /  / |  |   -\s             --\             ----\        /-----                          /- / |
 *     |  \     \                ---\     E        ---|------------------------------------       /
 * / \   \     \        C          ---\             |                        -----/    |       / |
 *      |   |     -                      ---\         |s       F     w  ------/          /      / /
 *      \   \      |--------\  w             ---\     |          ------/                |      | /
 *       |   \     |         ---------------\    ---\ |    -----/     ew3               /      / |
 *       \    \    |              ew1        -------------/                            |      / /
 *        |    |    \                                -v-\                              |w    /    /
 *        \    \    |                            ---/    ---\                 G        /    /     |
 *         |    \  w|         B              ---/            --\                      |    /     /
 *         \     |  |                    ---/                   ---\                  |   /      |
 *          |    \  |                ---/                         e3--\               /  /      /
 *          |     \ |            ---/   e1=edge       A                ---\          |  /      /
 *          \      \ \       ---/                                          --\       / /       |
 *           |      ||   ---/                                                 ---\  | /       /
 *           \      \|--/                             e2                          --|/        |
 *            |      |--------------------------------------------------------------|        /
 *            \     - --------\                          w                          \       /
 *             | s /           ----------------\                       H             -\     |
 *             \  /                             ----------------\                      \s  /
 *              |/                                               ----------------\      -  |
 *              |--------------------------------------------------------------------------
 *
 * where vertex v, the source of \a edge, is to split edge e2. The edges marked s are spokes and
 * the edges marked w are wavefront edges. We first split vertex v twice, so that there is now a
 * line v0--v--v1, at points that make the diagram look like the following (detail):
 *
 *
 *                                       | --------\
 *                           --          |          ---------
 *                             \-    E   |                    /--  --
 *        \------\          C    \-      |    F           /---  --/ |
 *         |  \--------\           \-    |           /----   --/    |
 *         \      \---- ------\      \-  |       /---     --/       /
 *          \          \---    -----\  \-|   /---      --/         |
 *           |             \--       ----|---        -/        G   |
 *           \      B          v0---------v--------v1 --           |
 *            \             /--       ---- ---\         \--        /
 *             |        /---  -------/         ------\     \-     |
 *             \    /--- ----/             A          ----   \--  |
 *              |---------------------------------------------- \-|vc
 *               va     \---------   en2       en1                \
 *                                \---------             H         \
 *                                          \---------       en3    -\
 *                                                    \---------      \
 *                                                              \----- -vb
 *
 * Finally, triangles A and H are deleted and new trangles are formed by adding diagonals from vb
 * to v and v0.
 */

std::tuple<Vert *, Vert *, Vert *> StraightSkeleton::handle_split_event(Edge edge)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "handle_split_event " << edge << "\n";
  }
  Vert *vert = v_src(edge);
  Edge e1 = edge;
  Edge e2 = e1.triangle_succ();
  Edge e3 = e2.triangle_succ();
  Edge ew1 = find_cw_wavefront_edge(e1);
  Edge ew3 = find_ccw_wavefront_edge(e1);
  BLI_assert(!ew1.is_null() && !ew3.is_null());
  if (dbg_level > 0) {
    std::cout << "e1=" << e1 << ", e2=" << e2 << ". e3=" << e3 << "\n";
    std::cout << "ew1=" << ew1 << ". ew3=" << ew3 << "\n";
  }
  /* Make the new wavefront verts by splitting vert twice. */
  trimesh_.validate();
  Vert *new_v0 = trimesh_.split_vert(vert, ew1, e1);
  Vert *new_v1 = trimesh_.split_vert(vert, neighbor_edge(e3), ew3);
  Edge evv0 = edge_between(vert, new_v0);
  Edge evv1 = edge_between(vert, new_v1);
  BLI_assert(!evv0.is_null() && !evv1.is_null());
  evv0.tri()->mark_spoke(evv0.tri_edge_index());
  evv1.tri()->mark_spoke(evv1.tri_edge_index());
  /* Get triangle on other side of the collided edge (e2). */
  Edge en_1 = neighbor_edge(e2);
  Edge en_2 = en_1.triangle_succ();
  Edge en_3 = en_2.triangle_succ();
  Vert *va = v_src(en_2);
  Vert *vb = v_src(en_3);
  Vert *vc = v_src(en_1);
  Triangle *tnew0 = trimesh_.add_triangle(vert, va, vb);
  Triangle *tnew1 = trimesh_.add_triangle(vert, vb, vc);
  Edge nbr_en_2 = neighbor_edge(en_2);
  Edge nbr_en_3 = neighbor_edge(en_3);
  set_mutual_neighbors(tnew0, 0, neighbor_edge(e1));
  set_mutual_neighbors(tnew0, 1, nbr_en_2);
  set_mutual_neighbors(tnew0, 2, tnew1, 0);
  set_mutual_neighbors(tnew1, 1, nbr_en_3);
  set_mutual_neighbors(tnew1, 2, neighbor_edge(e3));
  if (nbr_en_2.tri()->is_spoke(nbr_en_2.tri_edge_index())) {
    tnew0->mark_spoke(1);
  }
  if (nbr_en_3.tri()->is_spoke(nbr_en_3.tri_edge_index())) {
    tnew1->mark_spoke(1);
  }
  /* Any vertex that used an edge in triangle A or H as representative
   * now needs a new representative. */
  Triangle *ta = edge.tri();
  Triangle *th = en_1.tri();
  if (vert->e.tri() == ta) {
    vert->e = Edge(tnew0, 0);
  }
  if (va->e.tri() == ta || va->e.tri() == th) {
    va->e = Edge(tnew0, 1);
  }
  if (vb->e.tri() == th) {
    vb->e = Edge(tnew1, 1);
  }
  if (vc->e.tri() == ta || vc->e.tri() == th) {
    vc->e = Edge(tnew1, 2);
  }
  edge.tri()->mark_deleted();
  en_1.tri()->mark_deleted();
  return std::make_tuple(new_v0, vert, new_v1);
}

/* Handle a flip event. We need to flip a diagonal, tranforming this:
 *       -
 *     /  --\
 *  e1/ v4   ---\e2
 *   /      t0   ---\
 *  /                ---\
 * /v1     edge      v3  --
 * /--------------------------
 * \                     --/
 *  \       t1       ---/
 *   \            --/
 *  e3\        --/e4
 *     \v2 ---/
 *      --/
 *
 * into this:
 *
 *        -
 *       /|-\
 *      / |  --\
 *     /  |v4   --\
 *    /   |        -\
 *   /    |          --\
 *  /  t3 /    t4       --\
 * /v1   |enew         v3  --
 * \     |               --
 *  \    |           ---/
 *   \   |        --/
 *    \  |v2   --/
 *     \ | ---/
 *      --/
 *
 * Return the pair of edges (enew, neighbor_edge(enew)).
 * These are both "in_region" and so the new triangles need to be too.
 */
std::pair<Edge, Edge> StraightSkeleton::handle_flip_event(Edge edge)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "handle_flip_event " << edge << "\n";
  }
  Edge edge_rev = neighbor_edge(edge);
  Vert *v1 = v_src(edge);
  Vert *v2 = v_src(edge_rev.triangle_pred());
  Vert *v3 = v_dst(edge);
  Vert *v4 = v_src(edge.triangle_pred());
  Edge e1 = neighbor_edge(edge.triangle_pred());
  Edge e2 = neighbor_edge(edge.triangle_succ());
  Edge e3 = neighbor_edge(edge_rev.triangle_succ());
  Edge e4 = neighbor_edge(edge_rev.triangle_pred());
  Triangle *t0 = edge.tri();
  Triangle *t1 = edge_rev.tri();
  BLI_assert(t0->in_region() && t1->in_region());
  /* Rather than trying to edit t0 and t1, just delete them and make new ones. */
  t0->mark_deleted();
  t1->mark_deleted();
  /* If v1 and v3 might need new representative edges. They'll be assigned when make t3 and t4. */
  v1->e = null_edge;
  v3->e = null_edge;
  Triangle *t3 = trimesh_.add_triangle(v1, v2, v4);
  Triangle *t4 = trimesh_.add_triangle(v2, v3, v4);
  set_mutual_neighbors(t3, 0, e3);
  set_mutual_neighbors(t3, 1, t4, 2);
  set_mutual_neighbors(t3, 2, e1);
  set_mutual_neighbors(t4, 0, e4);
  set_mutual_neighbors(t4, 1, e2);
  t3->mark_in_region();
  t4->mark_in_region();
  return std::pair<Edge, Edge>(Edge(t3, 1), Edge(t4, 2));
}

/** Handle the event where \a edge's triangle collapses. */
void StraightSkeleton::handle_closing_event(Edge edge)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "handle_closing_event : " << edge << "\n";
  }
  /* Collapse the triangle that edge is part of, leaving only its first vertex still in trimesh. */
  trimesh_.collapse_triangle(edge.tri(), edge.tri_edge_index());
}

void StraightSkeleton::compute()
{
  constexpr int dbg_level = 0;

  contour_edges_ = init_contour_inset(trimesh_, contours_);
  if (dbg_level > 0) {
    std::cout << "\nstraight_skeleton, target_height=" << target_height_ << "\ncontour_edges:\n";
    for (int i : contour_edges_.index_range()) {
      std::cout << "contour " << i << ": ";
      for (int j : contour_edges_[i].index_range()) {
        std::cout << contour_edges_[i][j] << " ";
      }
      std::cout << "\n";
    }
  }

  calculate_contour_and_region_data();
  trimesh_.calculate_all_tri_normals();
  if (dbg_level > 0) {
    trimesh_.validate();
  }

  /* Create the skeleton vertices, first for the contours. */
  int count_non_zero = 0;
  for (Vector<Edge> &contour : contour_edges_) {
    int n = int(contour.size());
    for (int i = 0; i < n; ++i) {
      Edge e = contour[i];
      Edge e_prev = contour[(i + n - 1) % n];
      float e_weight = 1.0f;
      float e_prev_weight = 1.0f;
      /* TODO: add an edge_weight_map argument and use it to set e_weight. */
      Vert *vprev = v_src(e_prev);
      Vert *v = v_src(e);
      Vert *vnext = v_dst(e);
      /* The reference code uses prev and next as if we go ccw around
       * the contour (as usual), but measures deltas going cw.
       */
      float3 delta_prev = math::normalize(vprev->co - v->co) / e_prev_weight;
      float3 delta_next = math::normalize(v->co - vnext->co) / e_weight;
      if (math::length_squared(delta_next) > 0.0f) {
        count_non_zero++;
      }
      float3 normal = math::normalize(e_prev.tri()->normal() + e.tri()->normal());
      SkeletonVertex *skv = add_skeleton_vertex(v->co, 0.0f, &delta_prev, &delta_next, &normal);
      /* TODO: handle case where a vertex is used > 1 time in contours. */
      set_skel_vertex_map(v->id, skv);
      if (dbg_level > 0) {
        std::cout << "added skelvert for contour v" << v->id << " " << *skv << "\n";
      }
    }
  }

  /* Add the initial events and skeleton vertices for inner verts. */
  for (Triangle *tri : trimesh_.all_tris()) {
    if (!tri->in_region()) {
      continue;
    }
    Edge e = tri->edge(0);
    for (int i = 0; i < 3; ++i) {
      Vert *v = tri->vert(i);
      float3 vnormal = vertex_normal(v);
      if (!skel_vertex_map_has_id(v->id)) {
        SkeletonVertex *skv = add_skeleton_vertex(v->co, 0.0f, nullptr, nullptr, &vnormal);
        set_skel_vertex_map(v->id, skv);
        if (dbg_level > 0) {
          std::cout << "added skelvert for internal v" << v->id << " " << *skv << "\n";
        }
      }
    }
    add_triangle(e, 0.0f);
  };
  if (dbg_level > 0) {
    std::cout << "initial events\n";
    dump_event_queue();
  }

  if (event_queue_.empty()) {
    /* No events found. This is probably a bug. */
    BLI_assert(false);
    return;
  }

  while (!event_queue_.empty()) {
    if (dbg_level > 0) {
      std::cout << "\nTOP OF EVENT LOOP\n";
      if (dbg_level > 1) {
        dump_state();
      }
      else {
        dump_event_queue();
      }
      trimesh_.validate();
    }
    epoch_++;
    SkeletonEvent event = event_queue_.top();
    event_queue_.pop();
    if (!event.is_valid()) {
      if (dbg_level > 0) {
        std::cout << "popped event not valid, ignored\n";
      }
      continue;
    }
    float height = event.height();
    Edge edge = event.edge();
    if (height > target_height_) {
      if (dbg_level > 0) {
        std::cout << "event height > target_height; repush event and break\n";
      }
      event_queue_.push(event);
      break;
    }

    if (dbg_level > 0) {
      std::cout << "process event " << event << "\n";
    }

    bool out1 = is_wavefront_edge(edge);
    bool out2 = is_wavefront_edge(edge.triangle_succ());
    bool out3 = is_wavefront_edge(edge.triangle_pred());
    if (dbg_level > 0) {
      std::cout << "out1=" << out1 << " out2=" << out2 << " out3=" << out3 << "\n";
    }
    if (!out2 && (!out1 || !out3)) {
      /* Split/vertex type flip events. */
      if (dbg_level > 0) {
        std::cout << "Split/vertex type flip event\n";
      }
      bool flip_event = true;
      Edge flip_edge = edge.triangle_succ();
      if (out1 && !event.split_event()) {
        /* The exact same condition as for inner vertex event below.
         # In this case I can't be sure that this isn't an inner vertex event!
         # To check one could e.g. check if the length of the edge 1...
         */
        SkeletonVertex *skv2 = get_skel_vertex(flip_edge);
        if (skv2->dhdl() != 0.0f) {
          float3 p2 = skv2->position() + skv2->velo() * (height - skv2->height()) / skv2->dhdl();
          if (math::distance_squared(event.final_pos(), p2) < collision_epsilon) {
            /* TODO: remove epsilon here. */
            flip_event = false;
            if (dbg_level > 0) {
              std::cout << "not flip event\n";
            }
          }
          else {
            /* Decide which edge to flip. */
            SkeletonVertex *skv3 = get_skel_vertex(edge.triangle_succ().triangle_succ());
            if (skv3->dhdl() != 0.0f) {
              float3 p3 = skv3->position() +
                          skv3->velo() * (height - skv3->height()) / skv3->dhdl();
              if (math::distance_squared(event.final_pos(), p3) > math::distance_squared(p3, p2)) {
                flip_edge = flip_edge.triangle_succ();
              }
            }
          }
        }
        else {
          flip_edge = flip_edge.triangle_succ(); /* TODO: is this correct? */
        }
      }
      if (flip_event && tot_flip_events_ < 2 * tot_region_triangles_ * tot_region_triangles_) {
        /* TODO: check if this limit on total flip events is correct. */
        /* Rotate the flip_edge. */
        if (dbg_level) {
          std::cout << "handle_flip_event for flip_edge = " << flip_edge << "\n";
        }
        std::pair<Edge, Edge> edge_pair = handle_flip_event(flip_edge);
        if (dbg_level > 0) {
          std::cout << "handle_flip_event returned " << edge_pair.first << " and "
                    << edge_pair.second << "\n";
          std::cout << "add_triangle for those two edges at height " << height << "\n";
        }
        add_triangle(edge_pair.first, height);
        add_triangle(edge_pair.second, height);
        /* TODO: Python code here may replace the added last two events by dummy ones in a
         * peculiar test to avoid flip loops. */
        tot_flip_events_++;
        continue;
      }
    }
    /* Set the new vertex position. */
    v_src(edge)->co = event.final_pos();
    if (out1 && out2 && out3) {
      BLI_assert(!event.split_event());
      vertex_height_map.add(v_src(edge)->id, height);
      if (dbg_level > 0) {
        std::cout << "handle_closing_event for edge " << edge << "\n";
      }
      handle_closing_event(edge);
    }
    else if (out1 && (!out2 || !out3)) {
      BLI_assert(!event.split_event());
      /* Vertex event. */
      if (dbg_level > 0) {
        std::cout << "vertex event\n";
      }

      SkeletonVertex *skv = get_skel_vertex(edge);
      SkeletonVertex *skv_next = get_skel_vertex(edge.triangle_succ());
      float3 delta_prev = skv->delta_prev();
      float3 delta_next = skv_next->delta_next();
      float3 normal = math::normalize(skv->normal() + skv_next->normal());
      SkeletonVertex *new_skv = add_skeleton_vertex(
          event.final_pos(), height, &delta_prev, &delta_next, &normal);
      set_skel_vertex_map(v_src(edge)->id, new_skv);

      if (dbg_level > 0) {
        std::cout << "handle_vertex_event for edge " << edge << "\n";
      }
      Edge new_spoke = handle_vertex_event(edge);
      if (dbg_level > 0) {
        std::cout << "handle_vertex_event returned " << new_spoke << "\n";
      }
      Vert *new_v = v_dst(new_spoke);
      set_skel_vertex_map(v_src(new_spoke)->id, skv);
      new_v->co = event.final_pos();
      vertex_height_map.add(new_v->id, height);
      /* Also need to set the height for the other end of the spoke, which has 0 length right now.
       */
      vertex_height_map.add(v_src(new_spoke)->id, height);

      if (dbg_level > 0) {
        std::cout << "add_events for v" << new_v->id << " at height " << height
                  << " instant=" << (fabsf(new_skv->dhdl()) <= dhdl_epsilon) << "\n";
      }
      add_events(new_v, height, fabsf(skv->dhdl()) <= dhdl_epsilon);

      /* Check for the whisker case. */
      if (out2 || out3) {
        Edge new_spoke_rev = neighbor_edge(new_spoke);
        Edge e;
        if (out3) {
          e = neighbor_edge(find_cw_wavefront_edge(new_spoke_rev));
        }
        else {
          e = find_ccw_wavefront_edge(new_spoke_rev);
        }
        if (dbg_level > 0) {
          std::cout << "whisker case test, e = " << e << "\n";
        }
        SkeletonVertex *skv2 = get_skel_vertex(e);
        SkeletonVertex *skv3 = get_skel_vertex(e.triangle_succ());
        BLI_assert(skv2 != nullptr && skv3 != nullptr);
        float3 p2 = skv2->position() +
                    skv2->velo() *
                        (skv2->dhdl() != 0.0f ? (height - skv2->height()) / skv2->dhdl() : 0.0f);
        float3 p3 = skv3->position() +
                    skv3->velo() *
                        (skv3->dhdl() != 0.0f ? (height - skv3->height()) / skv3->dhdl() : 0.0f);
        if (math::distance_squared(p2, p3) < collision_epsilon) {
          event_queue_.push(SkeletonEvent(e, height, event.final_pos(), false, epoch_));
          if (dbg_level > 0) {
            std::cout << "whisker pushed event " << event_queue_.top() << "\n";
          }
        }
      }
    }
    else if (!out1 && out2 != out3) {
      BLI_assert(event.split_event());
      if (dbg_level > 0) {
        std::cout << "Split event\n";
      }
      /* First detect whether it is a real split or a "collision" event. */
      SkeletonVertex *skv2 = get_skel_vertex(edge.triangle_succ());
      SkeletonVertex *skv3 = get_skel_vertex(edge.triangle_succ().triangle_succ());
      float3 p2 = skv2->position() +
                  skv2->velo() *
                      (skv2->dhdl() != 0.0f ? (height - skv2->height()) / skv2->dhdl() : 0);
      float3 p3 = skv3->position() +
                  skv3->velo() *
                      (skv3->dhdl() != 0.0f ? (height - skv3->height()) / skv3->dhdl() : 0);
      bool collide1 = false;
      bool collide2 = false;
      float d1 = math::distance_squared(event.final_pos(), p2);
      float d2 = math::distance_squared(event.final_pos(), p3);
      if (d1 < collision_epsilon || d2 < collision_epsilon) {
        if (d1 < d2) {
          collide1 = true;
        }
        else {
          collide2 = true;
        }
      }
      if (dbg_level > 0) {
        std::cout << "collide1=" << collide1 << " collide2=" << collide2 << "\n";
      }
      Edge e1 = neighbor_edge(edge);
      Edge e2 = neighbor_edge(edge.triangle_succ().triangle_succ());
      BLI_assert(e1.tri()->in_region() && e2.tri()->in_region());
      BLI_assert(v_src(e1.triangle_succ()) == v_src(edge) && v_src(e2) == v_src(edge));

      /* Find the edge that holds the split event from the reflex vertex. */
      Edge reflex = null_edge;
      Edge e = e1.triangle_succ();
      do {
        if (!e.tri()->in_region() && !neighbor_edge(e).tri()->in_region()) {
          reflex = neighbor_edge(e);
          break;
        }
        e = rot_ccw(e);
      } while (e != e1.triangle_succ());
      BLI_assert(!reflex.is_null());
      if (dbg_level > 0) {
        std::cout << "reflex = " << reflex << "\n";
      }

      SkeletonVertex *ske1 = get_skel_vertex(e1);
      SkeletonVertex *ske1n = get_skel_vertex(e1.triangle_succ());
      SkeletonVertex *ske2 = get_skel_vertex(e2);
      SkeletonVertex *ske2n = get_skel_vertex(e2.triangle_succ());
      float3 delta_prev = ske1->delta_next();
      float3 delta_next = ske1n->delta_next();
      float3 delta_prev2 = ske2->delta_prev();
      float3 delta_next2 = ske2n->delta_prev();
      float3 normal1 = math::normalize(ske1->normal() + ske1n->normal());
      float3 normal2 = math::normalize(ske2->normal() + ske2n->normal());
      SkeletonVertex *skv1 = add_skeleton_vertex(
          event.final_pos(), height, &delta_prev, &delta_next, &normal1);
      skv2 = add_skeleton_vertex(event.final_pos(), height, &delta_prev2, &delta_next2, &normal2);

      if (dbg_level > 0) {
        std::cout << "handle_split_event for edge " << edge << "\n";
      }
      std::tuple<Vert *, Vert *, Vert *> vtriple = handle_split_event(edge);
      Vert *left_v = std::get<0>(vtriple);
      Vert *center_v = std::get<1>(vtriple);
      Vert *right_v = std::get<2>(vtriple);
      if (dbg_level > 0) {
        std::cout << "handle_split_event returned\n left_v = " << *left_v
                  << "\n center_v = " << *center_v << "\n right_v = " << *right_v << "\n";
      }
      set_skel_vertex_map(left_v->id, skv1);
      set_skel_vertex_map(right_v->id, skv2);
      vertex_height_map.add(center_v->id, height);

      if (dbg_level > 0) {
        std::cout << "add_events for v" << left_v->id << " and v" << right_v->id << " at height "
                  << height << "\n";
      }
      add_events(left_v, height, abs(skv1->dhdl()) <= dhdl_epsilon);
      add_events(right_v, height, abs(skv2->dhdl()) <= dhdl_epsilon);
      if (collide2) {
        Edge e = center_v->e;
        Edge collide_edge = null_edge;
        do {
          if (right_v == v_src(e.triangle_succ())) {
            collide_edge = e;
            break;
          }
          e = rot_ccw(e);
        } while (e != center_v->e);
        BLI_assert(!collide_edge.is_null());
        Edge event_e = neighbor_edge(neighbor_edge(collide_edge).triangle_pred());
        event_queue_.push(SkeletonEvent(event_e, height, event.final_pos(), false, epoch_));
        if (dbg_level > 0) {
          std::cout << "collide2 so pushed event " << event_queue_.top() << "\n";
        }
      }
      if (collide1) {
        Edge e = center_v->e;
        Edge collide_edge = null_edge;
        do {
          if (left_v == v_src(e.triangle_succ())) {
            collide_edge = e;
            break;
          }
          e = rot_ccw(e);
        } while (e != center_v->e);
        BLI_assert(!collide_edge.is_null());
        Edge event_e = neighbor_edge(collide_edge.triangle_succ());
        event_queue_.push(SkeletonEvent(event_e, height, event.final_pos(), false, epoch_));
        if (dbg_level > 0) {
          std::cout << "collide1 so pushed event " << event_queue_.top() << "\n";
        }
      }
    }
    else {
      /* Unknown event. */
      if (dbg_level > 0) {
        std::cout << "unknown event " << event << " out1=" << out1 << " out2=" << out2
                  << " out3=" << out3 << "\n";
      }
      continue;
      // BLI_assert_unreachable();
    }
  }

  if (dbg_level > 0) {
    std::cout << "finished, events left: " << event_queue_.size() << "\n";
  }

  /* Finish off remaining events. */
  while (!event_queue_.empty()) {
    SkeletonEvent event = event_queue_.top();
    if (dbg_level > 0) {
      std::cout << "process final event " << event << "\n";
    }
    event_queue_.pop();
    if (event.edge().is_null() || !event.is_valid()) {
      continue;
    }
    Triangle *tri = event.edge().tri();
    remaining_triangles_set.add(tri);
    for (int i = 0; i < 3; ++i) {
      Vert *vert = tri->vert(i);
      SkeletonVertex *skv = skel_vertex_map(vert->id);
      if (skv != nullptr) {
        if (skv->dhdl() != 0.0f) {
          vert->co = vert->co + skv->velo() * (target_height_ - skv->height()) / skv->dhdl();
          if (dbg_level > 0) {
            std::cout << "  skelv=" << *skv << ", v" << vert->id << ".co = " << vert->co << "\n";
          }
        }
        vertex_height_map.add(vert->id, target_height_);
        /* Remove skv from map so that don't process it multiple times. */
        remove_from_skel_vertex_map(vert->id);
      }
    }
  }
  if (dbg_level > 0) {
    std::cout << "Final state\n";
    dump_state();
  }
}

static float3 poly_normal(Span<Vert *> verts)
{
  Array<float3> poly(verts.size());
  for (const int64_t i : verts.index_range()) {
    poly[i] = verts[i]->co;
  }
  float3 n = math::cross_poly(poly.as_span());
  return math::normalize(n);
}

/** Canonicalize an int pair by putting smaller int first. */
static inline std::pair<int, int> canon_pair(int a, int b)
{
  return a < b ? std::pair<int, int>(a, b) : std::pair<int, int>(b, a);
}

/** Which position in triangle \a tri does the canonicalized pair of vertex ids \a vert_id_pair
 * appear as an edge? It is whichever of the pair ids appears first as long as the other is next.
 */
static int edgepos_by_canon_pair(const Triangle *tri, const std::pair<int, int> vert_id_pair)
{
  int a = vert_id_pair.first;
  int b = vert_id_pair.second;
  for (int i = 0; i < 3; ++i) {
    if ((tri->vert(i)->id == a && tri->vert(succ_index(i))->id == b) ||
        (tri->vert(i)->id == b && tri->vert(succ_index(i))->id == a)) {
      return i;
    }
  }
  return -1;
}

/** The vector has triangles which share edges but don't have neighbors preoperly
 * set yet. Set them. It is assumed that edges have only 1 or 2 triangles containing them. */
static void connect_neighbors(Span<Triangle *> tris)
{
  /* Map from canonicalized vert ids pairs to triangles. */
  Map<std::pair<int, int>, std::pair<Triangle *, Triangle *>> map;
  map.reserve(tris.size() * 2);
  for (const int64_t t : tris.index_range()) {
    Triangle *tri = tris[t];
    for (int i = 0; i < 3; ++i) {
      std::pair<int, int> vpair = canon_pair(tri->vert(i)->id, tri->vert(succ_index(i))->id);
      if (!map.contains(vpair)) {
        map.add_new(vpair, std::pair<Triangle *, Triangle *>(tri, nullptr));
      }
      else {
        std::pair<Triangle *, Triangle *> adj_tris = map.lookup(vpair);
        /* If adj_tris.second is not null, there are >= 3 triangles on the same edge.
         * This shouldn't happen, but if it just overwrite. Assert during development, however.
         */
        BLI_assert(adj_tris.second == nullptr);
        adj_tris.second = tri;
        map.add_overwrite(vpair, adj_tris);
      }
    }
  }
  /* Now can set the neighbor pointers correctly. */
  for (auto map_iter : map.items()) {
    Triangle *t1 = map_iter.value.first;
    Triangle *t2 = map_iter.value.second;
    if (t1 != nullptr && t2 != nullptr) {
      int t1_edgepos = edgepos_by_canon_pair(t1, map_iter.key);
      int t2_edgepos = edgepos_by_canon_pair(t2, map_iter.key);
      BLI_assert(t1_edgepos != -1 && t2_edgepos != -1);
      /* These may already be connected, as part of triangulation. */
      if (t1->neighbor(t1_edgepos) != null_edge && t2->neighbor(t2_edgepos) != null_edge) {
        continue;
      }
      set_mutual_neighbors(t1, t1_edgepos, t2, t2_edgepos);
    }
  }
}

/* Like BLI_math's is_quad_flip_v3_first_third_fast_with_normal, with const float3's. */
static bool is_quad_flip_first_third(
    const float3 &v1, const float3 &v2, const float3 &v3, const float3 &v4, const float3 &normal)
{
  float3 dir_v3v1 = v3 - v1;
  float3 tangent = math::cross(dir_v3v1, normal);
  float dot = math::dot(v1, tangent);
  return (math::dot(v4, tangent) >= dot) || (math::dot(v2, tangent) <= dot);
}

/** Triangulate face f of input and return it as a Vector of Triangle *. The vertices with
 * coordinates for face f will be found in base_trimesh. Caller assumes ownership of the
 * Triangles.
 */
static Vector<Triangle *> triangulate_face(int f,
                                           const MeshInset_Input &input,
                                           const TriangleMesh &base_trimesh)
{
  Vector<Triangle *> ans;
  Span<int> face = input.face[f].as_span();
  int flen = int(face.size());
  if (flen <= 2) {
    return ans;
  }
  ans.reserve(flen - 2);
  Array<Vert *> fvert(flen);
  for (int i = 0; i < flen; ++i) {
    fvert[i] = base_trimesh.get_vert_by_index(face[i]);
    BLI_assert(fvert[i] != nullptr);
  }
  if (flen == 3) {
    Triangle *t = new Triangle(fvert[0], fvert[1], fvert[2]);
    ans.append(t);
    return ans;
  }
  /* Need the face normal for the rest of this. */
  float3 norm = poly_normal(fvert.as_span());
  if (flen == 4) {
    Vert *v0 = fvert[0];
    Vert *v1 = fvert[1];
    Vert *v2 = fvert[2];
    Vert *v3 = fvert[3];
    Triangle *t0;
    Triangle *t1;
    float d02_sqr = math::distance_squared(v0->co, v2->co);
    float d13_sqr = math::distance_squared(v1->co, v3->co);
    if (d13_sqr < d02_sqr ||
        UNLIKELY(is_quad_flip_first_third(v0->co, v1->co, v2->co, v3->co, norm))) {
      t0 = new Triangle(v0, v1, v3);
      t1 = new Triangle(v1, v2, v3);
      set_mutual_neighbors(t0, 1, t1, 2);
      t0->mark_orig(0);
      t0->mark_orig(2);
      t1->mark_orig(0);
      t1->mark_orig(1);
    }
    else {
      t0 = new Triangle(v0, v1, v2);
      t1 = new Triangle(v0, v2, v3);
      set_mutual_neighbors(t0, 2, t1, 0);
      t0->mark_orig(0);
      t0->mark_orig(1);
      t1->mark_orig(1);
      t1->mark_orig(2);
    }
    ans.append(t0);
    ans.append(t1);
  }
  else {
    /* Face has 5 or more edges; use polyfill. */
    /* Project vertices to 2d along negative face normal. */
    float axis_mat[3][3];
    float(*projverts)[2];
    unsigned int(*tris)[3];
    const int totfilltri = flen - 2;
    tris = static_cast<unsigned int(*)[3]>(MEM_malloc_arrayN(totfilltri, sizeof(*tris), __func__));
    projverts = static_cast<float(*)[2]>(MEM_malloc_arrayN(flen, sizeof(*projverts), __func__));
    axis_dominant_v3_to_m3_negate(axis_mat, norm);
    Map<int, int> vert_index_to_facepos;
    for (int j = 0; j < flen; ++j) {
      mul_v2_m3v3(projverts[j], axis_mat, base_trimesh.get_vert_by_index(face[j])->co);
      vert_index_to_facepos.add(face[j], j);
    }
    MemArena *pf_arena = BLI_memarena_new(BLI_POLYFILL_ARENA_SIZE, __func__);
    Heap *pf_heap = BLI_heap_new_ex(BLI_POLYFILL_ALLOC_NGON_RESERVE);
    BLI_polyfill_calc(projverts, flen, 0, tris);
    BLI_polyfill_beautify(projverts, flen, tris, pf_arena, pf_heap);

    /* First add the triangles, without setting neighbors yet. */
    for (int t = 0; t < totfilltri; ++t) {
      unsigned int *tri = tris[t];
      Vert *v[3];
      for (int k = 0; k < 3; k++) {
        BLI_assert(tri[k] < flen);
        v[k] = fvert[tri[k]];
      }
      Triangle *newtri = new Triangle(v[0], v[1], v[2]);
      /* Find and mark the original edges. */
      for (int k = 0; k < 3; k++) {
        int v1 = newtri->vert(k)->id;
        int v2 = newtri->vert((k + 1) % 3)->id;
        int pos = vert_index_to_facepos.lookup_default(v1, -1);
        if (pos != -1) {
          BLI_assert(face[pos] == v1);
          if (face[(pos + 1) % flen] == v2) {
            newtri->mark_orig(k);
          }
        }
      }
      ans.append(newtri);
    }

    MEM_freeN(tris);
    MEM_freeN(projverts);
    BLI_memarena_free(pf_arena);
    BLI_heap_free(pf_heap, nullptr);
  }
  return ans;
}

static void add_ghost_triangles(TriangleMesh &trimesh)
{
  /* First get vector of edges that have no neighbor. */
  Vector<Edge> boundary_edges;
  for (const Triangle *t : trimesh.all_tris()) {
    for (int i = 0; i < 3; i++) {
      Edge e = Edge(t, i);
      if (neighbor_edge(e).is_null()) {
        boundary_edges.append(e);
      }
    }
  }
  /* Process the boundary_edges in order (for deterministic result),
   * but keep track of the ones that have already been handled.
   */
  Set<Edge> visited;
  visited.reserve(boundary_edges.size());
  for (const Edge e : boundary_edges) {
    if (visited.contains(e)) {
      continue;
    }
    Edge ecur = e;
    Triangle *prev_ghost = nullptr;
    Triangle *first_ghost = nullptr;
    do {
      visited.add_new(ecur);
      Vert *v0 = v_src(ecur);
      Vert *v1 = v_dst(ecur);
      Triangle *ghost_tri = trimesh.add_triangle(v0, nullptr, v1);
      /* Mark neighbor pairs: ecur is paired with ghost_tri's edge 2,
       * and ghost_tri's edge 0 is paired with the previous ghost_tri's
       * edge 1.
       */
      set_mutual_neighbors(ghost_tri, 2, ecur);
      if (prev_ghost != nullptr) {
        set_mutual_neighbors(ghost_tri, 0, prev_ghost, 1);
      }
      prev_ghost = ghost_tri;
      if (first_ghost == nullptr) {
        first_ghost = ghost_tri;
      }

      /* Find next ecur by going clockwise around v1 from ecur's
       * triangle successor until get an edge with no neighbor.
       */
      Edge etry = ecur.triangle_succ();
      while (etry != e && !neighbor_edge(etry).is_null()) {
        etry = neighbor_edge(etry).triangle_succ();
        BLI_assert(v_dst(etry) != v0);
      }
      ecur = etry;
    } while (ecur != e);
    /* Finally, connect the edges of prev_ghost to first_ghost because of wrap-around. */
    set_mutual_neighbors(prev_ghost, 1, first_ghost, 0);
  }
}

static TriangleMesh triangulate_input(const MeshInset_Input &input)
{
  TriangleMesh trimesh;
  /* First populate the verts_ array with original vertices. */
  for (const int64_t v : input.vert.index_range()) {
    /* We will need to add representative edges later. */
    trimesh.add_vert(input.vert[v]);
  }
  /* Triangulate each face. */
  /* TODO: perhaps parallelize the following loop. */
  int flen = int(input.face.size());
  for (int f = 0; f < flen; ++f) {
    Vector<Triangle *> face_tris = triangulate_face(f, input, trimesh);
    for (Triangle *tri : face_tris) {
      trimesh.add_allocated_triangle(tri);
    }
  }
  connect_neighbors(trimesh.all_tris());
  add_ghost_triangles(trimesh);
  return trimesh;
}

/** Starting with an original contour edge (not the inset copy), \a e_contour,
 * find the face that has that edge and follows spoke edges around until it joins back.
 * Return the vector of integer ids (in the trimesh indexing scheme) that make up the face.
 * Also, add any encountered wavefront edges to \a wavefront_edges (use the direction
 * of wavefront edge that goes ccw around the wavefront).
 */
static Vector<int> get_face_from_contour_edge(Edge e_contour,
                                              const TriangleMesh &trimesh,
                                              Vector<Edge> &wavefront_edges)
{
  Vector<int> face;
  face.append(v_src(e_contour)->id);
  face.append(v_dst(e_contour)->id);
  Edge e = find_cw_spoke_or_wavefront_edge(neighbor_edge(e_contour));
  BLI_assert(!e.is_null());
  /* We should always find a spoke coming back to e_contour, but
   * just in case, provide an emergency out. */
  int count = 0;
  int limit = int(trimesh.all_verts().size()) * 3;
  do {
    face.append(v_dst(e)->id);
    e = find_cw_spoke_or_wavefront_edge(neighbor_edge(e));
    if (is_wavefront_edge(e)) {
      wavefront_edges.append(neighbor_edge(e));
    }
    if (++count > limit) {
      BLI_assert_unreachable();
      return Vector<int>(0);
    }
  } while (v_dst(e) != v_src(e_contour));
  return face;
}

/** Assuming that the \a edges form some number of vertex-disjoint cycles,
 * find those cycles and return them. */
static Vector<Vector<Edge>> find_cycle_partition(const Vector<Edge> &edges)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 1) {
    std::cout << "find_cycle_partition";
    for (const Edge e : edges) {
      std::cout << " " << e;
    }
    std::cout << "\n";
  }
  Vector<Vector<Edge>> ans;
  /* If the cycles are vertex-disjoint, we can find the continuation edge
   * of the current cycle by finding the (should-be-unique) edge whose
   * source is that current tail's destination. */
  Map<int, int> src_to_edge;
  const int num_e = int(edges.size());
  src_to_edge.reserve(num_e);
  for (int ei = 0; ei < num_e; ++ei) {
    Edge e = edges[ei];
    Vert *vs = v_src(e);
    src_to_edge.add_new(vs->id, ei);
  }
  Array<bool> edge_used(edges.size(), false);
  for (;;) {
    int seed_i = 0;
    while (seed_i < edges.size() && edge_used[seed_i]) {
      seed_i++;
    }
    if (seed_i == edges.size()) {
      break;
    }
    ans.append(Vector<Edge>(1, edges[seed_i]));
    edge_used[seed_i] = true;
    Vector<Edge> &curcycle = ans.last();
    Edge curtail = curcycle.last();
    const Vert *vstart = v_src(curtail);
    for (;;) {
      const Vert *vtail = v_dst(curtail);
      if (vtail == vstart) {
        break;
      }
      int enexti = src_to_edge.lookup_default(vtail->id, -1);
      if (enexti == -1 || edge_used[enexti]) {
        /* The assumptions of this function are not true. */
        BLI_assert_unreachable();
        break;
      }
      Edge enext = edges[enexti];
      edge_used[enexti] = true;
      curcycle.append(enext);
      curtail = enext;
    }
  }
  if (dbg_level > 0) {
    std::cout << "answer\n";
    for (const Vector<Edge> &cycle : ans) {
      for (const Edge e : cycle) {
        std::cout << " " << e;
      }
      std::cout << "\n";
    }
  }
  return ans;
}

/** Find and append the faces inside the wavefront cycle \a contour and append them to \a
 * out_faces.  */
static void append_interior_faces_for_cycle(Vector<Vector<int>> &out_faces,
                                            const Vector<Edge> contour)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "append_interior_faces_for_cycle ";
    for (Edge e : contour) {
      std::cout << " " << e;
    }
    std::cout << "\n";
  }
  if (contour.size() < 3) {
    return;
  }
  Vector<Edge> stack;
  Set<Edge> processed;
  for (Edge e : contour) {
    stack.append(e);
  }
  while (!stack.is_empty()) {
    Edge estart = stack.pop_last();
    if (processed.contains(estart)) {
      continue;
    }
    /* Find a cycle of edges starting with estart that contain only original edges
     * or wavefront edges. */
    processed.add(estart);
    out_faces.append(Vector<int>());
    Vector<int> &face = out_faces.last();
    face.append(v_src(estart)->id);
    Edge ecur = estart;
    do {
      Edge enext = find_cw_wavefront_or_orig_edge(neighbor_edge(ecur));
      if (enext.is_null()) {
        /* Shouldn't happen unless didn't propagate "origness" properly. Just choose an edge. */
        BLI_assert_unreachable();
        enext = rot_cw(neighbor_edge(estart));
      }
      processed.add(enext);
      face.append(v_src(enext)->id);
      if (!is_wavefront_edge(enext)) {
        Edge en = neighbor_edge(enext);
        if (!processed.contains(en)) {
          stack.append(en);
        }
      }
      ecur = enext;
    } while (v_dst(ecur) != v_src(estart));
    if (dbg_level > 0) {
      std::cout << "added face";
      for (int v : face) {
        std::cout << " " << v;
      }
      std::cout << "\n";
    }
  }
}

/** Find the vertices and faces that make up the final result from \a trimesh.
 * Many triangles in the triangle mesh need to be merged to form an output face,
 * though we can sometimes avoid a merging step by just tracing around the
 * expected boundaries of faces.
 */
static MeshInset_Result trimesh_to_output(const TriangleMesh &trimesh,
                                          const MeshInset_Input &input)
{
  constexpr bool dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "trimesh_to_output\n";
  }
  MeshInset_Result result;
  /* Put the non-deleted vertices into result.vert, and keep track
   * of how to map a trimesh vertex index to an output vertex index. */
  int tot_all_verts = int(trimesh.all_verts().size());
  Array<int> vert_index_to_output_index(tot_all_verts);
  int totv = 0;
  for (int i = 0; i < tot_all_verts; ++i) {
    const Vert *v = trimesh.get_vert_by_index(i);
    vert_index_to_output_index[i] = totv;
    if (!v->is_deleted()) {
      totv++;
    }
  }
  result.vert = Array<float3>(totv);
  result.orig_vert = Array<int>(totv, -1);
  int out_v_index = 0;
  for (int i = 0; i < tot_all_verts; ++i) {
    const Vert *v = trimesh.get_vert_by_index(i);
    if (!v->is_deleted()) {
      if (i < input.vert.size()) {
        result.orig_vert[out_v_index] = i;
      }
      result.vert[out_v_index++] = v->co;
    }
  }
  /* Each edge in an original contour will generate a face
   * with that edge and some spokes and wavefront edges. */
  Vector<Vector<int>> out_faces;
  Vector<Edge> wavefront_edges;
  int num_contours = int(input.contour.size());
  for (int contour_index = 0; contour_index < num_contours; ++contour_index) {
    const Vector<int> &contour = input.contour[contour_index];
    int n = int(contour.size());
    for (int ci = 0; ci < n; ++ci) {
      const Vert *v_ci = trimesh.get_vert_by_index(contour[ci]);
      const Vert *v_ci1 = trimesh.get_vert_by_index(contour[(ci + 1) % n]);
      Edge e_contour = edge_between(v_ci, v_ci1);
      BLI_assert(!e_contour.is_null());
      Vector<int> face = get_face_from_contour_edge(e_contour, trimesh, wavefront_edges);
      /* Fix indices for output vert indexing. */
      if (dbg_level > 0) {
        std::cout << "new outer face:";
        for (int i : face.index_range()) {
          if (dbg_level > 0) {
            std::cout << " " << face[i];
          }
        }
        std::cout << "\n";
      }
      out_faces.append(face);
    }
  }
  /* Find the remaining inner faces. */
  if (wavefront_edges.size() > 0) {
    Vector<Vector<Edge>> cycles = find_cycle_partition(wavefront_edges);
    /* TODO: handle inner faces propery by preserving the
     * exsiting geometry, which means dissolving only the triangulation edges. */
    for (const Vector<Edge> &cycle : cycles) {
      append_interior_faces_for_cycle(out_faces, cycle);
    }
  }
  /* Change indices in faces to output vertex numbers, */
  for (int64_t fi : out_faces.index_range()) {
    Vector<int> &face = out_faces[fi];
    for (int64_t i : face.index_range()) {
      face[i] = vert_index_to_output_index[face[i]];
    }
  }
  result.face = Array<Vector<int>>(out_faces.size());
  for (int64_t fi : out_faces.index_range()) {
    result.face[fi] = out_faces[fi];
  }
  if (dbg_level > 0) {
    std::cout << "result:\n";
    for (int i : result.vert.index_range()) {
      std::cout << "vert[" << i << "] = " << result.vert[i] << "\n";
    }
    for (int i : result.face.index_range()) {
      const Vector<int> &f = result.face[i];
      std::cout << "face[" << i << "] =";
      for (int j : f.index_range()) {
        std::cout << " " << f[j];
      }
      std::cout << "\n";
    }
    for (int i : result.orig_vert.index_range()) {
      std::cout << "orig_vert[" << i << "] = " << result.orig_vert[i] << "\n";
    }
  }
  return result;
}

MeshInset_Result mesh_inset_calc(const MeshInset_Input &input)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "mesh_inset_calc\n";
    if (dbg_level > 1) {
      std::cout << "input\n";
      for (int i : input.vert.index_range()) {
        std::cout << "vert[" << i << "] = " << input.vert[i] << "\n";
      }
      for (int i : input.face.index_range()) {
        std::cout << "face[" << i << "] =";
        const Vector<int> &f = input.face[i];
        for (int j : f.index_range()) {
          std::cout << " " << f[j];
        }
        std::cout << "\n";
      }
      for (int i : input.contour.index_range()) {
        std::cout << "contour[" << i << "] =";
        const Vector<int> &c = input.contour[i];
        for (int j : c.index_range()) {
          std::cout << " " << c[j];
        }
        std::cout << "\n";
      }
    }
  }
  TriangleMesh trimesh = triangulate_input(input);
  StraightSkeleton ss(trimesh, input.contour, input.inset_amount);
  ss.compute();
  if (input.slope != 0.0f) {
    /* Gather all the deltas before applying, as changing height changes the vertex normals. */
    Array<float3> vco_delta(trimesh.all_verts().size(), float3(0.0f, 0.0f, 0.0f));
    trimesh.calculate_all_tri_normals();
    for (int64_t v_index : trimesh.all_verts().index_range()) {
      Vert *v = trimesh.get_vert_by_index(int(v_index));
      if (!v->is_deleted()) {
        if (ss.vertex_height_map.contains(v->id)) {
          float h = ss.vertex_height_map.lookup(v->id);
          if (h != 0.0f) {
            float shell_factor = vertex_shell_factor(v);
            vco_delta[v_index] = vertex_normal(v) * shell_factor * h * input.slope;
          }
        }
      }
    }
    for (int64_t v_index : trimesh.all_verts().index_range()) {
      Vert *v = trimesh.get_vert_by_index(int(v_index));
      v->co += vco_delta[v->id];
    }
  }
  if (dbg_level > 0) {
    trimesh_draw("after ss " + std::to_string(input.inset_amount), trimesh);
    std::cout << trimesh << "\n";
  }

  return trimesh_to_output(trimesh, input);
}

}  // namespace blender::meshinset
