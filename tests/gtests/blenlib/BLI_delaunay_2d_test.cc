/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_math.h"
#include "BLI_rand.h"
#include "PIL_time.h"

#include "BLI_delaunay_2d.h"
}

#include <iostream>
#include <fstream>
#include <sstream>

#define DO_REGULAR_TESTS 1
#define DO_RANDOM_TESTS 0
#define DO_FILE_TESTS 0

static void fill_input_verts(CDT_input *r_input, float (*vcos)[2], int nverts)
{
  r_input->verts_len = nverts;
  r_input->edges_len = 0;
  r_input->faces_len = 0;
  r_input->vert_coords = vcos;
  r_input->edges = NULL;
  r_input->faces = NULL;
  r_input->faces_start_table = NULL;
  r_input->faces_len_table = NULL;
  r_input->epsilon = 1e-5f;
  r_input->skip_input_modify = false;
}

static void add_input_edges(CDT_input *r_input, int (*edges)[2], int nedges)
{
  r_input->edges_len = nedges;
  r_input->edges = edges;
}

static void add_input_faces(
    CDT_input *r_input, int *faces, int *faces_start_table, int *faces_len_table, int nfaces)
{
  r_input->faces_len = nfaces;
  r_input->faces = faces;
  r_input->faces_start_table = faces_start_table;
  r_input->faces_len_table = faces_len_table;
}

/* The spec should have the form:
 * #verts #edges #faces
 * <float> <float>   [#verts lines)
 * <int> <int>   [#edges lines]
 * <int> <int> ... <int>   [#faces lines]
 */
static void fill_input_from_string(CDT_input *r_input, const char *spec)
{
  std::string line;
  std::vector<std::vector<int>> faces;
  int i, j;
  int nverts, nedges, nfaces;
  float(*p)[2];
  int(*e)[2];
  int *farr;
  int *flen;
  int *fstart;

  std::istringstream ss(spec);
  getline(ss, line);
  std::istringstream hdrss(line);
  hdrss >> nverts >> nedges >> nfaces;
  if (nverts == 0) {
    return;
  }
  p = (float(*)[2])MEM_malloc_arrayN(nverts, 2 * sizeof(float), __func__);
  if (nedges > 0) {
    e = (int(*)[2])MEM_malloc_arrayN(nedges, 2 * sizeof(int), __func__);
  }
  if (nfaces > 0) {
    flen = (int *)MEM_malloc_arrayN(nfaces, sizeof(int), __func__);
    fstart = (int *)MEM_malloc_arrayN(nfaces, sizeof(int), __func__);
  }
  i = 0;
  while (i < nverts && getline(ss, line)) {
    std::istringstream iss(line);
    iss >> p[i][0] >> p[i][1];
    i++;
  }
  i = 0;
  while (i < nedges && getline(ss, line)) {
    std::istringstream ess(line);
    ess >> e[i][0] >> e[i][1];
    i++;
  }
  i = 0;
  while (i < nfaces && getline(ss, line)) {
    std::istringstream fss(line);
    int v;
    faces.push_back(std::vector<int>());
    while (fss >> v) {
      faces[i].push_back(v);
    }
    i++;
  }
  fill_input_verts(r_input, p, nverts);
  if (nedges > 0) {
    add_input_edges(r_input, e, nedges);
  }
  if (nfaces > 0) {
    for (i = 0; i < nfaces; i++) {
      flen[i] = (int)faces[i].size();
      if (i == 0) {
        fstart[i] = 0;
      }
      else {
        fstart[i] = fstart[i - 1] + flen[i - 1];
      }
    }
    farr = (int *)MEM_malloc_arrayN(fstart[nfaces - 1] + flen[nfaces - 1], sizeof(int), __func__);
    for (i = 0; i < nfaces; i++) {
      for (j = 0; j < (int)faces[i].size(); j++) {
        farr[fstart[i] + j] = faces[i][j];
      }
    }
    add_input_faces(r_input, farr, fstart, flen, nfaces);
  }
}

static void fill_input_from_file(CDT_input *in, const char *filename)
{
  std::FILE *fp = std::fopen(filename, "rb");
  if (fp) {
    std::string contents;
    std::fseek(fp, 0, SEEK_END);
    contents.resize(std::ftell(fp));
    std::rewind(fp);
    std::fread(&contents[0], 1, contents.size(), fp);
    std::fclose(fp);
    fill_input_from_string(in, contents.c_str());
  }
  else {
    printf("couldn't open file %s\n", filename);
  }
}

static void free_spec_arrays(CDT_input *in)
{
  if (in->vert_coords) {
    MEM_freeN(in->vert_coords);
  }
  if (in->edges) {
    MEM_freeN(in->edges);
  }
  if (in->faces_len_table) {
    MEM_freeN(in->faces_len_table);
    MEM_freeN(in->faces_start_table);
    MEM_freeN(in->faces);
  }
}

/* which output vert index goes with given input vertex? -1 if not found */
static int get_output_vert_index(const CDT_result *r, int in_index)
{
  int i, j;

  for (i = 0; i < r->verts_len; i++) {
    for (j = 0; j < r->verts_orig_len_table[i]; j++) {
      if (r->verts_orig[r->verts_orig_start_table[i] + j] == in_index) {
        return i;
      }
    }
  }
  return -1;
}

/* which output edge index is for given output vert indices? */
static int get_edge(const CDT_result *r, int out_index_1, int out_index_2)
{
  int i;

  for (i = 0; i < r->edges_len; i++) {
    if ((r->edges[i][0] == out_index_1 && r->edges[i][1] == out_index_2) ||
        (r->edges[i][0] == out_index_2 && r->edges[i][1] == out_index_1))
      return i;
  }
  return -1;
}

/* return true if given output edge has given input edge id in its originals list */
static bool out_edge_has_input_id(const CDT_result *r, int out_edge_index, int in_edge_index)
{
  if (r->edges_orig == NULL)
    return false;
  if (out_edge_index < 0 || out_edge_index >= r->edges_len)
    return false;
  for (int i = 0; i < r->edges_orig_len_table[out_edge_index]; i++) {
    if (r->edges_orig[r->edges_orig_start_table[out_edge_index] + i] == in_edge_index)
      return true;
  }
  return false;
}

/* which face is for given output vertex ngon? */
static int get_face(const CDT_result *r, int *out_indices, int nverts)
{
  int f, cycle_start, k, fstart;
  bool ok;

  if (r->faces_len == 0)
    return -1;
  for (f = 0; f < r->faces_len; f++) {
    if (r->faces_len_table[f] != nverts)
      continue;
    fstart = r->faces_start_table[f];
    for (cycle_start = 0; cycle_start < nverts; cycle_start++) {
      ok = true;
      for (k = 0; ok && k < nverts; k++) {
        if (r->faces[fstart + ((cycle_start + k) % nverts)] != out_indices[k]) {
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

static int get_face_tri(const CDT_result *r, int out_index_1, int out_index_2, int out_index_3)
{
  int tri[3];

  tri[0] = out_index_1;
  tri[1] = out_index_2;
  tri[2] = out_index_3;
  return get_face(r, tri, 3);
}

/* return true if given otuput face has given input face id in its originals list */
static bool out_face_has_input_id(const CDT_result *r, int out_face_index, int in_face_index)
{
  if (r->faces_orig == NULL)
    return false;
  if (out_face_index < 0 || out_face_index >= r->faces_len)
    return false;
  for (int i = 0; i < r->faces_orig_len_table[out_face_index]; i++) {
    if (r->faces_orig[r->faces_orig_start_table[out_face_index] + i] == in_face_index)
      return true;
  }
  return false;
}

/* for debugging */
static void dump_result(CDT_result *r)
{
  int i, j;

  fprintf(stderr, "\nRESULT\n");
  fprintf(stderr,
          "verts_len=%d edges_len=%d faces_len=%d\n",
          r->verts_len,
          r->edges_len,
          r->faces_len);
  fprintf(stderr, "\nvert coords:\n");
  for (i = 0; i < r->verts_len; i++)
    fprintf(stderr, "%d: (%f,%f)\n", i, r->vert_coords[i][0], r->vert_coords[i][1]);
  fprintf(stderr, "vert orig:\n");
  for (i = 0; i < r->verts_len; i++) {
    fprintf(stderr, "%d:", i);
    for (j = 0; j < r->verts_orig_len_table[i]; j++)
      fprintf(stderr, " %d", r->verts_orig[r->verts_orig_start_table[i] + j]);
    fprintf(stderr, "\n");
  }
  fprintf(stderr, "\nedges:\n");
  for (i = 0; i < r->edges_len; i++)
    fprintf(stderr, "%d: (%d,%d)\n", i, r->edges[i][0], r->edges[i][1]);
  if (r->edges_orig) {
    fprintf(stderr, "edge orig:\n");
    for (i = 0; i < r->edges_len; i++) {
      fprintf(stderr, "%d:", i);
      for (j = 0; j < r->edges_orig_len_table[i]; j++)
        fprintf(stderr, " %d", r->edges_orig[r->edges_orig_start_table[i] + j]);
      fprintf(stderr, "\n");
    }
  }
  fprintf(stderr, "\nfaces:\n");
  for (i = 0; i < r->faces_len; i++) {
    fprintf(stderr, "%d: ", i);
    for (j = 0; j < r->faces_len_table[i]; j++)
      fprintf(stderr, " %d", r->faces[r->faces_start_table[i] + j]);
    fprintf(stderr, "\n");
  }
  if (r->faces_orig) {
    fprintf(stderr, "face orig:\n");
    for (i = 0; i < r->faces_len; i++) {
      fprintf(stderr, "%d:", i);
      for (j = 0; j < r->faces_orig_len_table[i]; j++)
        fprintf(stderr, " %d", r->faces_orig[r->faces_orig_start_table[i] + j]);
      fprintf(stderr, "\n");
    }
  }
}

#if DO_REGULAR_TESTS
TEST(delaunay, Empty)
{
  CDT_input in;
  CDT_result *out;

  fill_input_verts(&in, NULL, 0);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_NE((CDT_result *)NULL, out);
  EXPECT_EQ(out->verts_len, 0);
  EXPECT_EQ(out->edges_len, 0);
  EXPECT_EQ(out->faces_len, 0);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, OnePt)
{
  CDT_input in;
  CDT_result *out;
  const char *spec = R"(1 0 0
  0.0 0.0
  )";

  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 1);
  EXPECT_EQ(out->edges_len, 0);
  EXPECT_EQ(out->faces_len, 0);
  if (out->verts_len >= 1) {
    EXPECT_EQ(out->vert_coords[0][0], 0.0f);
    EXPECT_EQ(out->vert_coords[0][1], 0.0f);
  }
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, TwoPt)
{
  CDT_input in;
  CDT_result *out;
  int v0_out, v1_out, e0_out;
  const char *spec = R"(2 0 0
  0.0 -0.75
  0.0 0.75
  )";

  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 2);
  EXPECT_EQ(out->edges_len, 1);
  EXPECT_EQ(out->faces_len, 0);
  v0_out = get_output_vert_index(out, 0);
  v1_out = get_output_vert_index(out, 1);
  EXPECT_NE(v0_out, -1);
  EXPECT_NE(v1_out, -1);
  EXPECT_NE(v0_out, v1_out);
  if (out->verts_len >= 2) {
    EXPECT_NEAR(out->vert_coords[v0_out][0], 0.0, in.epsilon);
    EXPECT_NEAR(out->vert_coords[v0_out][1], -0.75, in.epsilon);
    EXPECT_NEAR(out->vert_coords[v1_out][0], 0.0, in.epsilon);
    EXPECT_NEAR(out->vert_coords[v1_out][1], 0.75, in.epsilon);
  }
  e0_out = get_edge(out, v0_out, v1_out);
  EXPECT_EQ(e0_out, 0);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, ThreePt)
{
  CDT_input in;
  CDT_result *out;
  int v0_out, v1_out, v2_out;
  int e0_out, e1_out, e2_out;
  int f0_out;
  const char *spec = R"(3 0 0
  -0.1 -0.75
  0.1 0.75
  0.5 0.5
  )";

  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 3);
  EXPECT_EQ(out->edges_len, 3);
  EXPECT_EQ(out->faces_len, 1);
  v0_out = get_output_vert_index(out, 0);
  v1_out = get_output_vert_index(out, 1);
  v2_out = get_output_vert_index(out, 2);
  EXPECT_TRUE(v0_out != -1 && v1_out != -1 && v2_out != -1);
  EXPECT_TRUE(v0_out != v1_out && v0_out != v2_out && v1_out != v2_out);
  e0_out = get_edge(out, v0_out, v1_out);
  e1_out = get_edge(out, v1_out, v2_out);
  e2_out = get_edge(out, v2_out, v0_out);
  EXPECT_TRUE(e0_out != -1 && e1_out != -1 && e2_out != -1);
  EXPECT_TRUE(e0_out != e1_out && e0_out != e2_out && e1_out != e2_out);
  f0_out = get_face_tri(out, v0_out, v2_out, v1_out);
  EXPECT_EQ(f0_out, 0);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, ThreePtsMerge)
{
  CDT_input in;
  CDT_result *out;
  int v0_out, v1_out, v2_out;
  const char *spec = R"(3 0 0
  -0.05 -0.05
  0.05 -0.05
  0.0 0.03660254
  )";

  /* First with epsilon such that points are within that distance of each other */
  fill_input_from_string(&in, spec);
  in.epsilon = 0.21f;
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 1);
  EXPECT_EQ(out->edges_len, 0);
  EXPECT_EQ(out->faces_len, 0);
  v0_out = get_output_vert_index(out, 0);
  v1_out = get_output_vert_index(out, 1);
  v2_out = get_output_vert_index(out, 2);
  EXPECT_EQ(v0_out, 0);
  EXPECT_EQ(v1_out, 0);
  EXPECT_EQ(v2_out, 0);
  BLI_delaunay_2d_cdt_free(out);
  /* Now with epsilon such that points are farther away than that.
   * Note that the points won't merge with each other if distance is
   * less than .01, but that they may merge with points on the Delaunay
   * triangulation lines, so make epsilon even smaller to avoid that for
   * this test.
   */
  in.epsilon = 0.05f;
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 3);
  EXPECT_EQ(out->edges_len, 3);
  EXPECT_EQ(out->faces_len, 1);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, MixedPts)
{
  CDT_input in;
  CDT_result *out;
  int v0_out, v1_out, v2_out, v3_out;
  int e0_out, e1_out, e2_out;
  const char *spec = R"(4 3 0
  0.0 0.0
  -0.5 -0.5
  -0.4 -0.25
  -0.3 0.8
  0 1
  1 2
  2 3
  )";

  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 4);
  EXPECT_EQ(out->edges_len, 6);
  v0_out = get_output_vert_index(out, 0);
  v1_out = get_output_vert_index(out, 1);
  v2_out = get_output_vert_index(out, 2);
  v3_out = get_output_vert_index(out, 3);
  EXPECT_TRUE(v0_out != -1 && v1_out != -1 && v2_out != -1 && v3_out != -1);
  e0_out = get_edge(out, v0_out, v1_out);
  e1_out = get_edge(out, v1_out, v2_out);
  e2_out = get_edge(out, v2_out, v3_out);
  EXPECT_TRUE(e0_out != -1 && e1_out != -1 && e2_out != -1);
  EXPECT_TRUE(out_edge_has_input_id(out, e0_out, 0));
  EXPECT_TRUE(out_edge_has_input_id(out, e1_out, 1));
  EXPECT_TRUE(out_edge_has_input_id(out, e2_out, 2));
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, Quad0)
{
  CDT_input in;
  CDT_result *out;
  int e_diag_out;
  const char *spec = R"(4 0 0
  0.0 1.0
  1,0. 0.0
  2.0 0.1
  2.25 0.5
  )";
  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 4);
  EXPECT_EQ(out->edges_len, 5);
  e_diag_out = get_edge(out, 1, 3);
  EXPECT_NE(e_diag_out, -1);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, Quad1)
{
  CDT_input in;
  CDT_result *out;
  int e_diag_out;
  const char *spec = R"(4 0 0
  0.0 0.0
  0.9 -1.0
  2.0 0.0
  0.9 3.0
  )";
  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 4);
  EXPECT_EQ(out->edges_len, 5);
  e_diag_out = get_edge(out, 0, 2);
  EXPECT_NE(e_diag_out, -1);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, Quad2)
{
  CDT_input in;
  CDT_result *out;
  int e_diag_out;
  const char *spec = R"(4 0 0
  0.5 0.0
  0.15 0.2
  0.3 0.4
  .45 0.35
  )";
  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 4);
  EXPECT_EQ(out->edges_len, 5);
  e_diag_out = get_edge(out, 1, 3);
  EXPECT_NE(e_diag_out, -1);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, Quad3)
{
  CDT_input in;
  CDT_result *out;
  int e_diag_out;
  const char *spec = R"(4 0 0
  0.5 0.0
  0.0 0.0
  0.3 0.4
  .45 0.35
  )";
  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 4);
  EXPECT_EQ(out->edges_len, 5);
  e_diag_out = get_edge(out, 0, 2);
  EXPECT_NE(e_diag_out, -1);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, Quad4)
{
  CDT_input in;
  CDT_result *out;
  int e_diag_out;
  const char *spec = R"(4 0 0
  1.0 1.0
  0.0 0.0
  1.0 -3.0
  0.0 1.0
  )";
  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 4);
  EXPECT_EQ(out->edges_len, 5);
  e_diag_out = get_edge(out, 0, 1);
  EXPECT_NE(e_diag_out, -1);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, LineInSquare)
{
  CDT_input in;
  CDT_result *out;
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
  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS);
  EXPECT_EQ(out->verts_len, 6);
  EXPECT_EQ(out->faces_len, 1);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, CrossSegs)
{
  CDT_input in;
  CDT_result *out;
  int v0_out, v1_out, v2_out, v3_out, v_intersect;
  int i;
  const char *spec = R"(4 2 0
  -0.5 0.0
  0.5 0.0
  -0.4 -0.5
  0.4 0.5
  0 1
  2 3
  )";

  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 5);
  EXPECT_EQ(out->edges_len, 8);
  EXPECT_EQ(out->faces_len, 4);
  v0_out = get_output_vert_index(out, 0);
  v1_out = get_output_vert_index(out, 1);
  v2_out = get_output_vert_index(out, 2);
  v3_out = get_output_vert_index(out, 3);
  EXPECT_TRUE(v0_out != -1 && v1_out != -1 && v2_out != -1 && v3_out != -1);
  v_intersect = -1;
  for (i = 0; i < out->verts_len; i++) {
    if (i != v0_out && i != v1_out && i != v2_out && i != v3_out) {
      EXPECT_EQ(v_intersect, -1);
      v_intersect = i;
    }
  }
  EXPECT_NE(v_intersect, -1);
  if (v_intersect != -1) {
    EXPECT_NEAR(out->vert_coords[v_intersect][0], 0.0f, in.epsilon);
    EXPECT_NEAR(out->vert_coords[v_intersect][1], 0.0f, in.epsilon);
  }
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, DiamondCross)
{
  CDT_input in;
  CDT_result *out;
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

  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 4);
  EXPECT_EQ(out->edges_len, 5);
  EXPECT_EQ(out->faces_len, 2);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, TwoDiamondsCrossed)
{
  CDT_input in;
  CDT_result *out;
  /* Input has some repetition of vertices, on purpose */
  int e[][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {5, 6}, {6, 7}, {7, 8}, {8, 9}, {10, 11}};
  int v_out[12];
  int e_out[9], e_cross_1, e_cross_2, e_cross_3;
  int i;
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

  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 8);
  EXPECT_EQ(out->edges_len, 15);
  EXPECT_EQ(out->faces_len, 8);
  for (i = 0; i < 12; i++) {
    v_out[i] = get_output_vert_index(out, i);
    EXPECT_NE(v_out[i], -1);
  }
  EXPECT_EQ(v_out[0], v_out[4]);
  EXPECT_EQ(v_out[0], v_out[10]);
  EXPECT_EQ(v_out[5], v_out[9]);
  EXPECT_EQ(v_out[7], v_out[11]);
  for (i = 0; i < 8; i++) {
    e_out[i] = get_edge(out, v_out[e[i][0]], v_out[e[i][1]]);
    EXPECT_NE(e_out[i], -1);
  }
  /* there won't be a single edge for the input cross edge, but rather 3 */
  EXPECT_EQ(get_edge(out, v_out[10], v_out[11]), -1);
  e_cross_1 = get_edge(out, v_out[0], v_out[2]);
  e_cross_2 = get_edge(out, v_out[2], v_out[5]);
  e_cross_3 = get_edge(out, v_out[5], v_out[7]);
  EXPECT_TRUE(e_cross_1 != -1 && e_cross_2 != -1 && e_cross_3 != -1);
  EXPECT_TRUE(out_edge_has_input_id(out, e_cross_1, 8));
  EXPECT_TRUE(out_edge_has_input_id(out, e_cross_2, 8));
  EXPECT_TRUE(out_edge_has_input_id(out, e_cross_3, 8));
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, ManyCross)
{
  CDT_input in;
  CDT_result *out;
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

  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 19);
  EXPECT_EQ(out->edges_len, 46);
  EXPECT_EQ(out->faces_len, 28);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, TwoFace)
{
  CDT_input in;
  CDT_result *out;
  int v_out[6], f0_out, f1_out, e0_out, e1_out, e2_out;
  int i;
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

  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 6);
  EXPECT_EQ(out->edges_len, 9);
  EXPECT_EQ(out->faces_len, 4);
  for (i = 0; i < 6; i++) {
    v_out[i] = get_output_vert_index(out, i);
    EXPECT_NE(v_out[i], -1);
  }
  f0_out = get_face(out, &v_out[0], 3);
  f1_out = get_face(out, &v_out[3], 3);
  EXPECT_NE(f0_out, -1);
  EXPECT_NE(f1_out, -1);
  e0_out = get_edge(out, v_out[0], v_out[1]);
  e1_out = get_edge(out, v_out[1], v_out[2]);
  e2_out = get_edge(out, v_out[2], v_out[0]);
  EXPECT_NE(e0_out, -1);
  EXPECT_NE(e1_out, -1);
  EXPECT_NE(e2_out, -1);
  EXPECT_TRUE(out_edge_has_input_id(out, e0_out, out->face_edge_offset + 0));
  EXPECT_TRUE(out_edge_has_input_id(out, e1_out, out->face_edge_offset + 1));
  EXPECT_TRUE(out_edge_has_input_id(out, e2_out, out->face_edge_offset + 2));
  EXPECT_TRUE(out_face_has_input_id(out, f0_out, 0));
  EXPECT_TRUE(out_face_has_input_id(out, f1_out, 1));
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, OverlapFaces)
{
  CDT_input in;
  CDT_result *out;
  int v_out[12], v_int1, v_int2, f0_out, f1_out, f2_out;
  int i;
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

  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 14);
  EXPECT_EQ(out->edges_len, 33);
  EXPECT_EQ(out->faces_len, 20);
  for (i = 0; i < 12; i++) {
    v_out[i] = get_output_vert_index(out, i);
    EXPECT_NE(v_out[i], -1);
  }
  v_int1 = 12;
  v_int2 = 13;
  if (out->verts_len > 13) {
    if (fabsf(out->vert_coords[v_int1][0] - 1.0f) > in.epsilon) {
      v_int1 = 13;
      v_int2 = 12;
    }
    EXPECT_NEAR(out->vert_coords[v_int1][0], 1.0, in.epsilon);
    EXPECT_NEAR(out->vert_coords[v_int1][1], 0.5, in.epsilon);
    EXPECT_NEAR(out->vert_coords[v_int2][0], 0.5, in.epsilon);
    EXPECT_NEAR(out->vert_coords[v_int2][1], 1.0, in.epsilon);
    EXPECT_EQ(out->verts_orig_len_table[v_int1], 0);
    EXPECT_EQ(out->verts_orig_len_table[v_int2], 0);
  }
  f0_out = get_face_tri(out, v_out[1], v_int1, v_out[4]);
  EXPECT_NE(f0_out, -1);
  EXPECT_TRUE(out_face_has_input_id(out, f0_out, 0));
  f1_out = get_face_tri(out, v_out[4], v_int1, v_out[2]);
  EXPECT_NE(f1_out, -1);
  EXPECT_TRUE(out_face_has_input_id(out, f1_out, 0));
  EXPECT_TRUE(out_face_has_input_id(out, f1_out, 1));
  f2_out = get_face_tri(out, v_out[8], v_out[9], v_out[10]);
  if (f2_out == -1)
    f2_out = get_face_tri(out, v_out[8], v_out[9], v_out[11]);
  EXPECT_NE(f2_out, -1);
  EXPECT_TRUE(out_face_has_input_id(out, f2_out, 0));
  EXPECT_TRUE(out_face_has_input_id(out, f2_out, 2));
  BLI_delaunay_2d_cdt_free(out);

  /* Different output types */
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_INSIDE);
  EXPECT_EQ(out->faces_len, 18);
  BLI_delaunay_2d_cdt_free(out);

  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS);
  EXPECT_EQ(out->faces_len, 4);
  BLI_delaunay_2d_cdt_free(out);

  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS_VALID_BMESH);
  EXPECT_EQ(out->faces_len, 5);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, TwoSquaresOverlap)
{
  CDT_input in;
  CDT_result *out;
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

  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS_VALID_BMESH);
  EXPECT_EQ(out->verts_len, 10);
  EXPECT_EQ(out->edges_len, 12);
  EXPECT_EQ(out->faces_len, 3);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, TwoFaceEdgeOverlap)
{
  CDT_input in;
  CDT_result *out;
  int i, v_out[6], v_int;
  int e01, e1i, ei2, e20, e24, e4i, ei0;
  int f02i, f24i, f10i;
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

  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS);
  EXPECT_EQ(out->verts_len, 5);
  EXPECT_EQ(out->edges_len, 7);
  EXPECT_EQ(out->faces_len, 3);
  if (out->verts_len == 5 && out->edges_len == 7 && out->faces_len == 3) {
    v_int = 4;
    for (i = 0; i < 6; i++) {
      v_out[i] = get_output_vert_index(out, i);
      EXPECT_NE(v_out[i], -1);
      EXPECT_NE(v_out[i], v_int);
    }
    EXPECT_EQ(v_out[0], v_out[3]);
    EXPECT_EQ(v_out[2], v_out[5]);
    e01 = get_edge(out, v_out[0], v_out[1]);
    EXPECT_TRUE(out_edge_has_input_id(out, e01, 1));
    e1i = get_edge(out, v_out[1], v_int);
    EXPECT_TRUE(out_edge_has_input_id(out, e1i, 0));
    ei2 = get_edge(out, v_int, v_out[2]);
    EXPECT_TRUE(out_edge_has_input_id(out, ei2, 0));
    e20 = get_edge(out, v_out[2], v_out[0]);
    EXPECT_TRUE(out_edge_has_input_id(out, e20, 2));
    EXPECT_TRUE(out_edge_has_input_id(out, e20, 5));
    e24 = get_edge(out, v_out[2], v_out[4]);
    EXPECT_TRUE(out_edge_has_input_id(out, e24, 3));
    e4i = get_edge(out, v_out[4], v_int);
    EXPECT_TRUE(out_edge_has_input_id(out, e4i, 4));
    ei0 = get_edge(out, v_int, v_out[0]);
    EXPECT_TRUE(out_edge_has_input_id(out, ei0, 4));
    f02i = get_face_tri(out, v_out[0], v_out[2], v_int);
    EXPECT_NE(f02i, -1);
    EXPECT_TRUE(out_face_has_input_id(out, f02i, 0));
    EXPECT_TRUE(out_face_has_input_id(out, f02i, 1));
    f24i = get_face_tri(out, v_out[2], v_out[4], v_int);
    EXPECT_NE(f24i, -1);
    EXPECT_TRUE(out_face_has_input_id(out, f24i, 1));
    EXPECT_FALSE(out_face_has_input_id(out, f24i, 0));
    f10i = get_face_tri(out, v_out[1], v_out[0], v_int);
    EXPECT_NE(f10i, -1);
    EXPECT_TRUE(out_face_has_input_id(out, f10i, 0));
    EXPECT_FALSE(out_face_has_input_id(out, f10i, 1));
  }
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, TriInTri)
{
  CDT_input in;
  CDT_result *out;
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

  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS_VALID_BMESH);
  EXPECT_EQ(out->verts_len, 6);
  EXPECT_EQ(out->edges_len, 8);
  EXPECT_EQ(out->faces_len, 3);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, DiamondInSquare)
{
  CDT_input in;
  CDT_result *out;
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
  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS_VALID_BMESH);
  EXPECT_EQ(out->verts_len, 8);
  EXPECT_EQ(out->edges_len, 10);
  EXPECT_EQ(out->faces_len, 3);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, DiamondInSquareWire)
{
  CDT_input in;
  CDT_result *out;
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
  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS);
  EXPECT_EQ(out->verts_len, 8);
  EXPECT_EQ(out->edges_len, 8);
  EXPECT_EQ(out->faces_len, 2);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, TinyEdge)
{
  CDT_input in;
  CDT_result *out;
  /* An intersect with triangle would be at (0.8, 0.2). */
  const char *spec = R"(4 1 1
  0.0 0.0
  1.0 0.0
  0.5 0.5
  0.84 0.21
  0 3
  0 1 2
  )";
  fill_input_from_string(&in, spec);
  in.epsilon = 0.1;
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS);
  EXPECT_EQ(out->verts_len, 4);
  EXPECT_EQ(out->edges_len, 5);
  EXPECT_EQ(out->faces_len, 2);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, TinyEdge2)
{
  CDT_input in;
  CDT_result *out;
  /* An intersect with triangle would be at (0.8, 0.2). */
  const char *spec = R"(6 1 1
  0.0 0.0
  0.2 -0.2
  1.0 0.0
  0.5 0.5
  0.2 0.4
  0.84 0.21
  0 5
  0 1 2 3 4
  )";
  fill_input_from_string(&in, spec);
  in.epsilon = 0.1;
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS);
  EXPECT_EQ(out->verts_len, 6);
  EXPECT_EQ(out->edges_len, 7);
  EXPECT_EQ(out->faces_len, 2);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, repeatededge)
{
  CDT_input in;
  CDT_result *out;
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
  fill_input_from_string(&in, spec);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS);
  EXPECT_EQ(out->edges_len, 2);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, NearSeg)
{
  CDT_input in;
  CDT_result *out;
  int v[4], e0, e1, e2, i;
  const char *spec = R"(4 2 0
  0.0 0.0
  1.0 0.0
  0.25 0.09
  0.25 1.0
  0 1
  2 3
  )";

  fill_input_from_string(&in, spec);
  in.epsilon = 0.1;
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS);
  EXPECT_EQ(out->verts_len, 4);
  EXPECT_EQ(out->edges_len, 3);
  EXPECT_EQ(out->faces_len, 0);
  if (out->edges_len == 3) {
    for (i = 0; i < 4; i++) {
      v[i] = get_output_vert_index(out, i);
      EXPECT_NE(v[i], -1);
    }
    e0 = get_edge(out, v[0], v[2]);
    e1 = get_edge(out, v[2], v[1]);
    e2 = get_edge(out, v[2], v[3]);
    EXPECT_TRUE(out_edge_has_input_id(out, e0, 0));
    EXPECT_TRUE(out_edge_has_input_id(out, e1, 0));
    EXPECT_TRUE(out_edge_has_input_id(out, e2, 1));
  }
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, OverlapSegs)
{
  CDT_input in;
  CDT_result *out;
  int v[4], e0, e1, e2, i;
  const char *spec = R"(4 2 0
  0.0 0.0
  1.0 0.0
  0.4 0.09
  1.4 0.09
  0 1
  2 3
  )";

  fill_input_from_string(&in, spec);
  in.epsilon = 0.1;
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS);
  EXPECT_EQ(out->verts_len, 4);
  EXPECT_EQ(out->edges_len, 3);
  EXPECT_EQ(out->faces_len, 0);
  if (out->edges_len == 3) {
    for (i = 0; i < 4; i++) {
      v[i] = get_output_vert_index(out, i);
      EXPECT_NE(v[i], -1);
    }
    e0 = get_edge(out, v[0], v[2]);
    e1 = get_edge(out, v[2], v[1]);
    e2 = get_edge(out, v[1], v[3]);
    EXPECT_TRUE(out_edge_has_input_id(out, e0, 0));
    EXPECT_TRUE(out_edge_has_input_id(out, e1, 0));
    EXPECT_TRUE(out_edge_has_input_id(out, e1, 1));
    EXPECT_TRUE(out_edge_has_input_id(out, e2, 1));
  }
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, NearSegWithDup)
{
  CDT_input in;
  CDT_result *out;
  int v[5], e0, e1, e2, e3, i;
  const char *spec = R"(5 3 0
  0.0 0.0
  1.0 0.0
  0.25 0.09
  0.25 1.0
  0.75 0.09
  0 1
  2 3
  2 4
  )";

  fill_input_from_string(&in, spec);
  in.epsilon = 0.1;
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS);
  EXPECT_EQ(out->verts_len, 5);
  EXPECT_EQ(out->edges_len, 4);
  EXPECT_EQ(out->faces_len, 0);
  if (out->edges_len == 5) {
    for (i = 0; i < 5; i++) {
      v[i] = get_output_vert_index(out, i);
      EXPECT_NE(v[i], -1);
    }
    e0 = get_edge(out, v[0], v[2]);
    e1 = get_edge(out, v[2], v[4]);
    e2 = get_edge(out, v[4], v[1]);
    e3 = get_edge(out, v[3], v[2]);
    EXPECT_TRUE(out_edge_has_input_id(out, e0, 0));
    EXPECT_TRUE(out_edge_has_input_id(out, e1, 0));
    EXPECT_TRUE(out_edge_has_input_id(out, e1, 2));
    EXPECT_TRUE(out_edge_has_input_id(out, e2, 0));
    EXPECT_TRUE(out_edge_has_input_id(out, e3, 1));
  }
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, TwoNearSeg)
{
  CDT_input in;
  CDT_result *out;
  int v[5], e0, e1, e2, e3, e4, i;
  const char *spec = R"(5 3 0
  0.0 0.0
  1.0 0.0
  0.25 0.09
  0.25 1.0
  0.75 0.09
  0 1
  3 2
  3 4
  )";

  fill_input_from_string(&in, spec);
  in.epsilon = 0.1;
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS);
  EXPECT_EQ(out->verts_len, 5);
  EXPECT_EQ(out->edges_len, 5);
  EXPECT_EQ(out->faces_len, 1);
  if (out->edges_len == 5) {
    for (i = 0; i < 5; i++) {
      v[i] = get_output_vert_index(out, i);
      EXPECT_NE(v[i], -1);
    }
    e0 = get_edge(out, v[0], v[2]);
    e1 = get_edge(out, v[2], v[4]);
    e2 = get_edge(out, v[4], v[1]);
    e3 = get_edge(out, v[3], v[2]);
    e4 = get_edge(out, v[3], v[4]);
    EXPECT_TRUE(out_edge_has_input_id(out, e0, 0));
    EXPECT_TRUE(out_edge_has_input_id(out, e1, 0));
    EXPECT_TRUE(out_edge_has_input_id(out, e2, 0));
    EXPECT_TRUE(out_edge_has_input_id(out, e3, 1));
    EXPECT_TRUE(out_edge_has_input_id(out, e4, 2));
  }
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, FaceNearSegs)
{
  CDT_input in;
  CDT_result *out;
  int v[9], e0, e1, e2, e3, i;
  const char *spec = R"(8 1 2
  0.0 0.0
  2.0 0.0
  1.0 1.0
  0.21 0.2
  1.79 0.2
  0.51 0.5
  1.49 0.5
  1.0 0.19
  2 7
  0 1 2
  3 4 6 5
  )";

  fill_input_from_string(&in, spec);
  in.epsilon = 0.05;
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS);
  EXPECT_EQ(out->verts_len, 9);
  EXPECT_EQ(out->edges_len, 13);
  EXPECT_EQ(out->faces_len, 5);
  if (out->verts_len == 9 && out->edges_len == 13) {
    for (i = 0; i < 8; i++) {
      v[i] = get_output_vert_index(out, i);
      EXPECT_NE(v[i], -1);
    }
    v[8] = 8;
    e0 = get_edge(out, v[0], v[1]);
    e1 = get_edge(out, v[4], v[6]);
    e2 = get_edge(out, v[3], v[0]);
    e3 = get_edge(out, v[2], v[8]);

    EXPECT_TRUE(out_edge_has_input_id(out, e0, 1));
    EXPECT_TRUE(out_edge_has_input_id(out, e1, 2));
    EXPECT_TRUE(out_edge_has_input_id(out, e1, 5));
    EXPECT_TRUE(out_edge_has_input_id(out, e2, 3));
    EXPECT_TRUE(out_edge_has_input_id(out, e3, 0));
  }
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, ChainNearIntersects)
{
  CDT_input in;
  CDT_result *out;
  const char *spec = R"(6 10 0
  0.8 1.25
  1.25 0.75
  3.25 1.25
  5.0 1.9
  2.5 4.0
  1.0 2.25
  0 1
  1 2
  2 3
  3 4
  4 5
  5 0
  0 2
  5 2
  4 2
  1 3
  )";

  fill_input_from_string(&in, spec);
  in.epsilon = 0.05;
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS);
  EXPECT_EQ(out->verts_len, 9);
  EXPECT_EQ(out->edges_len, 16);
  BLI_delaunay_2d_cdt_free(out);
  in.epsilon = 0.11;
  /* The chaining we want to test happens prematurely if modify input. */
  in.skip_input_modify = true;
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS);
  EXPECT_EQ(out->verts_len, 6);
  EXPECT_EQ(out->edges_len, 9);
  free_spec_arrays(&in);
  BLI_delaunay_2d_cdt_free(out);
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

#  define DO_TIMING
static void rand_delaunay_test(int test_kind,
                               int start_lg_size,
                               int max_lg_size,
                               int reps_per_size,
                               double param,
                               CDT_output_type otype)
{
  CDT_input in;
  CDT_result *out;
  int lg_size, size, rep, i, j, size_max, npts_max, nedges_max, nfaces_max, npts, nedges, nfaces;
  int ia, ib, ic;
  float(*p)[2];
  int(*e)[2];
  int *faces, *faces_start_table, *faces_len_table;
  double start_angle, angle_delta, angle1, angle2, angle3;
  float orient;
  double tstart;
  double *times;
  RNG *rng;

  rng = BLI_rng_new(0);
  e = NULL;
  faces = NULL;
  faces_start_table = NULL;
  faces_len_table = NULL;
  nedges_max = 0;
  nfaces_max = 0;

  /* Set up npts, nedges, nfaces, and allocate needed arrays at max length needed. */
  size_max = 1 << max_lg_size;
  switch (test_kind) {
    case RANDOM_PTS:
    case RANDOM_SEGS:
    case RANDOM_POLY:
      npts_max = size_max;
      if (test_kind == RANDOM_SEGS) {
        nedges_max = npts_max - 1;
      }
      else if (test_kind == RANDOM_POLY) {
        nedges_max = npts_max;
      }
      break;

    case RANDOM_TILTED_GRID:
      /* A 'size' x 'size' grid of points, tilted by angle 'param'.
       * Edges will go from left ends to right ends and tops to bottoms, so 2 x size of them.
       * Depending on epsilon, the vertical-ish edges may or may not go through the intermediate
       * vertices, but the horizontal ones always should.
       */
      npts_max = size_max * size_max;
      nedges_max = 2 * size_max;
      break;

    case RANDOM_CIRCLE:
      /* A circle with 'size' points, a random start angle, and equal spacing thereafter.
       * Will be input as one face.
       */
      npts_max = size_max;
      nfaces_max = 1;
      break;

    case RANDOM_TRI_BETWEEN_CIRCLES:
      /* A set of 'size' triangles, each has two random points on the unit circle,
       * and the third point is a random point on the circle with radius 'param'.
       * Each triangle will be input as a face.
       */
      npts_max = 3 * size_max;
      nfaces_max = size_max;
      break;

    default:
      fprintf(stderr, "unknown random delaunay test kind\n");
      return;
  }
  p = (float(*)[2])MEM_malloc_arrayN(npts_max, 2 * sizeof(float), __func__);
  if (nedges_max > 0) {
    e = (int(*)[2])MEM_malloc_arrayN(nedges_max, 2 * sizeof(int), __func__);
  }
  if (nfaces_max > 0) {
    faces_start_table = (int *)MEM_malloc_arrayN(nfaces_max, sizeof(int), __func__);
    faces_len_table = (int *)MEM_malloc_arrayN(nfaces_max, sizeof(int), __func__);
    faces = (int *)MEM_malloc_arrayN(npts_max, sizeof(int), __func__);
  }

  times = (double *)MEM_malloc_arrayN(max_lg_size + 1, sizeof(double), __func__);

  /* For powers of 2 sizes up to max_lg_size power of 2. */
  for (lg_size = start_lg_size; lg_size <= max_lg_size; lg_size++) {
    size = 1 << lg_size;
    nedges = 0;
    nfaces = 0;
    times[lg_size] = 0.0;
    if (size == 1 && test_kind != RANDOM_PTS) {
      continue;
    }
    /* Do 'rep' repetitions. */
    for (rep = 0; rep < reps_per_size; rep++) {
      /* Make vertices and edges or faces. */
      switch (test_kind) {
        case RANDOM_PTS:
        case RANDOM_SEGS:
        case RANDOM_POLY:
          npts = size;
          if (test_kind == RANDOM_SEGS) {
            nedges = npts - 1;
          }
          else if (test_kind == RANDOM_POLY) {
            nedges = npts;
          }
          for (i = 0; i < size; i++) {
            p[i][0] = (float)BLI_rng_get_double(rng); /* will be in range in [0,1) */
            p[i][1] = (float)BLI_rng_get_double(rng);
            if (test_kind != RANDOM_PTS) {
              if (i > 0) {
                e[i - 1][0] = i - 1;
                e[i - 1][1] = i;
              }
            }
          }
          if (test_kind == RANDOM_POLY) {
            e[size - 1][0] = size - 1;
            e[size - 1][1] = 0;
          }
          break;

        case RANDOM_TILTED_GRID:
          /* 'param' is slope of tilt of vertical lines. */
          npts = size * size;
          nedges = 2 * size;
          for (i = 0; i < size; i++) {
            for (j = 0; j < size; j++) {
              p[i * size + j][0] = i * param + j;
              p[i * size + j][1] = i;
            }
          }
          for (i = 0; i < size; i++) {
            /* Horizontal edges: connect p(i,0) to p(i,size-1). */
            e[i][0] = i * size;
            e[i][1] = i * size + size - 1;
            /* Vertical edges: conntect p(0,i) to p(size-1,i). */
            e[size + i][0] = i;
            e[size + i][1] = (size - 1) * size + i;
          }
          break;

        case RANDOM_CIRCLE:
          npts = size;
          nfaces = 1;
          faces_start_table[0] = 0;
          faces_len_table[0] = npts;
          start_angle = BLI_rng_get_double(rng) * 2.0 * M_PI;
          angle_delta = 2.0 * M_PI / size;
          for (i = 0; i < size; i++) {
            p[i][0] = (float)cos(start_angle + i * angle_delta);
            p[i][1] = (float)sin(start_angle + i * angle_delta);
            faces[i] = i;
          }
          break;

        case RANDOM_TRI_BETWEEN_CIRCLES:
          npts = 3 * size;
          nfaces = size;
          for (i = 0; i < size; i++) {
            /* Get three random angles in [0, 2pi). */
            angle1 = BLI_rng_get_double(rng) * 2.0 * M_PI;
            angle2 = BLI_rng_get_double(rng) * 2.0 * M_PI;
            angle3 = BLI_rng_get_double(rng) * 2.0 * M_PI;
            ia = 3 * i;
            ib = 3 * i + 1;
            ic = 3 * i + 2;
            p[ia][0] = (float)cos(angle1);
            p[ia][1] = (float)sin(angle1);
            p[ib][0] = (float)cos(angle2);
            p[ib][1] = (float)sin(angle2);
            p[ic][0] = (float)(param * cos(angle3));
            p[ic][1] = (float)(param * sin(angle3));
            faces_start_table[i] = 3 * i;
            faces_len_table[i] = 3;
            /* Put the coordinates in ccw order. */
            faces[ia] = ia;
            orient = (p[ia][0] - p[ic][0]) * (p[ib][1] - p[ic][1]) -
                     (p[ib][0] - p[ic][0]) * (p[ia][1] - p[ic][1]);
            if (orient >= 0.0f) {
              faces[ib] = ib;
              faces[ic] = ic;
            }
            else {
              faces[ib] = ic;
              faces[ic] = ib;
            }
          }
          break;
      }
      fill_input_verts(&in, p, npts);
      if (nedges > 0) {
        add_input_edges(&in, e, nedges);
      }
      if (nfaces > 0) {
        add_input_faces(&in, faces, faces_start_table, faces_len_table, nfaces);
      }

      /* Run the test. */
      tstart = PIL_check_seconds_timer();
      out = BLI_delaunay_2d_cdt_calc(&in, otype);
      EXPECT_NE(out->verts_len, 0);
      BLI_delaunay_2d_cdt_free(out);
      times[lg_size] += PIL_check_seconds_timer() - tstart;
    }
  }
#  ifdef DO_TIMING
  fprintf(stderr, "size,time\n");
  for (lg_size = 0; lg_size <= max_lg_size; lg_size++) {
    fprintf(stderr, "%d,%f\n", 1 << lg_size, times[lg_size] / reps_per_size);
  }
#  endif
  MEM_freeN(p);
  if (e) {
    MEM_freeN(e);
  }
  if (faces) {
    MEM_freeN(faces);
    MEM_freeN(faces_start_table);
    MEM_freeN(faces_len_table);
  }
  MEM_freeN(times);
  BLI_rng_free(rng);
}

TEST(delaunay, randompts)
{
  rand_delaunay_test(RANDOM_PTS, 0, 7, 1, 0.0, CDT_FULL);
}

TEST(delaunay, randomsegs)
{
  rand_delaunay_test(RANDOM_SEGS, 1, 7, 1, 0.0, CDT_FULL);
}

TEST(delaunay, randompoly)
{
  rand_delaunay_test(RANDOM_POLY, 1, 7, 1, 0.0, CDT_FULL);
}

TEST(delaunay, randompoly_inside)
{
  rand_delaunay_test(RANDOM_POLY, 1, 7, 1, 0.0, CDT_INSIDE);
}

TEST(delaunay, randompoly_constraints)
{
  rand_delaunay_test(RANDOM_POLY, 1, 7, 1, 0.0, CDT_CONSTRAINTS);
}

TEST(delaunay, randompoly_validbmesh)
{
  rand_delaunay_test(RANDOM_POLY, 1, 7, 1, 0.0, CDT_CONSTRAINTS_VALID_BMESH);
}

TEST(delaunay, grid)
{
  rand_delaunay_test(RANDOM_TILTED_GRID, 1, 6, 1, 0.0, CDT_FULL);
}

TEST(delaunay, tilted_grid_a)
{
  rand_delaunay_test(RANDOM_TILTED_GRID, 1, 6, 1, 1.0, CDT_FULL);
}

TEST(delaunay, tilted_grid_b)
{
  rand_delaunay_test(RANDOM_TILTED_GRID, 1, 6, 1, 0.01, CDT_FULL);
}

TEST(delaunay, randomcircle)
{
  rand_delaunay_test(RANDOM_CIRCLE, 1, 7, 1, 0.0, CDT_FULL);
}

TEST(delaunay, random_tris_circle)
{
  rand_delaunay_test(RANDOM_TRI_BETWEEN_CIRCLES, 1, 6, 1, 0.25, CDT_FULL);
}

TEST(delaunay, random_tris_circle_b)
{
  rand_delaunay_test(RANDOM_TRI_BETWEEN_CIRCLES, 1, 6, 1, 1e-4, CDT_FULL);
}
#endif

#if DO_FILE_TESTS
/* For timing large examples of points only.
 * See fill_input_from_file for file format.
 */
static void points_from_file_test(const char *filename)
{
  CDT_input in;
  CDT_result *out;
  double tstart;

  fill_input_from_file(&in, filename);
  tstart = PIL_check_seconds_timer();
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  fprintf(stderr, "time to triangulate=%f seconds\n", PIL_check_seconds_timer() - tstart);
  BLI_delaunay_2d_cdt_free(out);
  free_spec_arrays(&in);
}

#  if 0
TEST(delaunay, debug)
{
  CDT_input in;
  CDT_result *out;
  fill_input_from_file(&in, "/tmp/cdtinput.txt");
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS);
  BLI_delaunay_2d_cdt_free(out);
  free_spec_arrays(&in);
}
#  endif

#  if 1
#    define POINTFILEROOT "/tmp/"

TEST(delaunay, terrain1)
{
  points_from_file_test(POINTFILEROOT "points1.txt");
}

TEST(delaunay, terrain2)
{
  points_from_file_test(POINTFILEROOT "points2.txt");
}

TEST(delaunay, terrain3)
{
  points_from_file_test(POINTFILEROOT "points3.txt");
}
#  endif
#endif
