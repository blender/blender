/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "MEM_guardedalloc.h"

#include "BLI_rand.h"
#include "BLI_time.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <type_traits>

#define DO_CPP_TESTS 1
#define DO_TEXT_TESTS 0
#define DO_RANDOM_TESTS 0

#include "BLI_array.hh"
#include "BLI_math_boolean.hh"
#include "BLI_math_mpq.hh"
#include "BLI_math_vector_mpq_types.hh"
#include "BLI_vector.hh"

#include "BLI_delaunay_2d.hh"

namespace blender::meshintersect {

/* The spec should have the form:
 * #verts #edges #faces
 * <float> <float>   [#verts lines)
 * <int> <int>   [#edges lines]
 * <int> <int> ... <int>   [#faces lines]
 */
template<typename T> CDT_input<T> fill_input_from_string(const char *spec)
{
  std::istringstream ss(spec);
  std::string line;
  getline(ss, line);
  std::istringstream hdrss(line);
  int nverts, nedges, nfaces;
  hdrss >> nverts >> nedges >> nfaces;
  if (nverts == 0) {
    return CDT_input<T>();
  }
  Array<VecBase<T, 2>> verts(nverts);
  Array<std::pair<int, int>> edges(nedges);
  Array<Vector<int>> faces(nfaces);
  int i = 0;
  while (i < nverts && getline(ss, line)) {
    std::istringstream iss(line);
    double dp0, dp1;
    iss >> dp0 >> dp1;
    T p0(dp0);
    T p1(dp1);
    verts[i] = VecBase<T, 2>(p0, p1);
    i++;
  }
  i = 0;
  while (i < nedges && getline(ss, line)) {
    std::istringstream ess(line);
    int e0, e1;
    ess >> e0 >> e1;
    edges[i] = std::pair<int, int>(e0, e1);
    i++;
  }
  i = 0;
  while (i < nfaces && getline(ss, line)) {
    std::istringstream fss(line);
    int v;
    while (fss >> v) {
      faces[i].append(v);
    }
    i++;
  }
  CDT_input<T> ans;
  ans.vert = verts;
  ans.edge = edges;
  ans.face = faces;
#ifdef WITH_GMP
  if (std::is_same<mpq_class, T>::value) {
    ans.epsilon = T(0);
  }
  else {
    ans.epsilon = T(0.00001);
  }
#else
  ans.epsilon = T(0.00001);
#endif
  return ans;
}

/* Find an original index in a table mapping new to original.
 * Return -1 if not found.
 */
static int get_orig_index(const Span<Vector<int>> out_to_orig, int orig_index)
{
  int n = int(out_to_orig.size());
  for (int i = 0; i < n; ++i) {
    for (int orig : out_to_orig[i]) {
      if (orig == orig_index) {
        return i;
      }
    }
  }
  return -1;
}

template<typename T> static double math_to_double(const T /*v*/)
{
  BLI_assert(false); /* Need implementation for other type. */
  return 0.0;
}

template<> double math_to_double<double>(const double v)
{
  return v;
}

#ifdef WITH_GMP
template<> double math_to_double<mpq_class>(const mpq_class v)
{
  return v.get_d();
}
#endif

template<typename T> static T math_abs(const T v);

#ifdef WITH_GMP
template<> mpq_class math_abs(const mpq_class v)
{
  return abs(v);
}
#endif

template<> double math_abs(const double v)
{
  return fabs(v);
}

/* Find an output index corresponding to a given coordinate (approximately).
 * Return -1 if not found.
 */
template<typename T> int get_vertex_by_coord(const CDT_result<T> &out, double x, double y)
{
  int nv = int(out.vert.size());
  for (int i = 0; i < nv; ++i) {
    double vx = math_to_double(out.vert[i][0]);
    double vy = math_to_double(out.vert[i][1]);
    if (fabs(vx - x) <= 1e-5 && fabs(vy - y) <= 1e-5) {
      return i;
    }
  }
  return -1;
}

/* Find an edge between two given output vertex indices. -1 if not found, */
template<typename T>
int get_output_edge_index(const CDT_result<T> &out, int out_index_1, int out_index_2)
{
  int ne = int(out.edge.size());
  for (int i = 0; i < ne; ++i) {
    if ((out.edge[i].first == out_index_1 && out.edge[i].second == out_index_2) ||
        (out.edge[i].first == out_index_2 && out.edge[i].second == out_index_1))
    {
      return i;
    }
  }
  return -1;
}

template<typename T>
bool output_edge_has_input_id(const CDT_result<T> &out, int out_edge_index, int in_edge_index)
{
  return out_edge_index < int(out.edge_orig.size()) &&
         out.edge_orig[out_edge_index].contains(in_edge_index);
}

/* Which out face is for a give output vertex ngon? -1 if not found.
 * Allow for cyclic shifts vertices of one poly vs the other.
 */
template<typename T> int get_output_face_index(const CDT_result<T> &out, const Array<int> &poly)
{
  int nf = int(out.face.size());
  int npolyv = int(poly.size());
  for (int f = 0; f < nf; ++f) {
    if (out.face[f].size() != poly.size()) {
      continue;
    }
    for (int cycle_start = 0; cycle_start < npolyv; ++cycle_start) {
      bool ok = true;
      for (int k = 0; ok && k < npolyv; ++k) {
        if (out.face[f][(cycle_start + k) % npolyv] != poly[k]) {
          ok = false;
        }
      }
      if (ok) {
        return f;
      }
    }
  }
  return -1;
}

template<typename T>
int get_output_tri_index(const CDT_result<T> &out,
                         int out_index_1,
                         int out_index_2,
                         int out_index_3)
{
  Array<int> tri{out_index_1, out_index_2, out_index_3};
  return get_output_face_index(out, tri);
}

template<typename T>
bool output_face_has_input_id(const CDT_result<T> &out, int out_face_index, int in_face_index)
{
  return out_face_index < int(out.face_orig.size()) &&
         out.face_orig[out_face_index].contains(in_face_index);
}

/* For debugging. */
template<typename T> std::ostream &operator<<(std::ostream &os, const CDT_result<T> &r)
{
  os << "\nRESULT\n";
  os << r.vert.size() << " verts, " << r.edge.size() << " edges, " << r.face.size() << " faces\n";
  os << "\nVERTS\n";
  for (int i : r.vert.index_range()) {
    os << "v" << i << " = " << r.vert[i] << "\n";
    os << "  orig: ";
    for (int j : r.vert_orig[i].index_range()) {
      os << r.vert_orig[i][j] << " ";
    }
    os << "\n";
  }
  os << "\nEDGES\n";
  for (int i : r.edge.index_range()) {
    os << "e" << i << " = (" << r.edge[i].first << ", " << r.edge[i].second << ")\n";
    os << "  orig: ";
    for (int j : r.edge_orig[i].index_range()) {
      os << r.edge_orig[i][j] << " ";
    }
    os << "\n";
  }
  os << "\nFACES\n";
  for (int i : r.face.index_range()) {
    os << "f" << i << " = ";
    for (int j : r.face[i].index_range()) {
      os << r.face[i][j] << " ";
    }
    os << "\n";
    os << "  orig: ";
    for (int j : r.face_orig[i].index_range()) {
      os << r.face_orig[i][j] << " ";
    }
    os << "\n";
  }
  return os;
}

static bool draw_append = false; /* Will be set to true after first call. */

template<typename T>
void graph_draw(const std::string &label,
                const Span<VecBase<T, 2>> verts,
                const Span<std::pair<int, int>> edges,
                const Span<Vector<int>> faces)
{
  /* Would like to use BKE_tempdir_base() here, but that brings in dependence on kernel library.
   * This is just for developer debugging anyway, and should never be called in production Blender.
   */
#ifdef WIN32
  constexpr const char *drawfile = "./cdt_test_draw.html";
#else
  constexpr const char *drawfile = "/tmp/cdt_test_draw.html";
#endif
  constexpr int max_draw_width = 1400;
  constexpr int max_draw_height = 1000;
  constexpr int thin_line = 1;
  constexpr int vert_radius = 3;
  constexpr bool draw_vert_labels = false;
  constexpr bool draw_edge_labels = false;

  if (verts.is_empty()) {
    return;
  }
  double2 vmin(1e10, 1e10);
  double2 vmax(-1e10, -1e10);
  for (const VecBase<T, 2> &v : verts) {
    for (int i = 0; i < 2; ++i) {
      double dvi = math_to_double(v[i]);
      if (dvi < vmin[i]) {
        vmin[i] = dvi;
      }
      if (dvi > vmax[i]) {
        vmax[i] = dvi;
      }
    }
  }
  double draw_margin = ((vmax.x - vmin.x) + (vmax.y - vmin.y)) * 0.05;
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

#define SX(x) ((math_to_double(x) - minx) * scale)
#define SY(y) ((maxy - math_to_double(y)) * scale)

  std::ofstream f;
  if (draw_append) {
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

  for (const Vector<int> &fverts : faces) {
    f << "<polygon fill=\"azure\" stroke=\"none\"\n  points=\"";
    for (int vi : fverts) {
      const VecBase<T, 2> &co = verts[vi];
      f << SX(co[0]) << "," << SY(co[1]) << " ";
    }
    f << "\"\n  />\n";
  }

  for (const std::pair<int, int> &e : edges) {
    const VecBase<T, 2> &uco = verts[e.first];
    const VecBase<T, 2> &vco = verts[e.second];
    int strokew = thin_line;
    f << R"(<line fill="none" stroke="black" stroke-width=")" << strokew << "\" x1=\""
      << SX(uco[0]) << "\" y1=\"" << SY(uco[1]) << "\" x2=\"" << SX(vco[0]) << "\" y2=\""
      << SY(vco[1]) << "\">\n";
    f << "  <title>[" << e.first << "][" << e.second << "]</title>\n";
    f << "</line>\n";
    if (draw_edge_labels) {
      f << "<text x=\"" << SX(0.5 * (uco[0] + vco[0])) << "\" y=\"" << SY(0.5 * (uco[1] + vco[1]))
        << R"(" font-size="small">)";
      f << "[" << e.first << "][" << e.second << "]</text>\n";
    }
  }

  int i = 0;
  for (const VecBase<T, 2> &vco : verts) {
    f << R"(<circle fill="black" cx=")" << SX(vco[0]) << "\" cy=\"" << SY(vco[1]) << "\" r=\""
      << vert_radius << "\">\n";
    f << "  <title>[" << i << "]" << vco << "</title>\n";
    f << "</circle>\n";
    if (draw_vert_labels) {
      f << "<text x=\"" << SX(vco[0]) + vert_radius << "\" y=\"" << SY(vco[1]) - vert_radius
        << R"(" font-size="small">[)" << i << "]</text>\n";
    }
    ++i;
  }

  draw_append = true;
#undef SX
#undef SY
}

/* Should tests draw their output to an html file? */
constexpr bool DO_DRAW = false;

template<typename T>
void expect_coord_near(const VecBase<T, 2> &testco, const VecBase<T, 2> &refco);

#ifdef WITH_GMP
template<> void expect_coord_near<mpq_class>(const mpq2 &testco, const mpq2 &refco)
{
  EXPECT_EQ(testco[0], refco[0]);
  EXPECT_EQ(testco[0], refco[0]);
}
#endif

template<> void expect_coord_near<double>(const double2 &testco, const double2 &refco)
{
  EXPECT_NEAR(testco[0], refco[0], 1e-5);
  EXPECT_NEAR(testco[1], refco[1], 1e-5);
}

#if DO_CPP_TESTS

template<typename T> void empty_test()
{
  CDT_input<T> in;

  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(0, out.vert.size());
  EXPECT_EQ(0, out.edge.size());
  EXPECT_EQ(0, out.face.size());
  EXPECT_EQ(0, out.vert_orig.size());
  EXPECT_EQ(0, out.edge_orig.size());
  EXPECT_EQ(0, out.face_orig.size());
}

template<typename T> void onept_test()
{
  const char *spec = R"(1 0 0
  0.0 0.0
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(out.vert.size(), 1);
  EXPECT_EQ(out.edge.size(), 0);
  EXPECT_EQ(out.face.size(), 0);
  if (out.vert.size() >= 1) {
    expect_coord_near<T>(out.vert[0], VecBase<T, 2>(0, 0));
  }
}

template<typename T> void twopt_test()
{
  const char *spec = R"(2 0 0
  0.0 -0.75
  0.0 0.75
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(out.vert.size(), 2);
  EXPECT_EQ(out.edge.size(), 1);
  EXPECT_EQ(out.face.size(), 0);
  int v0_out = get_orig_index(out.vert_orig, 0);
  int v1_out = get_orig_index(out.vert_orig, 1);
  EXPECT_NE(v0_out, -1);
  EXPECT_NE(v1_out, -1);
  EXPECT_NE(v0_out, v1_out);
  if (out.vert.size() >= 1) {
    expect_coord_near<T>(out.vert[v0_out], VecBase<T, 2>(0.0, -0.75));
    expect_coord_near<T>(out.vert[v1_out], VecBase<T, 2>(0.0, 0.75));
  }
  int e0_out = get_output_edge_index(out, v0_out, v1_out);
  EXPECT_EQ(e0_out, 0);
  if (DO_DRAW) {
    graph_draw<T>("TwoPt", out.vert, out.edge, out.face);
  }
}

template<typename T> void threept_test()
{
  const char *spec = R"(3 0 0
  -0.1 -0.75
  0.1 0.75
  0.5 0.5
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(out.vert.size(), 3);
  EXPECT_EQ(out.edge.size(), 3);
  EXPECT_EQ(out.face.size(), 1);
  int v0_out = get_orig_index(out.vert_orig, 0);
  int v1_out = get_orig_index(out.vert_orig, 1);
  int v2_out = get_orig_index(out.vert_orig, 2);
  EXPECT_TRUE(v0_out != -1 && v1_out != -1 && v2_out != -1);
  EXPECT_TRUE(v0_out != v1_out && v0_out != v2_out && v1_out != v2_out);
  int e0_out = get_output_edge_index(out, v0_out, v1_out);
  int e1_out = get_output_edge_index(out, v1_out, v2_out);
  int e2_out = get_output_edge_index(out, v2_out, v0_out);
  EXPECT_TRUE(e0_out != -1 && e1_out != -1 && e2_out != -1);
  EXPECT_TRUE(e0_out != e1_out && e0_out != e2_out && e1_out != e2_out);
  int f0_out = get_output_tri_index(out, v0_out, v2_out, v1_out);
  EXPECT_EQ(f0_out, 0);
  if (DO_DRAW) {
    graph_draw<T>("ThreePt", out.vert, out.edge, out.face);
  }
}

template<typename T> void mixedpts_test()
{
  /* Edges form a chain of length 3. */
  const char *spec = R"(4 3 0
  0.0 0.0
  -0.5 -0.5
  -0.4 -0.25
  -0.3 0.8
  0 1
  1 2
  2 3
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(out.vert.size(), 4);
  EXPECT_EQ(out.edge.size(), 6);
  int v0_out = get_orig_index(out.vert_orig, 0);
  int v1_out = get_orig_index(out.vert_orig, 1);
  int v2_out = get_orig_index(out.vert_orig, 2);
  int v3_out = get_orig_index(out.vert_orig, 3);
  EXPECT_TRUE(v0_out != -1 && v1_out != -1 && v2_out != -1 && v3_out != -1);
  int e0_out = get_output_edge_index(out, v0_out, v1_out);
  int e1_out = get_output_edge_index(out, v1_out, v2_out);
  int e2_out = get_output_edge_index(out, v2_out, v3_out);
  EXPECT_TRUE(e0_out != -1 && e1_out != -1 && e2_out != -1);
  EXPECT_TRUE(output_edge_has_input_id(out, e0_out, 0));
  EXPECT_TRUE(output_edge_has_input_id(out, e1_out, 1));
  EXPECT_TRUE(output_edge_has_input_id(out, e2_out, 2));
  if (DO_DRAW) {
    graph_draw<T>("MixedPts", out.vert, out.edge, out.face);
  }
}

template<typename T> void quad0_test()
{
  const char *spec = R"(4 0 0
  0.0 1.0
  1.0 0.0
  2.0 0.1
  2.25 0.5
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(out.vert.size(), 4);
  EXPECT_EQ(out.edge.size(), 5);
  int e_diag_out = get_output_edge_index(out, 1, 3);
  EXPECT_NE(e_diag_out, -1);
  if (DO_DRAW) {
    graph_draw<T>("Quad0", out.vert, out.edge, out.face);
  }
}

template<typename T> void quad1_test()
{
  const char *spec = R"(4 0 0
  0.0 0.0
  0.9 -1.0
  2.0 0.0
  0.9 3.0
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(out.vert.size(), 4);
  EXPECT_EQ(out.edge.size(), 5);
  int e_diag_out = get_output_edge_index(out, 0, 2);
  EXPECT_NE(e_diag_out, -1);
  if (DO_DRAW) {
    graph_draw<T>("Quad1", out.vert, out.edge, out.face);
  }
}

template<typename T> void quad2_test()
{
  const char *spec = R"(4 0 0
  0.5 0.0
  0.15 0.2
  0.3 0.4
  .45 0.35
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(out.vert.size(), 4);
  EXPECT_EQ(out.edge.size(), 5);
  int e_diag_out = get_output_edge_index(out, 1, 3);
  EXPECT_NE(e_diag_out, -1);
  if (DO_DRAW) {
    graph_draw<T>("Quad2", out.vert, out.edge, out.face);
  }
}

template<typename T> void quad3_test()
{
  const char *spec = R"(4 0 0
  0.5 0.0
  0.0 0.0
  0.3 0.4
  .45 0.35
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(out.vert.size(), 4);
  EXPECT_EQ(out.edge.size(), 5);
  int e_diag_out = get_output_edge_index(out, 0, 2);
  EXPECT_NE(e_diag_out, -1);
  if (DO_DRAW) {
    graph_draw<T>("Quad3", out.vert, out.edge, out.face);
  }
}

template<typename T> void quad4_test()
{
  const char *spec = R"(4 0 0
  1.0 1.0
  0.0 0.0
  1.0 -3.0
  0.0 1.0
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(out.vert.size(), 4);
  EXPECT_EQ(out.edge.size(), 5);
  int e_diag_out = get_output_edge_index(out, 0, 1);
  EXPECT_NE(e_diag_out, -1);
  if (DO_DRAW) {
    graph_draw<T>("Quad4", out.vert, out.edge, out.face);
  }
}

template<typename T> void lineinsquare_test()
{
  const char *spec = R"(6 1 1
  -0.5 -0.5
  0.5 -0.5
  -0.5 0.5
  0.5 0.5
  -0.25 0.0
  0.25 0.0
  4 5
  0 1 3 2
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(out.vert.size(), 6);
  EXPECT_EQ(out.face.size(), 6);
  if (DO_DRAW) {
    graph_draw<T>("LineInSquare - full", out.vert, out.edge, out.face);
  }
  CDT_result<T> out2 = delaunay_2d_calc(in, CDT_CONSTRAINTS);
  EXPECT_EQ(out2.vert.size(), 6);
  EXPECT_EQ(out2.face.size(), 1);
  if (DO_DRAW) {
    graph_draw<T>("LineInSquare - constraints", out2.vert, out2.edge, out2.face);
  }
  CDT_result<T> out3 = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  EXPECT_EQ(out3.vert.size(), 6);
  EXPECT_EQ(out3.face.size(), 6);
  if (DO_DRAW) {
    graph_draw<T>("LineInSquare - inside with holes", out3.vert, out3.edge, out3.face);
  }
  CDT_result<T> out4 = delaunay_2d_calc(in, CDT_CONSTRAINTS_VALID_BMESH_WITH_HOLES);
  EXPECT_EQ(out4.vert.size(), 6);
  EXPECT_EQ(out4.face.size(), 2);
  if (DO_DRAW) {
    graph_draw<T>("LineInSquare - valid bmesh with holes", out4.vert, out4.edge, out4.face);
  }
}

template<typename T> void lineholeinsquare_test()
{
  const char *spec = R"(10 1 2
  -0.5 -0.5
  0.5 -0.5
  -0.5 0.5
  0.5 0.5
  -0.25 0.0
  0.25 0.0
  -0.4 -0.4
  0.4 -0.4
  0.4 -0.3
  -0.4 -0.3
  4 5
  0 1 3 2
  6 7 8 9
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(out.vert.size(), 10);
  EXPECT_EQ(out.face.size(), 14);
  if (DO_DRAW) {
    graph_draw<T>("LineHoleInSquare - full", out.vert, out.edge, out.face);
  }
  CDT_result<T> out2 = delaunay_2d_calc(in, CDT_CONSTRAINTS);
  EXPECT_EQ(out2.vert.size(), 10);
  EXPECT_EQ(out2.face.size(), 2);
  if (DO_DRAW) {
    graph_draw<T>("LineHoleInSquare - constraints", out2.vert, out2.edge, out2.face);
  }
  CDT_result<T> out3 = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  EXPECT_EQ(out3.vert.size(), 10);
  EXPECT_EQ(out3.face.size(), 12);
  if (DO_DRAW) {
    graph_draw<T>("LineHoleInSquare - inside with holes", out3.vert, out3.edge, out3.face);
  }
  CDT_result<T> out4 = delaunay_2d_calc(in, CDT_CONSTRAINTS_VALID_BMESH_WITH_HOLES);
  EXPECT_EQ(out4.vert.size(), 10);
  EXPECT_EQ(out4.face.size(), 2);
  if (DO_DRAW) {
    graph_draw<T>("LineHoleInSquare - valid bmesh with holes", out4.vert, out4.edge, out4.face);
  }
}

template<typename T> void nestedholes_test()
{
  const char *spec = R"(12 0 3
  -0.5 -0.5
  0.5 -0.5
  -0.5 0.5
  0.5 0.5
  -0.4 -0.4
  0.4 -0.4
  0.4 0.4
  -0.4 0.4
  -0.2 -0.2
  0.2 -0.2
  0.2 0.2
  -0.2 0.2
  0 1 3 2
  4 7 6 5
  8 9 10 11
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(out.vert.size(), 12);
  EXPECT_EQ(out.face.size(), 18);
  if (DO_DRAW) {
    graph_draw<T>("NestedHoles - full", out.vert, out.edge, out.face);
  }
  CDT_result<T> out2 = delaunay_2d_calc(in, CDT_CONSTRAINTS);
  EXPECT_EQ(out2.vert.size(), 12);
  EXPECT_EQ(out2.face.size(), 3);
  if (DO_DRAW) {
    graph_draw<T>("NestedHoles - constraints", out2.vert, out2.edge, out2.face);
  }
  CDT_result<T> out3 = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  EXPECT_EQ(out3.vert.size(), 12);
  EXPECT_EQ(out3.face.size(), 10);
  if (DO_DRAW) {
    graph_draw<T>("NestedHoles - inside with holes", out3.vert, out3.edge, out3.face);
  }
  CDT_result<T> out4 = delaunay_2d_calc(in, CDT_CONSTRAINTS_VALID_BMESH_WITH_HOLES);
  EXPECT_EQ(out4.vert.size(), 12);
  EXPECT_EQ(out4.face.size(), 3);
  if (DO_DRAW) {
    graph_draw<T>("NestedHoles - valid bmesh with holes", out4.vert, out4.edge, out4.face);
  }
}

/* Two overlapping squares with the same winding direction (both CCW).
 * Even-odd: overlap region is excluded (2 crossings = outside).
 * Non-zero: overlap region is included (winding = 2 = inside). */
template<typename T> void nonzero_winding_test()
{
  /* Square 1: (0,0)-(1,1) CCW, Square 2: (0.5,0.5)-(1.5,1.5) CCW. */
  const char *spec = R"(8 0 2
  0.0 0.0
  1.0 0.0
  1.0 1.0
  0.0 1.0
  0.5 0.5
  1.5 0.5
  1.5 1.5
  0.5 1.5
  0 1 2 3
  4 5 6 7
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);

  /* Even-odd: the overlap region (0.5,0.5)-(1,1) is a hole. */
  CDT_result<T> out_evenodd = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  EXPECT_EQ(out_evenodd.vert.size(), 10); /* 8 input + 2 intersections */
  if (DO_DRAW) {
    graph_draw<T>(
        "NonZeroWinding - even-odd", out_evenodd.vert, out_evenodd.edge, out_evenodd.face);
  }

  /* Non-zero: overlapping same-winding squares union. */
  CDT_result<T> out_nonzero = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES_NONZERO);
  EXPECT_EQ(out_nonzero.vert.size(), 10); /* 8 input + 2 intersections */
  if (DO_DRAW) {
    graph_draw<T>(
        "NonZeroWinding - non-zero", out_nonzero.vert, out_nonzero.edge, out_nonzero.face);
  }

  /* Non-zero should have more faces than even-odd (union vs hole in overlap). */
  EXPECT_EQ(out_evenodd.face.size(), 8);
  EXPECT_EQ(out_nonzero.face.size(), 10);

  /* Verify non-zero rule is winding-independent: flipping all face windings
   * should produce identical results since we only check if winding == 0. */
  CDT_input<T> in_flipped = in;
  for (Vector<int> &face : in_flipped.face) {
    std::reverse(face.begin(), face.end());
  }
  CDT_result<T> out_flipped = delaunay_2d_calc(in_flipped, CDT_INSIDE_WITH_HOLES_NONZERO);
  EXPECT_EQ(out_flipped.vert.size(), out_nonzero.vert.size());
  EXPECT_EQ(out_flipped.face.size(), out_nonzero.face.size());
}

/* One square inside another - tests hole creation with winding rules.
 * Outer square CCW, inner square CW (opposite winding) = inner is a hole.
 * Outer square CCW, inner square CCW (same winding) = inner is filled. */
template<typename T> void nonzero_winding_nested_test()
{
  /* Outer square (0,0)-(2,2) CCW, inner square (0.5,0.5)-(1.5,1.5) CW. */
  const char *spec_hole = R"(8 0 2
  0.0 0.0
  2.0 0.0
  2.0 2.0
  0.0 2.0
  0.5 0.5
  0.5 1.5
  1.5 1.5
  1.5 0.5
  0 1 2 3
  4 5 6 7
  )";

  CDT_input<T> in_hole = fill_input_from_string<T>(spec_hole);

  /* Even-odd: inner square is a hole (2 crossings = outside). */
  CDT_result<T> out_evenodd_hole = delaunay_2d_calc(in_hole, CDT_INSIDE_WITH_HOLES);
  EXPECT_EQ(out_evenodd_hole.vert.size(), 8);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingNested - even-odd, inner CW (hole)",
                  out_evenodd_hole.vert,
                  out_evenodd_hole.edge,
                  out_evenodd_hole.face);
  }

  /* Non-zero: inner CW square creates a hole (winding: +1 - 1 = 0). */
  CDT_result<T> out_nonzero_hole = delaunay_2d_calc(in_hole, CDT_INSIDE_WITH_HOLES_NONZERO);
  EXPECT_EQ(out_nonzero_hole.vert.size(), 8);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingNested - non-zero, inner CW (hole)",
                  out_nonzero_hole.vert,
                  out_nonzero_hole.edge,
                  out_nonzero_hole.face);
  }

  /* Both rules produce same face count when inner has opposite winding. */
  EXPECT_EQ(out_evenodd_hole.face.size(), 8);
  EXPECT_EQ(out_nonzero_hole.face.size(), out_evenodd_hole.face.size());

  /* Now test with inner square also CCW (same winding as outer). */
  const char *spec_filled = R"(8 0 2
  0.0 0.0
  2.0 0.0
  2.0 2.0
  0.0 2.0
  0.5 0.5
  1.5 0.5
  1.5 1.5
  0.5 1.5
  0 1 2 3
  4 5 6 7
  )";

  CDT_input<T> in_filled = fill_input_from_string<T>(spec_filled);

  /* Even-odd: inner square is still a hole (2 crossings = outside). */
  CDT_result<T> out_evenodd_filled = delaunay_2d_calc(in_filled, CDT_INSIDE_WITH_HOLES);
  EXPECT_EQ(out_evenodd_filled.vert.size(), 8);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingNested - even-odd, inner CCW (hole)",
                  out_evenodd_filled.vert,
                  out_evenodd_filled.edge,
                  out_evenodd_filled.face);
  }

  /* Non-zero: inner CCW square is filled (winding: +1 + 1 = 2 = inside). */
  CDT_result<T> out_nonzero_filled = delaunay_2d_calc(in_filled, CDT_INSIDE_WITH_HOLES_NONZERO);
  EXPECT_EQ(out_nonzero_filled.vert.size(), 8);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingNested - non-zero, inner CCW (filled)",
                  out_nonzero_filled.vert,
                  out_nonzero_filled.edge,
                  out_nonzero_filled.face);
  }

  /* Non-zero should have more faces (inner filled vs inner hole). */
  EXPECT_EQ(out_evenodd_filled.face.size(), 8);
  EXPECT_EQ(out_nonzero_filled.face.size(), 10);
}

/**
 * Outer square with a hole, and two overlapping filled squares inside the hole.
 * Tests union behavior: the two inner CCW squares should union together.
 *
 * \code{.unparsed}
 * Geometry:
 *
 *   3---------------------------------2  y=4
 *   |                                 |
 *   |   5-------------------------6   |  y=3.5
 *   |   |                         |   |
 *   |   |       15-----------14   |   |  y=3
 *   |   |        |            |   |   |
 *   |   |   11---+-------10   |   |   |  y=2.5
 *   |   |    |   | overlap|   |   |   |
 *   |   |    |   12-------+---13  |   |  y=1.5
 *   |   |    |            |       |   |
 *   |   |    8------------9       |   |  y=1
 *   |   |                         |   |
 *   |   4-------------------------7   |  y=0.5
 *   |                                 |
 *   0---------------------------------1  y=0
 *  x=0  .5   1  1.5      2.5  3  3.5  4
 * \endcode
 *
 * - Face 0 (verts 0,1,2,3): Outer boundary (0,0)-(4,4) CCW gives winding +1.
 * - Face 1 (verts 4,5,6,7): Hole (0.5,0.5)-(3.5,3.5) CW gives winding -1.
 * - Face 2 (verts 8,9,10,11): Inner1 (1,1)-(2.5,2.5) CCW gives winding +1.
 * - Face 3 (verts 12,13,14,15): Inner2 (1.5,1.5)-(3,3) CCW gives winding +1.
 *
 * Winding by region:
 * - Outer band (between face 0 and face 1): +1 (filled).
 * - Hole band (inside face 1, outside inners): +1-1 = 0 (empty).
 * - Inner1 only region: +1-1+1 = +1 (filled).
 * - Inner2 only region: +1-1+1 = +1 (filled).
 * - Overlap region (both inners): +1-1+1+1 = +2 (filled for non-zero, hole for even-odd).
 *
 * Even-odd: overlap has 4 crossings (outer, hole, inner1, inner2) which is outside.
 * Non-zero: overlap has winding +2 which is inside, so inner squares union together.
 */
template<typename T> void nonzero_winding_nested_union_test()
{
  const char *spec = R"(16 0 4
  0.0 0.0
  4.0 0.0
  4.0 4.0
  0.0 4.0
  0.5 0.5
  0.5 3.5
  3.5 3.5
  3.5 0.5
  1.0 1.0
  2.5 1.0
  2.5 2.5
  1.0 2.5
  1.5 1.5
  3.0 1.5
  3.0 3.0
  1.5 3.0
  0 1 2 3
  4 5 6 7
  8 9 10 11
  12 13 14 15
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);

  /* Even-odd: inner overlap has 4 crossings (outer, hole, inner1, inner2) = outside. */
  CDT_result<T> out_evenodd = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  EXPECT_EQ(out_evenodd.vert.size(), 18); /* 16 input + 2 intersections */
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingNestedUnion - even-odd",
                  out_evenodd.vert,
                  out_evenodd.edge,
                  out_evenodd.face);
  }

  /* Non-zero: inner squares union.
   * Winding in overlap: outer(+1) + hole(-1) + inner1(+1) + inner2(+1) = +2 = inside. */
  CDT_result<T> out_nonzero = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES_NONZERO);
  EXPECT_EQ(out_nonzero.vert.size(), 18); /* 16 input + 2 intersections */
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingNestedUnion - non-zero",
                  out_nonzero.vert,
                  out_nonzero.edge,
                  out_nonzero.face);
  }

  /* Non-zero should have more faces (union vs hole in overlap). */
  EXPECT_EQ(out_evenodd.face.size(), 16);
  EXPECT_EQ(out_nonzero.face.size(), 18);
}

/**
 * Stress test for non-zero winding with edges explicitly shared by 3+ faces.
 * Three overlapping rectangles that share bottom edge and overlapping side edges.
 *
 * \code{.unparsed}
 * Geometry:
 *   7---------6  y=3
 *   |         |
 *   5---------4  y=2
 *   |         |
 *   3---------2  y=1
 *   |         |
 *   0---------1  y=0
 *       x=0,3
 * \endcode
 *
 * Faces (all CCW):
 * - Face 0: 0,1,2,3 is the bottom rectangle with height 1.
 * - Face 1: 0,1,4,5 is the middle rectangle with height 2.
 * - Face 2: 0,1,6,7 is the tall rectangle with height 3.
 *
 * Shared edges with winding contributions:
 * - Edge (0,0)->(3,0) [bottom]: Faces 0,1,2 all traverse left->right giving winding +3.
 * - Edge (3,0)->(3,1): Faces 0,1,2 all traverse up giving winding +3.
 * - Edge (3,1)->(3,2): Faces 1,2 traverse up giving winding +2.
 * - Edge (3,2)->(3,3): Only face 2 traverses this edge giving winding +1.
 * - Similarly for left edges going down.
 *
 * Regions by y-band:
 * - [0,1]: All 3 faces overlap.
 * - [1,2]: Faces 1,2 overlap.
 * - [2,3]: Only face 2.
 *
 * Even-odd rule: [0,1]=3 crossings=inside, [1,2]=2=outside, [2,3]=1=inside
 * Non-zero rule: all regions have winding>0, all inside
 */
template<typename T> void nonzero_winding_multi_face_edge_test()
{
  const char *spec = R"(8 0 3
  0.0 0.0
  3.0 0.0
  3.0 1.0
  0.0 1.0
  3.0 2.0
  0.0 2.0
  3.0 3.0
  0.0 3.0
  0 1 2 3
  0 1 4 5
  0 1 6 7
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);

  CDT_result<T> out_evenodd = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingMultiFaceEdge - even-odd",
                  out_evenodd.vert,
                  out_evenodd.edge,
                  out_evenodd.face);
  }

  CDT_result<T> out_nonzero = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES_NONZERO);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingMultiFaceEdge - non-zero",
                  out_nonzero.vert,
                  out_nonzero.edge,
                  out_nonzero.face);
  }

  /* 8 input vertices, no intersections needed. */
  EXPECT_EQ(out_evenodd.vert.size(), 8);
  EXPECT_EQ(out_nonzero.vert.size(), 8);

  /* Even-odd: middle band [1,2] is a hole (2 crossings = outside).
   * Non-zero: all bands filled (winding > 0).
   * So non-zero should have more faces than even-odd. */
  EXPECT_LT(out_evenodd.face.size(), out_nonzero.face.size());
}

/**
 * Same geometry as MultiFaceEdge but with mixed winding directions to test cancellation.
 * Face 0: CCW (+1), Face 1: CW (-1), Face 2: CCW (+1).
 *
 * \code{.unparsed}
 * Geometry (same as MultiFaceEdge):
 *   7---------6      y=3  Face 2 (CCW)
 *   |         |
 *   5---------4      y=2  Face 1 (CW - reversed!)
 *   |         |
 *   3---------2      y=1  Face 0 (CCW)
 *   |         |
 *   0---------1      y=0
 *       x=0,3
 * \endcode
 *
 * Edge windings:
 * - Bottom (0,0)->(3,0): +1 - 1 + 1 = +1.
 * - Edge (3,0)->(3,1): +1 - 1 + 1 = +1.
 * - Edge (3,1)->(3,2): -1 + 1 = 0 (key difference).
 * - Edge (3,2)->(3,3): +1.
 *
 * With mixed winding, the middle band [1,2] has edges with zero net winding,
 * so it behaves differently than the all-CCW case. This tests that winding
 * accumulation correctly handles cancellation from opposite-wound faces.
 */
template<typename T> void nonzero_winding_multi_face_edge_mixed_test()
{
  /* Same vertices, but face 1 is CW (reversed order). */
  const char *spec = R"(8 0 3
  0.0 0.0
  3.0 0.0
  3.0 1.0
  0.0 1.0
  3.0 2.0
  0.0 2.0
  3.0 3.0
  0.0 3.0
  0 1 2 3
  5 4 1 0
  0 1 6 7
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);

  CDT_result<T> out_evenodd = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingMultiFaceEdgeMixed - even-odd",
                  out_evenodd.vert,
                  out_evenodd.edge,
                  out_evenodd.face);
  }

  CDT_result<T> out_nonzero = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES_NONZERO);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingMultiFaceEdgeMixed - non-zero",
                  out_nonzero.vert,
                  out_nonzero.edge,
                  out_nonzero.face);
  }

  EXPECT_EQ(out_evenodd.vert.size(), 8);
  EXPECT_EQ(out_nonzero.vert.size(), 8);

  /* With CW middle face, the winding calculation differs from all-CCW case.
   * This verifies winding accumulation with cancellation works correctly.
   * The middle face (CW) subtracts from the outer, so effectively:
   * - [0,1]: face 0 only (face 1 CW cancels contribution) -> inside
   * - [1,2]: face 1 (CW, negative) + face 2 (CCW, positive) -> net depends on geometry
   * - [2,3]: face 2 only -> inside */
  EXPECT_EQ(out_evenodd.face.size(), out_nonzero.face.size());
}

/**
 * Stress test: winding contributions that cancel exactly to zero.
 * Four faces share the same bottom edge with windings: CCW, CCW, CW, CW.
 * Net winding on shared edge: +1 + 1 - 1 - 1 = 0.
 *
 * This tests that zero-winding regions correctly become holes in non-zero mode.
 *
 * \code{.unparsed}
 * Geometry (4 overlapping rectangles, all sharing bottom edge at y=0):
 *
 *   9---------8      y=4  Face 3 (CW, -1)
 *   |         |
 *   7---------6      y=3  Face 2 (CW, -1)
 *   |         |
 *   5---------4      y=2  Face 1 (CCW, +1)
 *   |         |
 *   3---------2      y=1  Face 0 (CCW, +1)
 *   |         |
 *   0---------1      y=0
 *       x=0,3
 * \endcode
 *
 * - Face 0: (0,0)-(3,1) CCW (+1).
 * - Face 1: (0,0)-(3,2) CCW (+1).
 * - Face 2: (0,0)-(3,3) CW (-1).
 * - Face 3: (0,0)-(3,4) CW (-1).
 *
 * Winding by y-band:
 * - [0,1]: All 4 faces overlap giving +1+1-1-1 = 0 (HOLE - this is correct).
 * - [1,2]: Faces 1,2,3 overlap giving +1-1-1 = -1 (filled).
 * - [2,3]: Faces 2,3 overlap giving -1-1 = -2 (filled).
 * - [3,4]: Only face 3 giving -1 (filled).
 *
 * EXPECTED RESULT for non-zero: bottom band [0,1] is empty, top 3 bands filled.
 * This looks unusual but is correct - the bottom band has winding=0 because
 * two CCW faces (+1+1) and two CW faces (-1-1) cancel out exactly.
 *
 * Even-odd by y-band (for comparison):
 * - [0,1]: 4 crossings means outside (hole).
 * - [1,2]: 3 crossings means inside (filled).
 * - [2,3]: 2 crossings means outside (hole).
 * - [3,4]: 1 crossing means inside (filled).
 */
template<typename T> void nonzero_winding_cancel_to_zero_test()
{
  const char *spec = R"(10 0 4
  0.0 0.0
  3.0 0.0
  3.0 1.0
  0.0 1.0
  3.0 2.0
  0.0 2.0
  3.0 3.0
  0.0 3.0
  3.0 4.0
  0.0 4.0
  0 1 2 3
  0 1 4 5
  7 6 1 0
  9 8 1 0
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);

  CDT_result<T> out_evenodd = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingCancelToZero - even-odd",
                  out_evenodd.vert,
                  out_evenodd.edge,
                  out_evenodd.face);
  }

  CDT_result<T> out_nonzero = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES_NONZERO);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingCancelToZero - non-zero",
                  out_nonzero.vert,
                  out_nonzero.edge,
                  out_nonzero.face);
  }

  EXPECT_EQ(out_evenodd.vert.size(), 10);
  EXPECT_EQ(out_nonzero.vert.size(), 10);

  /* Non-zero fills 3 bands, even-odd fills 2 bands:
   * - Even-odd: [0,1] and [2,3] are holes (alternating pattern)
   * - Non-zero: only [0,1] is a hole (winding=0), bands [1,2],[2,3],[3,4] filled
   * The empty bottom band in non-zero is correct: winding cancels to zero there. */
  EXPECT_LT(out_evenodd.face.size(), out_nonzero.face.size());
}

/**
 * Stress test: high winding count with 6 faces sharing an edge.
 * Tests that winding accumulation handles large values correctly.
 *
 * \code{.unparsed}
 * Geometry (6 stacked rectangles, all CCW):
 *
 *  13--------12      y=6  Face 5
 *   |         |
 *  11--------10      y=5  Face 4
 *   |         |
 *   9---------8      y=4  Face 3
 *   |         |
 *   7---------6      y=3  Face 2
 *   |         |
 *   5---------4      y=2  Face 1
 *   |         |
 *   3---------2      y=1  Face 0
 *   |         |
 *   0---------1      y=0
 *       x=0,3
 * \endcode
 *
 * The bottom edge (0,0)->(3,0) has winding = +6.
 *
 * Winding by y-band (all positive, all inside for non-zero):
 * bands [0,1]: 6, [1,2]: 5, [2,3]: 4, [3,4]: 3, [4,5]: 2, [5,6]: 1.
 *
 * Even-odd: alternating inside/outside (odd crossings = inside).
 */
template<typename T> void nonzero_winding_high_count_test()
{
  const char *spec = R"(14 0 6
  0.0 0.0
  3.0 0.0
  3.0 1.0
  0.0 1.0
  3.0 2.0
  0.0 2.0
  3.0 3.0
  0.0 3.0
  3.0 4.0
  0.0 4.0
  3.0 5.0
  0.0 5.0
  3.0 6.0
  0.0 6.0
  0 1 2 3
  0 1 4 5
  0 1 6 7
  0 1 8 9
  0 1 10 11
  0 1 12 13
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);

  CDT_result<T> out_evenodd = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingHighCount - even-odd",
                  out_evenodd.vert,
                  out_evenodd.edge,
                  out_evenodd.face);
  }

  CDT_result<T> out_nonzero = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES_NONZERO);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingHighCount - non-zero",
                  out_nonzero.vert,
                  out_nonzero.edge,
                  out_nonzero.face);
  }

  EXPECT_EQ(out_evenodd.vert.size(), 14);
  EXPECT_EQ(out_nonzero.vert.size(), 14);

  /* Non-zero: all bands have positive winding, all filled.
   * Even-odd: bands with even crossing count are holes.
   * Non-zero should have more faces. */
  EXPECT_LT(out_evenodd.face.size(), out_nonzero.face.size());
}

/**
 * Stress test: fan of triangles around a central vertex.
 * Tests complex topology where multiple edges radiate from a single point.
 *
 * \code{.unparsed}
 * Geometry (4 triangles in a fan, all sharing center vertex 0):
 *        2
 *       /|\
 *      / | \
 *     /  |  \
 *    3---0---1
 *     \  |  /
 *      \ | /
 *       \|/
 *        4
 * \endcode
 *
 * Faces (all CCW):
 * - Face 0: 0,1,2 (right-top).
 * - Face 1: 0,2,3 (left-top).
 * - Face 2: 0,3,4 (left-bottom).
 * - Face 3: 0,4,1 (right-bottom).
 *
 * Each edge from center is shared by 2 adjacent faces with opposite traversal:
 * - Edge 0-1: Face 0 (0->1, +1), Face 3 (1->0, -1) gives net 0.
 * - Edge 0-2: Face 1 (0->2, +1), Face 0 (2->0, -1) gives net 0.
 * - And so on for other edges.
 *
 * This creates interesting winding behavior where the center edges have
 * zero net winding because adjacent triangles traverse them oppositely.
 */
template<typename T> void nonzero_winding_fan_test()
{
  const char *spec = R"(5 0 4
  0.0 0.0
  2.0 0.0
  0.0 2.0
  -2.0 0.0
  0.0 -2.0
  0 1 2
  0 2 3
  0 3 4
  0 4 1
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);

  CDT_result<T> out_evenodd = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  if (DO_DRAW) {
    graph_draw<T>(
        "NonZeroWindingFan - even-odd", out_evenodd.vert, out_evenodd.edge, out_evenodd.face);
  }

  CDT_result<T> out_nonzero = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES_NONZERO);
  if (DO_DRAW) {
    graph_draw<T>(
        "NonZeroWindingFan - non-zero", out_nonzero.vert, out_nonzero.edge, out_nonzero.face);
  }

  EXPECT_EQ(out_evenodd.vert.size(), 5);
  EXPECT_EQ(out_nonzero.vert.size(), 5);

  /* The adjacent triangles share edges with opposite traversal directions,
   * creating zero-winding edges at the center. This results in different
   * behavior between even-odd and non-zero rules.
   * Non-zero correctly fills all 4 triangular regions. */
  EXPECT_LT(out_evenodd.face.size(), out_nonzero.face.size());
  EXPECT_EQ(out_nonzero.face.size(), 4);
}

/**
 * Stress test: edge split propagation.
 * Tests that winding is correctly propagated when edges are split by
 * intersection points.
 *
 * \code{.unparsed}
 * Geometry: Two overlapping rectangles where one edge crosses through
 * the middle of another's edge, forcing edge splits.
 *
 *       4-------5  y=2
 *       |       |
 *   0---+-------+---1  y=1
 *   |   |       |   |
 *   |   |       |   |
 *   3---+-------+---2  y=-1
 *       |       |
 *       7-------6  y=-2
 * \endcode
 *
 * Face 0: (0,1)-(2,1)-(2,-1)-(0,-1) - wide rectangle, CCW
 * Face 1: (0.5,2)-(1.5,2)-(1.5,-2)-(0.5,-2) - tall rectangle, CCW
 *
 * The edges of face 1 cross through face 0's top and bottom edges,
 * causing splits at the + marks. Winding must propagate correctly through splits.
 */
template<typename T> void nonzero_winding_edge_split_test()
{
  const char *spec = R"(8 0 2
  0.0 1.0
  2.0 1.0
  2.0 -1.0
  0.0 -1.0
  0.5 2.0
  1.5 2.0
  1.5 -2.0
  0.5 -2.0
  0 1 2 3
  4 5 6 7
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);

  CDT_result<T> out_evenodd = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingEdgeSplit - even-odd",
                  out_evenodd.vert,
                  out_evenodd.edge,
                  out_evenodd.face);
  }

  CDT_result<T> out_nonzero = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES_NONZERO);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingEdgeSplit - non-zero",
                  out_nonzero.vert,
                  out_nonzero.edge,
                  out_nonzero.face);
  }

  /* 8 input vertices + 4 intersection points where rectangles cross. */
  EXPECT_EQ(out_evenodd.vert.size(), 12);
  EXPECT_EQ(out_nonzero.vert.size(), 12);

  /* The overlap region has winding +2 (both CCW), so non-zero fills it.
   * Even-odd treats it as a hole (2 crossings = outside).
   * Non-zero should have more faces. */
  EXPECT_LT(out_evenodd.face.size(), out_nonzero.face.size());
}

/**
 * Stress test: self-intersecting polygon (figure-8 / bow-tie shape).
 * Tests how winding is computed for a single face that crosses itself.
 *
 * \code{.unparsed}
 * Geometry (bowtie/figure-8 shape):
 *    2-----------1
 *     \         /
 *      \       /
 *       \     /
 *        \   /
 *         \ /
 *          X  (self-intersection at origin)
 *         / \
 *        /   \
 *       /     \
 *      /       \
 *     /         \
 *    0-----------3
 * \endcode
 *
 * Face 0: 0,1,2,3 forming a bowtie where edges 0->1 and 2->3 cross.
 * Vertices: 0=(-1,-1), 1=(1,1), 2=(-1,1), 3=(1,-1)
 * Edge 0->1: (-1,-1) to (1,1) - diagonal up-right
 * Edge 2->3: (-1,1) to (1,-1) - diagonal down-right, crosses edge 0->1
 *
 * The self-intersection creates two triangular regions.
 */
template<typename T> void nonzero_winding_self_intersect_test()
{
  const char *spec = R"(4 0 1
  -1.0 -1.0
  1.0 1.0
  -1.0 1.0
  1.0 -1.0
  0 1 2 3
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);

  CDT_result<T> out_evenodd = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingSelfIntersect - even-odd",
                  out_evenodd.vert,
                  out_evenodd.edge,
                  out_evenodd.face);
  }

  CDT_result<T> out_nonzero = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES_NONZERO);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingSelfIntersect - non-zero",
                  out_nonzero.vert,
                  out_nonzero.edge,
                  out_nonzero.face);
  }

  /* 4 input vertices + 1 intersection point at the crossing. */
  EXPECT_EQ(out_evenodd.vert.size(), 5);
  EXPECT_EQ(out_nonzero.vert.size(), 5);

  /* Self-intersecting polygon creates complex winding.
   * Both rules should fill the two triangular lobes of the figure-8. */
  EXPECT_EQ(out_evenodd.face.size(), out_nonzero.face.size());
}

/**
 * Stress test: deeply nested shapes with alternating winding.
 * Tests winding accumulation through many nesting levels.
 *
 * \code{.unparsed}
 * Geometry (5 nested squares, alternating CCW/CW):
 *
 *   +-------------------+  Square 0 (CCW, +1)
 *   | +---------------+ |  Square 1 (CW, -1)
 *   | | +-----------+ | |  Square 2 (CCW, +1)
 *   | | | +-------+ | | |  Square 3 (CW, -1)
 *   | | | | +---+ | | | |  Square 4 (CCW, +1)
 *   | | | | |   | | | | |
 *   | | | | +---+ | | | |
 *   | | | +-------+ | | |
 *   | | +-----------+ | |
 *   | +---------------+ |
 *   +-------------------+
 * \endcode
 *
 * - Square 0: (0,0)-(10,10) CCW  (+1).
 * - Square 1: (1,1)-(9,9)   CW   (-1).
 * - Square 2: (2,2)-(8,8)   CCW  (+1).
 * - Square 3: (3,3)-(7,7)   CW   (-1).
 * - Square 4: (4,4)-(6,6)   CCW  (+1).
 *
 * Winding at innermost region: +1-1+1-1+1 = +1 (inside).
 *
 * Even-odd: alternating inside/outside (5 crossings = inside).
 * Non-zero: all layers with non-zero winding are inside.
 */
template<typename T> void nonzero_winding_deep_nest_test()
{
  const char *spec = R"(20 0 5
  0.0 0.0
  10.0 0.0
  10.0 10.0
  0.0 10.0
  1.0 1.0
  1.0 9.0
  9.0 9.0
  9.0 1.0
  2.0 2.0
  8.0 2.0
  8.0 8.0
  2.0 8.0
  3.0 3.0
  3.0 7.0
  7.0 7.0
  7.0 3.0
  4.0 4.0
  6.0 4.0
  6.0 6.0
  4.0 6.0
  0 1 2 3
  4 5 6 7
  8 9 10 11
  12 13 14 15
  16 17 18 19
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);

  CDT_result<T> out_evenodd = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  if (DO_DRAW) {
    graph_draw<T>(
        "NonZeroWindingDeepNest - even-odd", out_evenodd.vert, out_evenodd.edge, out_evenodd.face);
  }

  CDT_result<T> out_nonzero = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES_NONZERO);
  if (DO_DRAW) {
    graph_draw<T>(
        "NonZeroWindingDeepNest - non-zero", out_nonzero.vert, out_nonzero.edge, out_nonzero.face);
  }

  EXPECT_EQ(out_evenodd.vert.size(), 20);
  EXPECT_EQ(out_nonzero.vert.size(), 20);

  /* With alternating winding (CCW, CW, CCW, CW, CCW):
   * - Regions between levels have winding: +1, 0, +1, 0, +1
   * - Non-zero fills regions with winding != 0
   * - Even-odd fills regions with odd crossing count
   * Both should produce similar results for alternating pattern. */
  EXPECT_EQ(out_evenodd.face.size(), out_nonzero.face.size());
}

/**
 * Stress test: partial overlap with shared subsegment.
 * Tests winding on a subsegment of larger edges after intersection splitting.
 *
 * \code{.unparsed}
 * Geometry (3 overlapping rectangles):
 *
 *          7-------------6  y=2
 *          |             |
 *   3------+------11-----+------2------10  y=1
 *   |      |      |      |      |      |
 *   0------4------8------5------1------9  y=0
 *  x=0    x=1    x=2    x=3    x=4    x=5
 *
 *   Face 0 (verts 0,1,2,3):   x=0 to x=4, y=0 to y=1
 *   Face 1 (verts 4,5,6,7):   x=1 to x=3, y=0 to y=2
 *   Face 2 (verts 8,9,10,11): x=2 to x=5, y=0 to y=1
 * \endcode
 *
 * - Face 0: (0,0)-(4,0)-(4,1)-(0,1) is a wide rectangle.
 * - Face 1: (1,0)-(3,0)-(3,2)-(1,2) is a tall rectangle that shares part of bottom edge.
 * - Face 2: (2,0)-(5,0)-(5,1)-(2,1) is a wide rectangle that shares part of bottom edge.
 *
 * The segment (2,0)-(3,0) is part of all 3 faces' bottom edges.
 * After CDT processes intersections, this subsegment should have winding +3.
 */
template<typename T> void nonzero_winding_shared_subsegment_test()
{
  const char *spec = R"(12 0 3
  0.0 0.0
  4.0 0.0
  4.0 1.0
  0.0 1.0
  1.0 0.0
  3.0 0.0
  3.0 2.0
  1.0 2.0
  2.0 0.0
  5.0 0.0
  5.0 1.0
  2.0 1.0
  0 1 2 3
  4 5 6 7
  8 9 10 11
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);

  CDT_result<T> out_evenodd = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingSharedSubsegment - even-odd",
                  out_evenodd.vert,
                  out_evenodd.edge,
                  out_evenodd.face);
  }

  CDT_result<T> out_nonzero = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES_NONZERO);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingSharedSubsegment - non-zero",
                  out_nonzero.vert,
                  out_nonzero.edge,
                  out_nonzero.face);
  }

  /* Multiple intersection points are created where edges cross. */
  EXPECT_GE(out_evenodd.vert.size(), 12);
  EXPECT_GE(out_nonzero.vert.size(), 12);

  /* The region where all 3 faces overlap (around x=2-3, y=0-1) has winding +3.
   * Non-zero fills it, while even-odd treats odd crossings as inside.
   * Different overlap patterns create different results. */
  EXPECT_NE(out_evenodd.face.size(), out_nonzero.face.size());
}

/**
 * Stress test: opposing faces creating an island inside a hole.
 * Tests winding cancellation and re-addition.
 *
 * \code{.unparsed}
 * Geometry:
 *
 *   3-----------------------2    Face 0 (CCW, outer)
 *   |                       |
 *   |   4---------------7   |    Face 1 (CW, hole)
 *   |   |               |   |
 *   |   |   8-------9   |   |    Face 2 (CCW, island)
 *   |   |   |       |   |   |
 *   |   |  11------10   |   |
 *   |   |               |   |
 *   |   5---------------6   |
 *   |                       |
 *   0-----------------------1
 * \endcode
 *
 * - Face 0: (0,0)-(6,0)-(6,6)-(0,6) CCW is the outer boundary.
 * - Face 1: (1,1)-(1,5)-(5,5)-(5,1) CW is the hole (cancels outer).
 * - Face 2: (2,2)-(4,2)-(4,4)-(2,4) CCW is the island inside hole (re-adds winding).
 *
 * Winding:
 * - Outer band has winding +1 (inside).
 * - Hole band has winding +1-1 = 0 (outside).
 * - Island has winding +1-1+1 = +1 (inside).
 *
 * This pattern is common in font glyphs with counter-shapes.
 */
template<typename T> void nonzero_winding_island_in_hole_test()
{
  const char *spec = R"(12 0 3
  0.0 0.0
  6.0 0.0
  6.0 6.0
  0.0 6.0
  1.0 1.0
  1.0 5.0
  5.0 5.0
  5.0 1.0
  2.0 2.0
  4.0 2.0
  4.0 4.0
  2.0 4.0
  0 1 2 3
  4 5 6 7
  8 9 10 11
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);

  CDT_result<T> out_evenodd = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingIslandInHole - even-odd",
                  out_evenodd.vert,
                  out_evenodd.edge,
                  out_evenodd.face);
  }

  CDT_result<T> out_nonzero = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES_NONZERO);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingIslandInHole - non-zero",
                  out_nonzero.vert,
                  out_nonzero.edge,
                  out_nonzero.face);
  }

  EXPECT_EQ(out_evenodd.vert.size(), 12);
  EXPECT_EQ(out_nonzero.vert.size(), 12);

  /* Both rules should produce the same result:
   * - Outer band is filled (1 or +1)
   * - Hole is empty (2 or 0)
   * - Island is filled (3 or +1)
   * This is the standard nested hole+island pattern. */
  EXPECT_EQ(out_evenodd.face.size(), out_nonzero.face.size());
}

/**
 * Stress test: coincident edges from separate faces with different vertices.
 * Tests vertex merging combined with winding accumulation.
 *
 * \code{.unparsed}
 * Geometry:
 *
 *          2 (1,1)
 *         / \          Face 0 (CCW, +1)
 *        /   \
 *       /     \
 *   0,3---------1,4    y=0 (shared edge, vertices merged)
 *       \     /
 *        \   /
 *         \ /          Face 1 (CW, -1)
 *          5 (1,-1)
 * \endcode
 *
 * - Face 0: Triangle with base (0,0)-(2,0), apex at (1,1), CCW, uses vertices 0,1,2.
 * - Face 1: Triangle with base (0,0)-(2,0), apex at (1,-1), CW, uses vertices 3,4,5.
 * - Vertices 0,3 are coincident (both at 0,0).
 * - Vertices 1,4 are coincident (both at 2,0).
 *
 * The shared edge (0,0)-(2,0) comes from two different vertex pairs that
 * get merged by CDT. Face 0 is CCW (+1), Face 1 is CW (-1).
 * The shared edge has winding +1-1 = 0.
 *
 * This tests that winding is correctly accumulated when vertex merging
 * creates shared edges, even when the faces have opposite orientations.
 */
template<typename T> void nonzero_winding_coincident_verts_test()
{
  const char *spec = R"(6 0 2
  0.0 0.0
  2.0 0.0
  1.0 1.0
  0.0 0.0
  2.0 0.0
  1.0 -1.0
  0 1 2
  3 4 5
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);

  CDT_result<T> out_evenodd = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingCoincidentVerts - even-odd",
                  out_evenodd.vert,
                  out_evenodd.edge,
                  out_evenodd.face);
  }

  CDT_result<T> out_nonzero = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES_NONZERO);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingCoincidentVerts - non-zero",
                  out_nonzero.vert,
                  out_nonzero.edge,
                  out_nonzero.face);
  }

  /* 6 input vertices but 2 pairs are coincident, so 4 unique after merging. */
  EXPECT_EQ(out_evenodd.vert.size(), 4);
  EXPECT_EQ(out_nonzero.vert.size(), 4);

  /* The shared edge has winding 0 (CCW + CW cancel). Non-zero fills both
   * triangles because their outer edges have non-zero winding.
   * Even-odd treats the shared edge differently. */
  EXPECT_LT(out_evenodd.face.size(), out_nonzero.face.size());
  EXPECT_EQ(out_nonzero.face.size(), 2);
}

/**
 * Stress test: ray casting through many constrained edges.
 * Tests the ray-casting accumulation in detect_holes() with many edge crossings.
 *
 * \code{.unparsed}
 * Geometry: 5 separate non-overlapping vertical strips.
 * A horizontal ray must cross multiple constrained edges.
 *
 *   Strip 0    Strip 1    Strip 2    Strip 3    Strip 4
 *   +--+       +--+       +--+       +--+       +--+
 *   |  |       |  |       |  |       |  |       |  |
 *   |  |       |  |       |  |       |  |       |  |
 *   +--+       +--+       +--+       +--+       +--+
 *   x=0-1      x=2-3      x=4-5      x=6-7      x=8-9
 *   CCW        CW         CCW        CW         CCW
 * \endcode
 *
 * Strips have alternating CCW/CW orientation due to vertex ordering.
 * Since they don't overlap, each strip is independently "inside" for
 * both even-odd and non-zero rules (winding +/-1 != 0).
 * Tests that ray casting correctly handles many separate regions.
 */
template<typename T> void nonzero_winding_many_crossings_test()
{
  const char *spec = R"(20 0 5
  0.0 0.0
  1.0 0.0
  1.0 2.0
  0.0 2.0
  2.0 0.0
  2.0 2.0
  3.0 2.0
  3.0 0.0
  4.0 0.0
  5.0 0.0
  5.0 2.0
  4.0 2.0
  6.0 0.0
  6.0 2.0
  7.0 2.0
  7.0 0.0
  8.0 0.0
  9.0 0.0
  9.0 2.0
  8.0 2.0
  0 1 2 3
  4 5 6 7
  8 9 10 11
  12 13 14 15
  16 17 18 19
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);

  CDT_result<T> out_evenodd = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingManyCrossings - even-odd",
                  out_evenodd.vert,
                  out_evenodd.edge,
                  out_evenodd.face);
  }

  CDT_result<T> out_nonzero = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES_NONZERO);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingManyCrossings - non-zero",
                  out_nonzero.vert,
                  out_nonzero.edge,
                  out_nonzero.face);
  }

  EXPECT_EQ(out_evenodd.vert.size(), 20);
  EXPECT_EQ(out_nonzero.vert.size(), 20);

  /* 5 non-overlapping strips with alternating CCW/CW orientation.
   * Both rules fill all strips (1 crossing = inside for even-odd,
   * winding +/-1 != 0 for non-zero). */
  EXPECT_EQ(out_evenodd.face.size(), out_nonzero.face.size());
}

/**
 * Stress test: all faces with negative winding (all CW).
 * Tests that negative winding values are correctly treated as "inside"
 * by the non-zero rule (any non-zero winding = inside).
 *
 * \code{.unparsed}
 * Geometry: 3 overlapping rectangles, all CW (clockwise).
 *
 *   7---------6      y=3  Face 2 (CW, -1)
 *   |         |
 *   5---------4      y=2  Face 1 (CW, -1)
 *   |         |
 *   3---------2      y=1  Face 0 (CW, -1)
 *   |         |
 *   0---------1      y=0
 *       x=0,3
 * \endcode
 *
 * - Face 0: (0,0)-(3,1) CW gives winding -1.
 * - Face 1: (0,0)-(3,2) CW gives winding -1.
 * - Face 2: (0,0)-(3,3) CW gives winding -1.
 *
 * Winding by y-band (all negative):
 * - [0,1]: -3 (all 3 overlap).
 * - [1,2]: -2 (faces 1,2 overlap).
 * - [2,3]: -1 (face 2 only).
 *
 * Non-zero rule: all bands have winding != 0, so all are inside.
 * Even-odd rule: alternating inside/outside.
 */
template<typename T> void nonzero_winding_negative_only_test()
{
  /* All faces are CW (vertices listed clockwise). */
  const char *spec = R"(8 0 3
  0.0 0.0
  3.0 0.0
  3.0 1.0
  0.0 1.0
  3.0 2.0
  0.0 2.0
  3.0 3.0
  0.0 3.0
  3 2 1 0
  5 4 1 0
  7 6 1 0
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);

  CDT_result<T> out_evenodd = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingNegativeOnly - even-odd",
                  out_evenodd.vert,
                  out_evenodd.edge,
                  out_evenodd.face);
  }

  CDT_result<T> out_nonzero = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES_NONZERO);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingNegativeOnly - non-zero",
                  out_nonzero.vert,
                  out_nonzero.edge,
                  out_nonzero.face);
  }

  EXPECT_EQ(out_evenodd.vert.size(), 8);
  EXPECT_EQ(out_nonzero.vert.size(), 8);

  /* Non-zero fills all bands (winding -3, -2, -1 are all != 0).
   * Even-odd has holes in even-crossing bands.
   * Non-zero should have more faces. */
  EXPECT_LT(out_evenodd.face.size(), out_nonzero.face.size());
}

/**
 * Stress test: overlapping rectangles with shared collinear edge segment.
 * Tests winding when one face's edge is a subsegment of another's edge.
 *
 * \code{.unparsed}
 * Geometry:
 *   Face 0: Large rectangle (0,0)-(4,2) CCW - vertices 0,1,2,3
 *   Face 1: Small rectangle (1,0)-(3,1) CCW - vertices 4,5,6,7
 *
 *      0-----------------1  y=2
 *      |                 |
 *      |     4-----5     |  y=1
 *      |     |     |     |
 *      3-----7-----6-----2  y=0
 *            ^     ^
 *       (1,0)     (3,0)
 * \endcode
 *
 * Face 1's bottom edge (1,0)-(3,0) lies on Face 0's bottom edge (0,0)-(4,0).
 * This creates shared collinear segments where Face 0's edge is split.
 * Face 1 is entirely inside Face 0, creating an overlap region (1,0)-(3,1).
 */
template<typename T> void nonzero_winding_tjunction_test()
{
  const char *spec = R"(8 0 2
  0.0 2.0
  4.0 2.0
  4.0 0.0
  0.0 0.0
  1.0 1.0
  3.0 1.0
  3.0 0.0
  1.0 0.0
  0 1 2 3
  4 5 6 7
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);

  CDT_result<T> out_evenodd = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingTJunction - even-odd",
                  out_evenodd.vert,
                  out_evenodd.edge,
                  out_evenodd.face);
  }

  CDT_result<T> out_nonzero = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES_NONZERO);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingTJunction - non-zero",
                  out_nonzero.vert,
                  out_nonzero.edge,
                  out_nonzero.face);
  }

  /* 8 input vertices. Face 1's bottom edge shares segment with Face 0's bottom edge. */
  EXPECT_EQ(out_evenodd.vert.size(), 8);
  EXPECT_EQ(out_nonzero.vert.size(), 8);

  /* The overlap region (1,0)-(3,1) has winding +2 (both CCW).
   * Non-zero fills it, even-odd treats it as a hole.
   * Non-zero should have more faces. */
  EXPECT_LT(out_evenodd.face.size(), out_nonzero.face.size());
}

/**
 * Stress test: exactly shared edge used by 3 triangles.
 * Three triangles that literally use the same two vertex indices (0,1) for one edge.
 *
 * \code{.unparsed}
 * Geometry:
 *
 *       2 (1,2)
 *      / \
 *     /   \
 *    /  4  \     4=(1,1) inside upper triangle
 *   / (1,1) \
 *  0---------1   0=(0,0), 1=(2,0)
 *   \       /
 *    \     /
 *     \   /
 *      \ /
 *       3 (1,-2)
 * \endcode
 *
 * - Face 0: 0,1,2 is the large triangle apex up, CCW.
 * - Face 1: 0,1,4 is the small triangle apex up (inside face 0), CCW.
 * - Face 2: 0,1,3 is the triangle apex down, CCW.
 *
 * Edge 0->1 is used by all 3 faces:
 * - Face 0: 0->1 in CCW order gives +1.
 * - Face 1: 0->1 in CCW order gives +1.
 * - Face 2: 0->1 in CCW order gives +1.
 * Total winding on edge 0->1 is +3.
 *
 * Faces 0 and 1 overlap (1 is inside 0), face 2 is separate.
 */
template<typename T> void nonzero_winding_exact_shared_edge_test()
{
  const char *spec = R"(5 0 3
  0.0 0.0
  2.0 0.0
  1.0 2.0
  1.0 -2.0
  1.0 1.0
  0 1 2
  0 1 4
  0 1 3
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);

  CDT_result<T> out_evenodd = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingExactSharedEdge - even-odd",
                  out_evenodd.vert,
                  out_evenodd.edge,
                  out_evenodd.face);
  }

  CDT_result<T> out_nonzero = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES_NONZERO);
  if (DO_DRAW) {
    graph_draw<T>("NonZeroWindingExactSharedEdge - non-zero",
                  out_nonzero.vert,
                  out_nonzero.edge,
                  out_nonzero.face);
  }

  EXPECT_EQ(out_evenodd.vert.size(), 5);
  EXPECT_EQ(out_nonzero.vert.size(), 5);

  /* 3 triangles all sharing edge 0-1 with same traversal direction (+1 each).
   * Face 1 (small triangle) is inside Face 0 (large triangle).
   * Face 2 (down triangle) is separate.
   *
   * Edge 0-1 has total winding = +3.
   *
   * Even-odd: small triangle region has 2 crossings = hole
   * Non-zero: small triangle region has winding = 2 = inside
   * Non-zero should have more faces. */
  EXPECT_LT(out_evenodd.face.size(), out_nonzero.face.size());
}

template<typename T> void crosssegs_test()
{
  const char *spec = R"(4 2 0
  -0.5 0.0
  0.5 0.0
  -0.4 -0.5
  0.4 0.5
  0 1
  2 3
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(out.vert.size(), 5);
  EXPECT_EQ(out.edge.size(), 8);
  EXPECT_EQ(out.face.size(), 4);
  int v0_out = get_orig_index(out.vert_orig, 0);
  int v1_out = get_orig_index(out.vert_orig, 1);
  int v2_out = get_orig_index(out.vert_orig, 2);
  int v3_out = get_orig_index(out.vert_orig, 3);
  EXPECT_TRUE(v0_out != -1 && v1_out != -1 && v2_out != -1 && v3_out != -1);
  if (out.vert.size() == 5) {
    int v_intersect = -1;
    for (int i = 0; i < 5; i++) {
      if (!ELEM(i, v0_out, v1_out, v2_out, v3_out)) {
        EXPECT_EQ(v_intersect, -1);
        v_intersect = i;
      }
    }
    EXPECT_NE(v_intersect, -1);
    if (v_intersect != -1) {
      expect_coord_near<T>(out.vert[v_intersect], VecBase<T, 2>(0, 0));
    }
  }
  if (DO_DRAW) {
    graph_draw<T>("CrossSegs", out.vert, out.edge, out.face);
  }
}

template<typename T> void cutacrosstri_test()
{
  /* Right triangle with horizontal segment exactly crossing in the middle. */
  const char *spec = R"(5 1 1
  0.0 0.0
  1.0 0.0
  0.0 1.0
  0.0 0.5
  0.5 0.5
  3 4
  0 1 2
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(out.vert.size(), 5);
  EXPECT_EQ(out.edge.size(), 7);
  EXPECT_EQ(out.face.size(), 3);
  int v0_out = get_orig_index(out.vert_orig, 0);
  int v1_out = get_orig_index(out.vert_orig, 1);
  int v2_out = get_orig_index(out.vert_orig, 2);
  int v3_out = get_orig_index(out.vert_orig, 3);
  int v4_out = get_orig_index(out.vert_orig, 4);
  EXPECT_TRUE(v0_out != -1 && v1_out != -1 && v2_out != -1 && v3_out != -1 && v4_out != -1);
  if (out.face.size() == 3) {
    int e0_out = get_orig_index(out.edge_orig, 0);
    EXPECT_NE(e0_out, -1);
    int fe0_out = get_output_edge_index(out, v0_out, v1_out);
    EXPECT_NE(fe0_out, -1);
    int fe1a_out = get_output_edge_index(out, v1_out, v4_out);
    EXPECT_NE(fe1a_out, -1);
    int fe1b_out = get_output_edge_index(out, v4_out, v2_out);
    EXPECT_NE(fe1b_out, -1);
    if (fe1a_out != 0 && fe1b_out != 0) {
      EXPECT_EQ(e0_out, get_orig_index(out.edge_orig, 0));
      EXPECT_TRUE(out.edge_orig[fe1a_out].size() == 1 && out.edge_orig[fe1a_out][0] == 11);
      EXPECT_TRUE(out.edge_orig[fe1b_out].size() == 1 && out.edge_orig[fe1b_out][0] == 11);
    }
    int e_diag = get_output_edge_index(out, v0_out, v4_out);
    EXPECT_NE(e_diag, -1);
    if (e_diag != -1) {
      EXPECT_EQ(out.edge_orig[e_diag].size(), 0);
    }
  }
  if (DO_DRAW) {
    graph_draw<T>("CutAcrossTri", out.vert, out.edge, out.face);
  }
}

template<typename T> void diamondcross_test()
{
  /* Diamond with constraint edge from top to bottom.  Some dup verts. */
  const char *spec = R"(7 5 0
  0.0 0.0
  1.0 3.0
  2.0 0.0
  1.0 -3.0
  0.0 0.0
  1.0 -3.0
  1.0 3.0
  0 1
  1 2
  2 3
  3 4
  5 6
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(out.vert.size(), 4);
  EXPECT_EQ(out.edge.size(), 5);
  EXPECT_EQ(out.face.size(), 2);
  if (DO_DRAW) {
    graph_draw<T>("DiamondCross", out.vert, out.edge, out.face);
  }
}

template<typename T> void twodiamondscross_test()
{
  const char *spec = R"(12 9 0
  0.0 0.0
  1.0 2.0
  2.0 0.0
  1.0 -2.0
  0.0 0.0
  3.0 0.0
  4.0 2.0
  5.0 0.0
  4.0 -2.0
  3.0 0.0
  0.0 0.0
  5.0 0.0
  0 1
  1 2
  2 3
  3 4
  5 6
  6 7
  7 8
  8 9
  10 11
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(out.vert.size(), 8);
  EXPECT_EQ(out.edge.size(), 15);
  EXPECT_EQ(out.face.size(), 8);
  if (out.vert.size() == 8 && out.edge.size() == 15 && out.face.size() == 8) {
    int v_out[12];
    for (int i = 0; i < 12; ++i) {
      v_out[i] = get_orig_index(out.vert_orig, i);
      EXPECT_NE(v_out[i], -1);
    }
    EXPECT_EQ(v_out[0], v_out[4]);
    EXPECT_EQ(v_out[0], v_out[10]);
    EXPECT_EQ(v_out[5], v_out[9]);
    EXPECT_EQ(v_out[7], v_out[11]);
    int e_out[9];
    for (int i = 0; i < 8; ++i) {
      e_out[i] = get_output_edge_index(out, v_out[in.edge[i].first], v_out[in.edge[i].second]);
      EXPECT_NE(e_out[i], -1);
    }
    /* there won't be a single edge for the input cross edge, but rather 3 */
    EXPECT_EQ(get_output_edge_index(out, v_out[10], v_out[11]), -1);
    int e_cross_1 = get_output_edge_index(out, v_out[0], v_out[2]);
    int e_cross_2 = get_output_edge_index(out, v_out[2], v_out[5]);
    int e_cross_3 = get_output_edge_index(out, v_out[5], v_out[7]);
    EXPECT_TRUE(e_cross_1 != -1 && e_cross_2 != -1 && e_cross_3 != -1);
    EXPECT_TRUE(output_edge_has_input_id(out, e_cross_1, 8));
    EXPECT_TRUE(output_edge_has_input_id(out, e_cross_2, 8));
    EXPECT_TRUE(output_edge_has_input_id(out, e_cross_3, 8));
  }
  if (DO_DRAW) {
    graph_draw<T>("TwoDiamondsCross", out.vert, out.edge, out.face);
  }
}

template<typename T> void manycross_test()
{
  /* Input has some repetition of vertices, on purpose */
  const char *spec = R"(27 21 0
  0.0 0.0
  6.0 9.0
  15.0 18.0
  35.0 13.0
  43.0 18.0
  57.0 12.0
  69.0 10.0
  78.0 0.0
  91.0 0.0
  107.0 22.0
  123.0 0.0
  0.0 0.0
  10.0 -14.0
  35.0 -8.0
  43.0 -12.0
  64.0 -13.0
  78.0 0.0
  91.0 0.0
  102.0 -9.0
  116.0 -9.0
  123.0 0.0
  43.0 18.0
  43.0 -12.0
  107.0 22.0
  102.0 -9.0
  0.0 0.0
  123.0 0.0
  0 1
  1 2
  2 3
  3 4
  4 5
  5 6
  6 7
  7 8
  8 9
  9 10
  11 12
  12 13
  13 14
  14 15
  15 16
  17 18
  18 19
  19 20
  21 22
  23 24
  25 26
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(out.vert.size(), 19);
  EXPECT_EQ(out.edge.size(), 46);
  EXPECT_EQ(out.face.size(), 28);
  if (DO_DRAW) {
    graph_draw<T>("ManyCross", out.vert, out.edge, out.face);
  }
}

template<typename T> void twoface_test()
{
  const char *spec = R"(6 0 2
  0.0 0.0
  1.0 0.0
  0.5 1.0
  1.1 1.0
  1.1 0.0
  1.6 1.0
  0 1 2
  3 4 5
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(out.vert.size(), 6);
  EXPECT_EQ(out.edge.size(), 9);
  EXPECT_EQ(out.face.size(), 4);
  if (out.vert.size() == 6 && out.edge.size() == 9 && out.face.size() == 4) {
    int v_out[6];
    for (int i = 0; i < 6; i++) {
      v_out[i] = get_orig_index(out.vert_orig, i);
      EXPECT_NE(v_out[i], -1);
    }
    int f0_out = get_output_tri_index(out, v_out[0], v_out[1], v_out[2]);
    int f1_out = get_output_tri_index(out, v_out[3], v_out[4], v_out[5]);
    EXPECT_NE(f0_out, -1);
    EXPECT_NE(f1_out, -1);
    int e0_out = get_output_edge_index(out, v_out[0], v_out[1]);
    int e1_out = get_output_edge_index(out, v_out[1], v_out[2]);
    int e2_out = get_output_edge_index(out, v_out[2], v_out[0]);
    EXPECT_NE(e0_out, -1);
    EXPECT_NE(e1_out, -1);
    EXPECT_NE(e2_out, -1);
    EXPECT_TRUE(output_edge_has_input_id(out, e0_out, out.face_edge_offset + 0));
    EXPECT_TRUE(output_edge_has_input_id(out, e1_out, out.face_edge_offset + 1));
    EXPECT_TRUE(output_edge_has_input_id(out, e2_out, out.face_edge_offset + 2));
    EXPECT_TRUE(output_face_has_input_id(out, f0_out, 0));
    EXPECT_TRUE(output_face_has_input_id(out, f1_out, 1));
  }
  if (DO_DRAW) {
    graph_draw<T>("TwoFace", out.vert, out.edge, out.face);
  }
}

template<typename T> void twoface2_test()
{
  const char *spec = R"(6 0 2
  0.0 0.0
  4.0 4.0
  -4.0 2.0
  3.0 0.0
  3.0 6.0
  -1.0 2.0
  0 1 2
  3 4 5
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_INSIDE);
  EXPECT_EQ(out.vert.size(), 10);
  EXPECT_EQ(out.edge.size(), 18);
  EXPECT_EQ(out.face.size(), 9);
  if (out.vert.size() == 10 && out.edge.size() == 18 && out.face.size() == 9) {
    /* Input verts have no duplicates, so expect output ones match input ones. */
    for (int i = 0; i < 6; i++) {
      EXPECT_EQ(get_orig_index(out.vert_orig, i), i);
    }
    int v6 = get_vertex_by_coord(out, 3.0, 3.0);
    EXPECT_NE(v6, -1);
    int v7 = get_vertex_by_coord(out, 3.0, 3.75);
    EXPECT_NE(v7, -1);
    int v8 = get_vertex_by_coord(out, 0.0, 3.0);
    EXPECT_NE(v8, -1);
    int v9 = get_vertex_by_coord(out, 1.0, 1.0);
    EXPECT_NE(v9, -1);
    /* f0 to f3 should be triangles part of input face 0, not part of input face 1. */
    int f0 = get_output_tri_index(out, 0, 9, 5);
    EXPECT_NE(f0, -1);
    EXPECT_TRUE(output_face_has_input_id(out, f0, 0));
    EXPECT_FALSE(output_face_has_input_id(out, f0, 1));
    int f1 = get_output_tri_index(out, 0, 5, 2);
    EXPECT_NE(f1, -1);
    EXPECT_TRUE(output_face_has_input_id(out, f1, 0));
    EXPECT_FALSE(output_face_has_input_id(out, f1, 1));
    int f2 = get_output_tri_index(out, 2, 5, 8);
    EXPECT_NE(f2, -1);
    EXPECT_TRUE(output_face_has_input_id(out, f2, 0));
    EXPECT_FALSE(output_face_has_input_id(out, f2, 1));
    int f3 = get_output_tri_index(out, 6, 1, 7);
    EXPECT_NE(f3, -1);
    EXPECT_TRUE(output_face_has_input_id(out, f3, 0));
    EXPECT_FALSE(output_face_has_input_id(out, f3, 1));
    /* f4 and f5 should be triangles part of input face 1, not part of input face 0. */
    int f4 = get_output_tri_index(out, 8, 7, 4);
    EXPECT_NE(f4, -1);
    EXPECT_FALSE(output_face_has_input_id(out, f4, 0));
    EXPECT_TRUE(output_face_has_input_id(out, f4, 1));
    int f5 = get_output_tri_index(out, 3, 6, 9);
    EXPECT_NE(f5, -1);
    EXPECT_FALSE(output_face_has_input_id(out, f5, 0));
    EXPECT_TRUE(output_face_has_input_id(out, f5, 1));
    /* f6 to f8 should be triangles part of both input faces. */
    int f6 = get_output_tri_index(out, 5, 9, 6);
    EXPECT_NE(f6, -1);
    EXPECT_TRUE(output_face_has_input_id(out, f6, 0));
    EXPECT_TRUE(output_face_has_input_id(out, f6, 1));
    int f7 = get_output_tri_index(out, 5, 6, 7);
    EXPECT_NE(f7, -1);
    EXPECT_TRUE(output_face_has_input_id(out, f7, 0));
    EXPECT_TRUE(output_face_has_input_id(out, f7, 1));
    int f8 = get_output_tri_index(out, 5, 7, 8);
    EXPECT_NE(f8, -1);
    EXPECT_TRUE(output_face_has_input_id(out, f8, 0));
    EXPECT_TRUE(output_face_has_input_id(out, f8, 1));
  }
  if (DO_DRAW) {
    graph_draw<T>("TwoFace2", out.vert, out.edge, out.face);
  }
}

template<typename T> void overlapfaces_test()
{
  const char *spec = R"(12 0 3
  0.0 0.0
  1.0 0.0
  1.0 1.0
  0.0 1.0
  0.5 0.5
  1.5 0.5
  1.5 1.3
  0.5 1.3
  0.1 0.1
  0.3 0.1
  0.3 0.3
  0.1 0.3
  0 1 2 3
  4 5 6 7
  8 9 10 11
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_FULL);
  EXPECT_EQ(out.vert.size(), 14);
  EXPECT_EQ(out.edge.size(), 33);
  EXPECT_EQ(out.face.size(), 20);
  if (out.vert.size() == 14 && out.edge.size() == 33 && out.face.size() == 20) {
    int v_out[12];
    for (int i = 0; i < 12; i++) {
      v_out[i] = get_orig_index(out.vert_orig, i);
      EXPECT_NE(v_out[i], -1);
    }
    int v_int1 = 12;
    int v_int2 = 13;
    T x = out.vert[v_int1][0] - T(1);
    if (math_abs(x) > in.epsilon) {
      v_int1 = 13;
      v_int2 = 12;
    }
    expect_coord_near<T>(out.vert[v_int1], VecBase<T, 2>(1, 0.5));
    expect_coord_near<T>(out.vert[v_int2], VecBase<T, 2>(0.5, 1));
    EXPECT_EQ(out.vert_orig[v_int1].size(), 0);
    EXPECT_EQ(out.vert_orig[v_int2].size(), 0);
    int f0_out = get_output_tri_index(out, v_out[1], v_int1, v_out[4]);
    EXPECT_NE(f0_out, -1);
    EXPECT_TRUE(output_face_has_input_id(out, f0_out, 0));
    int f1_out = get_output_tri_index(out, v_out[4], v_int1, v_out[2]);
    EXPECT_NE(f1_out, -1);
    EXPECT_TRUE(output_face_has_input_id(out, f1_out, 0));
    EXPECT_TRUE(output_face_has_input_id(out, f1_out, 1));
    int f2_out = get_output_tri_index(out, v_out[8], v_out[9], v_out[10]);
    if (f2_out == -1) {
      f2_out = get_output_tri_index(out, v_out[8], v_out[9], v_out[11]);
    }
    EXPECT_NE(f2_out, -1);
    EXPECT_TRUE(output_face_has_input_id(out, f2_out, 0));
    EXPECT_TRUE(output_face_has_input_id(out, f2_out, 2));
  }
  if (DO_DRAW) {
    graph_draw<T>("OverlapFaces - full", out.vert, out.edge, out.face);
  }

  /* Different output types. */
  CDT_result<T> out2 = delaunay_2d_calc(in, CDT_INSIDE);
  EXPECT_EQ(out2.face.size(), 18);
  if (DO_DRAW) {
    graph_draw<T>("OverlapFaces - inside", out2.vert, out2.edge, out2.face);
  }

  CDT_result<T> out3 = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  EXPECT_EQ(out3.face.size(), 14);
  if (DO_DRAW) {
    graph_draw<T>("OverlapFaces - inside with holes", out3.vert, out3.edge, out3.face);
  }

  CDT_result<T> out4 = delaunay_2d_calc(in, CDT_CONSTRAINTS);
  EXPECT_EQ(out4.face.size(), 4);
  if (DO_DRAW) {
    graph_draw<T>("OverlapFaces - constraints", out4.vert, out4.edge, out4.face);
  }

  CDT_result<T> out5 = delaunay_2d_calc(in, CDT_CONSTRAINTS_VALID_BMESH);
  EXPECT_EQ(out5.face.size(), 5);
  if (DO_DRAW) {
    graph_draw<T>("OverlapFaces - valid bmesh", out5.vert, out5.edge, out5.face);
  }

  CDT_result<T> out6 = delaunay_2d_calc(in, CDT_CONSTRAINTS_VALID_BMESH_WITH_HOLES);
  EXPECT_EQ(out6.face.size(), 3);
  if (DO_DRAW) {
    graph_draw<T>("OverlapFaces - valid bmesh with holes", out6.vert, out6.edge, out6.face);
  }
}

template<typename T> void twosquaresoverlap_test()
{
  const char *spec = R"(8 0 2
  1.0 -1.0
  -1.0 -1.0
  -1.0 1.0
  1.0 1.0
  -1.5 1.5
  0.5 1.5
  0.5 -0.5
  -1.5 -0.5
  7 6 5 4
  3 2 1 0
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_CONSTRAINTS_VALID_BMESH);
  EXPECT_EQ(out.vert.size(), 10);
  EXPECT_EQ(out.edge.size(), 12);
  EXPECT_EQ(out.face.size(), 3);
  if (DO_DRAW) {
    graph_draw<T>("TwoSquaresOverlap", out.vert, out.edge, out.face);
  }
}

template<typename T> void twofaceedgeoverlap_test()
{
  const char *spec = R"(6 0 2
  5.657 0.0
  -1.414 -5.831
  0.0 0.0
  5.657 0.0
  -2.121 -2.915
  0.0 0.0
  2 1 0
  5 4 3
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_CONSTRAINTS);
  EXPECT_EQ(out.vert.size(), 5);
  EXPECT_EQ(out.edge.size(), 7);
  EXPECT_EQ(out.face.size(), 3);
  if (out.vert.size() == 5 && out.edge.size() == 7 && out.face.size() == 3) {
    int v_int = 4;
    int v_out[6];
    for (int i = 0; i < 6; i++) {
      v_out[i] = get_orig_index(out.vert_orig, i);
      EXPECT_NE(v_out[i], -1);
      EXPECT_NE(v_out[i], v_int);
    }
    EXPECT_EQ(v_out[0], v_out[3]);
    EXPECT_EQ(v_out[2], v_out[5]);
    int e01 = get_output_edge_index(out, v_out[0], v_out[1]);
    int foff = out.face_edge_offset;
    EXPECT_TRUE(output_edge_has_input_id(out, e01, foff + 1));
    int e1i = get_output_edge_index(out, v_out[1], v_int);
    EXPECT_TRUE(output_edge_has_input_id(out, e1i, foff + 0));
    int ei2 = get_output_edge_index(out, v_int, v_out[2]);
    EXPECT_TRUE(output_edge_has_input_id(out, ei2, foff + 0));
    int e20 = get_output_edge_index(out, v_out[2], v_out[0]);
    EXPECT_TRUE(output_edge_has_input_id(out, e20, foff + 2));
    EXPECT_TRUE(output_edge_has_input_id(out, e20, 2 * foff + 2));
    int e24 = get_output_edge_index(out, v_out[2], v_out[4]);
    EXPECT_TRUE(output_edge_has_input_id(out, e24, 2 * foff + 0));
    int e4i = get_output_edge_index(out, v_out[4], v_int);
    EXPECT_TRUE(output_edge_has_input_id(out, e4i, 2 * foff + 1));
    int ei0 = get_output_edge_index(out, v_int, v_out[0]);
    EXPECT_TRUE(output_edge_has_input_id(out, ei0, 2 * foff + 1));
    int f02i = get_output_tri_index(out, v_out[0], v_out[2], v_int);
    EXPECT_NE(f02i, -1);
    EXPECT_TRUE(output_face_has_input_id(out, f02i, 0));
    EXPECT_TRUE(output_face_has_input_id(out, f02i, 1));
    int f24i = get_output_tri_index(out, v_out[2], v_out[4], v_int);
    EXPECT_NE(f24i, -1);
    EXPECT_TRUE(output_face_has_input_id(out, f24i, 1));
    EXPECT_FALSE(output_face_has_input_id(out, f24i, 0));
    int f10i = get_output_tri_index(out, v_out[1], v_out[0], v_int);
    EXPECT_NE(f10i, -1);
    EXPECT_TRUE(output_face_has_input_id(out, f10i, 0));
    EXPECT_FALSE(output_face_has_input_id(out, f10i, 1));
  }
  if (DO_DRAW) {
    graph_draw<T>("TwoFaceEdgeOverlap", out.vert, out.edge, out.face);
  }
}

template<typename T> void triintri_test()
{
  const char *spec = R"(6 0 2
  -5.65685 0.0
  1.41421 -5.83095
  0.0 0.0
  -2.47487 -1.45774
  -0.707107 -2.91548
  -1.06066 -1.45774
  0 1 2
  3 4 5
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_CONSTRAINTS_VALID_BMESH);
  EXPECT_EQ(out.vert.size(), 6);
  EXPECT_EQ(out.edge.size(), 8);
  EXPECT_EQ(out.face.size(), 3);
  if (DO_DRAW) {
    graph_draw<T>("TriInTri", out.vert, out.edge, out.face);
  }
}

template<typename T> void diamondinsquare_test()
{
  const char *spec = R"(8 0 2
  0.0 0.0
  1.0 0.0
  1.0 1.0
  0.0 1.0
  0.14644660940672627 0.5
  0.5 0.14644660940672627
  0.8535533905932737 0.5
  0.5 0.8535533905932737
  0 1 2 3
  4 5 6 7
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_CONSTRAINTS_VALID_BMESH);
  EXPECT_EQ(out.vert.size(), 8);
  EXPECT_EQ(out.edge.size(), 10);
  EXPECT_EQ(out.face.size(), 3);
  if (DO_DRAW) {
    graph_draw<T>("DiamondInSquare", out.vert, out.edge, out.face);
  }
}

template<typename T> void diamondinsquarewire_test()
{
  const char *spec = R"(8 8 0
  0.0 0.0
  1.0 0.0
  1.0 1.0
  0.0 1.0
  0.14644660940672627 0.5
  0.5 0.14644660940672627
  0.8535533905932737 0.5
  0.5 0.8535533905932737
  0 1
  1 2
  2 3
  3 0
  4 5
  5 6
  6 7
  7 4
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_CONSTRAINTS);
  EXPECT_EQ(out.vert.size(), 8);
  EXPECT_EQ(out.edge.size(), 8);
  EXPECT_EQ(out.face.size(), 2);
  if (DO_DRAW) {
    graph_draw<T>("DiamondInSquareWire", out.vert, out.edge, out.face);
  }
}

template<typename T> void repeatedge_test()
{
  const char *spec = R"(5 3 0
  0.0 0.0
  0.0 1.0
  1.0 1.1
  0.5 -0.5
  0.5 2.5
  0 1
  2 3
  2 3
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_CONSTRAINTS);
  EXPECT_EQ(out.edge.size(), 2);
  if (DO_DRAW) {
    graph_draw<T>("RepeatEdge", out.vert, out.edge, out.face);
  }
}

template<typename T> void repeattri_test()
{
  const char *spec = R"(3 0 2
  0.0 0.0
  1.0 0.0
  0.5 1.0
  0 1 2
  0 1 2
  )";

  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out = delaunay_2d_calc(in, CDT_CONSTRAINTS);
  EXPECT_EQ(out.edge.size(), 3);
  EXPECT_EQ(out.face.size(), 1);
  EXPECT_TRUE(output_face_has_input_id(out, 0, 0));
  EXPECT_TRUE(output_face_has_input_id(out, 0, 1));
  if (DO_DRAW) {
    graph_draw<T>("RepeatTri", out.vert, out.edge, out.face);
  }
}

template<typename T> void square_o_test()
{
  const char *spec = R"(8 0 2
  0.0 0.0
  1.0 0.0
  1.0 1.0
  0.0 1.0
  0.2 0.2
  0.2 0.8
  0.8 0.8
  0.8 0.2
  0 1 2 3
  4 5 6 7
  )";
  CDT_input<T> in = fill_input_from_string<T>(spec);
  CDT_result<T> out1 = delaunay_2d_calc(in, CDT_INSIDE_WITH_HOLES);
  EXPECT_EQ(out1.face.size(), 8);
  if (DO_DRAW) {
    graph_draw<T>("Square O - inside with holes", out1.vert, out1.edge, out1.face);
  }

  CDT_result<T> out2 = delaunay_2d_calc(in, CDT_CONSTRAINTS_VALID_BMESH_WITH_HOLES);
  EXPECT_EQ(out2.face.size(), 2);
  if (DO_DRAW) {
    graph_draw<T>("Square O - valid bmesh with holes", out2.vert, out2.edge, out2.face);
  }
}

TEST(delaunay_d, Empty)
{
  empty_test<double>();
}

TEST(delaunay_d, OnePt)
{
  onept_test<double>();
}

TEST(delaunay_d, TwoPt)
{
  twopt_test<double>();
}

TEST(delaunay_d, ThreePt)
{
  threept_test<double>();
}

TEST(delaunay_d, MixedPts)
{
  mixedpts_test<double>();
}

TEST(delaunay_d, Quad0)
{
  quad0_test<double>();
}

TEST(delaunay_d, Quad1)
{
  quad1_test<double>();
}

TEST(delaunay_d, Quad2)
{
  quad2_test<double>();
}

TEST(delaunay_d, Quad3)
{
  quad3_test<double>();
}

TEST(delaunay_d, Quad4)
{
  quad4_test<double>();
}

TEST(delaunay_d, LineInSquare)
{
  lineinsquare_test<double>();
}

TEST(delaunay_d, LineHoleInSquare)
{
  lineholeinsquare_test<double>();
}

TEST(delaunay_d, NestedHoles)
{
  nestedholes_test<double>();
}

TEST(delaunay_d, NonZeroWinding)
{
  nonzero_winding_test<double>();
}

TEST(delaunay_d, NonZeroWindingNested)
{
  nonzero_winding_nested_test<double>();
}

TEST(delaunay_d, NonZeroWindingNestedUnion)
{
  nonzero_winding_nested_union_test<double>();
}

TEST(delaunay_d, NonZeroWindingMultiFaceEdge)
{
  nonzero_winding_multi_face_edge_test<double>();
}

TEST(delaunay_d, NonZeroWindingMultiFaceEdgeMixed)
{
  nonzero_winding_multi_face_edge_mixed_test<double>();
}

TEST(delaunay_d, NonZeroWindingCancelToZero)
{
  nonzero_winding_cancel_to_zero_test<double>();
}

TEST(delaunay_d, NonZeroWindingHighCount)
{
  nonzero_winding_high_count_test<double>();
}

TEST(delaunay_d, NonZeroWindingFan)
{
  nonzero_winding_fan_test<double>();
}

TEST(delaunay_d, NonZeroWindingEdgeSplit)
{
  nonzero_winding_edge_split_test<double>();
}

TEST(delaunay_d, NonZeroWindingSelfIntersect)
{
  nonzero_winding_self_intersect_test<double>();
}

TEST(delaunay_d, NonZeroWindingDeepNest)
{
  nonzero_winding_deep_nest_test<double>();
}

TEST(delaunay_d, NonZeroWindingSharedSubsegment)
{
  nonzero_winding_shared_subsegment_test<double>();
}

TEST(delaunay_d, NonZeroWindingIslandInHole)
{
  nonzero_winding_island_in_hole_test<double>();
}

TEST(delaunay_d, NonZeroWindingCoincidentVerts)
{
  nonzero_winding_coincident_verts_test<double>();
}

TEST(delaunay_d, NonZeroWindingManyCrossings)
{
  nonzero_winding_many_crossings_test<double>();
}

TEST(delaunay_d, NonZeroWindingNegativeOnly)
{
  nonzero_winding_negative_only_test<double>();
}

TEST(delaunay_d, NonZeroWindingTJunction)
{
  nonzero_winding_tjunction_test<double>();
}

TEST(delaunay_d, NonZeroWindingExactSharedEdge)
{
  nonzero_winding_exact_shared_edge_test<double>();
}

TEST(delaunay_d, CrossSegs)
{
  crosssegs_test<double>();
}

TEST(delaunay_d, CutAcrossTri)
{
  cutacrosstri_test<double>();
}

TEST(delaunay_d, DiamondCross)
{
  diamondcross_test<double>();
}

TEST(delaunay_d, TwoDiamondsCross)
{
  twodiamondscross_test<double>();
}

TEST(delaunay_d, ManyCross)
{
  manycross_test<double>();
}

TEST(delaunay_d, TwoFace)
{
  twoface_test<double>();
}

TEST(delaunay_d, TwoFace2)
{
  twoface2_test<double>();
}

TEST(delaunay_d, OverlapFaces)
{
  overlapfaces_test<double>();
}

TEST(delaunay_d, TwoSquaresOverlap)
{
  twosquaresoverlap_test<double>();
}

TEST(delaunay_d, TwoFaceEdgeOverlap)
{
  twofaceedgeoverlap_test<double>();
}

TEST(delaunay_d, TriInTri)
{
  triintri_test<double>();
}

TEST(delaunay_d, DiamondInSquare)
{
  diamondinsquare_test<double>();
}

TEST(delaunay_d, DiamondInSquareWire)
{
  diamondinsquarewire_test<double>();
}

TEST(delaunay_d, RepeatEdge)
{
  repeatedge_test<double>();
}

TEST(delaunay_d, RepeatTri)
{
  repeattri_test<double>();
}

TEST(delaunay_d, SquareO)
{
  square_o_test<double>();
}

#  ifdef WITH_GMP
TEST(delaunay_m, Empty)
{
  empty_test<mpq_class>();
}

TEST(delaunay_m, OnePt)
{
  onept_test<mpq_class>();
}
TEST(delaunay_m, TwoPt)
{
  twopt_test<mpq_class>();
}

TEST(delaunay_m, ThreePt)
{
  threept_test<mpq_class>();
}

TEST(delaunay_m, MixedPts)
{
  mixedpts_test<mpq_class>();
}

TEST(delaunay_m, Quad0)
{
  quad0_test<mpq_class>();
}

TEST(delaunay_m, Quad1)
{
  quad1_test<mpq_class>();
}

TEST(delaunay_m, Quad2)
{
  quad2_test<mpq_class>();
}

TEST(delaunay_m, Quad3)
{
  quad3_test<mpq_class>();
}

TEST(delaunay_m, Quad4)
{
  quad4_test<mpq_class>();
}

TEST(delaunay_m, LineInSquare)
{
  lineinsquare_test<mpq_class>();
}

TEST(delaunay_m, LineHoleInSquare)
{
  lineholeinsquare_test<mpq_class>();
}

TEST(delaunay_m, NestedHoles)
{
  nestedholes_test<mpq_class>();
}

TEST(delaunay_m, NonZeroWinding)
{
  nonzero_winding_test<mpq_class>();
}

TEST(delaunay_m, NonZeroWindingNested)
{
  nonzero_winding_nested_test<mpq_class>();
}

TEST(delaunay_m, NonZeroWindingNestedUnion)
{
  nonzero_winding_nested_union_test<mpq_class>();
}

TEST(delaunay_m, NonZeroWindingMultiFaceEdge)
{
  nonzero_winding_multi_face_edge_test<mpq_class>();
}

TEST(delaunay_m, NonZeroWindingMultiFaceEdgeMixed)
{
  nonzero_winding_multi_face_edge_mixed_test<mpq_class>();
}

TEST(delaunay_m, NonZeroWindingCancelToZero)
{
  nonzero_winding_cancel_to_zero_test<mpq_class>();
}

TEST(delaunay_m, NonZeroWindingHighCount)
{
  nonzero_winding_high_count_test<mpq_class>();
}

TEST(delaunay_m, NonZeroWindingFan)
{
  nonzero_winding_fan_test<mpq_class>();
}

TEST(delaunay_m, NonZeroWindingEdgeSplit)
{
  nonzero_winding_edge_split_test<mpq_class>();
}

TEST(delaunay_m, NonZeroWindingSelfIntersect)
{
  nonzero_winding_self_intersect_test<mpq_class>();
}

TEST(delaunay_m, NonZeroWindingDeepNest)
{
  nonzero_winding_deep_nest_test<mpq_class>();
}

TEST(delaunay_m, NonZeroWindingSharedSubsegment)
{
  nonzero_winding_shared_subsegment_test<mpq_class>();
}

TEST(delaunay_m, NonZeroWindingIslandInHole)
{
  nonzero_winding_island_in_hole_test<mpq_class>();
}

TEST(delaunay_m, NonZeroWindingCoincidentVerts)
{
  nonzero_winding_coincident_verts_test<mpq_class>();
}

TEST(delaunay_m, NonZeroWindingManyCrossings)
{
  nonzero_winding_many_crossings_test<mpq_class>();
}

TEST(delaunay_m, NonZeroWindingNegativeOnly)
{
  nonzero_winding_negative_only_test<mpq_class>();
}

TEST(delaunay_m, NonZeroWindingTJunction)
{
  nonzero_winding_tjunction_test<mpq_class>();
}

TEST(delaunay_m, NonZeroWindingExactSharedEdge)
{
  nonzero_winding_exact_shared_edge_test<mpq_class>();
}

TEST(delaunay_m, CrossSegs)
{
  crosssegs_test<mpq_class>();
}

TEST(delaunay_m, CutAcrossTri)
{
  cutacrosstri_test<mpq_class>();
}

TEST(delaunay_m, DiamondCross)
{
  diamondcross_test<mpq_class>();
}

TEST(delaunay_m, TwoDiamondsCross)
{
  twodiamondscross_test<mpq_class>();
}

TEST(delaunay_m, ManyCross)
{
  manycross_test<mpq_class>();
}

TEST(delaunay_m, TwoFace)
{
  twoface_test<mpq_class>();
}

TEST(delaunay_m, TwoFace2)
{
  twoface2_test<mpq_class>();
}

TEST(delaunay_m, OverlapFaces)
{
  overlapfaces_test<mpq_class>();
}

TEST(delaunay_m, TwoSquaresOverlap)
{
  twosquaresoverlap_test<mpq_class>();
}

TEST(delaunay_m, TwoFaceEdgeOverlap)
{
  twofaceedgeoverlap_test<mpq_class>();
}

TEST(delaunay_m, TriInTri)
{
  triintri_test<mpq_class>();
}

TEST(delaunay_m, DiamondInSquare)
{
  diamondinsquare_test<mpq_class>();
}

TEST(delaunay_m, DiamondInSquareWire)
{
  diamondinsquarewire_test<mpq_class>();
}

TEST(delaunay_m, RepeatEdge)
{
  repeatedge_test<mpq_class>();
}

TEST(delaunay_m, RepeatTri)
{
  repeattri_test<mpq_class>();
}
#  endif
#endif

#if DO_TEXT_TESTS
template<typename T>
void text_test(
    int arc_points_num, int lets_per_line_num, int lines_num, CDT_output_type otype, bool need_ids)
{
  constexpr bool print_timing = true;
  /*
   * Make something like a letter B:
   *
   *    4------------3
   *    |              )
   *    |  12--11       )
   *    |  |     ) a3    ) a1
   *    |  9---10       )
   *    |              )
   *    |            2
   *    |              )
   *    |  8----7       )
   *    |  |     ) a2    ) a0
   *    |  5----6       )
   *    |              )
   *    0------------1
   *
   * Where the numbers are the first 13 vertices, and the rest of
   * the vertices are in arcs a0, a1, a2, a3, each of which have
   * arc_points_num per arc in them.
   */

  const char *b_before_arcs = R"(13 0 3
  0.0 0.0
  1.0 0.0
  1.0 1.5
  1.0 3.0
  0.0 3.0
  0.2 0.2
  0.6 0.2
  0.6 1.4
  0.2 1.4
  0.2 1.6
  0.6 1.6
  0.6 2.8
  0.2 2.8
  3 4 0 1 2
  6 5 8 7
  10 9 12 11
  )";

  CDT_input<T> b_before_arcs_in = fill_input_from_string<T>(b_before_arcs);
  constexpr int narcs = 4;
  int b_npts = b_before_arcs_in.vert.size() + narcs * arc_points_num;
  constexpr int b_nfaces = 3;
  Array<VecBase<T, 2>> b_vert(b_npts);
  Array<Vector<int>> b_face(b_nfaces);
  std::copy(b_before_arcs_in.vert.begin(), b_before_arcs_in.vert.end(), b_vert.begin());
  std::copy(b_before_arcs_in.face.begin(), b_before_arcs_in.face.end(), b_face.begin());
  if (arc_points_num > 0) {
    b_face[0].pop_last(); /* We'll add center point back between arcs for outer face. */
    for (int arc = 0; arc < narcs; ++arc) {
      int arc_origin_vert;
      int arc_terminal_vert;
      bool ccw;
      switch (arc) {
        case 0:
          arc_origin_vert = 1;
          arc_terminal_vert = 2;
          ccw = true;
          break;
        case 1:
          arc_origin_vert = 2;
          arc_terminal_vert = 3;
          ccw = true;
          break;
        case 2:
          arc_origin_vert = 7;
          arc_terminal_vert = 6;
          ccw = false;
          break;
        case 3:
          arc_origin_vert = 11;
          arc_terminal_vert = 10;
          ccw = false;
          break;
        default:
          BLI_assert(false);
      }
      VecBase<T, 2> start_co = b_vert[arc_origin_vert];
      VecBase<T, 2> end_co = b_vert[arc_terminal_vert];
      VecBase<T, 2> center_co = 0.5 * (start_co + end_co);
      BLI_assert(start_co[0] == end_co[0]);
      double radius = abs(math_to_double<T>(end_co[1] - center_co[1]));
      double angle_delta = M_PI / (arc_points_num + 1);
      int start_vert = b_before_arcs_in.vert.size() + arc * arc_points_num;
      Vector<int> &face = b_face[(arc <= 1) ? 0 : arc - 1];
      for (int i = 0; i < arc_points_num; ++i) {
        VecBase<T, 2> delta;
        float ang = ccw ? (-M_PI_2 + (i + 1) * angle_delta) : (M_PI_2 - (i + 1) * angle_delta);
        delta[0] = T(radius * cos(ang));
        delta[1] = T(radius * sin(ang));
        b_vert[start_vert + i] = center_co + delta;
        face.append(start_vert + i);
      }
      if (arc == 0) {
        face.append(arc_terminal_vert);
      }
    }
  }

  CDT_input<T> in;
  int tot_instances = lets_per_line_num * lines_num;
  if (tot_instances == 1) {
    in.vert = b_vert;
    in.face = b_face;
  }
  else {
    in.vert = Array<VecBase<T, 2>>(tot_instances * b_vert.size());
    in.face = Array<Vector<int>>(tot_instances * b_face.size());
    T cur_x = T(0);
    T cur_y = T(0);
    T delta_x = T(2);
    T delta_y = T(3.25);
    int instance = 0;
    for (int line = 0; line < lines_num; ++line) {
      for (int let = 0; let < lets_per_line_num; ++let) {
        VecBase<T, 2> co_offset(cur_x, cur_y);
        int in_v_offset = instance * b_vert.size();
        for (int v = 0; v < b_vert.size(); ++v) {
          in.vert[in_v_offset + v] = b_vert[v] + co_offset;
        }
        int in_f_offset = instance * b_face.size();
        for (int f : b_face.index_range()) {
          for (int fv : b_face[f]) {
            in.face[in_f_offset + f].append(in_v_offset + fv);
          }
        }
        cur_x += delta_x;
        ++instance;
      }
      cur_y += delta_y;
      cur_x = T(0);
    }
  }
  in.epsilon = b_before_arcs_in.epsilon;
  in.need_ids = need_ids;
  double tstart = BLI_time_now_seconds();
  CDT_result<T> out = delaunay_2d_calc(in, otype);
  double tend = BLI_time_now_seconds();
  if (print_timing) {
    std::cout << "time = " << tend - tstart << "\n";
  }
  if (!need_ids) {
    EXPECT_EQ(out.vert_orig.size(), 0);
    EXPECT_EQ(out.edge_orig.size(), 0);
    EXPECT_EQ(out.face_orig.size(), 0);
  }
  if (DO_DRAW) {
    std::string label = "Text arcpts=" + std::to_string(arc_points_num);
    if (lets_per_line_num > 1) {
      label += " linelen=" + std::to_string(lets_per_line_num);
    }
    if (lines_num > 1) {
      label += " lines=" + std::to_string(lines_num);
    }
    if (!need_ids) {
      label += " no_ids";
    }
    if (otype != CDT_INSIDE_WITH_HOLES) {
      label += " otype=" + std::to_string(otype);
    }
    graph_draw<T>(label, out.vert, out.edge, out.face);
  }
}

TEST(delaunay_d, TextB10)
{
  text_test<double>(10, 1, 1, CDT_INSIDE_WITH_HOLES, true);
}

TEST(delaunay_d, TextB10_noids)
{
  text_test<double>(10, 1, 1, CDT_INSIDE_WITH_HOLES, false);
}

TEST(delaunay_d, TextB10_inside)
{
  text_test<double>(10, 1, 1, CDT_INSIDE, true);
}

TEST(delaunay_d, TextB10_inside_noids)
{
  text_test<double>(10, 1, 1, CDT_INSIDE, false);
}

TEST(delaunay_d, TextB10_constraints)
{
  text_test<double>(10, 1, 1, CDT_CONSTRAINTS, true);
}

TEST(delaunay_d, TextB10_constraints_noids)
{
  text_test<double>(10, 1, 1, CDT_CONSTRAINTS, false);
}

TEST(delaunay_d, TextB10_constraints_valid_bmesh)
{
  text_test<double>(10, 1, 1, CDT_CONSTRAINTS_VALID_BMESH, true);
}

TEST(delaunay_d, TextB10_constraints_valid_bmesh_noids)
{
  text_test<double>(10, 1, 1, CDT_CONSTRAINTS_VALID_BMESH, false);
}

TEST(delaunay_d, TextB10_constraints_valid_bmesh_with_holes)
{
  text_test<double>(10, 1, 1, CDT_CONSTRAINTS_VALID_BMESH_WITH_HOLES, true);
}

TEST(delaunay_d, TextB10_constraints_valid_bmesh_with_holes_noids)
{
  text_test<double>(10, 1, 1, CDT_CONSTRAINTS_VALID_BMESH_WITH_HOLES, false);
}

TEST(delaunay_d, TextB200)
{
  text_test<double>(200, 1, 1, CDT_INSIDE_WITH_HOLES, true);
}

TEST(delaunay_d, TextB10_10_10)
{
  text_test<double>(10, 10, 10, CDT_INSIDE_WITH_HOLES, true);
}

TEST(delaunay_d, TextB10_10_10_noids)
{
  text_test<double>(10, 10, 10, CDT_INSIDE_WITH_HOLES, false);
}

#  ifdef WITH_GMP
TEST(delaunay_m, TextB10)
{
  text_test<mpq_class>(10, 1, 1, CDT_INSIDE_WITH_HOLES, true);
}

TEST(delaunay_m, TextB200)
{
  text_test<mpq_class>(200, 1, 1, CDT_INSIDE_WITH_HOLES, true);
}

TEST(delaunay_m, TextB10_10_10)
{
  text_test<mpq_class>(10, 10, 10, CDT_INSIDE_WITH_HOLES, true);
}

TEST(delaunay_m, TextB10_10_10_noids)
{
  text_test<mpq_class>(10, 10, 10, CDT_INSIDE_WITH_HOLES, false);
}
#  endif

#endif

#if DO_RANDOM_TESTS

enum {
  RANDOM_PTS,
  RANDOM_SEGS,
  RANDOM_POLY,
  RANDOM_TILTED_GRID,
  RANDOM_CIRCLE,
  RANDOM_TRI_BETWEEN_CIRCLES,
};

template<typename T>
void rand_delaunay_test(int test_kind,
                        int start_lg_size,
                        int max_lg_size,
                        int reps_per_size,
                        double param,
                        CDT_output_type otype)
{
  constexpr bool print_timing = true;
  RNG *rng = BLI_rng_new(0);
  Array<double> times(max_lg_size + 1);

  /* For powers of 2 sizes up to max_lg_size power of 2. */
  for (int lg_size = start_lg_size; lg_size <= max_lg_size; ++lg_size) {
    int size = 1 << lg_size;
    times[lg_size] = 0.0;
    if (size == 1 && test_kind != RANDOM_PTS) {
      continue;
    }
    /* Do 'rep' repetitions. */
    for (int rep = 0; rep < reps_per_size; ++rep) {
      /* First use test type and size to set npts, nedges, and nfaces. */
      int npts = 0;
      int nedges = 0;
      int nfaces = 0;
      std::string test_label;
      switch (test_kind) {
        case RANDOM_PTS: {
          npts = size;
          test_label = std::to_string(npts) + "Random points";
          break;
        }
        case RANDOM_SEGS: {
          npts = size;
          nedges = npts - 1;
          test_label = std::to_string(nedges) + "Random edges";
          break;
        }
        case RANDOM_POLY: {
          npts = size;
          nedges = npts;
          test_label = "Random poly with " + std::to_string(nedges) + " edges";
          break;
        }
        case RANDOM_TILTED_GRID: {
          /* A 'size' x 'size' grid of points, tilted by angle 'param'.
           * Edges will go from left ends to right ends and tops to bottoms,
           * so 2 x size of them.
           * Depending on epsilon, the vertical-ish edges may or may not go
           * through the intermediate vertices, but the horizontal ones always should.
           * 'param' is slope of tilt of vertical lines.
           */
          npts = size * size;
          nedges = 2 * size;
          test_label = "Tilted grid " + std::to_string(npts) + "x" + std::to_string(npts) +
                       " (tilt=" + std::to_string(param) + ")";
          break;
        }
        case RANDOM_CIRCLE: {
          /* A circle with 'size' points, a random start angle,
           * and equal spacing thereafter. Will be input as one face.
           */
          npts = size;
          nfaces = 1;
          test_label = "Circle with " + std::to_string(npts) + " points";
          break;
        }
        case RANDOM_TRI_BETWEEN_CIRCLES: {
          /* A set of 'size' triangles, each has two random points on the unit circle,
           * and the third point is a random point on the circle with radius 'param'.
           * Each triangle will be input as a face.
           */
          npts = 3 * size;
          nfaces = size;
          test_label = "Random " + std::to_string(nfaces) +
                       " triangles between circles (inner radius=" + std::to_string(param) + ")";
          break;
        }
        default:
          std::cout << "unknown delaunay test type\n";
          BLI_rng_free(rng);
          return;
      }
      if (otype != CDT_FULL) {
        if (otype == CDT_INSIDE) {
          test_label += " (inside)";
        }
        else if (otype == CDT_CONSTRAINTS) {
          test_label += " (constraints)";
        }
        else if (otype == CDT_CONSTRAINTS_VALID_BMESH) {
          test_label += " (valid bmesh)";
        }
      }

      CDT_input<T> in;
      in.vert = Array<VecBase<T, 2>>(npts);
      if (nedges > 0) {
        in.edge = Array<std::pair<int, int>>(nedges);
      }
      if (nfaces > 0) {
        in.face = Array<Vector<int>>(nfaces);
      }

      /* Make vertices and edges or faces. */
      switch (test_kind) {
        case RANDOM_PTS:
        case RANDOM_SEGS:
        case RANDOM_POLY: {
          for (int i = 0; i < size; i++) {
            in.vert[i][0] = T(BLI_rng_get_double(rng)); /* will be in range in [0,1) */
            in.vert[i][1] = T(BLI_rng_get_double(rng));
            if (test_kind != RANDOM_PTS) {
              if (i > 0) {
                in.edge[i - 1].first = i - 1;
                in.edge[i - 1].second = i;
              }
            }
          }
          if (test_kind == RANDOM_POLY) {
            in.edge[size - 1].first = size - 1;
            in.edge[size - 1].second = 0;
          }
          break;
        }
        case RANDOM_TILTED_GRID: {
          for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
              in.vert[i * size + j][0] = T(i * param + j);
              in.vert[i * size + j][1] = T(i);
            }
          }
          for (int i = 0; i < size; ++i) {
            /* Horizontal edges: connect `p(i,0)` to `p(i,size-1)`. */
            in.edge[i].first = i * size;
            in.edge[i].second = i * size + size - 1;
            /* Vertical edges: connect `p(0,i)` to `p(size-1,i)`. */
            in.edge[size + i].first = i;
            in.edge[size + i].second = (size - 1) * size + i;
          }
          break;
        }
        case RANDOM_CIRCLE: {
          double start_angle = BLI_rng_get_double(rng) * 2.0 * M_PI;
          double angle_delta = 2.0 * M_PI / size;
          for (int i = 0; i < size; i++) {
            in.vert[i][0] = T(cos(start_angle + i * angle_delta));
            in.vert[i][1] = T(sin(start_angle + i * angle_delta));
            in.face[0].append(i);
          }
          break;
        }
        case RANDOM_TRI_BETWEEN_CIRCLES: {
          for (int i = 0; i < size; i++) {
            /* Get three random angles in [0, 2pi). */
            double angle1 = BLI_rng_get_double(rng) * 2.0 * M_PI;
            double angle2 = BLI_rng_get_double(rng) * 2.0 * M_PI;
            double angle3 = BLI_rng_get_double(rng) * 2.0 * M_PI;
            int ia = 3 * i;
            int ib = 3 * i + 1;
            int ic = 3 * i + 2;
            in.vert[ia][0] = T(cos(angle1));
            in.vert[ia][1] = T(sin(angle1));
            in.vert[ib][0] = T(cos(angle2));
            in.vert[ib][1] = T(sin(angle2));
            in.vert[ic][0] = T((param * cos(angle3)));
            in.vert[ic][1] = T((param * sin(angle3)));
            /* Put the coordinates in CCW order. */
            in.face[i].append(ia);
            int orient = orient2d(in.vert[ia], in.vert[ib], in.vert[ic]);
            if (orient >= 0) {
              in.face[i].append(ib);
              in.face[i].append(ic);
            }
            else {
              in.face[i].append(ic);
              in.face[i].append(ib);
            }
          }
          break;
        }
      }

      /* Run the test. */
      double tstart = BLI_time_now_seconds();
      CDT_result<T> out = delaunay_2d_calc(in, otype);
      EXPECT_NE(out.vert.size(), 0);
      times[lg_size] += BLI_time_now_seconds() - tstart;
      if (DO_DRAW) {
        graph_draw<T>(test_label, out.vert, out.edge, out.face);
      }
    }
  }
  if (print_timing) {
    std::cout << "\nsize,time\n";
    for (int lg_size = 0; lg_size <= max_lg_size; lg_size++) {
      int size = 1 << lg_size;
      std::cout << size << "," << times[lg_size] << "\n";
    }
  }
  BLI_rng_free(rng);
}

TEST(delaunay_d, RandomPts)
{
  rand_delaunay_test<double>(RANDOM_PTS, 0, 7, 1, 0.0, CDT_FULL);
}

TEST(delaunay_d, RandomSegs)
{
  rand_delaunay_test<double>(RANDOM_SEGS, 1, 7, 1, 0.0, CDT_FULL);
}

TEST(delaunay_d, RandomPoly)
{
  rand_delaunay_test<double>(RANDOM_POLY, 1, 7, 1, 0.0, CDT_FULL);
}

TEST(delaunay_d, RandomPolyConstraints)
{
  rand_delaunay_test<double>(RANDOM_POLY, 1, 7, 1, 0.0, CDT_CONSTRAINTS);
}

TEST(delaunay_d, RandomPolyValidBmesh)
{
  rand_delaunay_test<double>(RANDOM_POLY, 1, 7, 1, 0.0, CDT_CONSTRAINTS_VALID_BMESH);
}

TEST(delaunay_d, Grid)
{
  rand_delaunay_test<double>(RANDOM_TILTED_GRID, 1, 6, 1, 0.0, CDT_FULL);
}

TEST(delaunay_d, TiltedGridA)
{
  rand_delaunay_test<double>(RANDOM_TILTED_GRID, 1, 6, 1, 1.0, CDT_FULL);
}

TEST(delaunay_d, TiltedGridB)
{
  rand_delaunay_test<double>(RANDOM_TILTED_GRID, 1, 6, 1, 0.01, CDT_FULL);
}

TEST(delaunay_d, RandomCircle)
{
  rand_delaunay_test<double>(RANDOM_CIRCLE, 1, 7, 1, 0.0, CDT_FULL);
}

TEST(delaunay_d, RandomTrisCircle)
{
  rand_delaunay_test<double>(RANDOM_TRI_BETWEEN_CIRCLES, 1, 6, 1, 0.25, CDT_FULL);
}

TEST(delaunay_d, RandomTrisCircleB)
{
  rand_delaunay_test<double>(RANDOM_TRI_BETWEEN_CIRCLES, 1, 6, 1, 1e-4, CDT_FULL);
}

#  ifdef WITH_GMP
TEST(delaunay_m, RandomPts)
{
  rand_delaunay_test<mpq_class>(RANDOM_PTS, 0, 7, 1, 0.0, CDT_FULL);
}

TEST(delaunay_m, RandomSegs)
{
  rand_delaunay_test<mpq_class>(RANDOM_SEGS, 1, 7, 1, 0.0, CDT_FULL);
}

TEST(delaunay_m, RandomPoly)
{
  rand_delaunay_test<mpq_class>(RANDOM_POLY, 1, 7, 1, 0.0, CDT_FULL);
}

TEST(delaunay_d, RandomPolyInside)
{
  rand_delaunay_test<double>(RANDOM_POLY, 1, 7, 1, 0.0, CDT_INSIDE);
}

TEST(delaunay_m, RandomPolyInside)
{
  rand_delaunay_test<mpq_class>(RANDOM_POLY, 1, 7, 1, 0.0, CDT_INSIDE);
}

TEST(delaunay_m, RandomPolyConstraints)
{
  rand_delaunay_test<mpq_class>(RANDOM_POLY, 1, 7, 1, 0.0, CDT_CONSTRAINTS);
}

TEST(delaunay_m, RandomPolyValidBmesh)
{
  rand_delaunay_test<mpq_class>(RANDOM_POLY, 1, 7, 1, 0.0, CDT_CONSTRAINTS_VALID_BMESH);
}

TEST(delaunay_m, Grid)
{
  rand_delaunay_test<mpq_class>(RANDOM_TILTED_GRID, 1, 6, 1, 0.0, CDT_FULL);
}

TEST(delaunay_m, TiltedGridA)
{
  rand_delaunay_test<mpq_class>(RANDOM_TILTED_GRID, 1, 6, 1, 1.0, CDT_FULL);
}

TEST(delaunay_m, TiltedGridB)
{
  rand_delaunay_test<mpq_class>(RANDOM_TILTED_GRID, 1, 6, 1, 0.01, CDT_FULL);
}

TEST(delaunay_m, RandomCircle)
{
  rand_delaunay_test<mpq_class>(RANDOM_CIRCLE, 1, 7, 1, 0.0, CDT_FULL);
}

TEST(delaunay_m, RandomTrisCircle)
{
  rand_delaunay_test<mpq_class>(RANDOM_TRI_BETWEEN_CIRCLES, 1, 6, 1, 0.25, CDT_FULL);
}

TEST(delaunay_m, RandomTrisCircleB)
{
  rand_delaunay_test<double>(RANDOM_TRI_BETWEEN_CIRCLES, 1, 6, 1, 1e-4, CDT_FULL);
}
#  endif

#endif

}  // namespace blender::meshintersect
