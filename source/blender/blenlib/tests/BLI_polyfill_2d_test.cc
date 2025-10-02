/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

/* Use to write out OBJ files, handy for checking output */
// #define USE_OBJ_PREVIEW

/* test every possible offset and reverse */
#define USE_COMBINATIONS_ALL
#define USE_BEAUTIFY

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.h"
#include "BLI_map.hh"
#include "BLI_math_geom.h"
#include "BLI_ordered_edge.hh"
#include "BLI_polyfill_2d.h"
#include "BLI_utildefines.h"

#ifdef USE_OBJ_PREVIEW
#  include "BLI_string.h"
#endif

#ifdef USE_BEAUTIFY
#  include "BLI_heap.h"
#  include "BLI_memarena.h"
#  include "BLI_polyfill_2d_beautify.h"
#endif

static void polyfill_to_obj(const char *id,
                            const float poly[][2],
                            const uint poly_num,
                            const uint tris[][3],
                            const uint tris_num);

enum ePolyFill2DTestFlag {
  POLYFILL2D_TEST_IS_DEGENERATE = (1 << 0),
  POLYFILL2D_TEST_NO_ZERO_AREA_TRIS = (1 << 1),
  POLYFILL2D_TEST_NOP = 0,
};

/* -------------------------------------------------------------------- */
/* test utility functions */

#define TRI_ERROR_VALUE uint(-1)

static void test_valid_polyfill_prepare(uint tris[][3], uint tris_num)
{
  uint i;
  for (i = 0; i < tris_num; i++) {
    uint j;
    for (j = 0; j < 3; j++) {
      tris[i][j] = TRI_ERROR_VALUE;
    }
  }
}

/**
 * Basic check for face index values:
 *
 * - no duplicates.
 * - all tris set.
 * - all verts used at least once.
 */
static void test_polyfill_simple(const float /*poly*/[][2],
                                 const uint poly_num,
                                 const uint tris[][3],
                                 const uint tris_num)
{
  uint i;
  int *used_num = MEM_calloc_arrayN<int>(poly_num, __func__);
  for (i = 0; i < tris_num; i++) {
    uint j;
    for (j = 0; j < 3; j++) {
      EXPECT_NE(TRI_ERROR_VALUE, tris[i][j]);
      used_num[tris[i][j]] += 1;
    }
    EXPECT_NE(tris[i][0], tris[i][1]);
    EXPECT_NE(tris[i][1], tris[i][2]);
    EXPECT_NE(tris[i][2], tris[i][0]);
  }
  for (i = 0; i < poly_num; i++) {
    EXPECT_NE(0, used_num[i]);
  }
  MEM_freeN(used_num);
}

static void test_polyfill_topology(const float /*poly*/[][2],
                                   const uint poly_num,
                                   const uint tris[][3],
                                   const uint tris_num)
{
  blender::Map<blender::OrderedEdge, int> edgehash;
  uint i;
  for (i = 0; i < tris_num; i++) {
    uint j;
    for (j = 0; j < 3; j++) {
      const uint v1 = tris[i][j];
      const uint v2 = tris[i][(j + 1) % 3];
      edgehash.add_or_modify(
          {v1, v2}, [](int *value) { *value = 1; }, [](int *value) { (*value)++; });
    }
  }
  EXPECT_EQ(edgehash.size(), poly_num + (poly_num - 3));

  for (i = 0; i < poly_num; i++) {
    const uint v1 = i;
    const uint v2 = (i + 1) % poly_num;
    EXPECT_TRUE(edgehash.contains({v1, v2}));
    EXPECT_EQ(edgehash.lookup({v1, v2}), 1);
  }

  for (const int value : edgehash.values()) {
    EXPECT_TRUE(ELEM(value, 1, 2));
  }
}

/**
 * Check all faces are flipped the same way
 */
static void test_polyfill_winding(const float poly[][2],
                                  const uint /*poly_num*/,
                                  const uint tris[][3],
                                  const uint tris_num)
{
  uint i;
  uint count[2] = {0, 0};
  for (i = 0; i < tris_num; i++) {
    float winding_test = cross_tri_v2(poly[tris[i][0]], poly[tris[i][1]], poly[tris[i][2]]);
    if (fabsf(winding_test) > FLT_EPSILON) {
      count[winding_test < 0.0f] += 1;
    }
  }
  EXPECT_TRUE(ELEM(0, count[0], count[1]));
}

/**
 * Check the accumulated triangle area is close to the original area.
 */
static void test_polyfill_area(const float poly[][2],
                               const uint poly_num,
                               const uint tris[][3],
                               const uint tris_num)
{
  uint i;
  const float area_total = area_poly_v2(poly, poly_num);
  float area_total_tris = 0.0f;
  const float eps_abs = 0.00001f;
  const float eps = area_total > 1.0f ? (area_total * eps_abs) : eps_abs;
  for (i = 0; i < tris_num; i++) {
    area_total_tris += area_tri_v2(poly[tris[i][0]], poly[tris[i][1]], poly[tris[i][2]]);
  }
  EXPECT_NEAR(area_total, area_total_tris, eps);
}

/**
 * Check that none of the tessellated triangles are zero area.
 */
static void test_polyfill_area_tri_nonzero(const float poly[][2],
                                           const uint /*poly_num*/,
                                           const uint tris[][3],
                                           const uint tris_num)
{
  uint i;
  uint total = 0;
  for (i = 0; i < tris_num; i++) {
    if (area_tri_v2(poly[tris[i][0]], poly[tris[i][1]], poly[tris[i][2]]) < 1e-6f) {
      total += 1;
    }
  }
  EXPECT_EQ(total, 0);
}

/* -------------------------------------------------------------------- */
/* Macro and helpers to manage checking */
/**
 * Main template for polyfill testing.
 */
static void test_polyfill_template_check(const char *id,
                                         const ePolyFill2DTestFlag test_flag,
                                         const float poly[][2],
                                         const uint poly_num,
                                         const uint tris[][3],
                                         const uint tris_num)
{
  test_polyfill_simple(poly, poly_num, tris, tris_num);
  test_polyfill_topology(poly, poly_num, tris, tris_num);
  if (!(test_flag & POLYFILL2D_TEST_IS_DEGENERATE)) {
    test_polyfill_winding(poly, poly_num, tris, tris_num);

    test_polyfill_area(poly, poly_num, tris, tris_num);

    /* Only check when non-degenerate, because the number of zero area triangles
     * are undefined for degenerate polygons as there is no correct solution. */
    if (test_flag & POLYFILL2D_TEST_NO_ZERO_AREA_TRIS) {
      test_polyfill_area_tri_nonzero(poly, poly_num, tris, tris_num);
    }
  }
  polyfill_to_obj(id, poly, poly_num, tris, tris_num);
}

static void test_polyfill_template(const char *id,
                                   const ePolyFill2DTestFlag test_flag,
                                   const float poly[][2],
                                   const uint poly_num,
                                   uint tris[][3],
                                   const uint tris_num)
{
  test_valid_polyfill_prepare(tris, tris_num);
  BLI_polyfill_calc(poly, poly_num, 0, tris);

  /* check all went well */
  test_polyfill_template_check(id, test_flag, poly, poly_num, tris, tris_num);

#ifdef USE_BEAUTIFY
  /* check beautify gives good results too */
  {
    MemArena *pf_arena = BLI_memarena_new(BLI_POLYFILL_ARENA_SIZE, __func__);
    Heap *pf_heap = BLI_heap_new_ex(BLI_POLYFILL_ALLOC_NGON_RESERVE);

    BLI_polyfill_beautify(poly, poly_num, tris, pf_arena, pf_heap);

    test_polyfill_template_check(id, test_flag, poly, poly_num, tris, tris_num);

    BLI_memarena_free(pf_arena);
    BLI_heap_free(pf_heap, nullptr);
  }
#endif
}

static void test_polyfill_template_flip_sign(const char *id,
                                             const ePolyFill2DTestFlag test_flag,
                                             const float poly[][2],
                                             const uint poly_num,
                                             uint tris[][3],
                                             const uint tris_num)
{
  float (*poly_copy)[2] = MEM_malloc_arrayN<float[2]>(poly_num, id);
  for (int flip_x = 0; flip_x < 2; flip_x++) {
    for (int flip_y = 0; flip_y < 2; flip_y++) {
      float sign_x = flip_x ? -1.0f : 1.0f;
      float sign_y = flip_y ? -1.0f : 1.0f;
      for (int i = 0; i < poly_num; i++) {
        poly_copy[i][0] = poly[i][0] * sign_x;
        poly_copy[i][1] = poly[i][1] * sign_y;
      }
      test_polyfill_template(id, test_flag, poly_copy, poly_num, tris, tris_num);
    }
  }
  MEM_freeN(poly_copy);
}

#ifdef USE_COMBINATIONS_ALL
static void test_polyfill_template_main(const char *id,
                                        const ePolyFill2DTestFlag test_flag,
                                        const float poly[][2],
                                        const uint poly_num,
                                        uint tris[][3],
                                        const uint tris_num)
{
  /* overkill? - try at _every_ offset & reverse */
  uint poly_reverse;
  float (*poly_copy)[2] = MEM_malloc_arrayN<float[2]>(poly_num, id);
  float tmp[2];

  memcpy(poly_copy, poly, sizeof(float[2]) * poly_num);

  for (poly_reverse = 0; poly_reverse < 2; poly_reverse++) {
    uint poly_cycle;

    if (poly_reverse) {
      BLI_array_reverse(poly_copy, poly_num);
    }

    for (poly_cycle = 0; poly_cycle < poly_num; poly_cycle++) {
      // printf("polytest %s ofs=%d, reverse=%d\n", id, poly_cycle, poly_reverse);
      test_polyfill_template_flip_sign(id, test_flag, poly, poly_num, tris, tris_num);

      /* cycle */
      copy_v2_v2(tmp, poly_copy[0]);
      memmove(&poly_copy[0], &poly_copy[1], (poly_num - 1) * sizeof(float[2]));
      copy_v2_v2(poly_copy[poly_num - 1], tmp);
    }
  }

  MEM_freeN(poly_copy);
}
#else  /* USE_COMBINATIONS_ALL */
static void test_polyfill_template_main(const char *id,
                                        const ePolyFill2DTestFlag test_flag,
                                        const float poly[][2],
                                        const uint poly_num,
                                        uint tris[][3],
                                        const uint tris_num)
{
  test_polyfill_template_flip_sign(id, test_flag, poly, poly_num, tris, tris_num);
}
#endif /* USE_COMBINATIONS_ALL */

#define TEST_POLYFILL_TEMPLATE_STATIC(poly, test_flag) \
  { \
    uint tris[POLY_TRI_COUNT(ARRAY_SIZE(poly))][3]; \
    const uint poly_num = ARRAY_SIZE(poly); \
    const uint tris_num = ARRAY_SIZE(tris); \
    const char *id = typeid(*this).name(); \
\
    test_polyfill_template_main(id, test_flag, poly, poly_num, tris, tris_num); \
  } \
  (void)0

/* -------------------------------------------------------------------- */
/* visualization functions (not needed for testing) */

#ifdef USE_OBJ_PREVIEW
static void polyfill_to_obj(const char *id,
                            const float poly[][2],
                            const uint poly_num,
                            const uint tris[][3],
                            const uint tris_num)
{
  char path[1024];
  FILE *f;
  uint i;

  SNPRINTF(path, "%s.obj", id);

  f = fopen(path, "w");
  if (!f) {
    return;
  }

  for (i = 0; i < poly_num; i++) {
    fprintf(f, "v %f %f 0.0\n", UNPACK2(poly[i]));
  }

  for (i = 0; i < tris_num; i++) {
    fprintf(f, "f %u %u %u\n", UNPACK3_EX(1 +, tris[i], ));
  }

  fclose(f);
}
#else
static void polyfill_to_obj(const char *id,
                            const float poly[][2],
                            const uint poly_num,
                            const uint tris[][3],
                            const uint tris_num)
{
  (void)id;
  (void)poly, (void)poly_num;
  (void)tris, (void)tris_num;
}
#endif /* USE_OBJ_PREVIEW */

/* -------------------------------------------------------------------- */
/* tests */

/**
 * Script to generate the data below:
 *
 * \code{.py}
 * # This example assumes we have a mesh object in edit-mode
 *
 * import bpy
 * import bmesh
 *
 * obj = bpy.context.edit_object
 * me = obj.data
 * bm = bmesh.from_edit_mesh(me)
 *
 * def clean_float(num):
 *     if int(num) == num:
 *         return str(int(num))
 *     prec = 1
 *     while True:
 *         text = f"{num:.{prec}f}"
 *         if float(text) == num:
 *             return text
 *         prec += 1
 *
 * for f in bm.faces:
 *     if f.select:
 *         print(f"\t// data for face: {f.index}")
 *         print("\tconst float poly[][2] = {", end="")
 *         coords = [[clean_float(num) for num in l.vert.co[0:2]] for l in f.loops]
 *         print("\t    ", end="")
 *         for i, (x, y) in enumerate(coords):
 *             if (i % 2) == 0:
 *                 print("\n\t    ", end="")
 *             print(f"{{{x}, {y}}}", end=",")
 *         print("\n\t};")
 * \endcode
 */

#define POLY_TRI_COUNT(len) ((len) - 2)

/* A counterclockwise triangle */
TEST(polyfill2d, TriangleCCW)
{
  const float poly[][2] = {{0, 0}, {0, 1}, {1, 0}};
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* A counterclockwise square */
TEST(polyfill2d, SquareCCW)
{
  const float poly[][2] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* A clockwise square */
TEST(polyfill2d, SquareCW)
{
  const float poly[][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Star-fleet insignia. */
TEST(polyfill2d, Starfleet)
{
  const float poly[][2] = {{0, 0}, {0.6f, 0.4f}, {1, 0}, {0.5f, 1}};
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Star-fleet insignia with repeated point. */
TEST(polyfill2d, StarfleetDegenerate)
{
  const float poly[][2] = {{0, 0}, {0.6f, 0.4f}, {0.6f, 0.4f}, {1, 0}, {0.5f, 1}};
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Three collinear points */
TEST(polyfill2d, 3Colinear)
{
  const float poly[][2] = {{0, 0}, {1, 0}, {2, 0}};
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Four collinear points */
TEST(polyfill2d, 4Colinear)
{
  const float poly[][2] = {{0, 0}, {1, 0}, {2, 0}, {3, 0}};
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Non-consecutive collinear points */
TEST(polyfill2d, UnorderedColinear)
{
  const float poly[][2] = {{0, 0}, {1, 1}, {2, 0}, {3, 1}, {4, 0}};
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Plus shape */
TEST(polyfill2d, PlusShape)
{
  const float poly[][2] = {
      {1, 0},
      {2, 0},
      {2, 1},
      {3, 1},
      {3, 2},
      {2, 2},
      {2, 3},
      {1, 3},
      {1, 2},
      {0, 2},
      {0, 1},
      {1, 1},
  };
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Star shape */
TEST(polyfill2d, StarShape)
{
  const float poly[][2] = {{4, 0}, {5, 3}, {8, 4}, {5, 5}, {4, 8}, {3, 5}, {0, 4}, {3, 3}};
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* U shape */
TEST(polyfill2d, UShape)
{
  const float poly[][2] = {
      {1, 0}, {2, 0}, {3, 1}, {3, 3}, {2, 3}, {2, 1}, {1, 1}, {1, 3}, {0, 3}, {0, 1}};
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Spiral */
TEST(polyfill2d, Spiral)
{
  const float poly[][2] = {
      {1, 0},
      {4, 0},
      {5, 1},
      {5, 4},
      {4, 5},
      {1, 5},
      {0, 4},
      {0, 3},
      {1, 2},
      {2, 2},
      {3, 3},
      {1, 3},
      {1, 4},
      {4, 4},
      {4, 1},
      {0, 1},
  };
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Test case from http://www.flipcode.com/archives/Efficient_Polygon_Triangulation.shtml */
TEST(polyfill2d, TestFlipCode)
{
  const float poly[][2] = {
      {0, 6},
      {0, 0},
      {3, 0},
      {4, 1},
      {6, 1},
      {8, 0},
      {12, 0},
      {13, 2},
      {8, 2},
      {8, 4},
      {11, 4},
      {11, 6},
      {6, 6},
      {4, 3},
      {2, 6},
  };
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Self-intersection */
TEST(polyfill2d, SelfIntersect)
{
  const float poly[][2] = {{0, 0}, {1, 1}, {2, -1}, {3, 1}, {4, 0}};
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_IS_DEGENERATE);
}

/* Self-touching */
TEST(polyfill2d, SelfTouch)
{
  const float poly[][2] = {
      {0, 0},
      {4, 0},
      {4, 4},
      {2, 4},
      {2, 3},
      {3, 3},
      {3, 1},
      {1, 1},
      {1, 3},
      {2, 3},
      {2, 4},
      {0, 4},
  };
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Self-overlapping */
TEST(polyfill2d, SelfOverlap)
{
  const float poly[][2] = {
      {0, 0},
      {4, 0},
      {4, 4},
      {1, 4},
      {1, 3},
      {3, 3},
      {3, 1},
      {1, 1},
      {1, 3},
      {3, 3},
      {3, 4},
      {0, 4},
  };
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_IS_DEGENERATE);
}

/* Test case from http://www.davdata.nl/math/polygons.html */
TEST(polyfill2d, TestDavData)
{
  const float poly[][2] = {
      {190, 480}, {140, 180}, {310, 100}, {330, 390}, {290, 390}, {280, 260}, {220, 260},
      {220, 430}, {370, 430}, {350, 30},  {50, 30},   {160, 560}, {730, 510}, {710, 20},
      {410, 30},  {470, 440}, {640, 410}, {630, 140}, {590, 140}, {580, 360}, {510, 370},
      {510, 60},  {650, 70},  {660, 450}, {190, 480},
  };
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Issue 815, http://code.google.com/p/libgdx/issues/detail?id=815 */
TEST(polyfill2d, Issue815)
{
  const float poly[][2] = {
      {-2.0f, 0.0f},
      {-2.0f, 0.5f},
      {0.0f, 1.0f},
      {0.5f, 2.875f},
      {1.0f, 0.5f},
      {1.5f, 1.0f},
      {2.0f, 1.0f},
      {2.0f, 0.0f},
  };
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Issue 207, comment #1, http://code.google.com/p/libgdx/issues/detail?id=207#c1 */
TEST(polyfill2d, Issue207_1)
{
  const float poly[][2] = {
      {72.42465f, 197.07095f},
      {78.485535f, 189.92776f},
      {86.12059f, 180.92929f},
      {99.68253f, 164.94557f},
      {105.24325f, 165.79604f},
      {107.21862f, 166.09814f},
      {112.41958f, 162.78253f},
      {113.73238f, 161.94562f},
      {123.29477f, 167.93805f},
      {126.70667f, 170.07617f},
      {73.22717f, 199.51062f},
  };
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_IS_DEGENERATE);
}

/* Issue 207, comment #11, http://code.google.com/p/libgdx/issues/detail?id=207#c11 */
/* Also on issue 1081, http://code.google.com/p/libgdx/issues/detail?id=1081 */
TEST(polyfill2d, Issue207_11)
{
  const float poly[][2] = {
      {2400.0f, 480.0f},        {2400.0f, 176.0f},        {1920.0f, 480.0f},
      {1920.0459f, 484.22314f}, {1920.1797f, 487.91016f}, {1920.3955f, 491.0874f},
      {1920.6875f, 493.78125f}, {1921.0498f, 496.01807f}, {1921.4766f, 497.82422f},
      {1921.9619f, 499.22607f}, {1922.5f, 500.25f},       {1923.085f, 500.92236f},
      {1923.7109f, 501.26953f}, {1924.3721f, 501.31787f}, {1925.0625f, 501.09375f},
      {1925.7764f, 500.62354f}, {1926.5078f, 499.9336f},  {1927.251f, 499.0503f},
      {1928.0f, 498.0f},        {1928.749f, 496.80908f},  {1929.4922f, 495.5039f},
      {1930.2236f, 494.11084f}, {1930.9375f, 492.65625f}, {1931.6279f, 491.1665f},
      {1932.2891f, 489.66797f}, {1932.915f, 488.187f},    {1933.5f, 486.75f},
      {1934.0381f, 485.3833f},  {1934.5234f, 484.11328f}, {1934.9502f, 482.9663f},
      {1935.3125f, 481.96875f}, {1935.6045f, 481.14697f}, {1935.8203f, 480.52734f},
      {1935.9541f, 480.13623f}, {1936.0f, 480.0f}};
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Issue 1407, http://code.google.com/p/libgdx/issues/detail?id=1407 */
TEST(polyfill2d, Issue1407)
{
  const float poly[][2] = {
      {3.914329f, 1.9008259f},
      {4.414321f, 1.903619f},
      {4.8973203f, 1.9063174f},
      {5.4979978f, 1.9096732f},
  };
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Issue 1407, http://code.google.com/p/libgdx/issues/detail?id=1407, */
/* with an additional point to show what is happening. */
TEST(polyfill2d, Issue1407_pt)
{
  const float poly[][2] = {
      {3.914329f, 1.9008259f},
      {4.414321f, 1.903619f},
      {4.8973203f, 1.9063174f},
      {5.4979978f, 1.9096732f},
      {4, 4},
  };
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Simplified from Blender bug #40777 */
TEST(polyfill2d, IssueT40777_colinear)
{
  const float poly[][2] = {
      {0.7, 0.37},  {0.7, 0},    {0.76, 0}, {0.76, 0.4}, {0.83, 0.4}, {0.83, 0},    {0.88, 0},
      {0.88, 0.4},  {0.94, 0.4}, {0.94, 0}, {1, 0},      {1, 0.4},    {0.03, 0.62}, {0.03, 0.89},
      {0.59, 0.89}, {0.03, 1},   {0, 1},    {0, 0},      {0.03, 0},   {0.03, 0.37},
  };
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Blender bug #41986 */
TEST(polyfill2d, IssueT41986_axis_align)
{
  const float poly[][2] = {
      {-0.25, -0.07}, {-0.25, 0.27},  {-1.19, 0.14},  {-0.06, 0.73},  {0.17, 1.25},
      {-0.25, 1.07},  {-0.38, 1.02},  {-0.25, 0.94},  {-0.40, 0.90},  {-0.41, 0.86},
      {-0.34, 0.83},  {-0.25, 0.82},  {-0.66, 0.73},  {-0.56, 1.09},  {-0.25, 1.10},
      {0.00, 1.31},   {-0.03, 1.47},  {-0.25, 1.53},  {0.12, 1.62},   {0.36, 1.07},
      {0.12, 0.67},   {0.29, 0.57},   {0.44, 0.45},   {0.57, 0.29},   {0.66, 0.12},
      {0.68, 0.06},   {0.57, -0.36},  {-0.25, -0.37}, {0.49, -0.74},  {-0.59, -1.21},
      {-0.25, -0.15}, {-0.46, -0.52}, {-1.08, -0.83}, {-1.45, -0.33}, {-1.25, -0.04}};

  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Blender bug #52834 */
TEST(polyfill2d, IssueT52834_axis_align_co_linear)
{
  const float poly[][2] = {
      {40, 0},  {36, 0},  {36, 5},  {35, 5},  {35, 0},  {30, 0},  {30, 5},  {29, 5},
      {29, 0},  {24, 0},  {24, 3},  {23, 4},  {23, 0},  {18, 0},  {18, 5},  {17, 5},
      {17, 0},  {12, 0},  {12, 5},  {11, 5},  {11, 0},  {6, 0},   {6, 5},   {5, 5},
      {5, 0},   {0, 0},   {0, 5},   {-1, 5},  {-1, 0},  {-6, 0},  {-9, -3}, {-6, -3},
      {-6, -2}, {-1, -2}, {0, -2},  {5, -2},  {6, -2},  {11, -2}, {12, -2}, {17, -2},
      {18, -2}, {23, -2}, {24, -2}, {29, -2}, {30, -2}, {35, -2}, {36, -2}, {40, -2},
  };

  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Blender bug #67109 (version a). */
/* Multiple versions are offset & rotated, this fails in cases where others works. */
TEST(polyfill2d, IssueT67109_axis_align_co_linear_a)
{
  const float poly[][2] = {
      {3.2060661, -11.438997},
      {2.8720665, -5.796999},
      {-2.8659325, -5.796999},
      {-2.8659325, -8.307999},
      {-3.2549324, -11.438997},
      {-2.8659325, -5.4869995},
      {2.8720665, -5.4869995},
      {2.8720665, -2.9759989},
      {2.8720665, -2.6659985},
      {2.8720665, -0.15499878},
  };
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Blender bug #67109, (version b). */
TEST(polyfill2d, IssueT67109_axis_align_co_linear_b)
{
  const float poly[][2] = {
      {32.41416, -12.122593},
      {28.094929, -8.477332},
      {24.141455, -12.636018},
      {25.96133, -14.366093},
      {27.96254, -16.805279},
      {23.916779, -12.422427},
      {27.870255, -8.263744},
      {26.050375, -6.533667},
      {25.825695, -6.320076},
      {24.00582, -4.5899982},
  };
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Blender bug #67109 (version c). */
TEST(polyfill2d, IssueT67109_axis_align_co_linear_c)
{
  const float poly[][2] = {
      {-67.10034, 43.677097},
      {-63.253956, 61.399143},
      {-80.98382, 66.36057},
      {-83.15499, 58.601795},
      {-87.06422, 49.263668},
      {-80.71576, 67.31843},
      {-62.985912, 62.35701},
      {-60.81475, 70.11576},
      {-60.546703, 71.07365},
      {-58.37554, 78.83239},
  };
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NOP);
}

/* Blender bug #103913 where co-linear edges create zero area tessellation
 * when a valid solution exists without zero area triangles. */
TEST(polyfill2d, Issue103913_axis_align_co_linear_no_zero_area_tri)
{
  const float poly[][2] = {
      {-10, 0}, {-10, 2}, {-8, 2},  {-6, 2},  {-4, 2},  {-2, 2},  {-2, 4},  {-2, 6},
      {-2, 8},  {-2, 10}, {0, 10},  {2, 10},  {2, 8},   {2, 6},   {2, 4},   {2, 2},
      {4, 2},   {6, 2},   {8, 2},   {10, 2},  {10, 0},  {10, -2}, {8, -2},  {6, -2},
      {4, -2},  {2, -2},  {2, -4},  {2, -6},  {2, -8},  {2, -10}, {0, -10}, {-2, -10},
      {-2, -8}, {-2, -6}, {-2, -4}, {-2, -2}, {-4, -2}, {-6, -2}, {-8, -2}, {-10, -2},
  };
  TEST_POLYFILL_TEMPLATE_STATIC(poly, POLYFILL2D_TEST_NO_ZERO_AREA_TRIS);
}
