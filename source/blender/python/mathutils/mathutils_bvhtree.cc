/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup mathutils
 *
 * This file defines the 'mathutils.bvhtree' module, a general purpose module to access
 * blenders bvhtree for mesh surface nearest-element search and ray casting.
 */

#include <Python.h>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_kdopbvh.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_memarena.h"
#include "BLI_polyfill_2d.h"
#include "BLI_utildefines.h"

#include "BKE_bvhutils.hh"

#include "../generic/py_capi_utils.hh"
#include "../generic/python_utildefines.hh"

#include "mathutils.hh"
#include "mathutils_bvhtree.hh" /* own include */

#ifndef MATH_STANDALONE
#  include "DNA_mesh_types.h"
#  include "DNA_object_types.h"

#  include "BKE_customdata.hh"
#  include "BKE_lib_id.hh"
#  include "BKE_mesh.hh"
#  include "BKE_mesh_runtime.hh"
#  include "BKE_object.hh"

#  include "DEG_depsgraph_query.hh"

#  include "bmesh.hh"

#  include "../bmesh/bmesh_py_types.hh"
#endif /* MATH_STANDALONE */

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

/* -------------------------------------------------------------------- */
/** \name Documentation String (snippets)
 * \{ */

#define PYBVH_FIND_GENERIC_DISTANCE_DOC \
  "   :arg distance: Maximum distance threshold.\n" \
  "   :type distance: float\n"

#define PYBVH_FIND_GENERIC_RETURN_DOC \
  "   :return: Returns a tuple: (position, normal, index, distance),\n" \
  "      Values will all be None if no hit is found.\n" \
  "   :rtype: tuple[:class:`Vector` | None, :class:`Vector` | None, int | None, float | None]\n"

#define PYBVH_FIND_GENERIC_RETURN_LIST_DOC \
  "   :return: Returns a list of tuples (position, normal, index, distance)\n" \
  "   :rtype: list[tuple[:class:`Vector`, :class:`Vector`, int, float]]\n"

#define PYBVH_FROM_GENERIC_EPSILON_DOC \
  "   :arg epsilon: Increase the threshold for detecting overlap and raycast hits.\n" \
  "   :type epsilon: float\n"

/** \} */

/* sqrt(FLT_MAX) */
#define PYBVH_MAX_DIST_STR "1.84467e+19"
static const float max_dist_default = 1.844674352395373e+19f;

static const char PY_BVH_TREE_TYPE_DEFAULT = 4;
static const char PY_BVH_AXIS_DEFAULT = 6;

struct PyBVHTree {
  PyObject_HEAD
  BVHTree *tree;
  float epsilon;

  float (*coords)[3];
  uint (*tris)[3];
  uint coords_len, tris_len;

  /* Optional members */
  /* aligned with 'tris' */
  int *orig_index;
  /* aligned with array that 'orig_index' points to */
  float (*orig_normal)[3];
};

/* -------------------------------------------------------------------- */
/** \name Utility helper functions
 * \{ */

static PyObject *bvhtree_CreatePyObject(BVHTree *tree,
                                        float epsilon,

                                        float (*coords)[3],
                                        uint coords_len,
                                        uint (*tris)[3],
                                        uint tris_len,

                                        /* optional arrays */
                                        int *orig_index,
                                        float (*orig_normal)[3])
{
  PyBVHTree *result = PyObject_New(PyBVHTree, &PyBVHTree_Type);

  result->tree = tree;
  result->epsilon = epsilon;

  result->coords = coords;
  result->tris = tris;
  result->coords_len = coords_len;
  result->tris_len = tris_len;

  result->orig_index = orig_index;
  result->orig_normal = orig_normal;

  return (PyObject *)result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BVHTreeRayHit to Python utilities
 * \{ */

static void py_bvhtree_raycast_to_py_tuple(const BVHTreeRayHit *hit, PyObject *py_retval)
{
  BLI_assert(hit->index >= 0);
  BLI_assert(PyTuple_GET_SIZE(py_retval) == 4);

  PyTuple_SET_ITEMS(py_retval,
                    Vector_CreatePyObject(hit->co, 3, nullptr),
                    Vector_CreatePyObject(hit->no, 3, nullptr),
                    PyLong_FromLong(hit->index),
                    PyFloat_FromDouble(hit->dist));
}

static PyObject *py_bvhtree_raycast_to_py(const BVHTreeRayHit *hit)
{
  PyObject *py_retval = PyTuple_New(4);

  py_bvhtree_raycast_to_py_tuple(hit, py_retval);

  return py_retval;
}

static PyObject *py_bvhtree_raycast_to_py_none()
{
  PyObject *py_retval = PyTuple_New(4);

  PyC_Tuple_Fill(py_retval, Py_None);

  return py_retval;
}

#if 0
static PyObject *py_bvhtree_raycast_to_py_and_check(const BVHTreeRayHit *hit)
{
  PyObject *py_retval;

  py_retval = PyTuple_New(4);

  if (hit->index != -1) {
    py_bvhtree_raycast_to_py_tuple(hit, py_retval);
  }
  else {
    PyC_Tuple_Fill(py_retval, Py_None);
  }

  return py_retval;
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name BVHTreeNearest to Python utilities
 * \{ */

static void py_bvhtree_nearest_to_py_tuple(const BVHTreeNearest *nearest, PyObject *py_retval)
{
  BLI_assert(nearest->index >= 0);
  BLI_assert(PyTuple_GET_SIZE(py_retval) == 4);

  PyTuple_SET_ITEMS(py_retval,
                    Vector_CreatePyObject(nearest->co, 3, nullptr),
                    Vector_CreatePyObject(nearest->no, 3, nullptr),
                    PyLong_FromLong(nearest->index),
                    PyFloat_FromDouble(sqrtf(nearest->dist_sq)));
}

static PyObject *py_bvhtree_nearest_to_py(const BVHTreeNearest *nearest)
{
  PyObject *py_retval = PyTuple_New(4);

  py_bvhtree_nearest_to_py_tuple(nearest, py_retval);

  return py_retval;
}

static PyObject *py_bvhtree_nearest_to_py_none()
{
  PyObject *py_retval = PyTuple_New(4);

  PyC_Tuple_Fill(py_retval, Py_None);

  return py_retval;
}

#if 0
static PyObject *py_bvhtree_nearest_to_py_and_check(const BVHTreeNearest *nearest)
{
  PyObject *py_retval;

  py_retval = PyTuple_New(4);

  if (nearest->index != -1) {
    py_bvhtree_nearest_to_py_tuple(nearest, py_retval);
  }
  else {
    PyC_Tuple_Fill(py_retval, Py_None);
  }

  return py_retval;
}
#endif

/** \} */

static void py_bvhtree__tp_dealloc(PyBVHTree *self)
{
  if (self->tree) {
    BLI_bvhtree_free(self->tree);
  }

  MEM_SAFE_FREE(self->coords);
  MEM_SAFE_FREE(self->tris);

  MEM_SAFE_FREE(self->orig_index);
  MEM_SAFE_FREE(self->orig_normal);

  Py_TYPE(self)->tp_free((PyObject *)self);
}

/* -------------------------------------------------------------------- */
/** \name Methods
 * \{ */

static void py_bvhtree_raycast_cb(void *userdata,
                                  int index,
                                  const BVHTreeRay *ray,
                                  BVHTreeRayHit *hit)
{
  const PyBVHTree *self = static_cast<const PyBVHTree *>(userdata);

  const float (*coords)[3] = self->coords;
  const uint *tri = self->tris[index];
  const float *tri_co[3] = {coords[tri[0]], coords[tri[1]], coords[tri[2]]};
  float dist;

  if (self->epsilon == 0.0f) {
    dist = blender::bke::bvhtree_ray_tri_intersection(ray, hit->dist, UNPACK3(tri_co));
  }
  else {
    dist = blender::bke::bvhtree_sphereray_tri_intersection(
        ray, self->epsilon, hit->dist, UNPACK3(tri_co));
  }

  if (dist >= 0 && dist < hit->dist) {
    hit->index = self->orig_index ? self->orig_index[index] : index;
    hit->dist = dist;
    madd_v3_v3v3fl(hit->co, ray->origin, ray->direction, dist);
    if (self->orig_normal) {
      copy_v3_v3(hit->no, self->orig_normal[hit->index]);
    }
    else {
      normal_tri_v3(hit->no, UNPACK3(tri_co));
    }
  }
}

static void py_bvhtree_nearest_point_cb(void *userdata,
                                        int index,
                                        const float co[3],
                                        BVHTreeNearest *nearest)
{
  PyBVHTree *self = static_cast<PyBVHTree *>(userdata);

  const float (*coords)[3] = (const float (*)[3])self->coords;
  const uint *tri = self->tris[index];
  const float *tri_co[3] = {coords[tri[0]], coords[tri[1]], coords[tri[2]]};
  float nearest_tmp[3], dist_sq;

  closest_on_tri_to_point_v3(nearest_tmp, co, UNPACK3(tri_co));
  dist_sq = len_squared_v3v3(co, nearest_tmp);

  if (dist_sq < nearest->dist_sq) {
    nearest->index = self->orig_index ? self->orig_index[index] : index;
    nearest->dist_sq = dist_sq;
    copy_v3_v3(nearest->co, nearest_tmp);
    if (self->orig_normal) {
      copy_v3_v3(nearest->no, self->orig_normal[nearest->index]);
    }
    else {
      normal_tri_v3(nearest->no, UNPACK3(tri_co));
    }
  }
}

PyDoc_STRVAR(
    /* Wrap. */
    py_bvhtree_ray_cast_doc,
    ".. method:: ray_cast(origin, direction, distance=sys.float_info.max, /)\n"
    "\n"
    "   Cast a ray onto the mesh.\n"
    "\n"
    "   :arg origin: Start location of the ray in object space.\n"
    "   :type origin: :class:`Vector`\n"
    "   :arg direction: Direction of the ray in object space.\n"
    "   :type direction: :class:`Vector`\n" PYBVH_FIND_GENERIC_DISTANCE_DOC
        PYBVH_FIND_GENERIC_RETURN_DOC);
static PyObject *py_bvhtree_ray_cast(PyBVHTree *self, PyObject *args)
{
  const char *error_prefix = "ray_cast";
  float co[3], direction[3];
  float max_dist = FLT_MAX;
  BVHTreeRayHit hit;

  /* parse args */
  {
    PyObject *py_co, *py_direction;

    if (!PyArg_ParseTuple(args, "OO|f:ray_cast", &py_co, &py_direction, &max_dist)) {
      return nullptr;
    }

    if ((mathutils_array_parse(co, 2, 3 | MU_ARRAY_ZERO, py_co, error_prefix) == -1) ||
        (mathutils_array_parse(direction, 2, 3 | MU_ARRAY_ZERO, py_direction, error_prefix) == -1))
    {
      return nullptr;
    }

    normalize_v3(direction);
  }

  hit.dist = max_dist;
  hit.index = -1;

  /* may fail if the mesh has no faces, in that case the ray-cast misses */
  if (self->tree) {
    if (BLI_bvhtree_ray_cast(self->tree, co, direction, 0.0f, &hit, py_bvhtree_raycast_cb, self) !=
        -1)
    {
      return py_bvhtree_raycast_to_py(&hit);
    }
  }

  return py_bvhtree_raycast_to_py_none();
}

PyDoc_STRVAR(
    /* Wrap. */
    py_bvhtree_find_nearest_doc,
    ".. method:: find_nearest(origin, distance=" PYBVH_MAX_DIST_STR
    ", /)\n"
    "\n"
    "   Find the nearest element (typically face index) to a point.\n"
    "\n"
    "   :arg co: Find nearest element to this point.\n"
    "   :type co: :class:`Vector`\n" PYBVH_FIND_GENERIC_DISTANCE_DOC
        PYBVH_FIND_GENERIC_RETURN_DOC);
static PyObject *py_bvhtree_find_nearest(PyBVHTree *self, PyObject *args)
{
  const char *error_prefix = "find_nearest";
  float co[3];
  float max_dist = max_dist_default;

  BVHTreeNearest nearest;

  /* parse args */
  {
    PyObject *py_co;

    if (!PyArg_ParseTuple(args, "O|f:find_nearest", &py_co, &max_dist)) {
      return nullptr;
    }

    if (mathutils_array_parse(co, 2, 3 | MU_ARRAY_ZERO, py_co, error_prefix) == -1) {
      return nullptr;
    }
  }

  nearest.index = -1;
  nearest.dist_sq = max_dist * max_dist;

  /* may fail if the mesh has no faces, in that case the ray-cast misses */
  if (self->tree) {
    if (BLI_bvhtree_find_nearest(self->tree, co, &nearest, py_bvhtree_nearest_point_cb, self) !=
        -1)
    {
      return py_bvhtree_nearest_to_py(&nearest);
    }
  }

  return py_bvhtree_nearest_to_py_none();
}

struct PyBVH_RangeData {
  PyBVHTree *self;
  PyObject *result;
  float dist_sq;
};

static void py_bvhtree_nearest_point_range_cb(void *userdata,
                                              int index,
                                              const float co[3],
                                              float /*dist_sq_bvh*/)
{
  PyBVH_RangeData *data = static_cast<PyBVH_RangeData *>(userdata);
  PyBVHTree *self = data->self;

  const float (*coords)[3] = self->coords;
  const uint *tri = self->tris[index];
  const float *tri_co[3] = {coords[tri[0]], coords[tri[1]], coords[tri[2]]};
  float nearest_tmp[3], dist_sq;

  closest_on_tri_to_point_v3(nearest_tmp, co, UNPACK3(tri_co));
  dist_sq = len_squared_v3v3(co, nearest_tmp);

  if (dist_sq < data->dist_sq) {
    BVHTreeNearest nearest;
    nearest.index = self->orig_index ? self->orig_index[index] : index;
    nearest.dist_sq = dist_sq;
    copy_v3_v3(nearest.co, nearest_tmp);
    if (self->orig_normal) {
      copy_v3_v3(nearest.no, self->orig_normal[nearest.index]);
    }
    else {
      normal_tri_v3(nearest.no, UNPACK3(tri_co));
    }

    PyList_APPEND(data->result, py_bvhtree_nearest_to_py(&nearest));
  }
}

PyDoc_STRVAR(
    /* Wrap. */
    py_bvhtree_find_nearest_range_doc,
    ".. method:: find_nearest_range(origin, distance=" PYBVH_MAX_DIST_STR
    ", /)\n"
    "\n"
    "   Find the nearest elements (typically face index) to a point in the distance range.\n"
    "\n"
    "   :arg co: Find nearest elements to this point.\n"
    "   :type co: :class:`Vector`\n" PYBVH_FIND_GENERIC_DISTANCE_DOC
        PYBVH_FIND_GENERIC_RETURN_LIST_DOC);
static PyObject *py_bvhtree_find_nearest_range(PyBVHTree *self, PyObject *args)
{
  const char *error_prefix = "find_nearest_range";
  float co[3];
  float max_dist = max_dist_default;

  /* parse args */
  {
    PyObject *py_co;

    if (!PyArg_ParseTuple(args, "O|f:find_nearest_range", &py_co, &max_dist)) {
      return nullptr;
    }

    if (mathutils_array_parse(co, 2, 3 | MU_ARRAY_ZERO, py_co, error_prefix) == -1) {
      return nullptr;
    }
  }

  PyObject *ret = PyList_New(0);

  if (self->tree) {
    PyBVH_RangeData data{};
    data.self = self;
    data.result = ret;
    data.dist_sq = square_f(max_dist);
    BLI_bvhtree_range_query(self->tree, co, max_dist, py_bvhtree_nearest_point_range_cb, &data);
  }

  return ret;
}

BLI_INLINE uint overlap_hash(const void *overlap_v)
{
  const BVHTreeOverlap *overlap = static_cast<const BVHTreeOverlap *>(overlap_v);
  /* same constants as edge-hash */
  return ((uint(overlap->indexA) * 65) ^ (uint(overlap->indexA) * 31));
}

BLI_INLINE bool overlap_cmp(const void *a_v, const void *b_v)
{
  const BVHTreeOverlap *a = static_cast<const BVHTreeOverlap *>(a_v);
  const BVHTreeOverlap *b = static_cast<const BVHTreeOverlap *>(b_v);
  return (memcmp(a, b, sizeof(*a)) != 0);
}

struct PyBVHTree_OverlapData {
  PyBVHTree *tree_pair[2];
  float epsilon;
};

static bool py_bvhtree_overlap_cb(void *userdata, int index_a, int index_b, int /*thread*/)
{
  PyBVHTree_OverlapData *data = static_cast<PyBVHTree_OverlapData *>(userdata);
  PyBVHTree *tree_a = data->tree_pair[0];
  PyBVHTree *tree_b = data->tree_pair[1];
  const uint *tri_a = tree_a->tris[index_a];
  const uint *tri_b = tree_b->tris[index_b];
  const float *tri_a_co[3] = {
      tree_a->coords[tri_a[0]], tree_a->coords[tri_a[1]], tree_a->coords[tri_a[2]]};
  const float *tri_b_co[3] = {
      tree_b->coords[tri_b[0]], tree_b->coords[tri_b[1]], tree_b->coords[tri_b[2]]};
  float ix_pair[2][3];
  int verts_shared = 0;

  if (tree_a == tree_b) {
    if (UNLIKELY(index_a == index_b)) {
      return false;
    }

    verts_shared = (ELEM(tri_a_co[0], UNPACK3(tri_b_co)) + ELEM(tri_a_co[1], UNPACK3(tri_b_co)) +
                    ELEM(tri_a_co[2], UNPACK3(tri_b_co)));

    /* if 2 points are shared, bail out */
    if (verts_shared >= 2) {
      return false;
    }
  }

  return (isect_tri_tri_v3(UNPACK3(tri_a_co), UNPACK3(tri_b_co), ix_pair[0], ix_pair[1]) &&
          ((verts_shared == 0) || (len_squared_v3v3(ix_pair[0], ix_pair[1]) > data->epsilon)));
}

PyDoc_STRVAR(
    /* Wrap. */
    py_bvhtree_overlap_doc,
    ".. method:: overlap(other_tree, /)\n"
    "\n"
    "   Find overlapping indices between 2 trees.\n"
    "\n"
    "   :arg other_tree: Other tree to perform overlap test on.\n"
    "   :type other_tree: :class:`BVHTree`\n"
    "   :return: Returns a list of unique index pairs,"
    "      the first index referencing this tree, the second referencing the **other_tree**.\n"
    "   :rtype: list[tuple[int, int]]\n");
static PyObject *py_bvhtree_overlap(PyBVHTree *self, PyBVHTree *other)
{
  PyBVHTree_OverlapData data;
  BVHTreeOverlap *overlap;
  uint overlap_len = 0;
  PyObject *ret;

  if (!PyBVHTree_CheckExact(other)) {
    PyErr_SetString(PyExc_ValueError, "Expected a BVHTree argument");
    return nullptr;
  }

  data.tree_pair[0] = self;
  data.tree_pair[1] = other;
  data.epsilon = max_ff(self->epsilon, other->epsilon);

  overlap = BLI_bvhtree_overlap(
      self->tree, other->tree, &overlap_len, py_bvhtree_overlap_cb, &data);

  ret = PyList_New(0);

  if (overlap == nullptr) {
    /* pass */
  }
  else {
    const bool use_unique = (self->orig_index || other->orig_index);
    GSet *pair_test = use_unique ?
                          BLI_gset_new_ex(overlap_hash, overlap_cmp, __func__, overlap_len) :
                          nullptr;
    /* simple case, no index remapping */
    uint i;

    for (i = 0; i < overlap_len; i++) {
      PyObject *item;
      if (use_unique) {
        if (self->orig_index) {
          overlap[i].indexA = self->orig_index[overlap[i].indexA];
        }
        if (other->orig_index) {
          overlap[i].indexB = other->orig_index[overlap[i].indexB];
        }

        /* skip if its already added */
        if (!BLI_gset_add(pair_test, &overlap[i])) {
          continue;
        }
      }

      item = PyTuple_New(2);
      PyTuple_SET_ITEMS(
          item, PyLong_FromLong(overlap[i].indexA), PyLong_FromLong(overlap[i].indexB));

      PyList_Append(ret, item);
      Py_DECREF(item);
    }

    if (pair_test) {
      BLI_gset_free(pair_test, nullptr);
    }
  }

  if (overlap) {
    MEM_freeN(overlap);
  }

  return ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Class Methods
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    C_BVHTree_FromPolygons_doc,
    ".. classmethod:: FromPolygons(vertices, polygons, *, all_triangles=False, epsilon=0.0)\n"
    "\n"
    "   BVH tree constructed geometry passed in as arguments.\n"
    "\n"
    "   :arg vertices: float triplets each representing ``(x, y, z)``\n"
    "   :type vertices: Sequence[Sequence[float]]\n"
    "   :arg polygons: Sequence of polygons, each containing indices to the vertices argument.\n"
    "   :type polygons: Sequence[Sequence[int]]\n"
    "   :arg all_triangles: Use when all **polygons** are triangles for more efficient "
    "conversion.\n"
    "   :type all_triangles: bool\n" PYBVH_FROM_GENERIC_EPSILON_DOC);
static PyObject *C_BVHTree_FromPolygons(PyObject * /*cls*/, PyObject *args, PyObject *kwargs)
{
  const char *error_prefix = "BVHTree.FromPolygons";
  const char *keywords[] = {"vertices", "polygons", "all_triangles", "epsilon", nullptr};

  PyObject *py_coords, *py_tris;
  PyObject *py_coords_fast = nullptr, *py_tris_fast = nullptr;

  MemArena *poly_arena = nullptr;
  MemArena *pf_arena = nullptr;

  float (*coords)[3] = nullptr;
  uint(*tris)[3] = nullptr;
  uint coords_len, tris_len;
  float epsilon = 0.0f;
  bool all_triangles = false;

  /* when all_triangles is False */
  int *orig_index = nullptr;
  float (*orig_normal)[3] = nullptr;

  uint i;
  bool valid = true;

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwargs,
                                   "OO|$O&f:BVHTree.FromPolygons",
                                   (char **)keywords,
                                   &py_coords,
                                   &py_tris,
                                   PyC_ParseBool,
                                   &all_triangles,
                                   &epsilon))
  {
    return nullptr;
  }

  if (!(py_coords_fast = PySequence_Fast(py_coords, error_prefix)) ||
      !(py_tris_fast = PySequence_Fast(py_tris, error_prefix)))
  {
    Py_XDECREF(py_coords_fast);
    return nullptr;
  }

  if (valid) {
    PyObject **py_coords_fast_items = PySequence_Fast_ITEMS(py_coords_fast);
    coords_len = uint(PySequence_Fast_GET_SIZE(py_coords_fast));
    coords = MEM_malloc_arrayN<float[3]>(size_t(coords_len), __func__);

    for (i = 0; i < coords_len; i++) {
      PyObject *py_vert = py_coords_fast_items[i];

      if (mathutils_array_parse(coords[i], 3, 3, py_vert, "BVHTree vertex: ") == -1) {
        valid = false;
        break;
      }
    }
  }

  if (valid == false) {
    /* pass */
  }
  else if (all_triangles) {
    /* all triangles, simple case */
    PyObject **py_tris_fast_items = PySequence_Fast_ITEMS(py_tris_fast);
    tris_len = uint(PySequence_Fast_GET_SIZE(py_tris_fast));
    tris = MEM_malloc_arrayN<uint[3]>(size_t(tris_len), __func__);

    for (i = 0; i < tris_len; i++) {
      PyObject *py_tricoords = py_tris_fast_items[i];
      PyObject *py_tricoords_fast;
      PyObject **py_tricoords_fast_items;
      uint *tri = tris[i];
      int j;

      if (!(py_tricoords_fast = PySequence_Fast(py_tricoords, error_prefix))) {
        valid = false;
        break;
      }

      if (PySequence_Fast_GET_SIZE(py_tricoords_fast) != 3) {
        Py_DECREF(py_tricoords_fast);
        PyErr_Format(PyExc_ValueError,
                     "%s: non triangle found at index %d with length of %d",
                     error_prefix,
                     i,
                     PySequence_Fast_GET_SIZE(py_tricoords_fast));
        valid = false;
        break;
      }

      py_tricoords_fast_items = PySequence_Fast_ITEMS(py_tricoords_fast);

      for (j = 0; j < 3; j++) {
        tri[j] = PyC_Long_AsU32(py_tricoords_fast_items[j]);
        if (UNLIKELY(tri[j] >= uint(coords_len))) {
          PyErr_Format(PyExc_ValueError,
                       "%s: index %d must be less than %d",
                       error_prefix,
                       tri[j],
                       coords_len);

          /* decref below */
          valid = false;
          break;
        }
      }

      Py_DECREF(py_tricoords_fast);
    }
  }
  else {
    /* ngon support (much more involved) */
    const uint polys_len = uint(PySequence_Fast_GET_SIZE(py_tris_fast));
    struct PolyLink {
      PolyLink *next;
      uint len;
      uint poly[0];
    } *plink_first = nullptr, **p_plink_prev = &plink_first, *plink = nullptr;
    int poly_index;

    tris_len = 0;

    poly_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

    for (i = 0; i < polys_len; i++) {
      PyObject *py_tricoords = PySequence_Fast_GET_ITEM(py_tris_fast, i);
      PyObject *py_tricoords_fast;
      PyObject **py_tricoords_fast_items;
      uint py_tricoords_len;
      uint j;

      if (!(py_tricoords_fast = PySequence_Fast(py_tricoords, error_prefix))) {
        valid = false;
        break;
      }

      py_tricoords_len = uint(PySequence_Fast_GET_SIZE(py_tricoords_fast));
      py_tricoords_fast_items = PySequence_Fast_ITEMS(py_tricoords_fast);

      plink = static_cast<PolyLink *>(BLI_memarena_alloc(
          poly_arena, sizeof(*plink) + (sizeof(int) * size_t(py_tricoords_len))));

      plink->len = py_tricoords_len;
      *p_plink_prev = plink;
      p_plink_prev = &plink->next;

      for (j = 0; j < py_tricoords_len; j++) {
        plink->poly[j] = PyC_Long_AsU32(py_tricoords_fast_items[j]);
        if (UNLIKELY(plink->poly[j] >= uint(coords_len))) {
          PyErr_Format(PyExc_ValueError,
                       "%s: index %d must be less than %d",
                       error_prefix,
                       plink->poly[j],
                       coords_len);
          /* decref below */
          valid = false;
          break;
        }
      }

      Py_DECREF(py_tricoords_fast);

      if (py_tricoords_len >= 3) {
        tris_len += (py_tricoords_len - 2);
      }
    }
    *p_plink_prev = nullptr;

    /* All NGON's are parsed, now tessellate. */

    pf_arena = BLI_memarena_new(BLI_POLYFILL_ARENA_SIZE, __func__);
    tris = MEM_malloc_arrayN<uint[3]>(size_t(tris_len), __func__);

    orig_index = MEM_malloc_arrayN<int>(size_t(tris_len), __func__);
    orig_normal = MEM_malloc_arrayN<float[3]>(size_t(polys_len), __func__);

    for (plink = plink_first, poly_index = 0, i = 0; plink; plink = plink->next, poly_index++) {
      if (plink->len == 3) {
        uint *tri = tris[i];
        memcpy(tri, plink->poly, sizeof(uint[3]));
        orig_index[i] = poly_index;
        normal_tri_v3(orig_normal[poly_index], coords[tri[0]], coords[tri[1]], coords[tri[2]]);
        i++;
      }
      else if (plink->len > 3) {
        float (*proj_coords)[2] = static_cast<float (*)[2]>(
            BLI_memarena_alloc(pf_arena, sizeof(*proj_coords) * plink->len));
        float *normal = orig_normal[poly_index];
        const float *co_prev;
        const float *co_curr;
        float axis_mat[3][3];
        uint(*tris_offset)[3] = &tris[i];
        uint j;

        /* calc normal and setup 'proj_coords' */
        zero_v3(normal);
        co_prev = coords[plink->poly[plink->len - 1]];
        for (j = 0; j < plink->len; j++) {
          co_curr = coords[plink->poly[j]];
          add_newell_cross_v3_v3v3(normal, co_prev, co_curr);
          co_prev = co_curr;
        }
        normalize_v3(normal);

        axis_dominant_v3_to_m3_negate(axis_mat, normal);

        for (j = 0; j < plink->len; j++) {
          mul_v2_m3v3(proj_coords[j], axis_mat, coords[plink->poly[j]]);
        }

        BLI_polyfill_calc_arena(proj_coords, plink->len, 1, tris_offset, pf_arena);

        j = plink->len - 2;
        while (j--) {
          uint *tri = tris_offset[j];
          /* remap to global indices */
          tri[0] = plink->poly[tri[0]];
          tri[1] = plink->poly[tri[1]];
          tri[2] = plink->poly[tri[2]];

          orig_index[i] = poly_index;
          i++;
        }

        BLI_memarena_clear(pf_arena);
      }
      else {
        zero_v3(orig_normal[poly_index]);
      }
    }
  }

  Py_DECREF(py_coords_fast);
  Py_DECREF(py_tris_fast);

  if (pf_arena) {
    BLI_memarena_free(pf_arena);
  }

  if (poly_arena) {
    BLI_memarena_free(poly_arena);
  }

  if (valid) {
    BVHTree *tree;

    tree = BLI_bvhtree_new(int(tris_len), epsilon, PY_BVH_TREE_TYPE_DEFAULT, PY_BVH_AXIS_DEFAULT);
    if (tree) {
      for (i = 0; i < tris_len; i++) {
        float co[3][3];

        copy_v3_v3(co[0], coords[tris[i][0]]);
        copy_v3_v3(co[1], coords[tris[i][1]]);
        copy_v3_v3(co[2], coords[tris[i][2]]);

        BLI_bvhtree_insert(tree, int(i), co[0], 3);
      }

      BLI_bvhtree_balance(tree);
    }

    return bvhtree_CreatePyObject(
        tree, epsilon, coords, coords_len, tris, tris_len, orig_index, orig_normal);
  }

  if (coords) {
    MEM_freeN(coords);
  }
  if (tris) {
    MEM_freeN(tris);
  }

  return nullptr;
}

#ifndef MATH_STANDALONE

PyDoc_STRVAR(
    /* Wrap. */
    C_BVHTree_FromBMesh_doc,
    ".. classmethod:: FromBMesh(bmesh, *, epsilon=0.0)\n"
    "\n"
    "   BVH tree based on :class:`BMesh` data.\n"
    "\n"
    "   :arg bmesh: BMesh data.\n"
    "   :type bmesh: :class:`BMesh`\n" PYBVH_FROM_GENERIC_EPSILON_DOC);
static PyObject *C_BVHTree_FromBMesh(PyObject * /*cls*/, PyObject *args, PyObject *kwargs)
{
  const char *keywords[] = {"bmesh", "epsilon", nullptr};

  BPy_BMesh *py_bm;

  float (*coords)[3] = nullptr;
  uint(*tris)[3] = nullptr;
  uint coords_len, tris_len;
  float epsilon = 0.0f;

  BMesh *bm;

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwargs,
                                   "O!|$f:BVHTree.FromBMesh",
                                   (char **)keywords,
                                   &BPy_BMesh_Type,
                                   &py_bm,
                                   &epsilon))
  {
    return nullptr;
  }

  bm = py_bm->bm;

  /* Get data for tessellation */

  coords_len = uint(bm->totvert);
  tris_len = uint(poly_to_tri_count(bm->totface, bm->totloop));

  coords = MEM_malloc_arrayN<float[3]>(size_t(coords_len), __func__);
  tris = MEM_malloc_arrayN<uint[3]>(size_t(tris_len), __func__);

  blender::Array<std::array<BMLoop *, 3>> corner_tris(tris_len);
  BM_mesh_calc_tessellation(bm, corner_tris);

  {
    BMIter iter;
    BVHTree *tree;
    uint i;

    int *orig_index = nullptr;
    float (*orig_normal)[3] = nullptr;

    tree = BLI_bvhtree_new(int(tris_len), epsilon, PY_BVH_TREE_TYPE_DEFAULT, PY_BVH_AXIS_DEFAULT);
    if (tree) {
      BMFace *f;
      BMVert *v;

      orig_index = MEM_malloc_arrayN<int>(size_t(tris_len), __func__);
      orig_normal = MEM_malloc_arrayN<float[3]>(size_t(bm->totface), __func__);

      BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
        copy_v3_v3(coords[i], v->co);
        BM_elem_index_set(v, int(i)); /* set_inline */
      }
      BM_ITER_MESH_INDEX (f, &iter, bm, BM_FACES_OF_MESH, i) {
        copy_v3_v3(orig_normal[i], f->no);
        BM_elem_index_set(f, int(i)); /* set_inline */
      }
      bm->elem_index_dirty &= char(~(BM_VERT | BM_FACE));

      for (i = 0; i < tris_len; i++) {
        float co[3][3];

        tris[i][0] = uint(BM_elem_index_get(corner_tris[i][0]->v));
        tris[i][1] = uint(BM_elem_index_get(corner_tris[i][1]->v));
        tris[i][2] = uint(BM_elem_index_get(corner_tris[i][2]->v));

        copy_v3_v3(co[0], coords[tris[i][0]]);
        copy_v3_v3(co[1], coords[tris[i][1]]);
        copy_v3_v3(co[2], coords[tris[i][2]]);

        BLI_bvhtree_insert(tree, int(i), co[0], 3);
        orig_index[i] = BM_elem_index_get(corner_tris[i][0]->f);
      }

      BLI_bvhtree_balance(tree);
    }

    return bvhtree_CreatePyObject(
        tree, epsilon, coords, coords_len, tris, tris_len, orig_index, orig_normal);
  }
}

/** Return various evaluated meshes based on requested settings. */
static const Mesh *bvh_get_mesh(const char *funcname,
                                Depsgraph *depsgraph,
                                Scene *scene,
                                Object *ob,
                                const bool use_deform,
                                const bool use_cage,
                                bool *r_free_mesh)
{
  Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
  /* we only need minimum mesh data for topology and vertex locations */
  const CustomData_MeshMasks data_masks = CD_MASK_BAREMESH;
  const bool use_render = DEG_get_mode(depsgraph) == DAG_EVAL_RENDER;
  *r_free_mesh = false;
  Mesh *mesh;

  /* Write the display mesh into the dummy mesh */
  if (use_deform) {
    if (use_render) {
      if (use_cage) {
        PyErr_Format(
            PyExc_ValueError,
            "%s(...): cage arg is unsupported when dependency graph evaluation mode is RENDER",
            funcname);
        return nullptr;
      }

      mesh = blender::bke::mesh_create_eval_final(depsgraph, scene, ob, &data_masks);
      if (mesh == nullptr) {
        PyErr_Format(PyExc_ValueError,
                     "%s(...): Cannot get a mesh from object '%s'",
                     ob->id.name + 2,
                     funcname);
        return nullptr;
      }

      *r_free_mesh = true;
      return mesh;
    }
    if (ob_eval != nullptr) {
      if (use_cage) {
        mesh = blender::bke::mesh_get_eval_deform(depsgraph, scene, ob_eval, &data_masks);
      }
      else {
        mesh = BKE_object_get_evaluated_mesh(ob_eval);
      }
      if (mesh == nullptr) {
        PyErr_Format(PyExc_ValueError,
                     "%s(...): Cannot get a mesh from object '%s'",
                     ob->id.name + 2,
                     funcname);
        return nullptr;
      }
      return mesh;
    }

    PyErr_Format(PyExc_ValueError,
                 "%s(...): Cannot get evaluated data from given dependency graph / object pair",
                 funcname);
    return nullptr;
  }

  /* !use_deform */
  if (use_render) {
    if (use_cage) {
      PyErr_Format(
          PyExc_ValueError,
          "%s(...): cage arg is unsupported when dependency graph evaluation mode is RENDER",
          funcname);
      return nullptr;
    }
    mesh = blender::bke::mesh_create_eval_no_deform_render(depsgraph, scene, ob, &data_masks);
    if (mesh == nullptr) {
      PyErr_Format(PyExc_ValueError,
                   "%s(...): Cannot get a mesh from object '%s'",
                   ob->id.name + 2,
                   funcname);
      return nullptr;
    }
    *r_free_mesh = true;
    return mesh;
  }

  if (use_cage) {
    PyErr_Format(PyExc_ValueError,
                 "%s(...): cage arg is unsupported when deform=False and dependency graph "
                 "evaluation mode is not RENDER",
                 funcname);
    return nullptr;
  }

  mesh = blender::bke::mesh_create_eval_no_deform(depsgraph, scene, ob, &data_masks);
  if (mesh == nullptr) {
    PyErr_Format(PyExc_ValueError,
                 "%s(...): Cannot get a mesh from object '%s'",
                 ob->id.name + 2,
                 funcname);
    return nullptr;
  }
  *r_free_mesh = true;
  return mesh;
}

PyDoc_STRVAR(
    /* Wrap. */
    C_BVHTree_FromObject_doc,
    ".. classmethod:: FromObject(object, depsgraph, *, deform=True, render=False, "
    "cage=False, epsilon=0.0)\n"
    "\n"
    "   BVH tree based on :class:`Object` data.\n"
    "\n"
    "   :arg object: Object data.\n"
    "   :type object: :class:`Object`\n"
    "   :arg depsgraph: Depsgraph to use for evaluating the mesh.\n"
    "   :type depsgraph: :class:`Depsgraph`\n"
    "   :arg deform: Use mesh with deformations.\n"
    "   :type deform: bool\n"
    "   :arg cage: Use modifiers cage.\n"
    "   :type cage: bool\n" PYBVH_FROM_GENERIC_EPSILON_DOC);
static PyObject *C_BVHTree_FromObject(PyObject * /*cls*/, PyObject *args, PyObject *kwargs)
{
  /* NOTE: options here match #bpy_bmesh_from_object. */
  const char *keywords[] = {"object", "depsgraph", "deform", "cage", "epsilon", nullptr};

  PyObject *py_ob, *py_depsgraph;
  Object *ob;
  Depsgraph *depsgraph;
  Scene *scene;
  const Mesh *mesh;
  bool use_deform = true;
  bool use_cage = false;
  bool free_mesh = false;

  float epsilon = 0.0f;

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwargs,
                                   "OO|$O&O&f:BVHTree.FromObject",
                                   (char **)keywords,
                                   &py_ob,
                                   &py_depsgraph,
                                   PyC_ParseBool,
                                   &use_deform,
                                   PyC_ParseBool,
                                   &use_cage,
                                   &epsilon) ||
      ((ob = static_cast<Object *>(PyC_RNA_AsPointer(py_ob, "Object"))) == nullptr) ||
      ((depsgraph = static_cast<Depsgraph *>(PyC_RNA_AsPointer(py_depsgraph, "Depsgraph"))) ==
       nullptr))
  {
    return nullptr;
  }

  scene = DEG_get_evaluated_scene(depsgraph);
  mesh = bvh_get_mesh("BVHTree", depsgraph, scene, ob, use_deform, use_cage, &free_mesh);

  if (mesh == nullptr) {
    return nullptr;
  }

  const blender::Span<int> corner_verts = mesh->corner_verts();
  const blender::Span<blender::int3> corner_tris = mesh->corner_tris();
  const blender::Span<int> tri_faces = mesh->corner_tri_faces();

  /* Get data for tessellation */

  const uint coords_len = uint(mesh->verts_num);

  float (*coords)[3] = MEM_malloc_arrayN<float[3]>(size_t(coords_len), __func__);
  uint(*tris)[3] = MEM_malloc_arrayN<uint[3]>(size_t(corner_tris.size()), __func__);
  memcpy(coords, mesh->vert_positions().data(), sizeof(float[3]) * size_t(mesh->verts_num));

  BVHTree *tree;

  int *orig_index = nullptr;
  blender::float3 *orig_normal = nullptr;

  tree = BLI_bvhtree_new(
      int(corner_tris.size()), epsilon, PY_BVH_TREE_TYPE_DEFAULT, PY_BVH_AXIS_DEFAULT);
  if (tree) {
    orig_index = MEM_malloc_arrayN<int>(size_t(corner_tris.size()), __func__);
    if (!BKE_mesh_face_normals_are_dirty(mesh)) {
      const blender::Span<blender::float3> face_normals = mesh->face_normals();
      orig_normal = MEM_malloc_arrayN<blender::float3>(size_t(mesh->faces_num), __func__);
      blender::MutableSpan(orig_normal, face_normals.size()).copy_from(face_normals);
    }

    for (const int64_t i : corner_tris.index_range()) {
      float co[3][3];

      tris[i][0] = uint(corner_verts[corner_tris[i][0]]);
      tris[i][1] = uint(corner_verts[corner_tris[i][1]]);
      tris[i][2] = uint(corner_verts[corner_tris[i][2]]);

      copy_v3_v3(co[0], coords[tris[i][0]]);
      copy_v3_v3(co[1], coords[tris[i][1]]);
      copy_v3_v3(co[2], coords[tris[i][2]]);

      BLI_bvhtree_insert(tree, int(i), co[0], 3);
      orig_index[i] = int(tri_faces[i]);
    }

    BLI_bvhtree_balance(tree);
  }

  if (free_mesh) {
    BKE_id_free(nullptr, const_cast<Mesh *>(mesh));
  }

  return bvhtree_CreatePyObject(tree,
                                epsilon,
                                coords,
                                coords_len,
                                tris,
                                uint(corner_tris.size()),
                                orig_index,
                                reinterpret_cast<float (*)[3]>(orig_normal));
}
#endif /* MATH_STANDALONE */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Module & Type definition
 * \{ */

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef py_bvhtree_methods[] = {
    {"ray_cast",
     reinterpret_cast<PyCFunction>(py_bvhtree_ray_cast),
     METH_VARARGS,
     py_bvhtree_ray_cast_doc},
    {"find_nearest",
     reinterpret_cast<PyCFunction>(py_bvhtree_find_nearest),
     METH_VARARGS,
     py_bvhtree_find_nearest_doc},
    {"find_nearest_range",
     reinterpret_cast<PyCFunction>(py_bvhtree_find_nearest_range),
     METH_VARARGS,
     py_bvhtree_find_nearest_range_doc},
    {"overlap", reinterpret_cast<PyCFunction>(py_bvhtree_overlap), METH_O, py_bvhtree_overlap_doc},

    /* class methods */
    {"FromPolygons",
     reinterpret_cast<PyCFunction>(C_BVHTree_FromPolygons),
     METH_VARARGS | METH_KEYWORDS | METH_CLASS,
     C_BVHTree_FromPolygons_doc},
#ifndef MATH_STANDALONE
    {"FromBMesh",
     reinterpret_cast<PyCFunction>(C_BVHTree_FromBMesh),
     METH_VARARGS | METH_KEYWORDS | METH_CLASS,
     C_BVHTree_FromBMesh_doc},
    {"FromObject",
     reinterpret_cast<PyCFunction>(C_BVHTree_FromObject),
     METH_VARARGS | METH_KEYWORDS | METH_CLASS,
     C_BVHTree_FromObject_doc},
#endif
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

PyTypeObject PyBVHTree_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "BVHTree",
    /*tp_basicsize*/ sizeof(PyBVHTree),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)py_bvhtree__tp_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ nullptr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ py_bvhtree_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ (allocfunc)PyType_GenericAlloc,
    /*tp_new*/ (newfunc)PyType_GenericNew,
    /*tp_free*/ (freefunc) nullptr,
    /*tp_is_gc*/ nullptr,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ (destructor) nullptr,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ nullptr,
};

/* -------------------------------------------------------------------- */
/* Module definition */

PyDoc_STRVAR(
    /* Wrap. */
    py_bvhtree_doc,
    "BVH tree structures for proximity searches and ray casts on geometry.");
static PyModuleDef bvhtree_moduledef = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "mathutils.bvhtree",
    /*m_doc*/ py_bvhtree_doc,
    /*m_size*/ 0,
    /*m_methods*/ nullptr,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyMODINIT_FUNC PyInit_mathutils_bvhtree()
{
  PyObject *m = PyModule_Create(&bvhtree_moduledef);

  if (m == nullptr) {
    return nullptr;
  }

  /* Register classes */
  if (PyType_Ready(&PyBVHTree_Type) < 0) {
    return nullptr;
  }

  PyModule_AddType(m, &PyBVHTree_Type);

  return m;
}

/** \} */
