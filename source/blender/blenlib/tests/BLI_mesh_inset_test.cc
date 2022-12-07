/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "BLI_array.hh"
#include "BLI_mesh_inset.hh"
#include "BLI_vector.hh"

namespace blender::meshinset {

namespace test {

class SpecArrays {
 public:
  Array<float3> vert;
  Array<Vector<int>> face;
  Array<Vector<int>> contour;
};

/* The spec should have the form:
 * #verts #faces #contours
 * <float> <float> <float>  [#verts lines]
 * <int> <int> ... <int>  [#faces lines]
 * <int> <int> ... <int> [#contours lines]
 */
static SpecArrays fill_input_from_string(const char *spec)
{
  SpecArrays ans;
  std::istringstream ss(spec);
  std::string line;
  getline(ss, line);
  std::istringstream hdrss(line);
  int nverts, nfaces, ncontours;
  hdrss >> nverts >> nfaces >> ncontours;
  if (nverts == 0) {
    return SpecArrays();
  }
  ans.vert = Array<float3>(nverts);
  ans.face = Array<Vector<int>>(nfaces);
  ans.contour = Array<Vector<int>>(ncontours);
  int i = 0;
  while (i < nverts && getline(ss, line)) {
    std::istringstream iss(line);
    float x, y, z;
    iss >> x >> y >> z;
    ans.vert[i] = float3(x, y, z);
    i++;
  }
  i = 0;
  while (i < nfaces && getline(ss, line)) {
    std::istringstream fss(line);
    int v;
    while (fss >> v) {
      ans.face[i].append(v);
    }
    i++;
  }
  i = 0;
  while (i < ncontours && getline(ss, line)) {
    std::istringstream css(line);
    int v;
    while (css >> v) {
      ans.contour[i].append(v);
    }
    i++;
  }
  return ans;
}

class InputHolder {
  SpecArrays spec_arrays_;

 public:
  MeshInset_Input input;

  InputHolder(const char *spec, float amount)
  {
    spec_arrays_ = fill_input_from_string(spec);
    input.vert = spec_arrays_.vert.as_span();
    input.face = spec_arrays_.face.as_span();
    input.contour = spec_arrays_.contour.as_span();
    input.inset_amount = amount;
    input.slope = 0.5f;
    input.need_ids = false;
  }
};

TEST(mesh_inset, Tri)
{
  const char *spec = R"(3 1 1
  0.0 0.0 0.0
  1.0 0.0 0.0
  0.5 0.5 0.0
  0 1 2
  0 1 2
  )";

  InputHolder in1(spec, 0.1);
  MeshInset_Result out1 = mesh_inset_calc(in1.input);
  EXPECT_EQ(out1.vert.size(), 6);
  EXPECT_EQ(out1.face.size(), 4);

  InputHolder in2(spec, 0.3);
  MeshInset_Result out2 = mesh_inset_calc(in2.input);
  EXPECT_EQ(out2.vert.size(), 4);
  EXPECT_EQ(out2.face.size(), 3);
}

/* An asymmetrical quadrilateral. */
TEST(mesh_inset, Quad)
{
  const char *spec = R"(4 1 1
  -1.0 -1.0 0.0
  1.1 -1.0 0.0
  0.9 0.9 0.0
  -0.5 1.0 0.0
  0 1 2 3
  0 1 2 3
  )";

  InputHolder in1(spec, 0.3);
  MeshInset_Result out1 = mesh_inset_calc(in1.input);
  EXPECT_EQ(out1.vert.size(), 8);
  EXPECT_EQ(out1.face.size(), 5);

  InputHolder in2(spec, .85);
  MeshInset_Result out2 = mesh_inset_calc(in2.input);
  EXPECT_EQ(out2.vert.size(), 8);
  EXPECT_EQ(out2.face.size(), 5);

  InputHolder in3(spec, .88);
  MeshInset_Result out3 = mesh_inset_calc(in3.input);
  EXPECT_EQ(out3.vert.size(), 6);
  EXPECT_EQ(out3.face.size(), 4);
}

TEST(mesh_inset, Square)
{
  const char *spec = R"(4 1 1
  0.0 0.0 0.0
  1.0 0.0 0.0
  1.0 1.0 0.0
  0.0 1.0 0.0
  0 1 2 3
  0 1 2 3
  )";

  InputHolder in1(spec, 0.4);
  MeshInset_Result out1 = mesh_inset_calc(in1.input);
  EXPECT_EQ(out1.vert.size(), 8);
  EXPECT_EQ(out1.face.size(), 5);

  InputHolder in2(spec, 0.51);
  in2.input.slope = 0.5f;
  MeshInset_Result out2 = mesh_inset_calc(in2.input);
  /* Note: current code wants all 3-valence vertices in
   * straight skeleton, so the center doesn't collapse to
   * a single vertex, but rather two vertices with a zero
   * length edge between them. */
  EXPECT_EQ(out2.vert.size(), 6);
  EXPECT_EQ(out2.face.size(), 4);
  /* The last two verts should be in the center, with height 0.25. */
  const float3 &v4 = out2.vert[4];
  const float3 &v5 = out2.vert[5];
  EXPECT_NEAR(v4.x, 0.5, 1e-5);
  EXPECT_NEAR(v4.y, 0.5, 1e-5);
  EXPECT_NEAR(v4.z, 0.25, 1e-5);
  EXPECT_NEAR(v5.x, 0.5, 1e-5);
  EXPECT_NEAR(v5.y, 0.5, 1e-5);
  EXPECT_NEAR(v5.z, 0.25, 1e-5);
}

TEST(mesh_inset, Pentagon)
{
  const char *spec = R"(5 1 1
  0.0 0.0 0.0
  1.0 0.0 0.0
  1.0 1.0 0.0
  0.5 1.5 0.0
  0.0 1.0 0.0
  0 1 2 3 4
  0 1 2 3 4
  )";

  InputHolder in1(spec, 0.2);
  MeshInset_Result out1 = mesh_inset_calc(in1.input);
  EXPECT_EQ(out1.vert.size(), 10);
  EXPECT_EQ(out1.face.size(), 6);

  InputHolder in2(spec, 1.0);
  MeshInset_Result out2 = mesh_inset_calc(in2.input);
  /* Because code wants all valence-3 vertices in the skeleton,
   * there is a zero-length edge in this output. */
  EXPECT_EQ(out2.vert.size(), 8);
  EXPECT_EQ(out2.face.size(), 5);
}

TEST(mesh_inset, Hexagon)
{
  const char *spec = R"(6 1 1
  0.0 1.0 0.0
  0.125 0.0 0.0
  0.625 -0.75 0.0
  1.5 -1.0 0.0
  2.875 0.0 0.0
  3.0 1.0 0.0
  0 1 2 3 4 5
  0 1 2 3 4 5
  )";

  InputHolder in1(spec, 0.4);
  MeshInset_Result out1 = mesh_inset_calc(in1.input);
  EXPECT_EQ(out1.vert.size(), 12);
  EXPECT_EQ(out1.face.size(), 7);

  InputHolder in2(spec, 0.67);
  MeshInset_Result out2 = mesh_inset_calc(in2.input);
  EXPECT_EQ(out2.vert.size(), 12);
  EXPECT_EQ(out2.face.size(), 7);

  InputHolder in3(spec, 0.85);
  MeshInset_Result out3 = mesh_inset_calc(in3.input);
  EXPECT_EQ(out3.vert.size(), 12);
  EXPECT_EQ(out3.face.size(), 7);

  InputHolder in4(spec, 0.945);
  MeshInset_Result out4 = mesh_inset_calc(in4.input);
  EXPECT_EQ(out4.vert.size(), 12);
  EXPECT_EQ(out4.face.size(), 7);

  InputHolder in5(spec, 0.97);
  MeshInset_Result out5 = mesh_inset_calc(in5.input);
  EXPECT_EQ(out5.vert.size(), 10);
  EXPECT_EQ(out5.face.size(), 6);
}

TEST(mesh_inset, Splitter)
{
  const char *spec = R"(5 1 1
  0.0 0.0 0.0
  1.5 0.1 0.0
  1.75 0.8 0.0
  0.8 0.6 0.0
  0.0 1.0 0.0
  0 1 2 3 4
  0 1 2 3 4
  )";

  InputHolder in1(spec, 0.25);
  MeshInset_Result out1 = mesh_inset_calc(in1.input);
  EXPECT_EQ(out1.vert.size(), 10);
  EXPECT_EQ(out1.face.size(), 6);

  InputHolder in2(spec, 0.29);
  MeshInset_Result out2 = mesh_inset_calc(in2.input);
  EXPECT_EQ(out2.vert.size(), 12);
  EXPECT_EQ(out2.face.size(), 7);

  InputHolder in3(spec, 0.40);
  MeshInset_Result out3 = mesh_inset_calc(in3.input);
  EXPECT_EQ(out3.vert.size(), 8);
  EXPECT_EQ(out3.face.size(), 5);
}

TEST(mesh_inset, Flipper)
{
  const char *spec = R"(20 1 1
  0.0 0.0 0.0
  1.5 0.0 0.0
  1.375 0.025 0.0
  1.25 0.06 0.0
  1.125 0.11 0.0
  1.0 0.2 0.0
  1.0 1.0 0.0
  0.79 1.0 0.0
  0.75 0.95 0.0
  0.71 1.0 0.0
  0.585 1.0 0.0
  0.55 0.9 0.0
  0.515 1.0 0.0
  0.38 1.0 0.0
  0.35 0.85 0.0
  0.32 1.0 0.0
  0.175 1.0 0.0
  0.15 0.8 0.0
  0.125 1.0 0.0
  0.0 1.0 0.0
  0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19
  0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19
  )";

  InputHolder in1(spec, 0.01);
  MeshInset_Result out1 = mesh_inset_calc(in1.input);
  EXPECT_EQ(out1.vert.size(), 40);
  EXPECT_EQ(out1.face.size(), 21);

  InputHolder in2(spec, 0.06);
  MeshInset_Result out2 = mesh_inset_calc(in2.input);
  EXPECT_EQ(out2.vert.size(), 40);
  EXPECT_EQ(out2.face.size(), 21);

  InputHolder in3(spec, 0.07);
  MeshInset_Result out3 = mesh_inset_calc(in3.input);
  EXPECT_EQ(out3.vert.size(), 40);
  EXPECT_EQ(out3.face.size(), 21);

  InputHolder in4(spec, 0.08);
  MeshInset_Result out4 = mesh_inset_calc(in4.input);
  EXPECT_EQ(out4.vert.size(), 40);
  EXPECT_EQ(out4.face.size(), 21);

  InputHolder in5(spec, 0.087);
  MeshInset_Result out5 = mesh_inset_calc(in5.input);
  EXPECT_EQ(out5.vert.size(), 40);
  EXPECT_EQ(out5.face.size(), 21);

  InputHolder in6(spec, 0.0878);
  MeshInset_Result out6 = mesh_inset_calc(in6.input);
  EXPECT_EQ(out6.vert.size(), 40);
  EXPECT_EQ(out6.face.size(), 21);

  InputHolder in7(spec, 0.11);
  MeshInset_Result out7 = mesh_inset_calc(in7.input);
  EXPECT_EQ(out7.vert.size(), 42);
  EXPECT_EQ(out7.face.size(), 22);

  InputHolder in8(spec, 0.24);
  MeshInset_Result out8 = mesh_inset_calc(in8.input);
  EXPECT_EQ(out8.vert.size(), 42);
  EXPECT_EQ(out8.face.size(), 22);

  InputHolder in9(spec, 0.255);
  MeshInset_Result out9 = mesh_inset_calc(in9.input);
  EXPECT_EQ(out9.vert.size(), 42);
  EXPECT_EQ(out9.face.size(), 22);

  InputHolder in10(spec, 0.30);
  MeshInset_Result out10 = mesh_inset_calc(in10.input);
  EXPECT_EQ(out10.vert.size(), 40);
  EXPECT_EQ(out10.face.size(), 21);

  InputHolder in11(spec, 0.35);
  MeshInset_Result out11 = mesh_inset_calc(in11.input);
  EXPECT_EQ(out11.vert.size(), 38);
  EXPECT_EQ(out11.face.size(), 20);
}

#if 0
TEST(mesh_inset, Grid)
{
  const char *spec = R"(16 9 1
  0.0 0.0 0.0
  1.0 0.0 0.0
  2.0 0.0 0.0
  3.0 0.0 0.0
  0.0 1.0 0.0
  1.0 1.0 0.0
  2.0 1.0 0.0
  3.0 1.0 0.0
  0.0 2.0 0.0
  1.0 2.0 0.0
  2.0 2.0 0.0
  3.0 2.0 0.0
  0.0 3.0 0.0
  1.0 3.0 0.0
  2.0 3.0 0.0
  3.0 3.0 0.0
  0 1 5 4
  1 2 6 5
  2 3 7 6
  4 5 9 8
  5 6 10 9
  6 7 11 10
  8 9 13 12
  9 10 14 13
  10 11 15 14
  0 1 2 3 7 11 15 14 13 12 8 4
  )";

  InputHolder in1(spec, 0.5);
  MeshInset_Result out1 = mesh_inset_calc(in1.input);
  EXPECT_EQ(out1.vert.size(), 28);
  EXPECT_EQ(out1.face.size(), 21);
}
#endif

}  // namespace test

}  // namespace blender::meshinset
