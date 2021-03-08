/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_map.hh"
#include "BLI_math_mpq.hh"
#include "BLI_mesh_boolean.hh"
#include "BLI_mpq3.hh"
#include "BLI_vector.hh"

#ifdef WITH_GMP
namespace blender::meshintersect::tests {

constexpr bool DO_OBJ = false;

/* Build and hold an IMesh from a string spec. Also hold and own resources used by IMesh. */
class IMeshBuilder {
 public:
  IMesh imesh;
  IMeshArena arena;

  /* "Edge orig" indices are an encoding of <input face#, position in face of start of edge>. */
  static constexpr int MAX_FACE_LEN = 1000; /* Used for forming "orig edge" indices only. */

  static int edge_index(int face_index, int facepos)
  {
    return face_index * MAX_FACE_LEN + facepos;
  }

  static std::pair<int, int> face_and_pos_for_edge_index(int e_index)
  {
    return std::pair<int, int>(e_index / MAX_FACE_LEN, e_index % MAX_FACE_LEN);
  }

  /*
   * Spec should have form:
   *  #verts #faces
   *  mpq_class mpq_class mpq_clas [#verts lines]
   *  int int int ... [#faces lines; indices into verts for given face]
   */
  IMeshBuilder(const char *spec)
  {
    std::istringstream ss(spec);
    std::string line;
    getline(ss, line);
    std::istringstream hdrss(line);
    int nv, nf;
    hdrss >> nv >> nf;
    if (nv == 0 || nf == 0) {
      return;
    }
    arena.reserve(nv, nf);
    Vector<const Vert *> verts;
    Vector<Face *> faces;
    bool spec_ok = true;
    int v_index = 0;
    while (v_index < nv && spec_ok && getline(ss, line)) {
      std::istringstream iss(line);
      mpq_class p0;
      mpq_class p1;
      mpq_class p2;
      iss >> p0 >> p1 >> p2;
      spec_ok = !iss.fail();
      verts.append(arena.add_or_find_vert(mpq3(p0, p1, p2), v_index));
      ++v_index;
    }
    if (v_index != nv) {
      spec_ok = false;
    }
    int f_index = 0;
    while (f_index < nf && spec_ok && getline(ss, line)) {
      std::istringstream fss(line);
      Vector<const Vert *> face_verts;
      Vector<int> edge_orig;
      int fpos = 0;
      while (spec_ok && fss >> v_index) {
        if (v_index < 0 || v_index >= nv) {
          spec_ok = false;
          continue;
        }
        face_verts.append(verts[v_index]);
        edge_orig.append(edge_index(f_index, fpos));
        ++fpos;
      }
      Face *facep = arena.add_face(face_verts, f_index, edge_orig);
      faces.append(facep);
      ++f_index;
    }
    if (f_index != nf) {
      spec_ok = false;
    }
    if (!spec_ok) {
      std::cout << "Bad spec: " << spec;
      return;
    }
    imesh = IMesh(faces);
  }
};

static int all_shape_zero(int UNUSED(t))
{
  return 0;
}

TEST(boolean_trimesh, Empty)
{
  IMeshArena arena;
  IMesh in;
  IMesh out = boolean_trimesh(in, BoolOpType::None, 1, all_shape_zero, true, false, &arena);
  out.populate_vert();
  EXPECT_EQ(out.vert_size(), 0);
  EXPECT_EQ(out.face_size(), 0);
}

TEST(boolean_trimesh, TetTetTrimesh)
{
  const char *spec = R"(8 8
  0 0 0
  2 0 0
  1 2 0
  1 1 2
  0 0 1
  2 0 1
  1 2 1
  1 1 3
  0 2 1
  0 1 3
  1 2 3
  2 0 3
  4 6 5
  4 5 7
  5 6 7
  6 4 7
  )";

  IMeshBuilder mb(spec);
  IMesh out = boolean_trimesh(
      mb.imesh, BoolOpType::None, 1, all_shape_zero, true, false, &mb.arena);
  out.populate_vert();
  EXPECT_EQ(out.vert_size(), 11);
  EXPECT_EQ(out.face_size(), 20);
  if (DO_OBJ) {
    write_obj_mesh(out, "tettet_tm");
  }

  IMeshBuilder mb2(spec);
  IMesh out2 = boolean_trimesh(
      mb2.imesh, BoolOpType::Union, 1, all_shape_zero, true, false, &mb2.arena);
  out2.populate_vert();
  EXPECT_EQ(out2.vert_size(), 10);
  EXPECT_EQ(out2.face_size(), 16);
  if (DO_OBJ) {
    write_obj_mesh(out2, "tettet_union_tm");
  }

  IMeshBuilder mb3(spec);
  IMesh out3 = boolean_trimesh(
      mb3.imesh,
      BoolOpType::Union,
      2,
      [](int t) { return t < 4 ? 0 : 1; },
      false,
      false,
      &mb3.arena);
  out3.populate_vert();
  EXPECT_EQ(out3.vert_size(), 10);
  EXPECT_EQ(out3.face_size(), 16);
  if (DO_OBJ) {
    write_obj_mesh(out3, "tettet_union_binary_tm");
  }

  IMeshBuilder mb4(spec);
  IMesh out4 = boolean_trimesh(
      mb4.imesh,
      BoolOpType::Union,
      2,
      [](int t) { return t < 4 ? 0 : 1; },
      true,
      false,
      &mb4.arena);
  out4.populate_vert();
  EXPECT_EQ(out4.vert_size(), 10);
  EXPECT_EQ(out4.face_size(), 16);
  if (DO_OBJ) {
    write_obj_mesh(out4, "tettet_union_binary_self_tm");
  }

  IMeshBuilder mb5(spec);
  IMesh out5 = boolean_trimesh(
      mb5.imesh,
      BoolOpType::Intersect,
      2,
      [](int t) { return t < 4 ? 0 : 1; },
      false,
      false,
      &mb5.arena);
  out5.populate_vert();
  EXPECT_EQ(out5.vert_size(), 4);
  EXPECT_EQ(out5.face_size(), 4);
  if (DO_OBJ) {
    write_obj_mesh(out5, "tettet_intersect_binary_tm");
  }

  IMeshBuilder mb6(spec);
  IMesh out6 = boolean_trimesh(
      mb6.imesh,
      BoolOpType::Difference,
      2,
      [](int t) { return t < 4 ? 0 : 1; },
      false,
      false,
      &mb6.arena);
  out6.populate_vert();
  EXPECT_EQ(out6.vert_size(), 6);
  EXPECT_EQ(out6.face_size(), 8);
  if (DO_OBJ) {
    write_obj_mesh(out6, "tettet_difference_binary_tm");
  }

  IMeshBuilder mb7(spec);
  IMesh out7 = boolean_trimesh(
      mb7.imesh,
      BoolOpType::Difference,
      2,
      [](int t) { return t < 4 ? 1 : 0; },
      false,
      false,
      &mb7.arena);
  out7.populate_vert();
  EXPECT_EQ(out7.vert_size(), 8);
  EXPECT_EQ(out7.face_size(), 12);
  if (DO_OBJ) {
    write_obj_mesh(out7, "tettet_difference_rev_binary_tm");
  }
}

TEST(boolean_trimesh, TetTet2Trimesh)
{
  const char *spec = R"(8 8
  0 1 -1
  7/8 -1/2 -1
  -7/8 -1/2 -1
  0 0 1
  0 1 0
  7/8 -1/2 0
  -7/8 -1/2 0
  0 0 2
  0 3 1
  0 1 2
  1 3 2
  2 3 0
  4 7 5
  4 5 6
  5 7 6
  6 7 4
  )";

  IMeshBuilder mb(spec);
  IMesh out = boolean_trimesh(
      mb.imesh, BoolOpType::Union, 1, all_shape_zero, true, false, &mb.arena);
  out.populate_vert();
  EXPECT_EQ(out.vert_size(), 10);
  EXPECT_EQ(out.face_size(), 16);
  if (DO_OBJ) {
    write_obj_mesh(out, "tettet2_union_tm");
  }
}

TEST(boolean_trimesh, CubeTetTrimesh)
{
  const char *spec = R"(12 16
  -1 -1 -1
  -1 -1 1
  -1 1 -1
  -1 1 1
  1 -1 -1
  1 -1 1
  1 1 -1
  1 1 1
  0 1/2 1/2
  1/2 -1/4 1/2
  -1/2 -1/4 1/2
  0 0 3/2
  0 1 3
  0 3 2
  2 3 7
  2 7 6
  6 7 5
  6 5 4
  4 5 1
  4 1 0
  2 6 4
  2 4 0
  7 3 1
  7 1 5
  8 11 9
  8 9 10
  9 11 10
  10 11 8
  )";

  IMeshBuilder mb(spec);
  IMesh out = boolean_trimesh(
      mb.imesh, BoolOpType::Union, 1, all_shape_zero, true, false, &mb.arena);
  out.populate_vert();
  EXPECT_EQ(out.vert_size(), 14);
  EXPECT_EQ(out.face_size(), 24);
  if (DO_OBJ) {
    write_obj_mesh(out, "cubetet_union_tm");
  }
}

TEST(boolean_trimesh, BinaryTetTetTrimesh)
{
  const char *spec = R"(8 8
  0 0 0
  2 0 0
  1 2 0
  1 1 2
  0 0 1
  2 0 1
  1 2 1
  1 1 3
  0 2 1
  0 1 3
  1 2 3
  2 0 3
  4 6 5
  4 5 7
  5 6 7
  6 4 7
  )";

  IMeshBuilder mb(spec);
  IMesh out = boolean_trimesh(
      mb.imesh,
      BoolOpType::Intersect,
      2,
      [](int t) { return t < 4 ? 0 : 1; },
      false,
      false,
      &mb.arena);
  out.populate_vert();
  EXPECT_EQ(out.vert_size(), 4);
  EXPECT_EQ(out.face_size(), 4);
  if (DO_OBJ) {
    write_obj_mesh(out, "binary_tettet_isect_tm");
  }
}

TEST(boolean_trimesh, TetTetCoplanarTrimesh)
{
  const char *spec = R"(8 8
  0 1 0
  7/8 -1/2 0
  -7/8 -1/2 0
  0 0 1
  0 1 0
  7/8 -1/2 0
  -7/8 -1/2 0
  0 0 -1
  0 3 1
  0 1 2
  1 3 2
  2 3 0
  4 5 7
  4 6 5
  5 6 7
  6 4 7
  )";

  IMeshBuilder mb(spec);
  IMesh out = boolean_trimesh(
      mb.imesh, BoolOpType::Union, 1, all_shape_zero, true, false, &mb.arena);
  out.populate_vert();
  EXPECT_EQ(out.vert_size(), 5);
  EXPECT_EQ(out.face_size(), 6);
  if (DO_OBJ) {
    write_obj_mesh(out, "tettet_coplanar_tm");
  }
}

TEST(boolean_trimesh, TetInsideTetTrimesh)
{
  const char *spec = R"(8 8
  0 0 0
  2 0 0
  1 2 0
  1 1 2
  -1 -3/4 -1/2
  3 -3/4 -1/2
  1 13/4 -1/2
  1 5/4 7/2
  0 2 1
  0 1 3
  1 2 3
  2 0 3
  4 6 5
  4 5 7
  5 6 7
  6 4 7
  )";

  IMeshBuilder mb(spec);
  IMesh out = boolean_trimesh(
      mb.imesh, BoolOpType::Union, 1, all_shape_zero, true, false, &mb.arena);
  out.populate_vert();
  EXPECT_EQ(out.vert_size(), 4);
  EXPECT_EQ(out.face_size(), 4);
  if (DO_OBJ) {
    write_obj_mesh(out, "tetinsidetet_tm");
  }
}

TEST(boolean_trimesh, TetBesideTetTrimesh)
{
  const char *spec = R"(8 8
  0 0 0
  2 0 0
  1 2 0
  1 1 2
  3 0 0
  5 0 0
  4 2 0
  4 1 2
  0 2 1
  0 1 3
  1 2 3
  2 0 3
  4 6 5
  4 5 7
  5 6 7
  6 4 7
  )";

  IMeshBuilder mb(spec);
  IMesh out = boolean_trimesh(
      mb.imesh, BoolOpType::Union, 1, all_shape_zero, true, false, &mb.arena);
  out.populate_vert();
  EXPECT_EQ(out.vert_size(), 8);
  EXPECT_EQ(out.face_size(), 8);
  if (DO_OBJ) {
    write_obj_mesh(out, "tetbesidetet_tm");
  }
}

TEST(boolean_trimesh, DegenerateTris)
{
  const char *spec = R"(10 10
  0 0 0
  2 0 0
  1 2 0
  1 1 2
  0 0 1
  2 0 1
  1 2 1
  1 1 3
  0 0 0
  1 0 0
  0 2 1
  0 8 1
  0 1 3
  1 2 3
  2 0 3
  4 6 5
  4 5 7
  5 6 7
  6 4 7
  0 1 9
  )";

  IMeshBuilder mb(spec);
  IMesh out = boolean_trimesh(
      mb.imesh,
      BoolOpType::Intersect,
      2,
      [](int t) { return t < 5 ? 0 : 1; },
      false,
      false,
      &mb.arena);
  out.populate_vert();
  EXPECT_EQ(out.vert_size(), 4);
  EXPECT_EQ(out.face_size(), 4);
  if (DO_OBJ) {
    write_obj_mesh(out, "degenerate_tris_tm");
  }
}

TEST(boolean_polymesh, TetTet)
{
  const char *spec = R"(8 8
  0 0 0
  2 0 0
  1 2 0
  1 1 2
  0 0 1
  2 0 1
  1 2 1
  1 1 3
  0 2 1
  0 1 3
  1 2 3
  2 0 3
  4 6 5
  4 5 7
  5 6 7
  6 4 7
  )";

  IMeshBuilder mb(spec);
  IMesh out = boolean_mesh(
      mb.imesh, BoolOpType::None, 1, all_shape_zero, true, false, nullptr, &mb.arena);
  out.populate_vert();
  EXPECT_EQ(out.vert_size(), 11);
  EXPECT_EQ(out.face_size(), 13);
  if (DO_OBJ) {
    write_obj_mesh(out, "tettet");
  }

  IMeshBuilder mb2(spec);
  IMesh out2 = boolean_mesh(
      mb2.imesh,
      BoolOpType::None,
      2,
      [](int t) { return t < 4 ? 0 : 1; },
      false,
      false,
      nullptr,
      &mb2.arena);
  out2.populate_vert();
  EXPECT_EQ(out2.vert_size(), 11);
  EXPECT_EQ(out2.face_size(), 13);
  if (DO_OBJ) {
    write_obj_mesh(out, "tettet2");
  }
}

TEST(boolean_polymesh, CubeCube)
{
  const char *spec = R"(16 12
  -1 -1 -1
  -1 -1 1
  -1 1 -1
  -1 1 1
  1 -1 -1
  1 -1 1
  1 1 -1
  1 1 1
  1/2 1/2 1/2
  1/2 1/2 5/2
  1/2 5/2 1/2
  1/2 5/2 5/2
  5/2 1/2 1/2
  5/2 1/2 5/2
  5/2 5/2 1/2
  5/2 5/2 5/2
  0 1 3 2
  6 2 3 7
  4 6 7 5
  0 4 5 1
  0 2 6 4
  3 1 5 7
  8 9 11 10
  14 10 11 15
  12 14 15 13
  8 12 13 9
  8 10 14 12
  11 9 13 15
  )";

  IMeshBuilder mb(spec);
  if (DO_OBJ) {
    write_obj_mesh(mb.imesh, "cube_cube_in");
  }
  IMesh out = boolean_mesh(
      mb.imesh, BoolOpType::Union, 1, all_shape_zero, true, false, nullptr, &mb.arena);
  out.populate_vert();
  EXPECT_EQ(out.vert_size(), 20);
  EXPECT_EQ(out.face_size(), 12);
  if (DO_OBJ) {
    write_obj_mesh(out, "cubecube_union");
  }

  IMeshBuilder mb2(spec);
  IMesh out2 = boolean_mesh(
      mb2.imesh,
      BoolOpType::None,
      2,
      [](int t) { return t < 6 ? 0 : 1; },
      false,
      false,
      nullptr,
      &mb2.arena);
  out2.populate_vert();
  EXPECT_EQ(out2.vert_size(), 22);
  EXPECT_EQ(out2.face_size(), 18);
  if (DO_OBJ) {
    write_obj_mesh(out2, "cubecube_none");
  }
}

TEST(boolean_polymesh, CubeCone)
{
  const char *spec = R"(14 12
  -1 -1 -1
  -1 -1 1
  -1 1 -1
  -1 1 1
  1 -1 -1
  1 -1 1
  1 1 -1
  1 1 1
  0 1/2 3/4
  119/250 31/200 3/4
  147/500 -81/200 3/4
  0 0 7/4
  -147/500 -81/200 3/4
  -119/250 31/200 3/4
  0 1 3 2
  2 3 7 6
  6 7 5 4
  4 5 1 0
  2 6 4 0
  7 3 1 5
  8 11 9
  9 11 10
  10 11 12
  12 11 13
  13 11 8
  8 9 10 12 13)";

  IMeshBuilder mb(spec);
  IMesh out = boolean_mesh(
      mb.imesh, BoolOpType::Union, 1, all_shape_zero, true, false, nullptr, &mb.arena);
  out.populate_vert();
  EXPECT_EQ(out.vert_size(), 14);
  EXPECT_EQ(out.face_size(), 12);
  if (DO_OBJ) {
    write_obj_mesh(out, "cubeccone");
  }
}

TEST(boolean_polymesh, CubeCubeCoplanar)
{
  const char *spec = R"(16 12
  -1 -1 -1
  -1 -1 1
  -1 1 -1
  -1 1 1
  1 -1 -1
  1 -1 1
  1 1 -1
  1 1 1
  -1/2 -1/2 1
  -1/2 -1/2 2
  -1/2 1/2 1
  -1/2 1/2 2
  1/2 -1/2 1
  1/2 -1/2 2
  1/2 1/2 1
  1/2 1/2 2
  0 1 3 2
  2 3 7 6
  6 7 5 4
  4 5 1 0
  2 6 4 0
  7 3 1 5
  8 9 11 10
  10 11 15 14
  14 15 13 12
  12 13 9 8
  10 14 12 8
  15 11 9 13
  )";

  IMeshBuilder mb(spec);
  IMesh out = boolean_mesh(
      mb.imesh,
      BoolOpType::Union,
      2,
      [](int t) { return t < 6 ? 0 : 1; },
      false,
      false,
      nullptr,
      &mb.arena);
  out.populate_vert();
  EXPECT_EQ(out.vert_size(), 16);
  EXPECT_EQ(out.face_size(), 12);
  if (DO_OBJ) {
    write_obj_mesh(out, "cubecube_coplanar");
  }
}

TEST(boolean_polymesh, TetTeTCoplanarDiff)
{
  const char *spec = R"(8 8
  0 1 0
  7/8 -1/2 0
  -7/8 -1/2 0
  0 0 1
  0 1 0
  7/8 -1/2 0
  -7/8 -1/2 0
  0 0 -1
  0 3 1
  0 1 2
  1 3 2
  2 3 0
  4 5 7
  4 6 5
  5 6 7
  6 4 7
  )";

  IMeshBuilder mb(spec);
  IMesh out = boolean_mesh(
      mb.imesh,
      BoolOpType::Difference,
      2,
      [](int t) { return t < 4 ? 0 : 1; },
      false,
      false,
      nullptr,
      &mb.arena);
  out.populate_vert();
  EXPECT_EQ(out.vert_size(), 4);
  EXPECT_EQ(out.face_size(), 4);
  if (DO_OBJ) {
    write_obj_mesh(out, "tettet_coplanar_diff");
  }
}

TEST(boolean_polymesh, CubeCubeStep)
{
  const char *spec = R"(16 12
  0 -1 0
  0 -1 2
  0 1 0
  0 1 2
  2 -1 0
  2 -1 2
  2 1 0
  2 1 2
  -1 -1 -1
  -1 -1 1
  -1 1 -1
  -1 1 1
  1 -1 -1
  1 -1 1
  1 1 -1
  1 1 1
  0 1 3 2
  2 3 7 6
  6 7 5 4
  4 5 1 0
  2 6 4 0
  7 3 1 5
  8 9 11 10
  10 11 15 14
  14 15 13 12
  12 13 9 8
  10 14 12 8
  15 11 9 13
  )";

  IMeshBuilder mb(spec);
  IMesh out = boolean_mesh(
      mb.imesh,
      BoolOpType::Difference,
      2,
      [](int t) { return t < 6 ? 0 : 1; },
      false,
      false,
      nullptr,
      &mb.arena);
  out.populate_vert();
  EXPECT_EQ(out.vert_size(), 12);
  EXPECT_EQ(out.face_size(), 8);
  if (DO_OBJ) {
    write_obj_mesh(out, "cubecubestep");
  }
}

TEST(boolean_polymesh, CubeCyl4)
{
  const char *spec = R"(16 12
  0 1 -1
  0 1 1
  1 0 -1
  1 0 1
  0 -1 -1
  0 -1 1
  -1 0 -1
  -1 0 1
  -1 -1 -1
  -1 -1 1
  -1 1 -1
  -1 1 1
  1 -1 -1
  1 -1 1
  1 1 -1
  1 1 1
  0 1 3 2
  2 3 5 4
  3 1 7 5
  4 5 7 6
  6 7 1 0
  0 2 4 6
  8 9 11 10
  10 11 15 14
  14 15 13 12
  12 13 9 8
  10 14 12 8
  15 11 9 13
  )";

  IMeshBuilder mb(spec);
  IMesh out = boolean_mesh(
      mb.imesh,
      BoolOpType::Difference,
      2,
      [](int t) { return t < 6 ? 1 : 0; },
      false,
      false,
      nullptr,
      &mb.arena);
  out.populate_vert();
  EXPECT_EQ(out.vert_size(), 16);
  EXPECT_EQ(out.face_size(), 20);
  if (DO_OBJ) {
    write_obj_mesh(out, "cubecyl4");
  }
}

TEST(boolean_polymesh, CubeCubesubdivDiff)
{
  /* A cube intersected by a subdivided cube that intersects first cubes edges exactly. */
  const char *spec = R"(26 22
  2 1/3 2
  2 -1/3 2
  2 -1/3 0
  2 1/3 0
  0 -1/3 2
  0 1/3 2
  0 1/3 0
  0 -1/3 0
  1 1/3 2
  1 -1/3 2
  1 1/3 0
  1 -1/3 0
  0 -1/3 1
  0 1/3 1
  2 1/3 1
  2 -1/3 1
  1 1/3 1
  1 -1/3 1
  -1 -1 -1
  -1 -1 1
  -1 1 -1
  -1 1 1
  1 -1 -1
  1 -1 1
  1 1 -1
  1 1 1
  17 9 4 12
  13 6 7 12
  15 2 3 14
  11 7 6 10
  16 13 5 8
  9 1 0 8
  4 9 8 5
  14 16 8 0
  2 11 10 3
  15 1 9 17
  2 15 17 11
  3 10 16 14
  10 6 13 16
  1 15 14 0
  5 13 12 4
  11 17 12 7
  19 21 20 18
  21 25 24 20
  25 23 22 24
  23 19 18 22
  18 20 24 22
  23 25 21 19
  )";

  IMeshBuilder mb(spec);
  IMesh out = boolean_mesh(
      mb.imesh,
      BoolOpType::Difference,
      2,
      [](int t) { return t < 16 ? 1 : 0; },
      false,
      false,
      nullptr,
      &mb.arena);
  out.populate_vert();
  EXPECT_EQ(out.vert_size(), 16);
  EXPECT_EQ(out.face_size(), 10);
  if (DO_OBJ) {
    write_obj_mesh(out, "cubecubesubdivdiff");
  }
}

TEST(boolean_polymesh, CubePlane)
{
  const char *spec = R"(12 7
  -2 -2 0
  2 -2 0
  -2 2 0
  2 2 0
  -1 -1 -1
  -1 -1 1
  -1 1 -1
  -1 1 1
  1 -1 -1
  1 -1 1
  1 1 -1
  1 1 1
  0 1 3 2
  4 5 7 6
  6 7 11 10
  10 11 9 8
  8 9 5 4
  6 10 8 4
  11 7 5 9
)";

  IMeshBuilder mb(spec);
  IMesh out = boolean_mesh(
      mb.imesh,
      BoolOpType::Difference,
      2,
      [](int t) { return t >= 1 ? 0 : 1; },
      false,
      false,
      nullptr,
      &mb.arena);
  out.populate_vert();
  EXPECT_EQ(out.vert_size(), 8);
  EXPECT_EQ(out.face_size(), 6);
  if (DO_OBJ) {
    write_obj_mesh(out, "cubeplane");
  }
}

}  // namespace blender::meshintersect::tests
#endif
