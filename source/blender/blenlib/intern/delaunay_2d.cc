/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <algorithm>
#include <atomic>
#include <fstream>
#include <iostream>
#include <sstream>

#include "BLI_array.hh"
#include "BLI_linklist.h"
#include "BLI_math_boolean.hh"
#include "BLI_math_vector_mpq_types.hh"
#include "BLI_set.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "BLI_delaunay_2d.hh"

namespace blender::meshintersect {

using namespace blender::math;

/* Throughout this file, template argument T will be an
 * arithmetic-like type, like float, double, or mpq_class. */

template<typename T> T math_abs(const T v)
{
  return (v < 0) ? -v : v;
}

#ifdef WITH_GMP
template<> mpq_class math_abs<mpq_class>(const mpq_class v)
{
  return abs(v);
}
#endif

template<> double math_abs<double>(const double v)
{
  return fabs(v);
}

template<typename T> double math_to_double(const T /*v*/)
{
  BLI_assert(false); /* Need implementation for other type. */
  return 0.0;
}

#ifdef WITH_GMP
template<> double math_to_double<mpq_class>(const mpq_class v)
{
  return v.get_d();
}
#endif

template<> double math_to_double<double>(const double v)
{
  return v;
}

/**
 * Define a templated 2D arrangement of vertices, edges, and faces.
 * The #SymEdge data structure is the basis for a structure that allows
 * easy traversal to neighboring (by topology) geometric elements.
 * Each of #CDTVert, #CDTEdge, and #CDTFace have an input_id set,
 * which contain integers that keep track of which input verts, edges,
 * and faces, respectively, that the element was derived from.
 *
 * While this could be cleaned up some, it is usable by other routines in Blender
 * that need to keep track of a 2D arrangement, with topology.
 */
template<typename T> struct CDTVert;
template<typename T> struct CDTEdge;
template<typename T> struct CDTFace;

template<typename T> struct SymEdge {
  /** Next #SymEdge in face, doing CCW traversal of face. */
  SymEdge<T> *next{nullptr};
  /** Next #SymEdge CCW around vert. */
  SymEdge<T> *rot{nullptr};
  /** Vert at origin. */
  CDTVert<T> *vert{nullptr};
  /** Un-directed edge this is for. */
  CDTEdge<T> *edge{nullptr};
  /** Face on left side. */
  CDTFace<T> *face{nullptr};

  SymEdge() = default;
};

/**
 * Return other #SymEdge for same #CDTEdge as \a se.
 */
template<typename T> inline SymEdge<T> *sym(const SymEdge<T> *se)
{
  return se->next->rot;
}

/** Return #SymEdge whose next is \a se. */
template<typename T> inline SymEdge<T> *prev(const SymEdge<T> *se)
{
  return se->rot->next->rot;
}

/** A coordinate class with extra information for fast filtered orient tests. */

template<typename T> struct FatCo {
  VecBase<T, 2> exact;
  double2 approx;
  double2 abs_approx;

  FatCo();
#ifdef WITH_GMP
  FatCo(const mpq2 &v);
#endif
  FatCo(const double2 &v);
};

#ifdef WITH_GMP
template<> struct FatCo<mpq_class> {
  mpq2 exact;
  double2 approx;
  double2 abs_approx;

  FatCo() : exact(mpq2(0, 0)), approx(double2(0, 0)), abs_approx(double2(0, 0)) {}

  FatCo(const mpq2 &v)
  {
    exact = v;
    approx = double2(v.x.get_d(), v.y.get_d());
    abs_approx = double2(fabs(approx.x), fabs(approx.y));
  }

  FatCo(const double2 &v)
  {
    exact = mpq2(v.x, v.y);
    approx = v;
    abs_approx = double2(fabs(approx.x), fabs(approx.y));
  }
};
#endif

template<> struct FatCo<double> {
  double2 exact;
  double2 approx;
  double2 abs_approx;

  FatCo() : exact(double2(0, 0)), approx(double2(0, 0)), abs_approx(double2(0, 0)) {}

#ifdef WITH_GMP
  FatCo(const mpq2 &v)
  {
    exact = double2(v.x.get_d(), v.y.get_d());
    approx = exact;
    abs_approx = double2(fabs(approx.x), fabs(approx.y));
  }
#endif

  FatCo(const double2 &v)
  {
    exact = v;
    approx = v;
    abs_approx = double2(fabs(approx.x), fabs(approx.y));
  }
};

template<typename T> std::ostream &operator<<(std::ostream &stream, const FatCo<T> &co)
{
  stream << "(" << co.approx.x << ", " << co.approx.y << ")";
  return stream;
}

template<typename T> struct CDTVert {
  /** Coordinate. */
  FatCo<T> co;
  /** Some edge attached to it. */
  SymEdge<T> *symedge{nullptr};
  /** Set of corresponding vertex input ids. Not used if don't need_ids. */
  blender::Set<int> input_ids;
  /** Index into array that #CDTArrangement keeps. */
  int index{-1};
  /** Index of a CDTVert that this has merged to. -1 if no merge. */
  int merge_to_index{-1};
  /** Used by algorithms operating on CDT structures. */
  int visit_index{0};

  CDTVert() = default;
  explicit CDTVert(const VecBase<T, 2> &pt);
};

template<typename T> struct CDTEdge {
  /** Set of input edge ids that this is part of.
   * If don't need_ids, then should contain 0 if it is a constrained edge,
   * else empty. */
  blender::Set<int> input_ids;
  /** The directed edges for this edge. */
  SymEdge<T> symedges[2]{SymEdge<T>(), SymEdge<T>()};

  CDTEdge() = default;
};

template<typename T> struct CDTFace {
  /** A symedge in face; only used during output, so only valid then. */
  SymEdge<T> *symedge{nullptr};
  /** Set of input face ids that this is part of.
   * If don't need_ids, then should contain 0 if it is part of a constrained face,
   * else empty. */
  blender::Set<int> input_ids;
  /** Used by algorithms operating on CDT structures. */
  int visit_index{0};
  /** Marks this face no longer used. */
  bool deleted{false};
  /** Marks this face as part of a hole. */
  bool hole{false};

  CDTFace() = default;
};

template<typename T> struct CDTArrangement {
  /* The arrangement owns the memory pointed to by the pointers in these vectors.
   * They are pointers instead of actual structures because these vectors may be resized and
   * other elements refer to the elements by pointer. */

  /** The verts. Some may be merged to others (see their merge_to_index). */
  Vector<CDTVert<T> *> verts;
  /** The edges. Some may be deleted (SymEdge next and rot pointers are null). */
  Vector<CDTEdge<T> *> edges;
  /** The faces. Some may be deleted (see their delete member). */
  Vector<CDTFace<T> *> faces;
  /** Which CDTFace is the outer face. */
  CDTFace<T> *outer_face{nullptr};

  CDTArrangement() = default;
  ~CDTArrangement();

  /** Hint to how much space to reserve in the Vectors of the arrangement,
   * based on these counts of input elements. */
  void reserve(int verts_num, int edges_num, int faces_num);

  /**
   * Add a new vertex to the arrangement, with the given 2D coordinate.
   * It will not be connected to anything yet.
   */
  CDTVert<T> *add_vert(const VecBase<T, 2> &pt);

  /**
   * Add an edge from v1 to v2. The edge will have a left face and a right face,
   * specified by \a fleft and \a fright. The edge will not be connected to anything yet.
   * If the vertices do not yet have a #SymEdge pointer,
   * their pointer is set to the #SymEdge in this new edge.
   */
  CDTEdge<T> *add_edge(CDTVert<T> *v1, CDTVert<T> *v2, CDTFace<T> *fleft, CDTFace<T> *fright);

  /**
   * Add a new face. It is disconnected until an add_edge makes it the
   * left or right face of an edge.
   */
  CDTFace<T> *add_face();

  /** Make a new edge from v to se->vert, splicing it in. */
  CDTEdge<T> *add_vert_to_symedge_edge(CDTVert<T> *v, SymEdge<T> *se);

  /**
   * Assuming s1 and s2 are both #SymEdge's in a face with > 3 sides and one is not the next of the
   * other, Add an edge from `s1->v` to `s2->v`, splitting the face in two. The original face will
   * be the one that s1 has as left face, and a new face will be added and made s2 and its
   * next-cycle's left face.
   */
  CDTEdge<T> *add_diagonal(SymEdge<T> *s1, SymEdge<T> *s2);

  /**
   * Connect the verts of se1 and se2, assuming that currently those two #SymEdge's are on the
   * outer boundary (have face == outer_face) of two components that are isolated from each other.
   */
  CDTEdge<T> *connect_separate_parts(SymEdge<T> *se1, SymEdge<T> *se2);

  /**
   * Split se at fraction lambda, and return the new #CDTEdge that is the new second half.
   * Copy the edge input_ids into the new one.
   */
  CDTEdge<T> *split_edge(SymEdge<T> *se, T lambda);

  /**
   * Delete an edge. The new combined face on either side of the deleted edge will be the one that
   * was e's face. There will now be an unused face, which will be marked deleted, and an unused
   * #CDTEdge, marked by setting the next and rot pointers of its #SymEdge's to #nullptr.
   */
  void delete_edge(SymEdge<T> *se);

  /**
   * If the vertex with index i in the vert array has not been merge, return it.
   * Else return the one that it has merged to.
   */
  CDTVert<T> *get_vert_resolve_merge(int i)
  {
    CDTVert<T> *v = this->verts[i];
    if (v->merge_to_index != -1) {
      v = this->verts[v->merge_to_index];
    }
    return v;
  }
};

template<typename T> class CDT_state {
 public:
  CDTArrangement<T> cdt;
  /** How many verts were in input (will be first in vert_array). */
  int input_vert_num;
  /** Used for visiting things without having to initialized their visit fields. */
  int visit_count;
  /**
   * Edge ids for face start with this, and each face gets this much index space
   * to encode positions within the face.
   */
  int face_edge_offset;
  /** How close before coords considered equal. */
  T epsilon;
  /** Do we need to track ids? */
  bool need_ids;

  explicit CDT_state(
      int input_verts_num, int input_edges_num, int input_faces_num, T epsilon, bool need_ids);
};

template<typename T> CDTArrangement<T>::~CDTArrangement()
{
  for (int i : this->verts.index_range()) {
    CDTVert<T> *v = this->verts[i];
    v->input_ids.clear();
    delete v;
    this->verts[i] = nullptr;
  }
  for (int i : this->edges.index_range()) {
    CDTEdge<T> *e = this->edges[i];
    e->input_ids.clear();
    delete e;
    this->edges[i] = nullptr;
  }
  for (int i : this->faces.index_range()) {
    CDTFace<T> *f = this->faces[i];
    f->input_ids.clear();
    delete f;
    this->faces[i] = nullptr;
  }
}

#define DEBUG_CDT
#ifdef DEBUG_CDT
/* Some functions to aid in debugging. */
template<typename T> std::string vertname(const CDTVert<T> *v)
{
  std::stringstream ss;
  ss << "[" << v->index << "]";
  return ss.str();
}

/* Abbreviated pointer value is easier to read in dumps. */
static std::string trunc_ptr(const void *p)
{
  constexpr int TRUNC_PTR_MASK = 0xFFFF;
  std::stringstream ss;
  ss << std::hex << (POINTER_AS_INT(p) & TRUNC_PTR_MASK);
  return ss.str();
}

template<typename T> std::string sename(const SymEdge<T> *se)
{
  std::stringstream ss;
  ss << "{" << trunc_ptr(se) << "}";
  return ss.str();
}

template<typename T> std::ostream &operator<<(std::ostream &os, const SymEdge<T> &se)
{
  if (se.next) {
    os << vertname(se.vert) << "(" << se.vert->co << "->" << se.next->vert->co << ")"
       << vertname(se.next->vert);
  }
  else {
    os << vertname(se.vert) << "(" << se.vert->co << "->null)";
  }
  return os;
}

template<typename T> std::ostream &operator<<(std::ostream &os, const SymEdge<T> *se)
{
  os << *se;
  return os;
}

template<typename T> std::string short_se_dump(const SymEdge<T> *se)
{
  if (se == nullptr) {
    return std::string("null");
  }
  return vertname(se->vert) +
         (se->next == nullptr ? std::string("[null]") : vertname(se->next->vert));
}

template<typename T> std::ostream &operator<<(std::ostream &os, const CDT_state<T> &cdt_state)
{
  os << "\nCDT\n\nVERTS\n";
  for (const CDTVert<T> *v : cdt_state.cdt.verts) {
    os << vertname(v) << " " << trunc_ptr(v) << ": " << v->co
       << " symedge=" << trunc_ptr(v->symedge);
    if (v->merge_to_index == -1) {
      os << "\n";
    }
    else {
      os << " merge to " << vertname(cdt_state.cdt.verts[v->merge_to_index]) << "\n";
    }
    const SymEdge<T> *se = v->symedge;
    int count = 0;
    constexpr int print_count_limit = 25;
    if (se) {
      os << "  edges out:\n";
      do {
        if (se->next == nullptr) {
          os << "    [null] next/rot symedge, se=" << trunc_ptr(se) << "\n";
          break;
        }
        if (se->next->next == nullptr) {
          os << "    [null] next-next/rot symedge, se=" << trunc_ptr(se) << "\n";
          break;
        }
        const CDTVert<T> *vother = sym(se)->vert;
        os << "    " << vertname(vother) << "(e=" << trunc_ptr(se->edge)
           << ", se=" << trunc_ptr(se) << ")\n";
        se = se->rot;
        count++;
      } while (se != v->symedge && count < print_count_limit);
      os << "\n";
    }
  }
  os << "\nEDGES\n";
  for (const CDTEdge<T> *e : cdt_state.cdt.edges) {
    if (e->symedges[0].next == nullptr) {
      continue;
    }
    os << trunc_ptr(&e) << ":\n";
    for (int i = 0; i < 2; ++i) {
      const SymEdge<T> *se = &e->symedges[i];
      os << "  se[" << i << "] @" << trunc_ptr(se) << " next=" << trunc_ptr(se->next)
         << ", rot=" << trunc_ptr(se->rot) << ", vert=" << trunc_ptr(se->vert) << " "
         << vertname(se->vert) << " " << se->vert->co << ", edge=" << trunc_ptr(se->edge)
         << ", face=" << trunc_ptr(se->face) << "\n";
    }
  }
  os << "\nFACES\n";
  os << "outer_face=" << trunc_ptr(cdt_state.cdt.outer_face) << "\n";
  /* Only after prepare_output do faces have non-null symedges. */
  if (cdt_state.cdt.outer_face->symedge != nullptr) {
    for (const CDTFace<T> *f : cdt_state.cdt.faces) {
      if (!f->deleted) {
        os << trunc_ptr(f) << " symedge=" << trunc_ptr(f->symedge) << "\n";
      }
    }
  }
  return os;
}

template<typename T> void cdt_draw(const std::string &label, const CDTArrangement<T> &cdt)
{
  static bool append = false; /* Will be set to true after first call. */

/* Would like to use #BKE_tempdir_base() here, but that brings in dependence on kernel library.
 * This is just for developer debugging anyway, and should never be called in production Blender.
 */
#  ifdef _WIN32
  const char *drawfile = "./cdt_debug_draw.html";
#  else
  const char *drawfile = "/tmp/cdt_debug_draw.html";
#  endif
  constexpr int max_draw_width = 1800;
  constexpr int max_draw_height = 1600;
  constexpr double margin_expand = 0.05;
  constexpr int thin_line = 1;
  constexpr int thick_line = 4;
  constexpr int vert_radius = 3;
  constexpr bool draw_vert_labels = true;
  constexpr bool draw_edge_labels = false;
  constexpr bool draw_face_labels = false;

  if (cdt.verts.is_empty()) {
    return;
  }
  double2 vmin(std::numeric_limits<double>::max());
  double2 vmax(std::numeric_limits<double>::lowest());
  for (const CDTVert<T> *v : cdt.verts) {
    math::min_max(v->co.approx, vmin, vmax);
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
  int view_height = int(view_width * aspect);
  if (view_height > max_draw_height) {
    view_height = max_draw_height;
    view_width = int(view_height / aspect);
  }
  double scale = view_width / width;

#  define SX(x) (((x) - minx) * scale)
#  define SY(y) ((maxy - (y)) * scale)

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
    << "width=\"" << view_width << "\" height=\"" << view_height << "\">n";

  for (const CDTEdge<T> *e : cdt.edges) {
    if (e->symedges[0].next == nullptr) {
      continue;
    }
    const CDTVert<T> *u = e->symedges[0].vert;
    const CDTVert<T> *v = e->symedges[1].vert;
    const double2 &uco = u->co.approx;
    const double2 &vco = v->co.approx;
    int strokew = e->input_ids.size() == 0 ? thin_line : thick_line;
    f << R"(<line fill="none" stroke="black" stroke-width=")" << strokew << "\" x1=\""
      << SX(uco[0]) << "\" y1=\"" << SY(uco[1]) << "\" x2=\"" << SX(vco[0]) << "\" y2=\""
      << SY(vco[1]) << "\">\n";
    f << "  <title>" << vertname(u) << vertname(v) << "</title>\n";
    f << "</line>\n";
    if (draw_edge_labels) {
      f << "<text x=\"" << SX((uco[0] + vco[0]) / 2) << "\" y=\"" << SY((uco[1] + vco[1]) / 2)
        << R"(" font-size="small">)";
      f << vertname(u) << vertname(v) << sename(&e->symedges[0]) << sename(&e->symedges[1])
        << "</text>\n";
    }
  }

  int i = 0;
  for (const CDTVert<T> *v : cdt.verts) {
    f << R"(<circle fill="black" cx=")" << SX(v->co.approx[0]) << "\" cy=\"" << SY(v->co.approx[1])
      << "\" r=\"" << vert_radius << "\">\n";
    f << "  <title>[" << i << "]" << v->co.approx << "</title>\n";
    f << "</circle>\n";
    if (draw_vert_labels) {
      f << "<text x=\"" << SX(v->co.approx[0]) + vert_radius << "\" y=\""
        << SY(v->co.approx[1]) - vert_radius << R"(" font-size="small">[)" << i << "]</text>\n";
    }
    ++i;
  }

  if (draw_face_labels) {
    for (const CDTFace<T> *face : cdt.faces) {
      if (!face->deleted) {
        /* Since may not have prepared output yet, need a slow find of a SymEdge for this face. */
        const SymEdge<T> *se_face_start = nullptr;
        for (const CDTEdge<T> *e : cdt.edges) {
          if (e->symedges[0].face == face) {
            se_face_start = &e->symedges[0];
            break;
          }
          if (e->symedges[1].face == face) {
            se_face_start = &e->symedges[1];
          }
        }
        if (se_face_start != nullptr) {
          /* Find center of face. */
          int face_nverts = 0;
          double2 cen(0.0, 0.0);
          if (face == cdt.outer_face) {
            cen.x = minx;
            cen.y = miny;
          }
          else {
            const SymEdge<T> *se = se_face_start;
            do {
              if (se->face == face) {
                cen = cen + se->vert->co.approx;
                face_nverts++;
              }
            } while ((se = se->next) != se_face_start);
            if (face_nverts > 0) {
              cen = cen / double(face_nverts);
            }
          }
          f << "<text x=\"" << SX(cen[0]) << "\" y=\"" << SY(cen[1]) << "\""
            << " font-size=\"small\">[";
          f << trunc_ptr(face);
          if (face->input_ids.size() > 0) {
            for (int id : face->input_ids) {
              f << " " << id;
            }
          }
          f << "]</text>\n";
        }
      }
    }
  }

  append = true;
#  undef SX
#  undef SY
}
#endif

/**
 * A filtered version of orient2d, which will usually be much faster when using exact arithmetic.
 * See EXACT GEOMETRIC COMPUTATION USING CASCADING, by Burnikel, Funke, and Seel.
 */
template<typename T>
static int filtered_orient2d(const FatCo<T> &a, const FatCo<T> &b, const FatCo<T> &c);

#ifdef WITH_GMP
template<>
int filtered_orient2d<mpq_class>(const FatCo<mpq_class> &a,
                                 const FatCo<mpq_class> &b,
                                 const FatCo<mpq_class> &c)
{
  double det = (a.approx[0] - c.approx[0]) * (b.approx[1] - c.approx[1]) -
               (a.approx[1] - c.approx[1]) * (b.approx[0] - c.approx[0]);
  double supremum = (a.abs_approx[0] + c.abs_approx[0]) * (b.abs_approx[1] + c.abs_approx[1]) +
                    (a.abs_approx[1] + c.abs_approx[1]) * (b.abs_approx[0] + c.abs_approx[0]);
  constexpr double index_orient2d = 6;
  double err_bound = supremum * index_orient2d * DBL_EPSILON;
  if (fabs(det) > err_bound) {
    return det > 0 ? 1 : -1;
  }
  return orient2d(a.exact, b.exact, c.exact);
}
#endif

template<>
int filtered_orient2d<double>(const FatCo<double> &a,
                              const FatCo<double> &b,
                              const FatCo<double> &c)
{
  return orient2d(a.approx, b.approx, c.approx);
}

/**
 * A filtered version of incircle.
 */
template<typename T>
static int filtered_incircle(const FatCo<T> &a,
                             const FatCo<T> &b,
                             const FatCo<T> &c,
                             const FatCo<T> &d);

#ifdef WITH_GMP
template<>
int filtered_incircle<mpq_class>(const FatCo<mpq_class> &a,
                                 const FatCo<mpq_class> &b,
                                 const FatCo<mpq_class> &c,
                                 const FatCo<mpq_class> &d)
{
  double adx = a.approx[0] - d.approx[0];
  double bdx = b.approx[0] - d.approx[0];
  double cdx = c.approx[0] - d.approx[0];
  double ady = a.approx[1] - d.approx[1];
  double bdy = b.approx[1] - d.approx[1];
  double cdy = c.approx[1] - d.approx[1];
  double bdxcdy = bdx * cdy;
  double cdxbdy = cdx * bdy;
  double alift = adx * adx + ady * ady;
  double cdxady = cdx * ady;
  double adxcdy = adx * cdy;
  double blift = bdx * bdx + bdy * bdy;
  double adxbdy = adx * bdy;
  double bdxady = bdx * ady;
  double clift = cdx * cdx + cdy * cdy;
  double det = alift * (bdxcdy - cdxbdy) + blift * (cdxady - adxcdy) + clift * (adxbdy - bdxady);

  double sup_adx = a.abs_approx[0] + d.abs_approx[0]; /* index 2. */
  double sup_bdx = b.abs_approx[0] + d.abs_approx[0];
  double sup_cdx = c.abs_approx[0] + d.abs_approx[0];
  double sup_ady = a.abs_approx[1] + d.abs_approx[1];
  double sup_bdy = b.abs_approx[1] + d.abs_approx[1];
  double sup_cdy = c.abs_approx[1] + d.abs_approx[1];
  double sup_bdxcdy = sup_bdx * sup_cdy; /* index 5. */
  double sup_cdxbdy = sup_cdx * sup_bdy;
  double sup_alift = sup_adx * sup_adx + sup_ady * sup_ady; /* index 6. */
  double sup_cdxady = sup_cdx * sup_ady;
  double sup_adxcdy = sup_adx * sup_cdy;
  double sup_blift = sup_bdx * sup_bdx + sup_bdy * sup_bdy;
  double sup_adxbdy = sup_adx * sup_bdy;
  double sup_bdxady = sup_bdx * sup_ady;
  double sup_clift = sup_cdx * sup_cdx + sup_cdy * sup_cdy;
  double sup_det = sup_alift * (sup_bdxcdy + sup_cdxbdy) + sup_blift * (sup_cdxady + sup_adxcdy) +
                   sup_clift * (sup_adxbdy + sup_bdxady);
  int index = 14;
  double err_bound = sup_det * index * DBL_EPSILON;
  if (fabs(det) > err_bound) {
    return det < 0.0 ? -1 : (det > 0.0 ? 1 : 0);
  }
  return incircle(a.exact, b.exact, c.exact, d.exact);
}
#endif

template<>
int filtered_incircle<double>(const FatCo<double> &a,
                              const FatCo<double> &b,
                              const FatCo<double> &c,
                              const FatCo<double> &d)
{
  return incircle(a.approx, b.approx, c.approx, d.approx);
}

/**
 * Return true if `a -- b -- c` are in that order, assuming they are on a straight line according
 * to #orient2d and we know the order is either `abc` or `bac`.
 * This means `ab . ac` and `bc . ac` must both be non-negative.
 * Use filtering to speed this up when using exact arithmetic.
 */
template<typename T> static bool in_line(const FatCo<T> &a, const FatCo<T> &b, const FatCo<T> &c);

#ifdef WITH_GMP
template<>
bool in_line<mpq_class>(const FatCo<mpq_class> &a,
                        const FatCo<mpq_class> &b,
                        const FatCo<mpq_class> &c)
{
  double2 ab = b.approx - a.approx;
  double2 bc = c.approx - b.approx;
  double2 ac = c.approx - a.approx;
  double2 supremum_ab = b.abs_approx + a.abs_approx;
  double2 supremum_bc = c.abs_approx + b.abs_approx;
  double2 supremum_ac = c.abs_approx + a.abs_approx;
  double dot_ab_ac = ab.x * ac.x + ab.y * ac.y;
  double supremum_dot_ab_ac = supremum_ab.x * supremum_ac.x + supremum_ab.y * supremum_ac.y;
  constexpr double index = 6;
  double err_bound = supremum_dot_ab_ac * index * DBL_EPSILON;
  if (dot_ab_ac < -err_bound) {
    return false;
  }
  double dot_bc_ac = bc.x * ac.x + bc.y * ac.y;
  double supremum_dot_bc_ac = supremum_bc.x * supremum_ac.x + supremum_bc.y * supremum_ac.y;
  err_bound = supremum_dot_bc_ac * index * DBL_EPSILON;
  if (dot_bc_ac < -err_bound) {
    return false;
  }
  mpq2 exact_ab = b.exact - a.exact;
  mpq2 exact_ac = c.exact - a.exact;
  if (dot(exact_ab, exact_ac) < 0) {
    return false;
  }
  mpq2 exact_bc = c.exact - b.exact;
  return dot(exact_bc, exact_ac) >= 0;
}
#endif

template<>
bool in_line<double>(const FatCo<double> &a, const FatCo<double> &b, const FatCo<double> &c)
{
  double2 ab = b.approx - a.approx;
  double2 ac = c.approx - a.approx;
  if (dot(ab, ac) < 0) {
    return false;
  }
  double2 bc = c.approx - b.approx;
  return dot(bc, ac) >= 0;
}

template<> CDTVert<double>::CDTVert(const double2 &pt)
{
  this->co.exact = pt;
  this->co.approx = pt;
  this->co.abs_approx = pt; /* Not used, so doesn't matter. */
  this->symedge = nullptr;
  this->index = -1;
  this->merge_to_index = -1;
  this->visit_index = 0;
}

#ifdef WITH_GMP
template<> CDTVert<mpq_class>::CDTVert(const mpq2 &pt)
{
  this->co.exact = pt;
  this->co.approx = double2(pt.x.get_d(), pt.y.get_d());
  this->co.abs_approx = double2(fabs(this->co.approx.x), fabs(this->co.approx.y));
  this->symedge = nullptr;
  this->index = -1;
  this->merge_to_index = -1;
  this->visit_index = 0;
}
#endif

template<typename T> CDTVert<T> *CDTArrangement<T>::add_vert(const VecBase<T, 2> &pt)
{
  CDTVert<T> *v = new CDTVert<T>(pt);
  int index = this->verts.append_and_get_index(v);
  v->index = index;
  return v;
}

template<typename T>
CDTEdge<T> *CDTArrangement<T>::add_edge(CDTVert<T> *v1,
                                        CDTVert<T> *v2,
                                        CDTFace<T> *fleft,
                                        CDTFace<T> *fright)
{
  CDTEdge<T> *e = new CDTEdge<T>();
  this->edges.append(e);
  SymEdge<T> *se = &e->symedges[0];
  SymEdge<T> *sesym = &e->symedges[1];
  se->edge = sesym->edge = e;
  se->face = fleft;
  sesym->face = fright;
  se->vert = v1;
  if (v1->symedge == nullptr) {
    v1->symedge = se;
  }
  sesym->vert = v2;
  if (v2->symedge == nullptr) {
    v2->symedge = sesym;
  }
  se->next = sesym->next = se->rot = sesym->rot = nullptr;
  return e;
}

template<typename T> CDTFace<T> *CDTArrangement<T>::add_face()
{
  CDTFace<T> *f = new CDTFace<T>();
  this->faces.append(f);
  return f;
}

template<typename T> void CDTArrangement<T>::reserve(int verts_num, int edges_num, int faces_num)
{
  /* These reserves are just guesses; OK if they aren't exactly right since vectors will resize. */
  this->verts.reserve(2 * verts_num);
  this->edges.reserve(3 * verts_num + 2 * edges_num + 3 * 2 * faces_num);
  this->faces.reserve(2 * verts_num + 2 * edges_num + 2 * faces_num);
}

template<typename T>
CDT_state<T>::CDT_state(
    int input_verts_num, int input_edges_num, int input_faces_num, T epsilon, bool need_ids)
{
  this->input_vert_num = input_verts_num;
  this->cdt.reserve(input_verts_num, input_edges_num, input_faces_num);
  this->cdt.outer_face = this->cdt.add_face();
  this->epsilon = epsilon;
  this->need_ids = need_ids;
  this->visit_count = 0;
}

/* Is any id in (range_start, range_start+1, ... , range_end) in id_list? */
static bool id_range_in_list(const blender::Set<int> &id_list, int range_start, int range_end)
{
  for (int id : id_list) {
    if (id >= range_start && id <= range_end) {
      return true;
    }
  }
  return false;
}

static void add_to_input_ids(blender::Set<int> &dst, int input_id)
{
  dst.add(input_id);
}

static void add_list_to_input_ids(blender::Set<int> &dst, const blender::Set<int> &src)
{
  for (int value : src) {
    dst.add(value);
  }
}

template<typename T> inline bool is_border_edge(const CDTEdge<T> *e, const CDT_state<T> *cdt)
{
  return e->symedges[0].face == cdt->outer_face || e->symedges[1].face == cdt->outer_face;
}

template<typename T> inline bool is_constrained_edge(const CDTEdge<T> *e)
{
  return e->input_ids.size() > 0;
}

template<typename T> inline bool is_deleted_edge(const CDTEdge<T> *e)
{
  return e->symedges[0].next == nullptr;
}

template<typename T> inline bool is_original_vert(const CDTVert<T> *v, CDT_state<T> *cdt)
{
  return (v->index < cdt->input_vert_num);
}

/**
 * Return the #SymEdge that goes from v1 to v2, if it exists, else return nullptr.
 */
template<typename T>
SymEdge<T> *find_symedge_between_verts(const CDTVert<T> *v1, const CDTVert<T> *v2)
{
  SymEdge<T> *t = v1->symedge;
  SymEdge<T> *tstart = t;
  do {
    if (t->next->vert == v2) {
      return t;
    }
  } while ((t = t->rot) != tstart);
  return nullptr;
}

/**
 * Return the SymEdge attached to v that has face f, if it exists, else return nullptr.
 */
template<typename T> SymEdge<T> *find_symedge_with_face(const CDTVert<T> *v, const CDTFace<T> *f)
{
  SymEdge<T> *t = v->symedge;
  SymEdge<T> *tstart = t;
  do {
    if (t->face == f) {
      return t;
    }
  } while ((t = t->rot) != tstart);
  return nullptr;
}

/**
 * Is there already an edge between a and b?
 */
template<typename T> inline bool exists_edge(const CDTVert<T> *v1, const CDTVert<T> *v2)
{
  return find_symedge_between_verts(v1, v2) != nullptr;
}

/**
 * Is the vertex v incident on face f?
 */
template<typename T> bool vert_touches_face(const CDTVert<T> *v, const CDTFace<T> *f)
{
  SymEdge<T> *se = v->symedge;
  do {
    if (se->face == f) {
      return true;
    }
  } while ((se = se->rot) != v->symedge);
  return false;
}

/**
 * Assume s1 and s2 are both #SymEdges in a face with > 3 sides,
 * and one is not the next of the other.
 * Add an edge from `s1->v` to `s2->v`, splitting the face in two.
 * The original face will continue to be associated with the sub-face
 * that has s1, and a new face will be made for s2's new face.
 * Return the new diagonal's #CDTEdge pointer.
 */
template<typename T> CDTEdge<T> *CDTArrangement<T>::add_diagonal(SymEdge<T> *s1, SymEdge<T> *s2)
{
  CDTFace<T> *fold = s1->face;
  CDTFace<T> *fnew = this->add_face();
  SymEdge<T> *s1prev = prev(s1);
  SymEdge<T> *s1prevsym = sym(s1prev);
  SymEdge<T> *s2prev = prev(s2);
  SymEdge<T> *s2prevsym = sym(s2prev);
  CDTEdge<T> *ediag = this->add_edge(s1->vert, s2->vert, fnew, fold);
  SymEdge<T> *sdiag = &ediag->symedges[0];
  SymEdge<T> *sdiagsym = &ediag->symedges[1];
  sdiag->next = s2;
  sdiagsym->next = s1;
  s2prev->next = sdiagsym;
  s1prev->next = sdiag;
  s1->rot = sdiag;
  sdiag->rot = s1prevsym;
  s2->rot = sdiagsym;
  sdiagsym->rot = s2prevsym;
  for (SymEdge<T> *se = s2; se != sdiag; se = se->next) {
    se->face = fnew;
  }
  add_list_to_input_ids(fnew->input_ids, fold->input_ids);
  return ediag;
}

template<typename T>
CDTEdge<T> *CDTArrangement<T>::add_vert_to_symedge_edge(CDTVert<T> *v, SymEdge<T> *se)
{
  SymEdge<T> *se_rot = se->rot;
  SymEdge<T> *se_rotsym = sym(se_rot);
  /* TODO: check: I think last arg in next should be sym(se)->face. */
  CDTEdge<T> *e = this->add_edge(v, se->vert, se->face, se->face);
  SymEdge<T> *new_se = &e->symedges[0];
  SymEdge<T> *new_se_sym = &e->symedges[1];
  new_se->next = se;
  new_se_sym->next = new_se;
  new_se->rot = new_se;
  new_se_sym->rot = se_rot;
  se->rot = new_se_sym;
  se_rotsym->next = new_se_sym;
  return e;
}

/**
 * Connect the verts of se1 and se2, assuming that currently those two #SymEdge's are on
 * the outer boundary (have face == outer_face) of two components that are isolated from
 * each other.
 */
template<typename T>
CDTEdge<T> *CDTArrangement<T>::connect_separate_parts(SymEdge<T> *se1, SymEdge<T> *se2)
{
  BLI_assert(se1->face == this->outer_face && se2->face == this->outer_face);
  SymEdge<T> *se1_rot = se1->rot;
  SymEdge<T> *se1_rotsym = sym(se1_rot);
  SymEdge<T> *se2_rot = se2->rot;
  SymEdge<T> *se2_rotsym = sym(se2_rot);
  CDTEdge<T> *e = this->add_edge(se1->vert, se2->vert, this->outer_face, this->outer_face);
  SymEdge<T> *new_se = &e->symedges[0];
  SymEdge<T> *new_se_sym = &e->symedges[1];
  new_se->next = se2;
  new_se_sym->next = se1;
  new_se->rot = se1_rot;
  new_se_sym->rot = se2_rot;
  se1->rot = new_se;
  se2->rot = new_se_sym;
  se1_rotsym->next = new_se;
  se2_rotsym->next = new_se_sym;
  return e;
}

/**
 * Split se at fraction lambda,
 * and return the new #CDTEdge that is the new second half.
 * Copy the edge input_ids into the new one.
 */
template<typename T> CDTEdge<T> *CDTArrangement<T>::split_edge(SymEdge<T> *se, T lambda)
{
  /* Split e at lambda. */
  const VecBase<T, 2> *a = &se->vert->co.exact;
  const VecBase<T, 2> *b = &se->next->vert->co.exact;
  SymEdge<T> *sesym = sym(se);
  SymEdge<T> *sesymprev = prev(sesym);
  SymEdge<T> *sesymprevsym = sym(sesymprev);
  SymEdge<T> *senext = se->next;
  CDTVert<T> *v = this->add_vert(interpolate(*a, *b, lambda));
  CDTEdge<T> *e = this->add_edge(v, se->next->vert, se->face, sesym->face);
  sesym->vert = v;
  SymEdge<T> *newse = &e->symedges[0];
  SymEdge<T> *newsesym = &e->symedges[1];
  se->next = newse;
  newsesym->next = sesym;
  newse->next = senext;
  newse->rot = sesym;
  sesym->rot = newse;
  senext->rot = newsesym;
  newsesym->rot = sesymprevsym;
  sesymprev->next = newsesym;
  if (newsesym->vert->symedge == sesym) {
    newsesym->vert->symedge = newsesym;
  }
  add_list_to_input_ids(e->input_ids, se->edge->input_ids);
  return e;
}

/**
 * Delete an edge from the structure. The new combined face on either side of
 * the deleted edge will be the one that was e's face.
 * There will be now an unused face, marked by setting its deleted flag,
 * and an unused #CDTEdge, marked by setting the next and rot pointers of
 * its #SymEdges to #nullptr.
 * <pre>
 *        .  v2               .
 *       / \                 / \
 *      /f|j\               /   \
 *     /  |  \             /     \
 *        |
 *      A |  B                A
 *    \  e|   /           \       /
 *     \  | /              \     /
 *      \h|i/               \   /
 *        .  v1               .
 * </pre>
 * Also handle variant cases where one or both ends
 * are attached only to e.
 */
template<typename T> void CDTArrangement<T>::delete_edge(SymEdge<T> *se)
{
  SymEdge<T> *sesym = sym(se);
  CDTVert<T> *v1 = se->vert;
  CDTVert<T> *v2 = sesym->vert;
  CDTFace<T> *aface = se->face;
  CDTFace<T> *bface = sesym->face;
  SymEdge<T> *f = se->next;
  SymEdge<T> *h = prev(se);
  SymEdge<T> *i = sesym->next;
  SymEdge<T> *j = prev(sesym);
  SymEdge<T> *jsym = sym(j);
  SymEdge<T> *hsym = sym(h);
  bool v1_isolated = (i == se);
  bool v2_isolated = (f == sesym);

  if (!v1_isolated) {
    h->next = i;
    i->rot = hsym;
  }
  if (!v2_isolated) {
    j->next = f;
    f->rot = jsym;
  }
  if (!v1_isolated && !v2_isolated && aface != bface) {
    for (SymEdge<T> *k = i; k != f; k = k->next) {
      k->face = aface;
    }
  }

  /* If e was representative symedge for v1 or v2, fix that. */
  if (v1_isolated) {
    v1->symedge = nullptr;
  }
  else if (v1->symedge == se) {
    v1->symedge = i;
  }
  if (v2_isolated) {
    v2->symedge = nullptr;
  }
  else if (v2->symedge == sesym) {
    v2->symedge = f;
  }

  /* Mark #SymEdge as deleted by setting all its pointers to null. */
  se->next = se->rot = nullptr;
  sesym->next = sesym->rot = nullptr;
  if (!v1_isolated && !v2_isolated && aface != bface) {
    bface->deleted = true;
    if (this->outer_face == bface) {
      this->outer_face = aface;
    }
  }
}

template<typename T> class SiteInfo {
 public:
  CDTVert<T> *v;
  int orig_index;
};

/**
 * Compare function for lexicographic sort: x, then y, then index.
 */
template<typename T> bool site_lexicographic_sort(const SiteInfo<T> &a, const SiteInfo<T> &b)
{
  const VecBase<T, 2> &co_a = a.v->co.exact;
  const VecBase<T, 2> &co_b = b.v->co.exact;
  if (co_a[0] < co_b[0]) {
    return true;
  }
  if (co_a[0] > co_b[0]) {
    return false;
  }
  if (co_a[1] < co_b[1]) {
    return true;
  }
  if (co_a[1] > co_b[1]) {
    return false;
  }
  return a.orig_index < b.orig_index;
}

/**
 * Find series of equal vertices in the sorted sites array
 * and use the vertices merge_to_index to indicate that
 * all vertices after the first merge to the first.
 */
template<typename T> void find_site_merges(Array<SiteInfo<T>> &sites)
{
  int n = sites.size();
  for (int i = 0; i < n - 1; ++i) {
    int j = i + 1;
    while (j < n && sites[j].v->co.exact == sites[i].v->co.exact) {
      sites[j].v->merge_to_index = sites[i].orig_index;
      ++j;
    }
    if (j - i > 1) {
      i = j - 1; /* j-1 because loop head will add another 1. */
    }
  }
}

template<typename T> inline bool vert_left_of_symedge(CDTVert<T> *v, SymEdge<T> *se)
{
  return filtered_orient2d(v->co, se->vert->co, se->next->vert->co) > 0;
}

template<typename T> inline bool vert_right_of_symedge(CDTVert<T> *v, SymEdge<T> *se)
{
  return filtered_orient2d(v->co, se->next->vert->co, se->vert->co) > 0;
}

/* Is se above basel? */
template<typename T>
inline bool dc_tri_valid(SymEdge<T> *se, SymEdge<T> *basel, SymEdge<T> *basel_sym)
{
  return filtered_orient2d(se->next->vert->co, basel_sym->vert->co, basel->vert->co) > 0;
}

/**
 * Delaunay triangulate sites[start} to sites[end-1].
 * Assume sites are lexicographically sorted by coordinate.
 * Return #SymEdge of CCW convex hull at left-most point in *r_le
 * and that of right-most point of cw convex null in *r_re.
 */
template<typename T>
void dc_tri(CDTArrangement<T> *cdt,
            Array<SiteInfo<T>> &sites,
            int start,
            int end,
            SymEdge<T> **r_le,
            SymEdge<T> **r_re)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "DC_TRI start=" << start << " end=" << end << "\n";
  }
  int n = end - start;
  if (n <= 1) {
    *r_le = nullptr;
    *r_re = nullptr;
    return;
  }

  /* Base case: if n <= 3, triangulate directly. */
  if (n <= 3) {
    CDTVert<T> *v1 = sites[start].v;
    CDTVert<T> *v2 = sites[start + 1].v;
    CDTEdge<T> *ea = cdt->add_edge(v1, v2, cdt->outer_face, cdt->outer_face);
    ea->symedges[0].next = &ea->symedges[1];
    ea->symedges[1].next = &ea->symedges[0];
    ea->symedges[0].rot = &ea->symedges[0];
    ea->symedges[1].rot = &ea->symedges[1];
    if (n == 2) {
      *r_le = &ea->symedges[0];
      *r_re = &ea->symedges[1];
      return;
    }
    CDTVert<T> *v3 = sites[start + 2].v;
    CDTEdge<T> *eb = cdt->add_vert_to_symedge_edge(v3, &ea->symedges[1]);
    int orient = filtered_orient2d(v1->co, v2->co, v3->co);
    if (orient > 0) {
      cdt->add_diagonal(&eb->symedges[0], &ea->symedges[0]);
      *r_le = &ea->symedges[0];
      *r_re = &eb->symedges[0];
    }
    else if (orient < 0) {
      cdt->add_diagonal(&ea->symedges[0], &eb->symedges[0]);
      *r_le = ea->symedges[0].rot;
      *r_re = eb->symedges[0].rot;
    }
    else {
      /* Collinear points. Just return a line. */
      *r_le = &ea->symedges[0];
      *r_re = &eb->symedges[0];
    }
    return;
  }
  /* Recursive case. Do left (L) and right (R) halves separately, then join. */
  int n2 = n / 2;
  BLI_assert(n2 >= 2 && end - (start + n2) >= 2);
  SymEdge<T> *ldo;
  SymEdge<T> *ldi;
  SymEdge<T> *rdi;
  SymEdge<T> *rdo;
  dc_tri(cdt, sites, start, start + n2, &ldo, &ldi);
  dc_tri(cdt, sites, start + n2, end, &rdi, &rdo);
  if (dbg_level > 0) {
    std::cout << "\nDC_TRI merge step for start=" << start << ", end=" << end << "\n";
    std::cout << "ldo " << ldo << "\n"
              << "ldi " << ldi << "\n"
              << "rdi " << rdi << "\n"
              << "rdo " << rdo << "\n";
    if (dbg_level > 1) {
      std::string lab = "dc_tri (" + std::to_string(start) + "," + std::to_string(start + n2) +
                        ")(" + std::to_string(start + n2) + "," + std::to_string(end) + ")";
      cdt_draw(lab, *cdt);
    }
  }
  /* Find lower common tangent of L and R. */
  for (;;) {
    if (vert_left_of_symedge(rdi->vert, ldi)) {
      ldi = ldi->next;
    }
    else if (vert_right_of_symedge(ldi->vert, rdi)) {
      rdi = sym(rdi)->rot; /* Previous edge to rdi with same right face. */
    }
    else {
      break;
    }
  }
  if (dbg_level > 0) {
    std::cout << "common lower tangent in between\n"
              << "rdi " << rdi << "\n"
              << "ldi" << ldi << "\n";
  }

  CDTEdge<T> *ebasel = cdt->connect_separate_parts(sym(rdi)->next, ldi);
  SymEdge<T> *basel = &ebasel->symedges[0];
  SymEdge<T> *basel_sym = &ebasel->symedges[1];
  if (dbg_level > 1) {
    std::cout << "basel " << basel;
    cdt_draw("after basel made", *cdt);
  }
  if (ldi->vert == ldo->vert) {
    ldo = basel_sym;
  }
  if (rdi->vert == rdo->vert) {
    rdo = basel;
  }

  /* Merge loop. */
  for (;;) {
    /* Locate the first point lcand->next->vert encountered by rising bubble,
     * and delete L edges out of basel->next->vert that fail the circle test. */
    SymEdge<T> *lcand = basel_sym->rot;
    SymEdge<T> *rcand = basel_sym->next;
    if (dbg_level > 1) {
      std::cout << "\ntop of merge loop\n";
      std::cout << "lcand " << lcand << "\n"
                << "rcand " << rcand << "\n"
                << "basel " << basel << "\n";
    }
    if (dc_tri_valid(lcand, basel, basel_sym)) {
      if (dbg_level > 1) {
        std::cout << "found valid lcand\n";
        std::cout << "  lcand" << lcand << "\n";
      }
      while (filtered_incircle(basel_sym->vert->co,
                               basel->vert->co,
                               lcand->next->vert->co,
                               lcand->rot->next->vert->co) > 0.0)
      {
        if (dbg_level > 1) {
          std::cout << "incircle says to remove lcand\n";
          std::cout << "  lcand" << lcand << "\n";
        }
        SymEdge<T> *t = lcand->rot;
        cdt->delete_edge(sym(lcand));
        lcand = t;
      }
    }
    /* Symmetrically, locate first R point to be hit and delete R edges. */
    if (dc_tri_valid(rcand, basel, basel_sym)) {
      if (dbg_level > 1) {
        std::cout << "found valid rcand\n";
        std::cout << "  rcand" << rcand << "\n";
      }
      while (filtered_incircle(basel_sym->vert->co,
                               basel->vert->co,
                               rcand->next->vert->co,
                               sym(rcand)->next->next->vert->co) > 0.0)
      {
        if (dbg_level > 0) {
          std::cout << "incircle says to remove rcand\n";
          std::cout << "  rcand" << rcand << "\n";
        }
        SymEdge<T> *t = sym(rcand)->next;
        cdt->delete_edge(rcand);
        rcand = t;
      }
    }
    /* If both lcand and rcand are invalid, then basel is the common upper tangent. */
    bool valid_lcand = dc_tri_valid(lcand, basel, basel_sym);
    bool valid_rcand = dc_tri_valid(rcand, basel, basel_sym);
    if (dbg_level > 0) {
      std::cout << "after bubbling up, valid_lcand=" << valid_lcand
                << ", valid_rand=" << valid_rcand << "\n";
      std::cout << "lcand" << lcand << "\n"
                << "rcand" << rcand << "\n";
    }
    if (!valid_lcand && !valid_rcand) {
      break;
    }
    /* The next cross edge to be connected is to either `lcand->next->vert` or `rcand->next->vert`;
     * if both are valid, choose the appropriate one using the #incircle test. */
    if (!valid_lcand ||
        (valid_rcand &&
         filtered_incircle(
             lcand->next->vert->co, lcand->vert->co, rcand->vert->co, rcand->next->vert->co) > 0))
    {
      if (dbg_level > 0) {
        std::cout << "connecting rcand\n";
        std::cout << "  se1=basel_sym" << basel_sym << "\n";
        std::cout << "  se2=rcand->next" << rcand->next << "\n";
      }
      ebasel = cdt->add_diagonal(rcand->next, basel_sym);
    }
    else {
      if (dbg_level > 0) {
        std::cout << "connecting lcand\n";
        std::cout << "  se1=sym(lcand)" << sym(lcand) << "\n";
        std::cout << "  se2=basel_sym->next" << basel_sym->next << "\n";
      }
      ebasel = cdt->add_diagonal(basel_sym->next, sym(lcand));
    }
    basel = &ebasel->symedges[0];
    basel_sym = &ebasel->symedges[1];
    BLI_assert(basel_sym->face == cdt->outer_face);
    if (dbg_level > 2) {
      cdt_draw("after adding new crossedge", *cdt);
    }
  }
  *r_le = ldo;
  *r_re = rdo;
  BLI_assert(sym(ldo)->face == cdt->outer_face && rdo->face == cdt->outer_face);
}

/* Guibas-Stolfi Divide-and_Conquer algorithm. */
template<typename T> void dc_triangulate(CDTArrangement<T> *cdt, Array<SiteInfo<T>> &sites)
{
  /* Compress sites in place to eliminated verts that merge to others. */
  int i = 0;
  int j = 0;
  int nsites = sites.size();
  while (j < nsites) {
    /* Invariant: `sites[0..i-1]` have non-merged verts from `0..(j-1)` in them. */
    sites[i] = sites[j++];
    if (sites[i].v->merge_to_index < 0) {
      i++;
    }
  }
  int n = i;
  if (n == 0) {
    return;
  }
  SymEdge<T> *le;
  SymEdge<T> *re;
  dc_tri(cdt, sites, 0, n, &le, &re);
}

/**
 * Do a Delaunay Triangulation of the points in cdt.verts.
 * This is only a first step in the Constrained Delaunay triangulation,
 * because it doesn't yet deal with the segment constraints.
 * The algorithm used is the Divide & Conquer algorithm from the
 * Guibas-Stolfi "Primitives for the Manipulation of General Subdivision
 * and the Computation of Voronoi Diagrams" paper.
 * The data structure here is similar to but not exactly the same as
 * the quad-edge structure described in that paper.
 * If T is not exact arithmetic, incircle and CCW tests are done using
 * Shewchuk's exact primitives, so that this routine is robust.
 *
 * As a preprocessing step, we want to merge all vertices that the same.
 * This is accomplished by lexicographically
 * sorting the coordinates first (which is needed anyway for the D&C algorithm).
 * The CDTVerts with merge_to_index not equal to -1 are after this regarded
 * as having been merged into the vertex with the corresponding index.
 */
template<typename T> void initial_triangulation(CDTArrangement<T> *cdt)
{
  int n = cdt->verts.size();
  if (n <= 1) {
    return;
  }
  Array<SiteInfo<T>> sites(n);
  for (int i = 0; i < n; ++i) {
    sites[i].v = cdt->verts[i];
    sites[i].orig_index = i;
  }
  std::sort(sites.begin(), sites.end(), site_lexicographic_sort<T>);
  find_site_merges(sites);
  dc_triangulate(cdt, sites);
}

/**
 * Re-triangulates, assuring constrained delaunay condition,
 * the pseudo-polygon that cycles from se.
 * "pseudo" because a vertex may be repeated.
 * See Anglada paper, "An Improved incremental algorithm
 * for constructing restricted Delaunay triangulations".
 */
template<typename T> static void re_delaunay_triangulate(CDTArrangement<T> *cdt, SymEdge<T> *se)
{
  if (se->face == cdt->outer_face || sym(se)->face == cdt->outer_face) {
    return;
  }
  /* `se` is a diagonal just added, and it is base of area to re-triangulate (face on its left). */
  int count = 1;
  for (const SymEdge<T> *ss = se->next; ss != se; ss = ss->next) {
    count++;
  }
  if (count <= 3) {
    return;
  }
  /* First and last are the SymEdges whose verts are first and last off of base,
   * continuing from 'se'. */
  SymEdge<T> *first = se->next->next;
  /* We want to make a triangle with 'se' as base and some other c as 3rd vertex. */
  CDTVert<T> *a = se->vert;
  CDTVert<T> *b = se->next->vert;
  CDTVert<T> *c = first->vert;
  SymEdge<T> *cse = first;
  for (SymEdge<T> *ss = first->next; ss != se; ss = ss->next) {
    CDTVert<T> *v = ss->vert;
    if (filtered_incircle(a->co, b->co, c->co, v->co) > 0) {
      c = v;
      cse = ss;
    }
  }
  /* Add diagonals necessary to make `abc` a triangle. */
  CDTEdge<T> *ebc = nullptr;
  CDTEdge<T> *eca = nullptr;
  if (!exists_edge(b, c)) {
    ebc = cdt->add_diagonal(se->next, cse);
  }
  if (!exists_edge(c, a)) {
    eca = cdt->add_diagonal(cse, se);
  }
  /* Now recurse. */
  if (ebc) {
    re_delaunay_triangulate(cdt, &ebc->symedges[1]);
  }
  if (eca) {
    re_delaunay_triangulate(cdt, &eca->symedges[1]);
  }
}

template<typename T> inline int tri_orient(const SymEdge<T> *t)
{
  return filtered_orient2d(t->vert->co, t->next->vert->co, t->next->next->vert->co);
}

/**
 * The #CrossData class defines either an endpoint or an intermediate point
 * in the path we will take to insert an edge constraint.
 * Each such point will either be
 * (a) a vertex or
 * (b) a fraction lambda (0 < lambda < 1) along some #SymEdge.]
 *
 * In general, lambda=0 indicates case a and lambda != 0 indicates case be.
 * The 'in' edge gives the destination attachment point of a diagonal from the previous crossing,
 * and the 'out' edge gives the origin attachment point of a diagonal to the next crossing.
 * But in some cases, 'in' and 'out' are undefined or not needed, and will be null.
 *
 * For case (a), 'vert' will be the vertex, and lambda will be 0, and 'in' will be the #SymEdge
 * from 'vert' that has as face the one that you go through to get to this vertex. If you go
 * exactly along an edge then we set 'in' to null, since it won't be needed. The first crossing
 * will have 'in' = null. We set 'out' to the #SymEdge that has the face we go through to get to
 * the next crossing, or, if the next crossing is a case (a), then it is the edge that goes to that
 * next vertex. 'out' will be null for the last one.
 *
 * For case (b), vert will be null at first, and later filled in with the created split vertex,
 * and 'in' will be the #SymEdge that we go through, and lambda will be between 0 and 1,
 * the fraction from in's vert to in->next's vert to put the split vertex.
 * 'out' is not needed in this case, since the attachment point will be the sym of the first
 * half of the split edge.
 */
template<typename T> class CrossData {
 public:
  T lambda = T(0);
  CDTVert<T> *vert;
  SymEdge<T> *in;
  SymEdge<T> *out;

  CrossData() : lambda(T(0)), vert(nullptr), in(nullptr), out(nullptr) {}
  CrossData(T l, CDTVert<T> *v, SymEdge<T> *i, SymEdge<T> *o) : lambda(l), vert(v), in(i), out(o)
  {
  }
  CrossData(const CrossData &other)
      : lambda(other.lambda), vert(other.vert), in(other.in), out(other.out)
  {
  }
  CrossData(CrossData &&other) noexcept
      : lambda(std::move(other.lambda)),
        vert(std::move(other.vert)),
        in(std::move(other.in)),
        out(std::move(other.out))
  {
  }
  ~CrossData() = default;
  CrossData &operator=(const CrossData &other)
  {
    if (this != &other) {
      lambda = other.lambda;
      vert = other.vert;
      in = other.in;
      out = other.out;
    }
    return *this;
  }
  CrossData &operator=(CrossData &&other) noexcept
  {
    lambda = std::move(other.lambda);
    vert = std::move(other.vert);
    in = std::move(other.in);
    out = std::move(other.out);
    return *this;
  }
};

template<typename T>
bool get_next_crossing_from_vert(CDT_state<T> *cdt_state,
                                 CrossData<T> *cd,
                                 CrossData<T> *cd_next,
                                 const CDTVert<T> *v2);

/**
 * As part of finding crossings, we found a case where the next crossing goes through vert v.
 * If it came from a previous vert in cd, then cd_out is the edge that leads from that to v.
 * Else cd_out can be null, because it won't be used.
 * Set *cd_next to indicate this. We can set 'in' but not 'out'.  We can set the 'out' of the
 * current cd.
 */
template<typename T>
void fill_crossdata_for_through_vert(CDTVert<T> *v,
                                     SymEdge<T> *cd_out,
                                     CrossData<T> *cd,
                                     CrossData<T> *cd_next)
{
  SymEdge<T> *se;

  cd_next->lambda = T(0);
  cd_next->vert = v;
  cd_next->in = nullptr;
  cd_next->out = nullptr;
  if (cd->lambda == 0) {
    cd->out = cd_out;
  }
  else {
    /* One of the edges in the triangle with edge sym(cd->in) contains v. */
    se = sym(cd->in);
    if (se->vert != v) {
      se = se->next;
      if (se->vert != v) {
        se = se->next;
      }
    }
    BLI_assert(se->vert == v);
    cd_next->in = se;
  }
}

/**
 * As part of finding crossings, we found a case where orient tests say that the next crossing
 * is on the #SymEdge t, while intersecting with the ray from \a curco to \a v2.
 * Find the intersection point and fill in the #CrossData for that point.
 * It may turn out that when doing the intersection, we get an answer that says that
 * this case is better handled as through-vertex case instead, so we may do that.
 * In the latter case, we want to avoid a situation where the current crossing is on an edge
 * and the next will be an endpoint of the same edge. When that happens, we "rewrite history"
 * and turn the current crossing into a vert one, and then extend from there.
 *
 * We cannot fill cd_next's 'out' edge yet, in the case that the next one ends up being a vert
 * case. We need to fill in cd's 'out' edge if it was a vert case.
 */
template<typename T>
void fill_crossdata_for_intersect(const FatCo<T> &curco,
                                  const CDTVert<T> *v2,
                                  SymEdge<T> *t,
                                  CrossData<T> *cd,
                                  CrossData<T> *cd_next,
                                  const T epsilon)
{
  CDTVert<T> *va = t->vert;
  CDTVert<T> *vb = t->next->vert;
  CDTVert<T> *vc = t->next->next->vert;
  SymEdge<T> *se_vcvb = sym(t->next);
  SymEdge<T> *se_vcva = t->next->next;
  BLI_assert(se_vcva->vert == vc && se_vcva->next->vert == va);
  BLI_assert(se_vcvb->vert == vc && se_vcvb->next->vert == vb);
  UNUSED_VARS_NDEBUG(vc);
  auto isect = isect_seg_seg(va->co.exact, vb->co.exact, curco.exact, v2->co.exact);
  T &lambda = isect.lambda;
  switch (isect.kind) {
    case isect_result<VecBase<T, 2>>::LINE_LINE_CROSS: {
#ifdef WITH_GMP
      if (!std::is_same_v<T, mpq_class>)
#else
      if (true)
#endif
      {
        double len_ab = distance(va->co.approx, vb->co.approx);
        if (lambda * len_ab <= epsilon) {
          fill_crossdata_for_through_vert(va, se_vcva, cd, cd_next);
        }
        else if ((1 - lambda) * len_ab <= epsilon) {
          fill_crossdata_for_through_vert(vb, se_vcvb, cd, cd_next);
        }
        else {
          *cd_next = CrossData<T>(lambda, nullptr, t, nullptr);
          if (cd->lambda == 0) {
            cd->out = se_vcva;
          }
        }
      }
      else {
        *cd_next = CrossData<T>(lambda, nullptr, t, nullptr);
        if (cd->lambda == 0) {
          cd->out = se_vcva;
        }
      }
      break;
    }
    case isect_result<VecBase<T, 2>>::LINE_LINE_EXACT: {
      if (lambda == 0) {
        fill_crossdata_for_through_vert(va, se_vcva, cd, cd_next);
      }
      else if (lambda == 1) {
        fill_crossdata_for_through_vert(vb, se_vcvb, cd, cd_next);
      }
      else {
        *cd_next = CrossData<T>(lambda, nullptr, t, nullptr);
        if (cd->lambda == 0) {
          cd->out = se_vcva;
        }
      }
      break;
    }
    case isect_result<VecBase<T, 2>>::LINE_LINE_NONE: {
#ifdef WITH_GMP
      if (std::is_same_v<T, mpq_class>) {
        BLI_assert(false);
      }
#endif
      /* It should be very near one end or other of segment. */
      const T middle_lambda = 0.5;
      if (lambda <= middle_lambda) {
        fill_crossdata_for_through_vert(va, se_vcva, cd, cd_next);
      }
      else {
        fill_crossdata_for_through_vert(vb, se_vcvb, cd, cd_next);
      }
      break;
    }
    case isect_result<VecBase<T, 2>>::LINE_LINE_COLINEAR: {
      if (distance_squared(va->co.approx, v2->co.approx) <=
          distance_squared(vb->co.approx, v2->co.approx))
      {
        fill_crossdata_for_through_vert(va, se_vcva, cd, cd_next);
      }
      else {
        fill_crossdata_for_through_vert(vb, se_vcvb, cd, cd_next);
      }
      break;
    }
  }
}  // namespace blender::meshintersect

/**
 * As part of finding the crossings of a ray to v2, find the next crossing after 'cd', assuming
 * 'cd' represents a crossing that goes through a vertex.
 *
 * We do a rotational scan around cd's vertex, looking for the triangle where the ray from cd->vert
 * to v2 goes between the two arms from cd->vert, or where it goes along one of the edges.
 */
template<typename T>
bool get_next_crossing_from_vert(CDT_state<T> *cdt_state,
                                 CrossData<T> *cd,
                                 CrossData<T> *cd_next,
                                 const CDTVert<T> *v2)
{
  SymEdge<T> *tstart = cd->vert->symedge;
  SymEdge<T> *t = tstart;
  CDTVert<T> *vcur = cd->vert;
  bool ok = false;
  do {
    /* The ray from `vcur` to v2 has to go either between two successive
     * edges around `vcur` or exactly along them. This time through the
     * loop, check to see if the ray goes along `vcur-va`
     * or between `vcur-va` and `vcur-vb`, where va is the end of t
     * and vb is the next vertex (on the next rot edge around vcur, but
     * should also be the next vert of triangle starting with `vcur-va`. */
    if (t->face != cdt_state->cdt.outer_face && tri_orient(t) < 0) {
      BLI_assert(false); /* Shouldn't happen. */
    }
    CDTVert<T> *va = t->next->vert;
    CDTVert<T> *vb = t->next->next->vert;
    int orient1 = filtered_orient2d(t->vert->co, va->co, v2->co);
    if (orient1 == 0 && in_line<T>(vcur->co, va->co, v2->co)) {
      fill_crossdata_for_through_vert(va, t, cd, cd_next);
      ok = true;
      break;
    }
    if (t->face != cdt_state->cdt.outer_face) {
      int orient2 = filtered_orient2d(vcur->co, vb->co, v2->co);
      /* Don't handle orient2 == 0 case here: next rotation will get it. */
      if (orient1 > 0 && orient2 < 0) {
        /* Segment intersection. */
        t = t->next;
        fill_crossdata_for_intersect(vcur->co, v2, t, cd, cd_next, cdt_state->epsilon);
        ok = true;
        break;
      }
    }
  } while ((t = t->rot) != tstart);
  return ok;
}

/**
 * As part of finding the crossings of a ray to `v2`, find the next crossing after 'cd', assuming
 * 'cd' represents a crossing that goes through a an edge, not at either end of that edge.
 *
 * We have the triangle `vb-va-vc`, where `va` and vb are the split edge and `vc` is the third
 * vertex on that new side of the edge (should be closer to `v2`).
 * The next crossing should be through `vc` or intersecting `vb-vc` or `va-vc`.
 */
template<typename T>
void get_next_crossing_from_edge(CrossData<T> *cd,
                                 CrossData<T> *cd_next,
                                 const CDTVert<T> *v2,
                                 const T epsilon)
{
  CDTVert<T> *va = cd->in->vert;
  CDTVert<T> *vb = cd->in->next->vert;
  VecBase<T, 2> curco = interpolate(va->co.exact, vb->co.exact, cd->lambda);
  FatCo<T> fat_curco(curco);
  SymEdge<T> *se_ac = sym(cd->in)->next;
  CDTVert<T> *vc = se_ac->next->vert;
  int orient = filtered_orient2d(fat_curco, v2->co, vc->co);
  if (orient < 0) {
    fill_crossdata_for_intersect<T>(fat_curco, v2, se_ac->next, cd, cd_next, epsilon);
  }
  else if (orient > 0.0) {
    fill_crossdata_for_intersect(fat_curco, v2, se_ac, cd, cd_next, epsilon);
  }
  else {
    *cd_next = CrossData<T>{0.0, vc, se_ac->next, nullptr};
  }
}

template<typename T> void dump_crossings(const Span<CrossData<T>> crossings)
{
  std::cout << "CROSSINGS\n";
  for (int i = 0; i < crossings.size(); ++i) {
    std::cout << i << ": ";
    const CrossData<T> &cd = crossings[i];
    if (cd.lambda == 0) {
      std::cout << "v" << cd.vert->index;
    }
    else {
      std::cout << "lambda=" << cd.lambda;
    }
    if (cd.in != nullptr) {
      std::cout << " in=" << short_se_dump(cd.in);
      std::cout << " out=" << short_se_dump(cd.out);
    }
    std::cout << "\n";
  }
}

/**
 * Add a constrained edge between v1 and v2 to cdt structure.
 * This may result in a number of #CDTEdges created, due to intersections
 * and partial overlaps with existing cdt vertices and edges.
 * Each created #CDTEdge will have input_id added to its input_ids list.
 *
 * If \a r_edges is not null, the #CDTEdges generated or found that go from
 * v1 to v2 are put into that linked list, in order.
 *
 * Assumes that #blender_constrained_delaunay_get_output has not been called yet.
 */
template<typename T>
void add_edge_constraint(
    CDT_state<T> *cdt_state, CDTVert<T> *v1, CDTVert<T> *v2, int input_id, LinkNode **r_edges)
{
  constexpr int dbg_level = 0;
  if (dbg_level > 0) {
    std::cout << "\nADD EDGE CONSTRAINT\n" << vertname(v1) << " " << vertname(v2) << "\n";
  }
  LinkNodePair edge_list = {nullptr, nullptr};

  if (r_edges) {
    *r_edges = nullptr;
  }

  /*
   * Handle two special cases first:
   * 1) The two end vertices are the same (can happen because of merging).
   * 2) There is already an edge between v1 and v2.
   */
  if (v1 == v2) {
    return;
  }
  SymEdge<T> *t = find_symedge_between_verts(v1, v2);
  if (t != nullptr) {
    /* Segment already there. */
    add_to_input_ids(t->edge->input_ids, input_id);
    if (r_edges != nullptr) {
      BLI_linklist_append(&edge_list, t->edge);
      *r_edges = edge_list.list;
    }
    return;
  }

  /*
   * Fill crossings array with CrossData points for intersection path from v1 to v2.
   *
   * At every point, the crossings array has the path so far, except that
   * the `.out` field of the last element of it may not be known yet -- if that
   * last element is a vertex, then we won't know the output edge until we
   * find the next crossing.
   *
   * To protect against infinite loops, we keep track of which vertices
   * we have visited by setting their visit_index to a new visit epoch.
   *
   * We check a special case first: where the segment is already there in
   * one hop. Saves a bunch of orient2d tests in that common case.
   */
  int visit = ++cdt_state->visit_count;
  Vector<CrossData<T>, 128> crossings;
  crossings.append(CrossData<T>(T(0), v1, nullptr, nullptr));
  int n;
  while (!((n = crossings.size()) > 0 && crossings[n - 1].vert == v2)) {
    crossings.append(CrossData<T>());
    CrossData<T> *cd = &crossings[n - 1];
    CrossData<T> *cd_next = &crossings[n];
    bool ok;
    if (crossings[n - 1].lambda == 0) {
      ok = get_next_crossing_from_vert(cdt_state, cd, cd_next, v2);
    }
    else {
      get_next_crossing_from_edge<T>(cd, cd_next, v2, cdt_state->epsilon);
      ok = true;
    }
    constexpr int unreasonably_large_crossings = 100000;
    if (!ok || crossings.size() == unreasonably_large_crossings) {
      /* Shouldn't happen but if does, just bail out. */
      BLI_assert(false);
      return;
    }
    if (crossings[n].lambda == 0) {
      if (crossings[n].vert->visit_index == visit) {
        /* Shouldn't happen but if it does, just bail out. */
        BLI_assert(false);
        return;
      }
      crossings[n].vert->visit_index = visit;
    }
  }

  if (dbg_level > 0) {
    dump_crossings<T>(crossings);
  }

  /*
   * Post-process crossings.
   * Some crossings may have an intersection crossing followed
   * by a vertex crossing that is on the same edge that was just
   * intersected. We prefer to go directly from the previous
   * crossing directly to the vertex. This may chain backwards.
   *
   * This loop marks certain crossings as "deleted", by setting
   * their lambdas to -1.0.
   */
  int ncrossings = crossings.size();
  for (int i = 2; i < ncrossings; ++i) {
    CrossData<T> *cd = &crossings[i];
    if (cd->lambda == 0.0) {
      CDTVert<T> *v = cd->vert;
      int j;
      CrossData<T> *cd_prev;
      for (j = i - 1; j > 0; --j) {
        cd_prev = &crossings[j];
        if ((cd_prev->lambda == 0.0 && cd_prev->vert != v) ||
            (cd_prev->lambda != 0.0 && cd_prev->in->vert != v && cd_prev->in->next->vert != v))
        {
          break;
        }
        cd_prev->lambda = -1.0; /* Mark cd_prev as 'deleted'. */
      }
      if (j < i - 1) {
        /* Some crossings were deleted. Fix the in and out edges across gap. */
        cd_prev = &crossings[j];
        SymEdge<T> *se;
        if (cd_prev->lambda == 0.0) {
          se = find_symedge_between_verts(cd_prev->vert, v);
          if (se == nullptr) {
            return;
          }
          cd_prev->out = se;
          cd->in = nullptr;
        }
        else {
          se = find_symedge_with_face(v, sym(cd_prev->in)->face);
          if (se == nullptr) {
            return;
          }
          cd->in = se;
        }
      }
    }
  }

  /*
   * Insert all intersection points on constrained edges.
   */
  for (int i = 0; i < ncrossings; ++i) {
    CrossData<T> *cd = &crossings[i];
    if (cd->lambda != 0.0 && cd->lambda != -1.0 && is_constrained_edge(cd->in->edge)) {
      CDTEdge<T> *edge = cdt_state->cdt.split_edge(cd->in, cd->lambda);
      cd->vert = edge->symedges[0].vert;
    }
  }

  /*
   * Remove any crossed, non-intersected edges.
   */
  for (int i = 0; i < ncrossings; ++i) {
    CrossData<T> *cd = &crossings[i];
    if (cd->lambda != 0.0 && cd->lambda != -1.0 && !is_constrained_edge(cd->in->edge)) {
      cdt_state->cdt.delete_edge(cd->in);
    }
  }

  /*
   * Insert segments for v1->v2.
   */
  SymEdge<T> *tstart = crossings[0].out;
  for (int i = 1; i < ncrossings; i++) {
    CrossData<T> *cd = &crossings[i];
    if (cd->lambda == -1.0) {
      continue; /* This crossing was deleted. */
    }
    t = nullptr;
    SymEdge<T> *tnext = t;
    CDTEdge<T> *edge;
    if (cd->lambda != 0.0) {
      if (is_constrained_edge(cd->in->edge)) {
        t = cd->vert->symedge;
        tnext = sym(t)->next;
      }
    }
    else if (cd->lambda == 0.0) {
      t = cd->in;
      tnext = cd->out;
      if (t == nullptr) {
        /* Previous non-deleted crossing must also have been a vert, and segment should exist. */
        int j;
        CrossData<T> *cd_prev;
        for (j = i - 1; j >= 0; j--) {
          cd_prev = &crossings[j];
          if (cd_prev->lambda != -1.0) {
            break;
          }
        }
        BLI_assert(cd_prev->lambda == 0.0);
        BLI_assert(cd_prev->out->next->vert == cd->vert);
        edge = cd_prev->out->edge;
        add_to_input_ids(edge->input_ids, input_id);
        if (r_edges != nullptr) {
          BLI_linklist_append(&edge_list, edge);
        }
      }
    }
    if (t != nullptr) {
      if (tstart->next->vert == t->vert) {
        edge = tstart->edge;
      }
      else {
        edge = cdt_state->cdt.add_diagonal(tstart, t);
      }
      add_to_input_ids(edge->input_ids, input_id);
      if (r_edges != nullptr) {
        BLI_linklist_append(&edge_list, edge);
      }
      /* Now re-triangulate upper and lower gaps. */
      re_delaunay_triangulate(&cdt_state->cdt, &edge->symedges[0]);
      re_delaunay_triangulate(&cdt_state->cdt, &edge->symedges[1]);
    }
    if (i < ncrossings - 1) {
      if (tnext != nullptr) {
        tstart = tnext;
      }
    }
  }

  if (r_edges) {
    *r_edges = edge_list.list;
  }
}

/**
 * Incrementally add edge input edge as a constraint. This may cause the graph structure
 * to change, in cases where the constraints intersect existing edges.
 * The code will ensure that #CDTEdge's created will have ids that tie them back
 * to the original edge constraint index.
 */
template<typename T> void add_edge_constraints(CDT_state<T> *cdt_state, const CDT_input<T> &input)
{
  int ne = input.edge.size();
  int nv = input.vert.size();
  for (int i = 0; i < ne; i++) {
    int iv1 = input.edge[i].first;
    int iv2 = input.edge[i].second;
    if (iv1 < 0 || iv1 >= nv || iv2 < 0 || iv2 >= nv) {
      /* Ignore invalid indices in edges. */
      continue;
    }
    CDTVert<T> *v1 = cdt_state->cdt.get_vert_resolve_merge(iv1);
    CDTVert<T> *v2 = cdt_state->cdt.get_vert_resolve_merge(iv2);
    int id = cdt_state->need_ids ? i : 0;
    add_edge_constraint(cdt_state, v1, v2, id, nullptr);
  }
  cdt_state->face_edge_offset = ne;
}

/**
 * Add face_id to the input_ids lists of all #CDTFace's on the interior of the input face with that
 * id. face_symedge is on edge of the boundary of the input face, with assumption that interior is
 * on the left of that #SymEdge.
 *
 * The algorithm is: starting from the #CDTFace for face_symedge, add the face_id and then
 * process all adjacent faces where the adjacency isn't across an edge that was a constraint added
 * for the boundary of the input face.
 * fedge_start..fedge_end is the inclusive range of edge input ids that are for the given face.
 *
 * NOTE: if the input face is not CCW oriented, we'll be labeling the outside, not the inside.
 * Note 2: if the boundary has self-crossings, this method will arbitrarily pick one of the
 * contiguous set of faces enclosed by parts of the boundary, leaving the other such un-tagged.
 * This may be a feature instead of a bug if the first contiguous section is most of the face and
 * the others are tiny self-crossing triangles at some parts of the boundary.
 * On the other hand, if decide we want to handle these in full generality, then will need a more
 * complicated algorithm (using "inside" tests and a parity rule) to decide on the interior.
 */
template<typename T>
void add_face_ids(
    CDT_state<T> *cdt_state, SymEdge<T> *face_symedge, int face_id, int fedge_start, int fedge_end)
{
  /* Can't loop forever since eventually would visit every face. */
  cdt_state->visit_count++;
  int visit = cdt_state->visit_count;
  Vector<SymEdge<T> *> stack;
  stack.append(face_symedge);
  while (!stack.is_empty()) {
    SymEdge<T> *se = stack.pop_last();
    CDTFace<T> *face = se->face;
    if (face->visit_index == visit) {
      continue;
    }
    face->visit_index = visit;
    add_to_input_ids(face->input_ids, face_id);
    SymEdge<T> *se_start = se;
    for (se = se->next; se != se_start; se = se->next) {
      if (!id_range_in_list(se->edge->input_ids, fedge_start, fedge_end)) {
        SymEdge<T> *se_sym = sym(se);
        CDTFace<T> *face_other = se_sym->face;
        if (face_other->visit_index != visit) {
          stack.append(se_sym);
        }
      }
    }
  }
}

/* Return a power of 10 that is greater than or equal to x. */
static int power_of_10_greater_equal_to(int x)
{
  if (x <= 0) {
    return 1;
  }
  int ans = 1;
  BLI_assert(x < std::numeric_limits<int>::max() / 10);
  while (ans < x) {
    ans *= 10;
  }
  return ans;
}

/**
 * Incrementally each edge of each input face as an edge constraint.
 * The code will ensure that the #CDTEdge's created will have ids that tie them
 * back to the original face edge (using a numbering system for those edges
 * that starts with cdt->face_edge_offset, and continues with the edges in
 * order around each face in turn. And then the next face starts at
 * cdt->face_edge_offset beyond the start for the previous face.
 * Return the number of faces added, which may be less than input.face.size()
 * in the case that some faces have less than 3 sides.
 */
template<typename T>
int add_face_constraints(CDT_state<T> *cdt_state,
                         const CDT_input<T> &input,
                         CDT_output_type output_type)
{
  int nv = input.vert.size();
  const Span<Vector<int>> input_faces = input.face;
  SymEdge<T> *face_symedge0 = nullptr;
  CDTArrangement<T> *cdt = &cdt_state->cdt;

  int maxflen = 0;
  for (const int f : input_faces.index_range()) {
    maxflen = std::max<int>(maxflen, input_faces[f].size());
  }
  /* For convenience in debugging, make face_edge_offset be a power of 10. */
  cdt_state->face_edge_offset = power_of_10_greater_equal_to(
      std::max(maxflen, cdt_state->face_edge_offset));
  /* The original_edge encoding scheme doesn't work if the following is false.
   * If we really have that many faces and that large a max face length that when multiplied
   * together the are >= INT_MAX, then the Delaunay calculation will take unreasonably long anyway.
   */
  BLI_assert(std::numeric_limits<int>::max() / cdt_state->face_edge_offset > input_faces.size());
  int faces_added = 0;
  for (const int f : input_faces.index_range()) {
    const Span<int> face = input_faces[f];
    if (face.size() <= 2) {
      /* Ignore faces with fewer than 3 vertices. */
      continue;
    }
    int fedge_start = (f + 1) * cdt_state->face_edge_offset;
    for (const int i : face.index_range()) {
      int face_edge_id = fedge_start + i;
      int iv1 = face[i];
      int iv2 = face[(i + 1) % face.size()];
      if (iv1 < 0 || iv1 >= nv || iv2 < 0 || iv2 >= nv) {
        /* Ignore face edges with invalid vertices. */
        continue;
      }
      ++faces_added;
      CDTVert<T> *v1 = cdt->get_vert_resolve_merge(iv1);
      CDTVert<T> *v2 = cdt->get_vert_resolve_merge(iv2);
      LinkNode *edge_list;
      int id = cdt_state->need_ids ? face_edge_id : 0;
      add_edge_constraint(cdt_state, v1, v2, id, &edge_list);
      /* Set a new face_symedge0 each time since earlier ones may not
       * survive later symedge splits. Really, just want the one when
       * `i == face.size() - 1`, but this code guards against that one somehow being null. */
      if (edge_list != nullptr) {
        CDTEdge<T> *face_edge = static_cast<CDTEdge<T> *>(edge_list->link);
        face_symedge0 = &face_edge->symedges[0];
        if (face_symedge0->vert != v1) {
          face_symedge0 = &face_edge->symedges[1];
          BLI_assert(face_symedge0->vert == v1);
        }
      }
      BLI_linklist_free(edge_list, nullptr);
    }
    int fedge_end = fedge_start + face.size() - 1;
    if (face_symedge0 != nullptr) {
      /* We need to propagate face ids to all faces that represent #f, if #need_ids.
       * Even if `need_ids == false`, we need to propagate at least the fact that
       * the face ids set would be non-empty if the output type is one of the ones
       * making valid BMesh faces. */
      int id = cdt_state->need_ids ? f : 0;
      add_face_ids(cdt_state, face_symedge0, id, fedge_start, fedge_end);
      if (cdt_state->need_ids ||
          ELEM(output_type, CDT_CONSTRAINTS_VALID_BMESH, CDT_CONSTRAINTS_VALID_BMESH_WITH_HOLES))
      {
        add_face_ids(cdt_state, face_symedge0, f, fedge_start, fedge_end);
      }
    }
  }
  return faces_added;
}

/* Delete_edge but try not to mess up outer face.
 * Also faces have symedges now, so make sure not
 * to mess those up either. */
template<typename T> void dissolve_symedge(CDT_state<T> *cdt_state, SymEdge<T> *se)
{
  CDTArrangement<T> *cdt = &cdt_state->cdt;
  SymEdge<T> *symse = sym(se);
  if (symse->face == cdt->outer_face) {
    se = sym(se);
    symse = sym(se);
  }
  if (ELEM(cdt->outer_face->symedge, se, symse)) {
    /* Advancing by 2 to get past possible 'sym(se)'. */
    if (se->next->next == se) {
      cdt->outer_face->symedge = nullptr;
    }
    else {
      cdt->outer_face->symedge = se->next->next;
    }
  }
  else {
    if (se->face->symedge == se) {
      se->face->symedge = se->next;
    }
    if (symse->face->symedge == symse) {
      symse->face->symedge = symse->next;
    }
  }
  cdt->delete_edge(se);
}

/**
 * Remove all non-constraint edges.
 */
template<typename T> void remove_non_constraint_edges(CDT_state<T> *cdt_state)
{
  for (CDTEdge<T> *e : cdt_state->cdt.edges) {
    SymEdge<T> *se = &e->symedges[0];
    if (!is_deleted_edge(e) && !is_constrained_edge(e)) {
      dissolve_symedge(cdt_state, se);
    }
  }
}

/*
 * Remove the non-constraint edges, but leave enough of them so that all of the
 * faces that would be #BMesh faces (that is, the faces that have some input representative)
 * are valid: they can't have holes, they can't have repeated vertices, and they can't have
 * repeated edges.
 *
 * Not essential, but to make the result look more aesthetically nice,
 * remove the edges in order of decreasing length, so that it is more likely that the
 * final remaining support edges are short, and therefore likely to make a fairly
 * direct path from an outer face to an inner hole face.
 */

/**
 * For sorting edges by decreasing length (squared).
 */
template<typename T> struct EdgeToSort {
  double len_squared = 0.0;
  CDTEdge<T> *e{nullptr};

  EdgeToSort() = default;
  EdgeToSort(const EdgeToSort &other) : len_squared(other.len_squared), e(other.e) {}
  EdgeToSort(EdgeToSort &&other) noexcept : len_squared(std::move(other.len_squared)), e(other.e)
  {
  }
  ~EdgeToSort() = default;
  EdgeToSort &operator=(const EdgeToSort &other)
  {
    if (this != &other) {
      len_squared = other.len_squared;
      e = other.e;
    }
    return *this;
  }
  EdgeToSort &operator=(EdgeToSort &&other)
  {
    len_squared = std::move(other.len_squared);
    e = other.e;
    return *this;
  }
};

template<typename T> void remove_non_constraint_edges_leave_valid_bmesh(CDT_state<T> *cdt_state)
{
  CDTArrangement<T> *cdt = &cdt_state->cdt;
  size_t nedges = cdt->edges.size();
  if (nedges == 0) {
    return;
  }
  Vector<EdgeToSort<T>> dissolvable_edges;
  dissolvable_edges.reserve(cdt->edges.size());
  int i = 0;
  for (CDTEdge<T> *e : cdt->edges) {
    if (!is_deleted_edge(e) && !is_constrained_edge(e)) {
      dissolvable_edges.append(EdgeToSort<T>());
      dissolvable_edges[i].e = e;
      const double2 &co1 = e->symedges[0].vert->co.approx;
      const double2 &co2 = e->symedges[1].vert->co.approx;
      dissolvable_edges[i].len_squared = distance_squared(co1, co2);
      i++;
    }
  }
  std::sort(dissolvable_edges.begin(),
            dissolvable_edges.end(),
            [](const EdgeToSort<T> &a, const EdgeToSort<T> &b) -> bool {
              return (a.len_squared < b.len_squared);
            });
  for (EdgeToSort<T> &ets : dissolvable_edges) {
    CDTEdge<T> *e = ets.e;
    SymEdge<T> *se = &e->symedges[0];
    bool dissolve = true;
    CDTFace<T> *fleft = se->face;
    CDTFace<T> *fright = sym(se)->face;
    if (fleft != cdt->outer_face && fright != cdt->outer_face &&
        (fleft->input_ids.size() > 0 || fright->input_ids.size() > 0))
    {
      /* Is there another #SymEdge with same left and right faces?
       * Or is there a vertex not part of e touching the same left and right faces? */
      for (SymEdge<T> *se2 = se->next; dissolve && se2 != se; se2 = se2->next) {
        if (sym(se2)->face == fright ||
            (se2->vert != se->next->vert && vert_touches_face(se2->vert, fright)))
        {
          dissolve = false;
        }
      }
    }

    if (dissolve) {
      dissolve_symedge(cdt_state, se);
    }
  }
}

template<typename T> void remove_outer_edges_until_constraints(CDT_state<T> *cdt_state)
{
  int visit = ++cdt_state->visit_count;

  cdt_state->cdt.outer_face->visit_index = visit;
  /* Walk around outer face, adding faces on other side of dissolvable edges to stack. */
  Vector<CDTFace<T> *> fstack;
  SymEdge<T> *se_start = cdt_state->cdt.outer_face->symedge;
  SymEdge<T> *se = se_start;
  do {
    if (!is_constrained_edge(se->edge)) {
      CDTFace<T> *fsym = sym(se)->face;
      if (fsym->visit_index != visit) {
        fstack.append(fsym);
      }
    }
  } while ((se = se->next) != se_start);

  while (!fstack.is_empty()) {
    LinkNode *to_dissolve = nullptr;
    bool dissolvable;
    CDTFace<T> *f = fstack.pop_last();
    if (f->visit_index == visit) {
      continue;
    }
    BLI_assert(f != cdt_state->cdt.outer_face);
    f->visit_index = visit;
    se_start = se = f->symedge;
    do {
      dissolvable = !is_constrained_edge(se->edge);
      if (dissolvable) {
        CDTFace<T> *fsym = sym(se)->face;
        if (fsym->visit_index != visit) {
          fstack.append(fsym);
        }
        else {
          BLI_linklist_prepend(&to_dissolve, se);
        }
      }
      se = se->next;
    } while (se != se_start);
    while (to_dissolve != nullptr) {
      se = static_cast<SymEdge<T> *>(BLI_linklist_pop(&to_dissolve));
      if (se->next != nullptr) {
        dissolve_symedge(cdt_state, se);
      }
    }
  }
}

template<typename T> void remove_faces_in_holes(CDT_state<T> *cdt_state)
{
  CDTArrangement<T> *cdt = &cdt_state->cdt;
  for (int i : cdt->faces.index_range()) {
    CDTFace<T> *f = cdt->faces[i];
    if (!f->deleted && f->hole) {
      f->deleted = true;
      SymEdge<T> *se = f->symedge;
      SymEdge<T> *se_start = se;
      SymEdge<T> *se_next = nullptr;
      do {
        BLI_assert(se != nullptr);
        se_next = se->next; /* In case we delete this edge. */
        if (se->edge && !is_constrained_edge(se->edge)) {
          /* Invalidate one half of this edge. The other will be, or has already been handled
           * with the adjacent triangle is processed: it should be part of the same hole. */
          se->next = nullptr;
        }
        se = se_next;
      } while (se != se_start);
    }
  }
}

/**
 * Set the hole member of each CDTFace to true for each face that is detected to be part of a
 * hole. A hole face is define as one for which, when a ray is shot from a point inside the face
 * to infinity, it crosses an even number of constraint edges. We'll choose a ray direction that
 * is extremely unlikely to exactly superimpose some edge, so avoiding the need to be careful
 * about such overlaps.
 *
 * To improve performance, we gather together faces that should have the same winding number, and
 * only shoot rays once.
 */
template<typename T> void detect_holes(CDT_state<T> *cdt_state)
{
  CDTArrangement<T> *cdt = &cdt_state->cdt;

  /* Make it so that each face with the same visit_index is connected through a path of
   * non-constraint edges. */
  Vector<CDTFace<T> *> fstack;
  Vector<CDTFace<T> *> region_rep_face;
  for (int i : cdt->faces.index_range()) {
    cdt->faces[i]->visit_index = -1;
  }
  int cur_region = -1;
  cdt->outer_face->visit_index = -2; /* Don't visit this one. */
  for (int i : cdt->faces.index_range()) {
    CDTFace<T> *f = cdt->faces[i];
    if (!f->deleted && f->symedge && f->visit_index == -1) {
      fstack.append(f);
      ++cur_region;
      region_rep_face.append(f);
      while (!fstack.is_empty()) {
        CDTFace<T> *f = fstack.pop_last();
        if (f->visit_index != -1) {
          continue;
        }
        f->visit_index = cur_region;
        SymEdge<T> *se_start = f->symedge;
        SymEdge<T> *se = se_start;
        do {
          if (se->edge && !is_constrained_edge(se->edge)) {
            CDTFace<T> *fsym = sym(se)->face;
            if (fsym && !fsym->deleted && fsym->visit_index == -1) {
              fstack.append(fsym);
            }
          }
          se = se->next;
        } while (se != se_start);
      }
    }
  }
  cdt_state->visit_count = ++cur_region; /* Good start for next use of visit_count. */

  /* Now get hole status for each region_rep_face. */

  /* Pick a ray end almost certain to be outside everything and in direction
   * that is unlikely to hit a vertex or overlap an edge exactly. */
  FatCo<T> ray_end;
  ray_end.exact = VecBase<T, 2>(123456, 654321);
  for (int i : region_rep_face.index_range()) {
    CDTFace<T> *f = region_rep_face[i];
    FatCo<T> mid;
    mid.exact[0] = (f->symedge->vert->co.exact[0] + f->symedge->next->vert->co.exact[0] +
                    f->symedge->next->next->vert->co.exact[0]) /
                   3;
    mid.exact[1] = (f->symedge->vert->co.exact[1] + f->symedge->next->vert->co.exact[1] +
                    f->symedge->next->next->vert->co.exact[1]) /
                   3;
    std::atomic<int> hits = 0;
    /* TODO: Use CDT data structure here to greatly reduce search for intersections! */
    threading::parallel_for(cdt->edges.index_range(), 256, [&](IndexRange range) {
      for (const int i : range) {
        const CDTEdge<T> *e = cdt->edges[i];
        if (!is_deleted_edge(e) && is_constrained_edge(e)) {
          if (e->symedges[0].face->visit_index == e->symedges[1].face->visit_index) {
            continue; /* Don't count hits on edges between faces in same region. */
          }
          auto isect = isect_seg_seg(ray_end.exact,
                                     mid.exact,
                                     e->symedges[0].vert->co.exact,
                                     e->symedges[1].vert->co.exact);
          switch (isect.kind) {
            case isect_result<VecBase<T, 2>>::LINE_LINE_CROSS: {
              hits++;
              break;
            }
            case isect_result<VecBase<T, 2>>::LINE_LINE_EXACT:
            case isect_result<VecBase<T, 2>>::LINE_LINE_NONE:
            case isect_result<VecBase<T, 2>>::LINE_LINE_COLINEAR:
              break;
          }
        }
      }
    });
    f->hole = (hits.load() % 2) == 0;
  }

  /* Finally, propagate hole status to all holes of a region. */
  for (int i : cdt->faces.index_range()) {
    CDTFace<T> *f = cdt->faces[i];
    int region = f->visit_index;
    if (region < 0) {
      continue;
    }
    CDTFace<T> *f_region_rep = region_rep_face[region];
    if (i >= 0) {
      f->hole = f_region_rep->hole;
    }
  }
}

/**
 * Remove edges and merge faces to get desired output, as per options.
 * \note the cdt cannot be further changed after this.
 */
template<typename T>
void prepare_cdt_for_output(CDT_state<T> *cdt_state, const CDT_output_type output_type)
{
  CDTArrangement<T> *cdt = &cdt_state->cdt;
  if (cdt->edges.is_empty()) {
    return;
  }

  /* Make sure all non-deleted faces have a symedge. */
  for (CDTEdge<T> *e : cdt->edges) {
    if (!is_deleted_edge(e)) {
      if (e->symedges[0].face->symedge == nullptr) {
        e->symedges[0].face->symedge = &e->symedges[0];
      }
      if (e->symedges[1].face->symedge == nullptr) {
        e->symedges[1].face->symedge = &e->symedges[1];
      }
    }
  }

  bool need_holes = output_type == CDT_INSIDE_WITH_HOLES ||
                    output_type == CDT_CONSTRAINTS_VALID_BMESH_WITH_HOLES;

  if (need_holes) {
    detect_holes(cdt_state);
  }

  if (output_type == CDT_CONSTRAINTS) {
    remove_non_constraint_edges(cdt_state);
  }
  else if (output_type == CDT_CONSTRAINTS_VALID_BMESH) {
    remove_non_constraint_edges_leave_valid_bmesh(cdt_state);
  }
  else if (output_type == CDT_INSIDE) {
    remove_outer_edges_until_constraints(cdt_state);
  }
  else if (output_type == CDT_INSIDE_WITH_HOLES) {
    remove_outer_edges_until_constraints(cdt_state);
    remove_faces_in_holes(cdt_state);
  }
  else if (output_type == CDT_CONSTRAINTS_VALID_BMESH_WITH_HOLES) {
    remove_outer_edges_until_constraints(cdt_state);
    remove_non_constraint_edges_leave_valid_bmesh(cdt_state);
    remove_faces_in_holes(cdt_state);
  }
}

template<typename T>
CDT_result<T> get_cdt_output(CDT_state<T> *cdt_state,
                             const CDT_input<T> /*input*/,
                             CDT_output_type output_type)
{
  CDT_output_type oty = output_type;
  prepare_cdt_for_output(cdt_state, oty);
  CDT_result<T> result;
  CDTArrangement<T> *cdt = &cdt_state->cdt;
  result.face_edge_offset = cdt_state->face_edge_offset;

  /* All verts without a merge_to_index will be output.
   * vert_to_output_map[i] will hold the output vertex index
   * corresponding to the vert in position i in cdt->verts.
   * This first loop sets vert_to_output_map for un-merged verts. */
  int verts_size = cdt->verts.size();
  Array<int> vert_to_output_map(verts_size);
  int nv = 0;
  for (int i = 0; i < verts_size; ++i) {
    CDTVert<T> *v = cdt->verts[i];
    if (v->merge_to_index == -1) {
      vert_to_output_map[i] = nv;
      ++nv;
    }
  }
  if (nv <= 0) {
    return result;
  }
  /* Now we can set vert_to_output_map for merged verts,
   * and also add the input indices of merged verts to the input_ids
   * list of the merge target if they were an original input id. */
  if (nv < verts_size) {
    for (int i = 0; i < verts_size; ++i) {
      CDTVert<T> *v = cdt->verts[i];
      if (v->merge_to_index != -1) {
        if (cdt_state->need_ids) {
          if (i < cdt_state->input_vert_num) {
            add_to_input_ids(cdt->verts[v->merge_to_index]->input_ids, i);
          }
        }
        vert_to_output_map[i] = vert_to_output_map[v->merge_to_index];
      }
    }
  }
  result.vert = Array<VecBase<T, 2>>(nv);
  if (cdt_state->need_ids) {
    result.vert_orig = Array<Vector<int>>(nv);
  }
  int i_out = 0;
  for (int i = 0; i < verts_size; ++i) {
    CDTVert<T> *v = cdt->verts[i];
    if (v->merge_to_index == -1) {
      result.vert[i_out] = v->co.exact;
      if (cdt_state->need_ids) {
        if (i < cdt_state->input_vert_num) {
          result.vert_orig[i_out].append(i);
        }
        for (int vert : v->input_ids) {
          result.vert_orig[i_out].append(vert);
        }
      }
      ++i_out;
    }
  }

  /* All non-deleted edges will be output. */
  int ne = std::count_if(cdt->edges.begin(), cdt->edges.end(), [](const CDTEdge<T> *e) -> bool {
    return !is_deleted_edge(e);
  });
  result.edge = Array<std::pair<int, int>>(ne);
  if (cdt_state->need_ids) {
    result.edge_orig = Array<Vector<int>>(ne);
  }
  int e_out = 0;
  for (const CDTEdge<T> *e : cdt->edges) {
    if (!is_deleted_edge(e)) {
      int vo1 = vert_to_output_map[e->symedges[0].vert->index];
      int vo2 = vert_to_output_map[e->symedges[1].vert->index];
      result.edge[e_out] = std::pair<int, int>(vo1, vo2);
      if (cdt_state->need_ids) {
        for (int edge : e->input_ids) {
          result.edge_orig[e_out].append(edge);
        }
      }
      ++e_out;
    }
  }

  /* All non-deleted, non-outer faces will be output. */
  int nf = std::count_if(cdt->faces.begin(), cdt->faces.end(), [=](const CDTFace<T> *f) -> bool {
    return !f->deleted && f != cdt->outer_face;
  });
  result.face = Array<Vector<int>>(nf);
  if (cdt_state->need_ids) {
    result.face_orig = Array<Vector<int>>(nf);
  }
  int f_out = 0;
  for (const CDTFace<T> *f : cdt->faces) {
    if (!f->deleted && f != cdt->outer_face) {
      SymEdge<T> *se = f->symedge;
      BLI_assert(se != nullptr);
      SymEdge<T> *se_start = se;
      do {
        result.face[f_out].append(vert_to_output_map[se->vert->index]);
        se = se->next;
      } while (se != se_start);
      if (cdt_state->need_ids) {
        for (int face : f->input_ids) {
          result.face_orig[f_out].append(face);
        }
      }
      ++f_out;
    }
  }
  return result;
}

/**
 * Add all the input verts into cdt. This will deduplicate,
 * setting vertices merge_to_index to show merges.
 */
template<typename T> void add_input_verts(CDT_state<T> *cdt_state, const CDT_input<T> &input)
{
  for (int i = 0; i < cdt_state->input_vert_num; ++i) {
    cdt_state->cdt.add_vert(input.vert[i]);
  }
}

template<typename T>
CDT_result<T> delaunay_calc(const CDT_input<T> &input, CDT_output_type output_type)
{
  int nv = input.vert.size();
  int ne = input.edge.size();
  int nf = input.face.size();
  CDT_state<T> cdt_state(nv, ne, nf, input.epsilon, input.need_ids);
  add_input_verts(&cdt_state, input);
  initial_triangulation(&cdt_state.cdt);
  add_edge_constraints(&cdt_state, input);
  int actual_nf = add_face_constraints(&cdt_state, input, output_type);
  if (actual_nf == 0 && !ELEM(output_type, CDT_FULL, CDT_INSIDE, CDT_CONSTRAINTS)) {
    /* Can't look for faces or holes if there were no valid input faces. */
    output_type = CDT_INSIDE;
  }
  return get_cdt_output(&cdt_state, input, output_type);
}

blender::meshintersect::CDT_result<double> delaunay_2d_calc(const CDT_input<double> &input,
                                                            CDT_output_type output_type)
{
  return delaunay_calc(input, output_type);
}

#ifdef WITH_GMP
blender::meshintersect::CDT_result<mpq_class> delaunay_2d_calc(const CDT_input<mpq_class> &input,
                                                               CDT_output_type output_type)
{
  return delaunay_calc(input, output_type);
}
#endif

} /* namespace blender::meshintersect */
