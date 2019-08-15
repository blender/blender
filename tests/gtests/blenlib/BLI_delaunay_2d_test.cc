/* Apache License, Version 2.0 */

#include "testing/testing.h"

extern "C" {
#include "MEM_guardedalloc.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "PIL_time.h"

#include "BLI_delaunay_2d.h"
}

#include <iostream>
#include <fstream>
#include <sstream>

#define DLNY_EPSILON 1e-8

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
  r_input->epsilon = 1e-6f;
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
  float p[][2] = {{0.0f, 0.0f}};

  fill_input_verts(&in, p, 1);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 1);
  EXPECT_EQ(out->edges_len, 0);
  EXPECT_EQ(out->faces_len, 0);
  EXPECT_EQ(out->vert_coords[0][0], 0.0f);
  EXPECT_EQ(out->vert_coords[0][1], 0.0f);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, TwoPt)
{
  CDT_input in;
  CDT_result *out;
  int v0_out, v1_out, e0_out;
  float p[][2] = {{0.0f, -0.75f}, {0.0f, 0.75f}};

  fill_input_verts(&in, p, 2);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 2);
  EXPECT_EQ(out->edges_len, 1);
  EXPECT_EQ(out->faces_len, 0);
  v0_out = get_output_vert_index(out, 0);
  v1_out = get_output_vert_index(out, 1);
  EXPECT_NE(v0_out, -1);
  EXPECT_NE(v1_out, -1);
  EXPECT_NE(v0_out, v1_out);
  EXPECT_NEAR(out->vert_coords[v0_out][0], p[0][0], in.epsilon);
  EXPECT_NEAR(out->vert_coords[v0_out][1], p[0][1], in.epsilon);
  EXPECT_NEAR(out->vert_coords[v1_out][0], p[1][0], in.epsilon);
  EXPECT_NEAR(out->vert_coords[v1_out][1], p[1][1], in.epsilon);
  e0_out = get_edge(out, v0_out, v1_out);
  EXPECT_EQ(e0_out, 0);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, ThreePt)
{
  CDT_input in;
  CDT_result *out;
  int v0_out, v1_out, v2_out;
  int e0_out, e1_out, e2_out;
  int f0_out;
  float p[][2] = {{-0.1f, -0.75f}, {0.1f, 0.75f}, {0.5f, 0.5f}};

  fill_input_verts(&in, p, 3);
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
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, ThreePtsMerge)
{
  CDT_input in;
  CDT_result *out;
  int v0_out, v1_out, v2_out;
  /* equilateral triangle with side 0.1 */
  float p[][2] = {{-0.05f, -0.05f}, {0.05f, -0.05f}, {0.0f, 0.03660254f}};

  /* First with epsilon such that points are within that distance of each other */
  fill_input_verts(&in, p, 3);
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
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, MixedPts)
{
  CDT_input in;
  CDT_result *out;
  float p[][2] = {{0.0f, 0.0f}, {-0.5f, -0.5f}, {-0.4f, -0.25f}, {-0.3f, 0.8}};
  int e[][2] = {{0, 1}, {1, 2}, {2, 3}};
  int v0_out, v1_out, v2_out, v3_out;
  int e0_out, e1_out, e2_out;

  fill_input_verts(&in, p, 4);
  add_input_edges(&in, e, 3);
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
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, CrossSegs)
{
  CDT_input in;
  CDT_result *out;
  float p[][2] = {{-0.5f, 0.0f}, {0.5f, 0.0f}, {-0.4f, -0.5f}, {0.4f, 0.5f}};
  int e[][2] = {{0, 1}, {2, 3}};
  int v0_out, v1_out, v2_out, v3_out, v_intersect;
  int i;

  fill_input_verts(&in, p, 4);
  add_input_edges(&in, e, 2);
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
  EXPECT_NEAR(out->vert_coords[v_intersect][0], 0.0f, in.epsilon);
  EXPECT_NEAR(out->vert_coords[v_intersect][1], 0.0f, in.epsilon);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, DiamondCross)
{
  CDT_input in;
  CDT_result *out;
  float p[][2] = {
      {0.0f, 0.0f},
      {1.0f, 3.0f},
      {2.0f, 0.0f},
      {1.0f, -3.0f},
      {0.0f, 0.0f},
      {1.0f, -3.0f},
      {1.0f, 3.0f},
  };
  int e[][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {5, 6}};

  fill_input_verts(&in, p, 7);
  add_input_edges(&in, e, 5);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 4);
  EXPECT_EQ(out->edges_len, 5);
  EXPECT_EQ(out->faces_len, 2);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, TwoDiamondsCrossed)
{
  CDT_input in;
  CDT_result *out;
  /* Input has some repetition of vertices, on purpose */
  float p[][2] = {
      {0.0f, 0.0f},
      {1.0f, 2.0f},
      {2.0f, 0.0f},
      {1.0f, -2.0f},
      {0.0f, 0.0f},
      {3.0f, 0.0f},
      {4.0f, 2.0f},
      {5.0f, 0.0f},
      {4.0f, -2.0f},
      {3.0f, 0.0f},
      {0.0f, 0.0f},
      {5.0f, 0.0f},
  };
  int e[][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {5, 6}, {6, 7}, {7, 8}, {8, 9}, {10, 11}};
  int v_out[12];
  int e_out[9], e_cross_1, e_cross_2, e_cross_3;
  int i;

  fill_input_verts(&in, p, 12);
  add_input_edges(&in, e, 9);
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
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, ManyCross)
{
  CDT_input in;
  CDT_result *out;
  /* Input has some repetition of vertices, on purpose */
  float p[][2] = {
      /* upper: verts 0 to 10 */
      {0.0f, 0.0f},
      {6.0f, 9.0f},
      {15.0f, 18.0f},
      {35.0f, 13.0f},
      {43.0f, 18.0f},
      {57.0f, 12.0f},
      {69.0f, 10.0f},
      {78.0f, 0.0f},
      {91.0f, 0.0f},
      {107.0f, 22.0f},
      {123.0f, 0.0f},
      /* lower part 1: verts 11 to 16 */
      {0.0f, 0.0f},
      {10.0f, -14.0f},
      {35.0f, -8.0f},
      {43.0f, -12.0f},
      {64.0f, -13.0f},
      {78.0f, 0.0f},
      /* lower part 2: verts 17 to 20 */
      {91.0f, 0.0f},
      {102.0f, -9.0f},
      {116.0f, -9.0f},
      {123.0f, 0.0f},
      /* cross 1: verts 21, 22 */
      {43.0f, 18.0f},
      {43.0f, -12.0f},
      /* cross 2: verts 23, 24 */
      {107.0f, 22.0f},
      {102.0f, -9.0f},
      /* cross all: verts 25, 26 */
      {0.0f, 0.0f},
      {123.0f, 0.0f},
  };
  int e[][2] = {
      {0, 1},   {1, 2},   {2, 3},   {3, 4},   {4, 5},   {5, 6},   {6, 7},
      {7, 8},   {8, 9},   {9, 10},  {11, 12}, {12, 13}, {13, 14}, {14, 15},
      {15, 16}, {17, 18}, {18, 19}, {19, 20}, {21, 22}, {23, 24}, {25, 26},
  };

  fill_input_verts(&in, p, 27);
  add_input_edges(&in, e, 21);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  EXPECT_EQ(out->verts_len, 19);
  EXPECT_EQ(out->edges_len, 46);
  EXPECT_EQ(out->faces_len, 28);
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, TwoFace)
{
  CDT_input in;
  CDT_result *out;
  float p[][2] = {
      {0.0f, 0.0f}, {1.0f, 0.0f}, {0.5f, 1.0f}, {1.1f, 1.0f}, {1.1f, 0.0f}, {1.6f, 1.0f}};
  int f[] = {/* 0 */ 0, 1, 2, /* 1 */ 3, 4, 5};
  int fstart[] = {0, 3};
  int flen[] = {3, 3};
  int v_out[6], f0_out, f1_out, e0_out, e1_out, e2_out;
  int i;

  fill_input_verts(&in, p, 6);
  add_input_faces(&in, f, fstart, flen, 2);
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
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, OverlapFaces)
{
  CDT_input in;
  CDT_result *out;
  float p[][2] = {
      {0.0f, 0.0f},
      {1.0f, 0.0f},
      {1.0f, 1.0f},
      {0.0f, 1.0f},
      {0.5f, 0.5f},
      {1.5f, 0.5f},
      {1.5f, 1.3f},
      {0.5f, 1.3f},
      {0.1f, 0.1f},
      {0.3f, 0.1f},
      {0.3f, 0.3f},
      {0.1f, 0.3f},
  };
  int f[] = {/* 0 */ 0, 1, 2, 3, /* 1 */ 4, 5, 6, 7, /* 2*/ 8, 9, 10, 11};
  int fstart[] = {0, 4, 8};
  int flen[] = {4, 4, 4};
  int v_out[12], v_int1, v_int2, f0_out, f1_out, f2_out;
  int i;

  fill_input_verts(&in, p, 12);
  add_input_faces(&in, f, fstart, flen, 3);
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
  if (fabsf(out->vert_coords[v_int1][0] - 1.0f) > in.epsilon) {
    v_int1 = 13;
    v_int2 = 12;
  }
  EXPECT_NEAR(out->vert_coords[v_int1][0], 1.0, in.epsilon);
  EXPECT_NEAR(out->vert_coords[v_int1][1], 0.5, in.epsilon);
  EXPECT_NEAR(out->vert_coords[v_int2][0], 0.5, in.epsilon);
  EXPECT_NEAR(out->vert_coords[v_int2][1], 1.0, in.epsilon);
  f0_out = get_face_tri(out, v_out[1], v_int1, v_out[4]);
  EXPECT_NE(f0_out, -1);
  EXPECT_TRUE(out_face_has_input_id(out, f0_out, 0));
  f1_out = get_face_tri(out, v_out[4], v_int1, v_out[2]);
  EXPECT_NE(f1_out, -1);
  EXPECT_TRUE(out_face_has_input_id(out, f1_out, 0));
  EXPECT_TRUE(out_face_has_input_id(out, f1_out, 0));
  f2_out = get_face_tri(out, v_out[8], v_out[9], v_out[10]);
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
  BLI_delaunay_2d_cdt_free(out);
}

TEST(delaunay, TwoSquaresOverlap)
{
  CDT_input in;
  CDT_result *out;
  float p[][2] = {
      {1.0f, -1.0f},
      {-1.0f, -1.0f},
      {-1.0f, 1.0f},
      {1.0f, 1.0f},
      {-1.5f, 1.5f},
      {0.5f, 1.5f},
      {0.5f, -0.5f},
      {-1.5f, -0.5f},
  };
  int f[] = {/* 0 */ 7, 6, 5, 4, /* 1 */ 3, 2, 1, 0};
  int fstart[] = {0, 4};
  int flen[] = {4, 4};

  fill_input_verts(&in, p, 8);
  add_input_faces(&in, f, fstart, flen, 2);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_CONSTRAINTS_VALID_BMESH);
  EXPECT_EQ(out->verts_len, 10);
  EXPECT_EQ(out->edges_len, 12);
  EXPECT_EQ(out->faces_len, 3);
  BLI_delaunay_2d_cdt_free(out);
}

enum {
  RANDOM_PTS,
  RANDOM_SEGS,
  RANDOM_POLY,
};

// #define DO_TIMING
static void rand_delaunay_test(int test_kind,
                               int max_lg_size,
                               int reps_per_size,
                               CDT_output_type otype)
{
  CDT_input in;
  CDT_result *out;
  int lg_size, size, rep, i, npts, nedges;
  float(*p)[2];
  int(*e)[2];
  double tstart;
  double *times;
  RNG *rng;

  rng = BLI_rng_new(0);
  npts = (1 << max_lg_size);
  p = (float(*)[2])MEM_malloc_arrayN(npts, 2 * sizeof(float), "delaunay");
  switch (test_kind) {
    case RANDOM_PTS:
      nedges = 0;
      e = NULL;
      break;

    case RANDOM_SEGS:
    case RANDOM_POLY:
      /* TODO: use faces for poly case, but need to deal with winding parity issue */
      nedges = npts - 1 + (test_kind == RANDOM_POLY);
      e = (int(*)[2])MEM_malloc_arrayN(nedges, 2 * sizeof(int), "delaunay");
      break;

    default:
      fprintf(stderr, "unknown random delaunay test kind\n");
      return;
  }
  times = (double *)MEM_malloc_arrayN(max_lg_size + 1, sizeof(double), "delaunay");
  for (lg_size = 0; lg_size <= max_lg_size; lg_size++) {
    size = 1 << lg_size;
    times[lg_size] = 0.0;
    if (size == 1 && test_kind != RANDOM_PTS)
      continue;
    for (rep = 0; rep < reps_per_size; rep++) {
      for (i = 0; i < size; i++) {
        p[i][0] = (float)BLI_rng_get_double(rng); /* will be in range in [0,1) */
        p[i][1] = (float)BLI_rng_get_double(rng);
      }
      fill_input_verts(&in, p, size);

      if (test_kind == RANDOM_SEGS || test_kind == RANDOM_POLY) {
        for (i = 0; i < size - 1; i++) {
          e[i][0] = i;
          e[i][1] = i + 1;
        }
        if (test_kind == RANDOM_POLY) {
          e[size - 1][0] = size - 1;
          e[size - 1][1] = 0;
        }
        add_input_edges(&in, e, size - 1 + (test_kind == RANDOM_POLY));
      }
      tstart = PIL_check_seconds_timer();
      out = BLI_delaunay_2d_cdt_calc(&in, otype);
      EXPECT_NE(out->verts_len, 0);
      BLI_delaunay_2d_cdt_free(out);
      times[lg_size] += PIL_check_seconds_timer() - tstart;
    }
  }
#ifdef DO_TIMING
  fprintf(stderr, "size,time\n");
  for (lg_size = 0; lg_size <= max_lg_size; lg_size++) {
    fprintf(stderr, "%d,%f\n", 1 << lg_size, times[lg_size] / reps_per_size);
  }
#endif
  MEM_freeN(p);
  if (e)
    MEM_freeN(e);
  MEM_freeN(times);
  BLI_rng_free(rng);
}

TEST(delaunay, randompts)
{
  rand_delaunay_test(RANDOM_PTS, 7, 1, CDT_FULL);
}

TEST(delaunay, randomsegs)
{
  rand_delaunay_test(RANDOM_SEGS, 7, 1, CDT_FULL);
}

TEST(delaunay, randompoly)
{
  rand_delaunay_test(RANDOM_POLY, 7, 1, CDT_FULL);
}

TEST(delaunay, randompoly_inside)
{
  rand_delaunay_test(RANDOM_POLY, 7, 1, CDT_INSIDE);
}

TEST(delaunay, randompoly_constraints)
{
  rand_delaunay_test(RANDOM_POLY, 7, 1, CDT_CONSTRAINTS);
}

TEST(delaunay, randompoly_validbmesh)
{
  rand_delaunay_test(RANDOM_POLY, 7, 1, CDT_CONSTRAINTS_VALID_BMESH);
}

#if 0
/* For debugging or timing large examples.
 * The given file should have one point per line, as a space-separated pair of floats
 */
static void points_from_file_test(const char *filename)
{
  std::ifstream f;
  std::string line;
  struct XY {
    float x;
    float y;
  } xy;
  std::vector<XY> pts;
  int i, n;
  CDT_input in;
  CDT_result *out;
  double tstart;
  float (*p)[2];

  f.open(filename);
  while (getline(f, line)) {
    std::istringstream iss(line);
    iss >> xy.x >> xy.y;
    pts.push_back(xy);
  }
  n = (int)pts.size();
  fprintf(stderr, "read %d points\n", (int)pts.size());
  p = (float (*)[2])MEM_malloc_arrayN(n, 2 * sizeof(float), "delaunay");
  for (i = 0; i < n; i++) {
    xy = pts[i];
    p[i][0] = xy.x;
    p[i][1] = xy.y;
  }
  tstart = PIL_check_seconds_timer();
  fill_input_verts(&in, p, n);
  out = BLI_delaunay_2d_cdt_calc(&in, CDT_FULL);
  fprintf(stderr, "time to triangulate=%f seconds\n", PIL_check_seconds_timer() - tstart);
  BLI_delaunay_2d_cdt_free(out);
  MEM_freeN(p);
}

#  define POINTFILEROOT "/tmp/"

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
#endif
