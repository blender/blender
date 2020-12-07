/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_math.h"
#include "BLI_rand.h"
#include "PIL_time.h"
}

#include <fstream>
#include <iostream>
#include <sstream>
#include <type_traits>

#define DO_CPP_TESTS 1
#define DO_C_TESTS 1
#define DO_RANDOM_TESTS 0

#include "BLI_array.hh"
#include "BLI_double2.hh"
#include "BLI_math_boolean.hh"
#include "BLI_math_mpq.hh"
#include "BLI_mpq2.hh"
#include "BLI_vector.hh"

#include "BLI_delaunay_2d.h"

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
  Array<vec2<T>> verts(nverts);
  Array<std::pair<int, int>> edges(nedges);
  Array<Vector<int>> faces(nfaces);
  int i = 0;
  while (i < nverts && getline(ss, line)) {
    std::istringstream iss(line);
    double dp0, dp1;
    iss >> dp0 >> dp1;
    T p0(dp0);
    T p1(dp1);
    verts[i] = vec2<T>(p0, p1);
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
static int get_orig_index(const Array<Vector<int>> &out_to_orig, int orig_index)
{
  int n = static_cast<int>(out_to_orig.size());
  for (int i = 0; i < n; ++i) {
    for (int orig : out_to_orig[i]) {
      if (orig == orig_index) {
        return i;
      }
    }
  }
  return -1;
}

template<typename T> static double math_to_double(const T UNUSED(v))
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
  int nv = static_cast<int>(out.vert.size());
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
  int ne = static_cast<int>(out.edge.size());
  for (int i = 0; i < ne; ++i) {
    if ((out.edge[i].first == out_index_1 && out.edge[i].second == out_index_2) ||
        (out.edge[i].first == out_index_2 && out.edge[i].second == out_index_1)) {
      return i;
    }
  }
  return -1;
}

template<typename T>
bool output_edge_has_input_id(const CDT_result<T> &out, int out_edge_index, int in_edge_index)
{
  return out_edge_index < static_cast<int>(out.edge_orig.size()) &&
         out.edge_orig[out_edge_index].contains(in_edge_index);
}

/* Which out face is for a give output vertex ngon? -1 if not found.
 * Allow for cyclic shifts vertices of one poly vs the other.
 */
template<typename T> int get_output_face_index(const CDT_result<T> &out, const Array<int> &poly)
{
  int nf = static_cast<int>(out.face.size());
  int npolyv = static_cast<int>(poly.size());
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
  return out_face_index < static_cast<int>(out.face_orig.size()) &&
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
    for (int j : r.edge_orig[i].size()) {
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
                const Array<vec2<T>> &verts,
                const Array<std::pair<int, int>> &edges,
                const Array<Vector<int>> &UNUSED(faces))
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
  constexpr bool draw_vert_labels = true;
  constexpr bool draw_edge_labels = false;

  if (verts.size() == 0) {
    return;
  }
  vec2<double> vmin(1e10, 1e10);
  vec2<double> vmax(-1e10, -1e10);
  for (const vec2<T> &v : verts) {
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
  int view_height = static_cast<int>(view_width * aspect);
  if (view_height > max_draw_height) {
    view_height = max_draw_height;
    view_width = static_cast<int>(view_height / aspect);
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

  for (const std::pair<int, int> &e : edges) {
    const vec2<T> &uco = verts[e.first];
    const vec2<T> &vco = verts[e.second];
    int strokew = thin_line;
    f << "<line fill=\"none\" stroke=\"black\" stroke-width=\"" << strokew << "\" x1=\""
      << SX(uco[0]) << "\" y1=\"" << SY(uco[1]) << "\" x2=\"" << SX(vco[0]) << "\" y2=\""
      << SY(vco[1]) << "\">\n";
    f << "  <title>[" << e.first << "][" << e.second << "]</title>\n";
    f << "</line>\n";
    if (draw_edge_labels) {
      f << "<text x=\"" << SX(0.5 * (uco[0] + vco[0])) << "\" y=\"" << SY(0.5 * (uco[1] + vco[1]))
        << "\" font-size=\"small\">";
      f << "[" << e.first << "][" << e.second << "]</text>\n";
    }
  }

  int i = 0;
  for (const vec2<T> &vco : verts) {
    f << "<circle fill=\"black\" cx=\"" << SX(vco[0]) << "\" cy=\"" << SY(vco[1]) << "\" r=\""
      << vert_radius << "\">\n";
    f << "  <title>[" << i << "]" << vco << "</title>\n";
    f << "</circle>\n";
    if (draw_vert_labels) {
      f << "<text x=\"" << SX(vco[0]) + vert_radius << "\" y=\"" << SY(vco[1]) - vert_radius
        << "\" font-size=\"small\">[" << i << "]</text>\n";
    }
    ++i;
  }

  draw_append = true;
#undef SX
#undef SY
}

/* Should tests draw their output to an html file? */
constexpr bool DO_DRAW = false;

template<typename T> void expect_coord_near(const vec2<T> &testco, const vec2<T> &refco);

#ifdef WITH_GMP
template<>
void expect_coord_near<mpq_class>(const vec2<mpq_class> &testco, const vec2<mpq_class> &refco)
{
  EXPECT_EQ(testco[0], refco[0]);
  EXPECT_EQ(testco[0], refco[0]);
}
#endif

template<> void expect_coord_near<double>(const vec2<double> &testco, const vec2<double> &refco)
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
    expect_coord_near<T>(out.vert[0], vec2<T>(0, 0));
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
    expect_coord_near<T>(out.vert[v0_out], vec2<T>(0.0, -0.75));
    expect_coord_near<T>(out.vert[v1_out], vec2<T>(0.0, 0.75));
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
      expect_coord_near<T>(out.vert[v_intersect], vec2<T>(0, 0));
    }
  }
  if (DO_DRAW) {
    graph_draw<T>("CrossSegs", out.vert, out.edge, out.face);
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
    /* Input verts have no dups, so expect output ones match input ones. */
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
    expect_coord_near<T>(out.vert[v_int1], vec2<T>(1, 0.5));
    expect_coord_near<T>(out.vert[v_int2], vec2<T>(0.5, 1));
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

  CDT_result<T> out3 = delaunay_2d_calc(in, CDT_CONSTRAINTS);
  EXPECT_EQ(out3.face.size(), 4);
  if (DO_DRAW) {
    graph_draw<T>("OverlapFaces - constraints", out3.vert, out3.edge, out3.face);
  }

  CDT_result<T> out4 = delaunay_2d_calc(in, CDT_CONSTRAINTS_VALID_BMESH);
  EXPECT_EQ(out4.face.size(), 5);
  if (DO_DRAW) {
    graph_draw<T>("OverlapFaces - valid bmesh", out4.vert, out4.edge, out4.face);
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

TEST(delaunay_d, CrossSegs)
{
  crosssegs_test<double>();
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

TEST(delaunay_m, CrossSegs)
{
  crosssegs_test<mpq_class>();
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

#if DO_C_TESTS

TEST(delaunay_d, CintTwoFace)
{
  float vert_coords[][2] = {
      {0.0, 0.0}, {1.0, 0.0}, {0.5, 1.0}, {1.1, 1.0}, {1.1, 0.0}, {1.6, 1.0}};
  int faces[] = {0, 1, 2, 3, 4, 5};
  int faces_len[] = {3, 3};
  int faces_start[] = {0, 3};

  ::CDT_input input;
  input.verts_len = 6;
  input.edges_len = 0;
  input.faces_len = 2;
  input.vert_coords = vert_coords;
  input.edges = nullptr;
  input.faces = faces;
  input.faces_len_table = faces_len;
  input.faces_start_table = faces_start;
  input.epsilon = 1e-5f;
  ::CDT_result *output = BLI_delaunay_2d_cdt_calc(&input, CDT_FULL);
  BLI_delaunay_2d_cdt_free(output);
}
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
        } break;
        case RANDOM_SEGS: {
          npts = size;
          nedges = npts - 1;
          test_label = std::to_string(nedges) + "Random edges";
        } break;
        case RANDOM_POLY: {
          npts = size;
          nedges = npts;
          test_label = "Random poly with " + std::to_string(nedges) + " edges";
        } break;
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
        } break;
        case RANDOM_CIRCLE: {
          /* A circle with 'size' points, a random start angle,
           * and equal spacing thereafter. Will be input as one face.
           */
          npts = size;
          nfaces = 1;
          test_label = "Circle with " + std::to_string(npts) + " points";
        } break;
        case RANDOM_TRI_BETWEEN_CIRCLES: {
          /* A set of 'size' triangles, each has two random points on the unit circle,
           * and the third point is a random point on the circle with radius 'param'.
           * Each triangle will be input as a face.
           */
          npts = 3 * size;
          nfaces = size;
          test_label = "Random " + std::to_string(nfaces) +
                       " triangles between circles (inner radius=" + std::to_string(param) + ")";
        } break;
        default:
          std::cout << "unknown delaunay test type\n";
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
      in.vert = Array<vec2<T>>(npts);
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
        } break;

        case RANDOM_TILTED_GRID: {
          for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
              in.vert[i * size + j][0] = T(i * param + j);
              in.vert[i * size + j][1] = T(i);
            }
          }
          for (int i = 0; i < size; ++i) {
            /* Horizontal edges: connect p(i,0) to p(i,size-1). */
            in.edge[i].first = i * size;
            in.edge[i].second = i * size + size - 1;
            /* Vertical edges: conntect p(0,i) to p(size-1,i). */
            in.edge[size + i].first = i;
            in.edge[size + i].second = (size - 1) * size + i;
          }
        } break;

        case RANDOM_CIRCLE: {
          double start_angle = BLI_rng_get_double(rng) * 2.0 * M_PI;
          double angle_delta = 2.0 * M_PI / size;
          for (int i = 0; i < size; i++) {
            in.vert[i][0] = T(cos(start_angle + i * angle_delta));
            in.vert[i][1] = T(sin(start_angle + i * angle_delta));
            in.face[0].append(i);
          }
        } break;

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
            /* Put the coordinates in ccw order. */
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
        } break;
      }

      /* Run the test. */
      double tstart = PIL_check_seconds_timer();
      CDT_result<T> out = delaunay_2d_calc(in, otype);
      EXPECT_NE(out.vert.size(), 0);
      times[lg_size] += PIL_check_seconds_timer() - tstart;
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
