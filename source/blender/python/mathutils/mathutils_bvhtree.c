/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup mathutils
 *
 * This file defines the 'mathutils.bvhtree' module, a general purpose module to access
 * blenders bvhtree for mesh surface nearest-element search and ray casting.
 */

#include <Python.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_kdopbvh.h"
#include "BLI_polyfill_2d.h"
#include "BLI_math.h"
#include "BLI_ghash.h"
#include "BLI_memarena.h"

#include "BKE_bvhutils.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

#include "mathutils.h"
#include "mathutils_bvhtree.h" /* own include */

#ifndef MATH_STANDALONE
#  include "DNA_object_types.h"
#  include "DNA_mesh_types.h"
#  include "DNA_meshdata_types.h"

#  include "BKE_customdata.h"
#  include "BKE_editmesh_bvh.h"
#  include "BKE_library.h"
#  include "BKE_mesh.h"
#  include "BKE_mesh_runtime.h"

#  include "DEG_depsgraph_query.h"

#  include "bmesh.h"

#  include "../bmesh/bmesh_py_types.h"
#endif /* MATH_STANDALONE */

#include "BLI_strict_flags.h"

/* -------------------------------------------------------------------- */
/** \name Documentation String (snippets)
 * \{ */

#define PYBVH_FIND_GENERIC_DISTANCE_DOC \
  "   :arg distance: Maximum distance threshold.\n" \
  "   :type distance: float\n"

#define PYBVH_FIND_GENERIC_RETURN_DOC \
  "   :return: Returns a tuple\n" \
  "      (:class:`Vector` location, :class:`Vector` normal, int index, float distance),\n" \
  "      Values will all be None if no hit is found.\n" \
  "   :rtype: :class:`tuple`\n"

#define PYBVH_FIND_GENERIC_RETURN_LIST_DOC \
  "   :return: Returns a list of tuples\n" \
  "      (:class:`Vector` location, :class:`Vector` normal, int index, float distance),\n" \
  "   :rtype: :class:`list`\n"

#define PYBVH_FROM_GENERIC_EPSILON_DOC \
  "   :arg epsilon: Increase the threshold for detecting overlap and raycast hits.\n" \
  "   :type epsilon: float\n"

/** \} */

/* sqrt(FLT_MAX) */
#define PYBVH_MAX_DIST_STR "1.84467e+19"
static const float max_dist_default = 1.844674352395373e+19f;

static const char PY_BVH_TREE_TYPE_DEFAULT = 4;
static const char PY_BVH_AXIS_DEFAULT = 6;

typedef struct {
  PyObject_HEAD BVHTree *tree;
  float epsilon;

  float (*coords)[3];
  unsigned int (*tris)[3];
  unsigned int coords_len, tris_len;

  /* Optional members */
  /* aligned with 'tris' */
  int *orig_index;
  /* aligned with array that 'orig_index' points to */
  float (*orig_normal)[3];
} PyBVHTree;

/* -------------------------------------------------------------------- */
/** \name Utility helper functions
 * \{ */

static PyObject *bvhtree_CreatePyObject(BVHTree *tree,
                                        float epsilon,

                                        float (*coords)[3],
                                        unsigned int coords_len,
                                        unsigned int (*tris)[3],
                                        unsigned int tris_len,

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
                    Vector_CreatePyObject(hit->co, 3, NULL),
                    Vector_CreatePyObject(hit->no, 3, NULL),
                    PyLong_FromLong(hit->index),
                    PyFloat_FromDouble(hit->dist));
}

static PyObject *py_bvhtree_raycast_to_py(const BVHTreeRayHit *hit)
{
  PyObject *py_retval = PyTuple_New(4);

  py_bvhtree_raycast_to_py_tuple(hit, py_retval);

  return py_retval;
}

static PyObject *py_bvhtree_raycast_to_py_none(void)
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
                    Vector_CreatePyObject(nearest->co, 3, NULL),
                    Vector_CreatePyObject(nearest->no, 3, NULL),
                    PyLong_FromLong(nearest->index),
                    PyFloat_FromDouble(sqrtf(nearest->dist_sq)));
}

static PyObject *py_bvhtree_nearest_to_py(const BVHTreeNearest *nearest)
{
  PyObject *py_retval = PyTuple_New(4);

  py_bvhtree_nearest_to_py_tuple(nearest, py_retval);

  return py_retval;
}

static PyObject *py_bvhtree_nearest_to_py_none(void)
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
  const PyBVHTree *self = userdata;

  const float(*coords)[3] = (const float(*)[3])self->coords;
  const unsigned int *tri = self->tris[index];
  const float *tri_co[3] = {coords[tri[0]], coords[tri[1]], coords[tri[2]]};
  float dist;

  if (self->epsilon == 0.0f) {
    dist = bvhtree_ray_tri_intersection(ray, hit->dist, UNPACK3(tri_co));
  }
  else {
    dist = bvhtree_sphereray_tri_intersection(ray, self->epsilon, hit->dist, UNPACK3(tri_co));
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
  PyBVHTree *self = userdata;

  const float(*coords)[3] = (const float(*)[3])self->coords;
  const unsigned int *tri = self->tris[index];
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

PyDoc_STRVAR(py_bvhtree_ray_cast_doc,
             ".. method:: ray_cast(origin, direction, distance=sys.float_info.max)\n"
             "\n"
             "   Cast a ray onto the mesh.\n"
             "\n"
             "   :arg co: Start location of the ray in object space.\n"
             "   :type co: :class:`Vector`\n"
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

    if (!PyArg_ParseTuple(args, (char *)"OO|f:ray_cast", &py_co, &py_direction, &max_dist)) {
      return NULL;
    }

    if ((mathutils_array_parse(co, 2, 3 | MU_ARRAY_ZERO, py_co, error_prefix) == -1) ||
        (mathutils_array_parse(direction, 2, 3 | MU_ARRAY_ZERO, py_direction, error_prefix) ==
         -1)) {
      return NULL;
    }

    normalize_v3(direction);
  }

  hit.dist = max_dist;
  hit.index = -1;

  /* may fail if the mesh has no faces, in that case the ray-cast misses */
  if (self->tree) {
    if (BLI_bvhtree_ray_cast(self->tree, co, direction, 0.0f, &hit, py_bvhtree_raycast_cb, self) !=
        -1) {
      return py_bvhtree_raycast_to_py(&hit);
    }
  }

  return py_bvhtree_raycast_to_py_none();
}

PyDoc_STRVAR(py_bvhtree_find_nearest_doc,
             ".. method:: find_nearest(origin, distance=" PYBVH_MAX_DIST_STR
             ")\n"
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

    if (!PyArg_ParseTuple(args, (char *)"O|f:find_nearest", &py_co, &max_dist)) {
      return NULL;
    }

    if (mathutils_array_parse(co, 2, 3 | MU_ARRAY_ZERO, py_co, error_prefix) == -1) {
      return NULL;
    }
  }

  nearest.index = -1;
  nearest.dist_sq = max_dist * max_dist;

  /* may fail if the mesh has no faces, in that case the ray-cast misses */
  if (self->tree) {
    if (BLI_bvhtree_find_nearest(self->tree, co, &nearest, py_bvhtree_nearest_point_cb, self) !=
        -1) {
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
                                              float UNUSED(dist_sq_bvh))
{
  struct PyBVH_RangeData *data = userdata;
  PyBVHTree *self = data->self;

  const float(*coords)[3] = (const float(*)[3])self->coords;
  const unsigned int *tri = self->tris[index];
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
    py_bvhtree_find_nearest_range_doc,
    ".. method:: find_nearest_range(origin, distance=" PYBVH_MAX_DIST_STR
    ")\n"
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

    if (!PyArg_ParseTuple(args, (char *)"O|f:find_nearest_range", &py_co, &max_dist)) {
      return NULL;
    }

    if (mathutils_array_parse(co, 2, 3 | MU_ARRAY_ZERO, py_co, error_prefix) == -1) {
      return NULL;
    }
  }

  PyObject *ret = PyList_New(0);

  if (self->tree) {
    struct PyBVH_RangeData data = {
        .self = self,
        .result = ret,
        .dist_sq = SQUARE(max_dist),
    };

    BLI_bvhtree_range_query(self->tree, co, max_dist, py_bvhtree_nearest_point_range_cb, &data);
  }

  return ret;
}

BLI_INLINE unsigned int overlap_hash(const void *overlap_v)
{
  const BVHTreeOverlap *overlap = overlap_v;
  /* same constants as edge-hash */
  return (((unsigned int)overlap->indexA * 65) ^ ((unsigned int)overlap->indexA * 31));
}

BLI_INLINE bool overlap_cmp(const void *a_v, const void *b_v)
{
  const BVHTreeOverlap *a = a_v;
  const BVHTreeOverlap *b = b_v;
  return (memcmp(a, b, sizeof(*a)) != 0);
}

struct PyBVHTree_OverlapData {
  PyBVHTree *tree_pair[2];
  float epsilon;
};

static bool py_bvhtree_overlap_cb(void *userdata, int index_a, int index_b, int UNUSED(thread))
{
  struct PyBVHTree_OverlapData *data = userdata;
  PyBVHTree *tree_a = data->tree_pair[0];
  PyBVHTree *tree_b = data->tree_pair[1];
  const unsigned int *tri_a = tree_a->tris[index_a];
  const unsigned int *tri_b = tree_b->tris[index_b];
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

  return (isect_tri_tri_epsilon_v3(
              UNPACK3(tri_a_co), UNPACK3(tri_b_co), ix_pair[0], ix_pair[1], data->epsilon) &&
          ((verts_shared == 0) || (len_squared_v3v3(ix_pair[0], ix_pair[1]) > data->epsilon)));
}

PyDoc_STRVAR(
    py_bvhtree_overlap_doc,
    ".. method:: overlap(other_tree)\n"
    "\n"
    "   Find overlapping indices between 2 trees.\n"
    "\n"
    "   :arg other_tree: Other tree to perform overlap test on.\n"
    "   :type other_tree: :class:`BVHTree`\n"
    "   :return: Returns a list of unique index pairs,"
    "      the first index referencing this tree, the second referencing the **other_tree**.\n"
    "   :rtype: :class:`list`\n");
static PyObject *py_bvhtree_overlap(PyBVHTree *self, PyBVHTree *other)
{
  struct PyBVHTree_OverlapData data;
  BVHTreeOverlap *overlap;
  unsigned int overlap_len = 0;
  PyObject *ret;

  if (!PyBVHTree_CheckExact(other)) {
    PyErr_SetString(PyExc_ValueError, "Expected a BVHTree argument");
    return NULL;
  }

  data.tree_pair[0] = self;
  data.tree_pair[1] = other;
  data.epsilon = max_ff(self->epsilon, other->epsilon);

  overlap = BLI_bvhtree_overlap(
      self->tree, other->tree, &overlap_len, py_bvhtree_overlap_cb, &data);

  ret = PyList_New(0);

  if (overlap == NULL) {
    /* pass */
  }
  else {
    bool use_unique = (self->orig_index || other->orig_index);
    GSet *pair_test = use_unique ?
                          BLI_gset_new_ex(overlap_hash, overlap_cmp, __func__, overlap_len) :
                          NULL;
    /* simple case, no index remapping */
    unsigned int i;

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
      BLI_gset_free(pair_test, NULL);
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
    C_BVHTree_FromPolygons_doc,
    ".. classmethod:: FromPolygons(vertices, polygons, all_triangles=False, epsilon=0.0)\n"
    "\n"
    "   BVH tree constructed geometry passed in as arguments.\n"
    "\n"
    "   :arg vertices: float triplets each representing ``(x, y, z)``\n"
    "   :type vertices: float triplet sequence\n"
    "   :arg polygons: Sequence of polyugons, each containing indices to the vertices argument.\n"
    "   :type polygons: Sequence of sequences containing ints\n"
    "   :arg all_triangles: Use when all **polygons** are triangles for more efficient "
    "conversion.\n"
    "   :type all_triangles: bool\n" PYBVH_FROM_GENERIC_EPSILON_DOC);
static PyObject *C_BVHTree_FromPolygons(PyObject *UNUSED(cls), PyObject *args, PyObject *kwargs)
{
  const char *error_prefix = "BVHTree.FromPolygons";
  const char *keywords[] = {"vertices", "polygons", "all_triangles", "epsilon", NULL};

  PyObject *py_coords, *py_tris;
  PyObject *py_coords_fast = NULL, *py_tris_fast = NULL;

  MemArena *poly_arena = NULL;
  MemArena *pf_arena = NULL;

  float(*coords)[3] = NULL;
  unsigned int(*tris)[3] = NULL;
  unsigned int coords_len, tris_len;
  float epsilon = 0.0f;
  bool all_triangles = false;

  /* when all_triangles is False */
  int *orig_index = NULL;
  float(*orig_normal)[3] = NULL;

  unsigned int i;
  bool valid = true;

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwargs,
                                   (char *)"OO|$O&f:BVHTree.FromPolygons",
                                   (char **)keywords,
                                   &py_coords,
                                   &py_tris,
                                   PyC_ParseBool,
                                   &all_triangles,
                                   &epsilon)) {
    return NULL;
  }

  if (!(py_coords_fast = PySequence_Fast(py_coords, error_prefix)) ||
      !(py_tris_fast = PySequence_Fast(py_tris, error_prefix))) {
    Py_XDECREF(py_coords_fast);
    return NULL;
  }

  if (valid) {
    PyObject **py_coords_fast_items = PySequence_Fast_ITEMS(py_coords_fast);
    coords_len = (unsigned int)PySequence_Fast_GET_SIZE(py_coords_fast);
    coords = MEM_mallocN((size_t)coords_len * sizeof(*coords), __func__);

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
    tris_len = (unsigned int)PySequence_Fast_GET_SIZE(py_tris_fast);
    tris = MEM_mallocN((size_t)tris_len * sizeof(*tris), __func__);

    for (i = 0; i < tris_len; i++) {
      PyObject *py_tricoords = py_tris_fast_items[i];
      PyObject *py_tricoords_fast;
      PyObject **py_tricoords_fast_items;
      unsigned int *tri = tris[i];
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
        if (UNLIKELY(tri[j] >= (unsigned int)coords_len)) {
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
    const unsigned int polys_len = (unsigned int)PySequence_Fast_GET_SIZE(py_tris_fast);
    struct PolyLink {
      struct PolyLink *next;
      unsigned int len;
      unsigned int poly[0];
    } *plink_first = NULL, **p_plink_prev = &plink_first, *plink = NULL;
    int poly_index;

    tris_len = 0;

    poly_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

    for (i = 0; i < polys_len; i++) {
      PyObject *py_tricoords = PySequence_Fast_GET_ITEM(py_tris_fast, i);
      PyObject *py_tricoords_fast;
      PyObject **py_tricoords_fast_items;
      unsigned int py_tricoords_len;
      unsigned int j;

      if (!(py_tricoords_fast = PySequence_Fast(py_tricoords, error_prefix))) {
        valid = false;
        break;
      }

      py_tricoords_len = (unsigned int)PySequence_Fast_GET_SIZE(py_tricoords_fast);
      py_tricoords_fast_items = PySequence_Fast_ITEMS(py_tricoords_fast);

      plink = BLI_memarena_alloc(poly_arena,
                                 sizeof(*plink) + (sizeof(int) * (size_t)py_tricoords_len));

      plink->len = (unsigned int)py_tricoords_len;
      *p_plink_prev = plink;
      p_plink_prev = &plink->next;

      for (j = 0; j < py_tricoords_len; j++) {
        plink->poly[j] = PyC_Long_AsU32(py_tricoords_fast_items[j]);
        if (UNLIKELY(plink->poly[j] >= (unsigned int)coords_len)) {
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
    *p_plink_prev = NULL;

    /* all ngon's are parsed, now tessellate */

    pf_arena = BLI_memarena_new(BLI_POLYFILL_ARENA_SIZE, __func__);
    tris = MEM_mallocN(sizeof(*tris) * (size_t)tris_len, __func__);

    orig_index = MEM_mallocN(sizeof(*orig_index) * (size_t)tris_len, __func__);
    orig_normal = MEM_mallocN(sizeof(*orig_normal) * (size_t)polys_len, __func__);

    for (plink = plink_first, poly_index = 0, i = 0; plink; plink = plink->next, poly_index++) {
      if (plink->len == 3) {
        unsigned int *tri = tris[i];
        memcpy(tri, plink->poly, sizeof(unsigned int[3]));
        orig_index[i] = poly_index;
        normal_tri_v3(orig_normal[poly_index], coords[tri[0]], coords[tri[1]], coords[tri[2]]);
        i++;
      }
      else if (plink->len > 3) {
        float(*proj_coords)[2] = BLI_memarena_alloc(pf_arena, sizeof(*proj_coords) * plink->len);
        float *normal = orig_normal[poly_index];
        const float *co_prev;
        const float *co_curr;
        float axis_mat[3][3];
        unsigned int(*tris_offset)[3] = &tris[i];
        unsigned int j;

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
          unsigned int *tri = tris_offset[j];
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

    tree = BLI_bvhtree_new((int)tris_len, epsilon, PY_BVH_TREE_TYPE_DEFAULT, PY_BVH_AXIS_DEFAULT);
    if (tree) {
      for (i = 0; i < tris_len; i++) {
        float co[3][3];

        copy_v3_v3(co[0], coords[tris[i][0]]);
        copy_v3_v3(co[1], coords[tris[i][1]]);
        copy_v3_v3(co[2], coords[tris[i][2]]);

        BLI_bvhtree_insert(tree, (int)i, co[0], 3);
      }

      BLI_bvhtree_balance(tree);
    }

    return bvhtree_CreatePyObject(
        tree, epsilon, coords, coords_len, tris, tris_len, orig_index, orig_normal);
  }
  else {
    if (coords) {
      MEM_freeN(coords);
    }
    if (tris) {
      MEM_freeN(tris);
    }

    return NULL;
  }
}

#ifndef MATH_STANDALONE

PyDoc_STRVAR(C_BVHTree_FromBMesh_doc,
             ".. classmethod:: FromBMesh(bmesh, epsilon=0.0)\n"
             "\n"
             "   BVH tree based on :class:`BMesh` data.\n"
             "\n"
             "   :arg bmesh: BMesh data.\n"
             "   :type bmesh: :class:`BMesh`\n" PYBVH_FROM_GENERIC_EPSILON_DOC);
static PyObject *C_BVHTree_FromBMesh(PyObject *UNUSED(cls), PyObject *args, PyObject *kwargs)
{
  const char *keywords[] = {"bmesh", "epsilon", NULL};

  BPy_BMesh *py_bm;

  float(*coords)[3] = NULL;
  unsigned int(*tris)[3] = NULL;
  unsigned int coords_len, tris_len;
  float epsilon = 0.0f;

  BMesh *bm;
  BMLoop *(*looptris)[3];

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwargs,
                                   (char *)"O!|$f:BVHTree.FromBMesh",
                                   (char **)keywords,
                                   &BPy_BMesh_Type,
                                   &py_bm,
                                   &epsilon)) {
    return NULL;
  }

  bm = py_bm->bm;

  /* Get data for tessellation */
  {
    int tris_len_dummy;

    coords_len = (unsigned int)bm->totvert;
    tris_len = (unsigned int)poly_to_tri_count(bm->totface, bm->totloop);

    coords = MEM_mallocN(sizeof(*coords) * (size_t)coords_len, __func__);
    tris = MEM_mallocN(sizeof(*tris) * (size_t)tris_len, __func__);

    looptris = MEM_mallocN(sizeof(*looptris) * (size_t)tris_len, __func__);

    BM_mesh_calc_tessellation(bm, looptris, &tris_len_dummy);
    BLI_assert(tris_len_dummy == (int)tris_len);
  }

  {
    BMIter iter;
    BVHTree *tree;
    unsigned int i;

    int *orig_index = NULL;
    float(*orig_normal)[3] = NULL;

    tree = BLI_bvhtree_new((int)tris_len, epsilon, PY_BVH_TREE_TYPE_DEFAULT, PY_BVH_AXIS_DEFAULT);
    if (tree) {
      BMFace *f;
      BMVert *v;

      orig_index = MEM_mallocN(sizeof(*orig_index) * (size_t)tris_len, __func__);
      orig_normal = MEM_mallocN(sizeof(*orig_normal) * (size_t)bm->totface, __func__);

      BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
        copy_v3_v3(coords[i], v->co);
        BM_elem_index_set(v, (int)i); /* set_inline */
      }
      BM_ITER_MESH_INDEX (f, &iter, bm, BM_FACES_OF_MESH, i) {
        copy_v3_v3(orig_normal[i], f->no);
        BM_elem_index_set(f, (int)i); /* set_inline */
      }
      bm->elem_index_dirty &= (char)~(BM_VERT | BM_FACE);

      for (i = 0; i < tris_len; i++) {
        float co[3][3];

        tris[i][0] = (unsigned int)BM_elem_index_get(looptris[i][0]->v);
        tris[i][1] = (unsigned int)BM_elem_index_get(looptris[i][1]->v);
        tris[i][2] = (unsigned int)BM_elem_index_get(looptris[i][2]->v);

        copy_v3_v3(co[0], coords[tris[i][0]]);
        copy_v3_v3(co[1], coords[tris[i][1]]);
        copy_v3_v3(co[2], coords[tris[i][2]]);

        BLI_bvhtree_insert(tree, (int)i, co[0], 3);
        orig_index[i] = BM_elem_index_get(looptris[i][0]->f);
      }

      BLI_bvhtree_balance(tree);
    }

    MEM_freeN(looptris);

    return bvhtree_CreatePyObject(
        tree, epsilon, coords, coords_len, tris, tris_len, orig_index, orig_normal);
  }
}

/* return various derived meshes based on requested settings */
static Mesh *bvh_get_mesh(const char *funcname,
                          struct Depsgraph *depsgraph,
                          struct Scene *scene,
                          Object *ob,
                          const bool use_deform,
                          const bool use_cage,
                          bool *r_free_mesh)
{
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
  /* we only need minimum mesh data for topology and vertex locations */
  CustomData_MeshMasks data_masks = CD_MASK_BAREMESH;
  const bool use_render = DEG_get_mode(depsgraph) == DAG_EVAL_RENDER;
  *r_free_mesh = false;

  /* Write the display mesh into the dummy mesh */
  if (use_deform) {
    if (use_render) {
      if (use_cage) {
        PyErr_Format(
            PyExc_ValueError,
            "%s(...): cage arg is unsupported when dependency graph evaluation mode is RENDER",
            funcname);
        return NULL;
      }
      else {
        *r_free_mesh = true;
        return mesh_create_eval_final_render(depsgraph, scene, ob, &data_masks);
      }
    }
    else if (ob_eval != NULL) {
      if (use_cage) {
        return mesh_get_eval_deform(depsgraph, scene, ob_eval, &data_masks);
      }
      else {
        return mesh_get_eval_final(depsgraph, scene, ob_eval, &data_masks);
      }
    }
    else {
      PyErr_Format(PyExc_ValueError,
                   "%s(...): Cannot get evaluated data from given dependency graph / object pair",
                   funcname);
      return NULL;
    }
  }
  else {
    /* !use_deform */
    if (use_render) {
      if (use_cage) {
        PyErr_Format(
            PyExc_ValueError,
            "%s(...): cage arg is unsupported when dependency graph evaluation mode is RENDER",
            funcname);
        return NULL;
      }
      else {
        *r_free_mesh = true;
        return mesh_create_eval_no_deform_render(depsgraph, scene, ob, &data_masks);
      }
    }
    else {
      if (use_cage) {
        PyErr_Format(PyExc_ValueError,
                     "%s(...): cage arg is unsupported when deform=False and dependency graph "
                     "evaluation mode is not RENDER",
                     funcname);
        return NULL;
      }
      else {
        *r_free_mesh = true;
        return mesh_create_eval_no_deform(depsgraph, scene, ob, &data_masks);
      }
    }
  }
}

PyDoc_STRVAR(C_BVHTree_FromObject_doc,
             ".. classmethod:: FromObject(object, depsgraph, deform=True, render=False, "
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
static PyObject *C_BVHTree_FromObject(PyObject *UNUSED(cls), PyObject *args, PyObject *kwargs)
{
  /* note, options here match 'bpy_bmesh_from_object' */
  const char *keywords[] = {"object", "depsgraph", "deform", "cage", "epsilon", NULL};

  PyObject *py_ob, *py_depsgraph;
  Object *ob;
  struct Depsgraph *depsgraph;
  struct Scene *scene;
  Mesh *mesh;
  bool use_deform = true;
  bool use_cage = false;
  bool free_mesh = false;

  const MLoopTri *lt;
  const MLoop *mloop;

  float(*coords)[3] = NULL;
  unsigned int(*tris)[3] = NULL;
  unsigned int coords_len, tris_len;
  float epsilon = 0.0f;

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwargs,
                                   (char *)"OO|$O&O&f:BVHTree.FromObject",
                                   (char **)keywords,
                                   &py_ob,
                                   &py_depsgraph,
                                   PyC_ParseBool,
                                   &use_deform,
                                   PyC_ParseBool,
                                   &use_cage,
                                   &epsilon) ||
      ((ob = PyC_RNA_AsPointer(py_ob, "Object")) == NULL) ||
      ((depsgraph = PyC_RNA_AsPointer(py_depsgraph, "Depsgraph")) == NULL)) {
    return NULL;
  }

  scene = DEG_get_evaluated_scene(depsgraph);
  mesh = bvh_get_mesh("BVHTree", depsgraph, scene, ob, use_deform, use_cage, &free_mesh);

  if (mesh == NULL) {
    return NULL;
  }

  /* Get data for tessellation */
  {
    lt = BKE_mesh_runtime_looptri_ensure(mesh);

    tris_len = (unsigned int)BKE_mesh_runtime_looptri_len(mesh);
    coords_len = (unsigned int)mesh->totvert;

    coords = MEM_mallocN(sizeof(*coords) * (size_t)coords_len, __func__);
    tris = MEM_mallocN(sizeof(*tris) * (size_t)tris_len, __func__);

    MVert *mv = mesh->mvert;
    for (int i = 0; i < mesh->totvert; i++, mv++) {
      copy_v3_v3(coords[i], mv->co);
    }

    mloop = mesh->mloop;
  }

  {
    BVHTree *tree;
    unsigned int i;

    int *orig_index = NULL;
    float(*orig_normal)[3] = NULL;

    tree = BLI_bvhtree_new((int)tris_len, epsilon, PY_BVH_TREE_TYPE_DEFAULT, PY_BVH_AXIS_DEFAULT);
    if (tree) {
      orig_index = MEM_mallocN(sizeof(*orig_index) * (size_t)tris_len, __func__);
      CustomData *pdata = &mesh->pdata;
      orig_normal = CustomData_get_layer(pdata, CD_NORMAL); /* can be NULL */
      if (orig_normal) {
        orig_normal = MEM_dupallocN(orig_normal);
      }

      for (i = 0; i < tris_len; i++, lt++) {
        float co[3][3];

        tris[i][0] = mloop[lt->tri[0]].v;
        tris[i][1] = mloop[lt->tri[1]].v;
        tris[i][2] = mloop[lt->tri[2]].v;

        copy_v3_v3(co[0], coords[tris[i][0]]);
        copy_v3_v3(co[1], coords[tris[i][1]]);
        copy_v3_v3(co[2], coords[tris[i][2]]);

        BLI_bvhtree_insert(tree, (int)i, co[0], 3);
        orig_index[i] = (int)lt->poly;
      }

      BLI_bvhtree_balance(tree);
    }

    if (free_mesh) {
      BKE_id_free(NULL, mesh);
    }

    return bvhtree_CreatePyObject(
        tree, epsilon, coords, coords_len, tris, tris_len, orig_index, orig_normal);
  }
}
#endif /* MATH_STANDALONE */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Module & Type definition
 * \{ */

static PyMethodDef py_bvhtree_methods[] = {
    {"ray_cast", (PyCFunction)py_bvhtree_ray_cast, METH_VARARGS, py_bvhtree_ray_cast_doc},
    {"find_nearest",
     (PyCFunction)py_bvhtree_find_nearest,
     METH_VARARGS,
     py_bvhtree_find_nearest_doc},
    {"find_nearest_range",
     (PyCFunction)py_bvhtree_find_nearest_range,
     METH_VARARGS,
     py_bvhtree_find_nearest_range_doc},
    {"overlap", (PyCFunction)py_bvhtree_overlap, METH_O, py_bvhtree_overlap_doc},

    /* class methods */
    {"FromPolygons",
     (PyCFunction)C_BVHTree_FromPolygons,
     METH_VARARGS | METH_KEYWORDS | METH_CLASS,
     C_BVHTree_FromPolygons_doc},
#ifndef MATH_STANDALONE
    {"FromBMesh",
     (PyCFunction)C_BVHTree_FromBMesh,
     METH_VARARGS | METH_KEYWORDS | METH_CLASS,
     C_BVHTree_FromBMesh_doc},
    {"FromObject",
     (PyCFunction)C_BVHTree_FromObject,
     METH_VARARGS | METH_KEYWORDS | METH_CLASS,
     C_BVHTree_FromObject_doc},
#endif
    {NULL, NULL, 0, NULL},
};

PyTypeObject PyBVHTree_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "BVHTree", /* tp_name */
    sizeof(PyBVHTree),                        /* tp_basicsize */
    0,                                        /* tp_itemsize */
    /* methods */
    (destructor)py_bvhtree__tp_dealloc, /* tp_dealloc */
    NULL,                               /* tp_print */
    NULL,                               /* tp_getattr */
    NULL,                               /* tp_setattr */
    NULL,                               /* tp_compare */
    NULL,                               /* tp_repr */
    NULL,                               /* tp_as_number */
    NULL,                               /* tp_as_sequence */
    NULL,                               /* tp_as_mapping */
    NULL,                               /* tp_hash */
    NULL,                               /* tp_call */
    NULL,                               /* tp_str */
    NULL,                               /* tp_getattro */
    NULL,                               /* tp_setattro */
    NULL,                               /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                 /* tp_flags */
    NULL,                               /* Documentation string */
    NULL,                               /* tp_traverse */
    NULL,                               /* tp_clear */
    NULL,                               /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    NULL,                               /* tp_iter */
    NULL,                               /* tp_iternext */
    py_bvhtree_methods,                 /* tp_methods */
    NULL,                               /* tp_members */
    NULL,                               /* tp_getset */
    NULL,                               /* tp_base */
    NULL,                               /* tp_dict */
    NULL,                               /* tp_descr_get */
    NULL,                               /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    NULL,                               /* tp_init */
    (allocfunc)PyType_GenericAlloc,     /* tp_alloc */
    (newfunc)PyType_GenericNew,         /* tp_new */
    (freefunc)0,                        /* tp_free */
    NULL,                               /* tp_is_gc */
    NULL,                               /* tp_bases */
    NULL,                               /* tp_mro */
    NULL,                               /* tp_cache */
    NULL,                               /* tp_subclasses */
    NULL,                               /* tp_weaklist */
    (destructor)NULL,                   /* tp_del */
};

/* -------------------------------------------------------------------- */
/* Module definition */

PyDoc_STRVAR(py_bvhtree_doc,
             "BVH tree structures for proximity searches and ray casts on geometry.");
static struct PyModuleDef bvhtree_moduledef = {
    PyModuleDef_HEAD_INIT,
    "mathutils.bvhtree", /* m_name */
    py_bvhtree_doc,      /* m_doc */
    0,                   /* m_size */
    NULL,                /* m_methods */
    NULL,                /* m_reload */
    NULL,                /* m_traverse */
    NULL,                /* m_clear */
    NULL,                /* m_free */
};

PyMODINIT_FUNC PyInit_mathutils_bvhtree(void)
{
  PyObject *m = PyModule_Create(&bvhtree_moduledef);

  if (m == NULL) {
    return NULL;
  }

  /* Register classes */
  if (PyType_Ready(&PyBVHTree_Type) < 0) {
    return NULL;
  }

  PyModule_AddObject(m, "BVHTree", (PyObject *)&PyBVHTree_Type);

  return m;
}

/** \} */
