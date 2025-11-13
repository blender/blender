/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pymathutils
 */

#include <Python.h>

#include "mathutils.hh"
#include "mathutils_geometry.hh"

/* Used for PolyFill */
#ifndef MATH_STANDALONE /* define when building outside blender */
#  include "BLI_boxpack_2d.h"
#  include "BLI_convexhull_2d.hh"
#  include "BLI_delaunay_2d.hh"
#  include "BLI_listbase.h"

#  include "BKE_curve.hh"
#  include "BKE_displist.h"

#  include "MEM_guardedalloc.h"
#endif /* !MATH_STANDALONE */

#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "../generic/py_capi_utils.hh"
#include "../generic/python_compat.hh" /* IWYU pragma: keep. */
#include "../generic/python_utildefines.hh"

/* ---------------------------------INTERSECTION FUNCTIONS-------------------- */

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_intersect_ray_tri_doc,
    ".. function:: intersect_ray_tri(v1, v2, v3, ray, orig, clip=True, /)\n"
    "\n"
    "   Returns the intersection between a ray and a triangle, if possible, returns None "
    "otherwise.\n"
    "\n"
    "   :arg v1: Point1\n"
    "   :type v1: :class:`mathutils.Vector`\n"
    "   :arg v2: Point2\n"
    "   :type v2: :class:`mathutils.Vector`\n"
    "   :arg v3: Point3\n"
    "   :type v3: :class:`mathutils.Vector`\n"
    "   :arg ray: Direction of the projection\n"
    "   :type ray: :class:`mathutils.Vector`\n"
    "   :arg orig: Origin\n"
    "   :type orig: :class:`mathutils.Vector`\n"
    "   :arg clip: When False, don't restrict the intersection to the area of the "
    "triangle, use the infinite plane defined by the triangle.\n"
    "   :type clip: bool\n"
    "   :return: The point of intersection or None if no intersection is found\n"
    "   :rtype: :class:`mathutils.Vector` | None\n");
static PyObject *M_Geometry_intersect_ray_tri(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "intersect_ray_tri";
  PyObject *py_ray, *py_ray_off, *py_tri[3];
  float dir[3], orig[3], tri[3][3], e1[3], e2[3], pvec[3], tvec[3], qvec[3];
  float det, inv_det, u, v, t;
  bool clip = true;
  int i;

  if (!PyArg_ParseTuple(args,
                        "OOOOO|O&:intersect_ray_tri",
                        UNPACK3_EX(&, py_tri, ),
                        &py_ray,
                        &py_ray_off,
                        PyC_ParseBool,
                        &clip))
  {
    return nullptr;
  }

  if (((mathutils_array_parse(dir, 2, 3 | MU_ARRAY_SPILL | MU_ARRAY_ZERO, py_ray, error_prefix) !=
        -1) &&
       (mathutils_array_parse(
            orig, 2, 3 | MU_ARRAY_SPILL | MU_ARRAY_ZERO, py_ray_off, error_prefix) != -1)) == 0)
  {
    return nullptr;
  }

  for (i = 0; i < ARRAY_SIZE(tri); i++) {
    if (mathutils_array_parse(
            tri[i], 2, 3 | MU_ARRAY_SPILL | MU_ARRAY_ZERO, py_tri[i], error_prefix) == -1)
    {
      return nullptr;
    }
  }

  normalize_v3(dir);

  /* find vectors for two edges sharing v1 */
  sub_v3_v3v3(e1, tri[1], tri[0]);
  sub_v3_v3v3(e2, tri[2], tri[0]);

  /* begin calculating determinant - also used to calculated U parameter */
  cross_v3_v3v3(pvec, dir, e2);

  /* if determinant is near zero, ray lies in plane of triangle */
  det = dot_v3v3(e1, pvec);

  if (det > -0.000001f && det < 0.000001f) {
    Py_RETURN_NONE;
  }

  inv_det = 1.0f / det;

  /* calculate distance from v1 to ray origin */
  sub_v3_v3v3(tvec, orig, tri[0]);

  /* calculate U parameter and test bounds */
  u = dot_v3v3(tvec, pvec) * inv_det;
  if (clip && (u < 0.0f || u > 1.0f)) {
    Py_RETURN_NONE;
  }

  /* prepare to test the V parameter */
  cross_v3_v3v3(qvec, tvec, e1);

  /* calculate V parameter and test bounds */
  v = dot_v3v3(dir, qvec) * inv_det;

  if (clip && (v < 0.0f || u + v > 1.0f)) {
    Py_RETURN_NONE;
  }

  /* calculate t, ray intersects triangle */
  t = dot_v3v3(e2, qvec) * inv_det;

  /* ray hit behind */
  if (t < 0.0f) {
    Py_RETURN_NONE;
  }

  mul_v3_fl(dir, t);
  add_v3_v3v3(pvec, orig, dir);

  return Vector_CreatePyObject(pvec, 3, nullptr);
}

/* Line-Line intersection using algorithm from mathworld.wolfram.com */

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_intersect_line_line_doc,
    ".. function:: intersect_line_line(v1, v2, v3, v4, /)\n"
    "\n"
    "   Returns a tuple with the points on each line respectively closest to the other.\n"
    "\n"
    "   :arg v1: First point of the first line\n"
    "   :type v1: :class:`mathutils.Vector`\n"
    "   :arg v2: Second point of the first line\n"
    "   :type v2: :class:`mathutils.Vector`\n"
    "   :arg v3: First point of the second line\n"
    "   :type v3: :class:`mathutils.Vector`\n"
    "   :arg v4: Second point of the second line\n"
    "   :type v4: :class:`mathutils.Vector`\n"
    "   :return: The intersection on each line or None when the lines are co-linear.\n"
    "   :rtype: tuple[:class:`mathutils.Vector`, :class:`mathutils.Vector`] | None\n");
static PyObject *M_Geometry_intersect_line_line(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "intersect_line_line";
  PyObject *tuple;
  PyObject *py_lines[4];
  float lines[4][3], i1[3], i2[3];
  int ix_vec_num;
  int result;

  if (!PyArg_ParseTuple(args, "OOOO:intersect_line_line", UNPACK4_EX(&, py_lines, ))) {
    return nullptr;
  }

  if ((((ix_vec_num = mathutils_array_parse(
             lines[0], 2, 3 | MU_ARRAY_SPILL | MU_ARRAY_ZERO, py_lines[0], error_prefix)) != -1) &&
       (mathutils_array_parse(lines[1],
                              ix_vec_num,
                              ix_vec_num | MU_ARRAY_SPILL | MU_ARRAY_ZERO,
                              py_lines[1],
                              error_prefix) != -1) &&
       (mathutils_array_parse(lines[2],
                              ix_vec_num,
                              ix_vec_num | MU_ARRAY_SPILL | MU_ARRAY_ZERO,
                              py_lines[2],
                              error_prefix) != -1) &&
       (mathutils_array_parse(lines[3],
                              ix_vec_num,
                              ix_vec_num | MU_ARRAY_SPILL | MU_ARRAY_ZERO,
                              py_lines[3],
                              error_prefix) != -1)) == 0)
  {
    return nullptr;
  }

  /* Zero 3rd axis of 2D vectors. */
  if (ix_vec_num == 2) {
    lines[1][2] = 0.0f;
    lines[2][2] = 0.0f;
    lines[3][2] = 0.0f;
  }

  result = isect_line_line_v3(UNPACK4(lines), i1, i2);
  /* The return-code isn't exposed,
   * this way we can check know how close the lines are. */
  if (result == 1) {
    closest_to_line_v3(i2, i1, lines[2], lines[3]);
  }

  if (result == 0) {
    /* Collinear. */
    Py_RETURN_NONE;
  }

  tuple = PyTuple_New(2);
  PyTuple_SET_ITEMS(tuple,
                    Vector_CreatePyObject(i1, ix_vec_num, nullptr),
                    Vector_CreatePyObject(i2, ix_vec_num, nullptr));
  return tuple;
}

/* Line-Line intersection using algorithm from mathworld.wolfram.com */

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_intersect_sphere_sphere_2d_doc,
    ".. function:: intersect_sphere_sphere_2d(p_a, radius_a, p_b, radius_b, /)\n"
    "\n"
    "   Returns 2 points on between intersecting circles.\n"
    "\n"
    "   :arg p_a: Center of the first circle\n"
    "   :type p_a: :class:`mathutils.Vector`\n"
    "   :arg radius_a: Radius of the first circle\n"
    "   :type radius_a: float\n"
    "   :arg p_b: Center of the second circle\n"
    "   :type p_b: :class:`mathutils.Vector`\n"
    "   :arg radius_b: Radius of the second circle\n"
    "   :type radius_b: float\n"
    "   :return: 2 points on between intersecting circles or None when there is no intersection.\n"
    "   :rtype: tuple[:class:`mathutils.Vector`, :class:`mathutils.Vector`] | "
    "tuple[None, None]\n");
static PyObject *M_Geometry_intersect_sphere_sphere_2d(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "intersect_sphere_sphere_2d";
  PyObject *ret;
  PyObject *py_v_a, *py_v_b;
  float v_a[2], v_b[2];
  float rad_a, rad_b;
  float v_ab[2];
  float dist;

  if (!PyArg_ParseTuple(args, "OfOf:intersect_sphere_sphere_2d", &py_v_a, &rad_a, &py_v_b, &rad_b))
  {
    return nullptr;
  }

  if (((mathutils_array_parse(v_a, 2, 2, py_v_a, error_prefix) != -1) &&
       (mathutils_array_parse(v_b, 2, 2, py_v_b, error_prefix) != -1)) == 0)
  {
    return nullptr;
  }

  ret = PyTuple_New(2);

  sub_v2_v2v2(v_ab, v_b, v_a);
  dist = len_v2(v_ab);

  if (/* out of range */
      (dist > rad_a + rad_b) ||
      /* fully-contained in the other */
      (dist < fabsf(rad_a - rad_b)) ||
      /* co-incident */
      (dist < FLT_EPSILON))
  {
    /* out of range */
    PyTuple_SET_ITEMS(ret, Py_NewRef(Py_None), Py_NewRef(Py_None));
  }
  else {
    const float dist_delta = ((rad_a * rad_a) - (rad_b * rad_b) + (dist * dist)) / (2.0f * dist);
    const float h = powf(fabsf((rad_a * rad_a) - (dist_delta * dist_delta)), 0.5f);
    float i_cent[2];
    float i1[2], i2[2];

    i_cent[0] = v_a[0] + ((v_ab[0] * dist_delta) / dist);
    i_cent[1] = v_a[1] + ((v_ab[1] * dist_delta) / dist);

    i1[0] = i_cent[0] + h * v_ab[1] / dist;
    i1[1] = i_cent[1] - h * v_ab[0] / dist;

    i2[0] = i_cent[0] - h * v_ab[1] / dist;
    i2[1] = i_cent[1] + h * v_ab[0] / dist;

    PyTuple_SET_ITEMS(
        ret, Vector_CreatePyObject(i1, 2, nullptr), Vector_CreatePyObject(i2, 2, nullptr));
  }

  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_intersect_tri_tri_2d_doc,
    ".. function:: intersect_tri_tri_2d(tri_a1, tri_a2, tri_a3, tri_b1, tri_b2, tri_b3, /)\n"
    "\n"
    "   Check if two 2D triangles intersect.\n"
    "\n"
    "   :rtype: bool\n");
static PyObject *M_Geometry_intersect_tri_tri_2d(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "intersect_tri_tri_2d";
  PyObject *tri_pair_py[2][3];
  float tri_pair[2][3][2];

  if (!PyArg_ParseTuple(args,
                        "OOOOOO:intersect_tri_tri_2d",
                        &tri_pair_py[0][0],
                        &tri_pair_py[0][1],
                        &tri_pair_py[0][2],
                        &tri_pair_py[1][0],
                        &tri_pair_py[1][1],
                        &tri_pair_py[1][2]))
  {
    return nullptr;
  }

  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 3; j++) {
      if (mathutils_array_parse(
              tri_pair[i][j], 2, 2 | MU_ARRAY_SPILL, tri_pair_py[i][j], error_prefix) == -1)
      {
        return nullptr;
      }
    }
  }

  const bool ret = isect_tri_tri_v2(UNPACK3(tri_pair[0]), UNPACK3(tri_pair[1]));
  return PyBool_FromLong(ret);
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_normal_doc,
    ".. function:: normal(*vectors)\n"
    "\n"
    "   Returns the normal of a 3D polygon.\n"
    "\n"
    "   :arg vectors: 3 or more vectors to calculate normals.\n"
    "   :type vectors: Sequence[Sequence[float]]\n"
    "   :rtype: :class:`mathutils.Vector`\n");
static PyObject *M_Geometry_normal(PyObject * /*self*/, PyObject *args)
{
  float (*coords)[3];
  int coords_len;
  float n[3];
  PyObject *ret = nullptr;

  /* use */
  if (PyTuple_GET_SIZE(args) == 1) {
    args = PyTuple_GET_ITEM(args, 0);
  }

  if ((coords_len = mathutils_array_parse_alloc_v(
           (float **)&coords, 3 | MU_ARRAY_SPILL, args, "normal")) == -1)
  {
    return nullptr;
  }

  if (coords_len < 3) {
    PyErr_SetString(PyExc_ValueError, "Expected 3 or more vectors");
    goto finally;
  }

  normal_poly_v3(n, coords, coords_len);
  ret = Vector_CreatePyObject(n, 3, nullptr);

finally:
  PyMem_Free(coords);
  return ret;
}

/* --------------------------------- AREA FUNCTIONS-------------------- */

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_area_tri_doc,
    ".. function:: area_tri(v1, v2, v3, /)\n"
    "\n"
    "   Returns the area size of the 2D or 3D triangle defined.\n"
    "\n"
    "   :arg v1: Point1\n"
    "   :type v1: :class:`mathutils.Vector`\n"
    "   :arg v2: Point2\n"
    "   :type v2: :class:`mathutils.Vector`\n"
    "   :arg v3: Point3\n"
    "   :type v3: :class:`mathutils.Vector`\n"
    "   :rtype: float\n");
static PyObject *M_Geometry_area_tri(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "area_tri";
  PyObject *py_tri[3];
  float tri[3][3];
  int len;

  if (!PyArg_ParseTuple(args, "OOO:area_tri", UNPACK3_EX(&, py_tri, ))) {
    return nullptr;
  }

  if ((((len = mathutils_array_parse(tri[0], 2, 3, py_tri[0], error_prefix)) != -1) &&
       (mathutils_array_parse(tri[1], len, len, py_tri[1], error_prefix) != -1) &&
       (mathutils_array_parse(tri[2], len, len, py_tri[2], error_prefix) != -1)) == 0)
  {
    return nullptr;
  }

  return PyFloat_FromDouble((len == 3 ? area_tri_v3 : area_tri_v2)(UNPACK3(tri)));
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_volume_tetrahedron_doc,
    ".. function:: volume_tetrahedron(v1, v2, v3, v4, /)\n"
    "\n"
    "   Return the volume formed by a tetrahedron (points can be in any order).\n"
    "\n"
    "   :arg v1: Point1\n"
    "   :type v1: :class:`mathutils.Vector`\n"
    "   :arg v2: Point2\n"
    "   :type v2: :class:`mathutils.Vector`\n"
    "   :arg v3: Point3\n"
    "   :type v3: :class:`mathutils.Vector`\n"
    "   :arg v4: Point4\n"
    "   :type v4: :class:`mathutils.Vector`\n"
    "   :rtype: float\n");
static PyObject *M_Geometry_volume_tetrahedron(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "volume_tetrahedron";
  PyObject *py_tet[4];
  float tet[4][3];
  int i;

  if (!PyArg_ParseTuple(args, "OOOO:volume_tetrahedron", UNPACK4_EX(&, py_tet, ))) {
    return nullptr;
  }

  for (i = 0; i < ARRAY_SIZE(tet); i++) {
    if (mathutils_array_parse(tet[i], 3, 3 | MU_ARRAY_SPILL, py_tet[i], error_prefix) == -1) {
      return nullptr;
    }
  }

  return PyFloat_FromDouble(volume_tetrahedron_v3(UNPACK4(tet)));
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_intersect_line_line_2d_doc,
    ".. function:: intersect_line_line_2d(lineA_p1, lineA_p2, lineB_p1, lineB_p2, /)\n"
    "\n"
    "   Takes 2 segments (defined by 4 vectors) and returns a vector for their point of "
    "intersection or None.\n"
    "\n"
    "   .. warning:: Despite its name, this function works on segments, and not on lines.\n"
    "\n"
    "   :arg lineA_p1: First point of the first line\n"
    "   :type lineA_p1: :class:`mathutils.Vector`\n"
    "   :arg lineA_p2: Second point of the first line\n"
    "   :type lineA_p2: :class:`mathutils.Vector`\n"
    "   :arg lineB_p1: First point of the second line\n"
    "   :type lineB_p1: :class:`mathutils.Vector`\n"
    "   :arg lineB_p2: Second point of the second line\n"
    "   :type lineB_p2: :class:`mathutils.Vector`\n"
    "   :return: The point of intersection or None when not found\n"
    "   :rtype: :class:`mathutils.Vector` | None\n");
static PyObject *M_Geometry_intersect_line_line_2d(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "intersect_line_line_2d";
  PyObject *py_lines[4];
  float lines[4][2];
  float vi[2];
  int i;

  if (!PyArg_ParseTuple(args, "OOOO:intersect_line_line_2d", UNPACK4_EX(&, py_lines, ))) {
    return nullptr;
  }

  for (i = 0; i < ARRAY_SIZE(lines); i++) {
    if (mathutils_array_parse(lines[i], 2, 2 | MU_ARRAY_SPILL, py_lines[i], error_prefix) == -1) {
      return nullptr;
    }
  }

  if (isect_seg_seg_v2_point(UNPACK4(lines), vi) == 1) {
    return Vector_CreatePyObject(vi, 2, nullptr);
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_intersect_line_plane_doc,
    ".. function:: intersect_line_plane(line_a, line_b, plane_co, plane_no, no_flip=False, /)\n"
    "\n"
    "   Calculate the intersection between a line (as 2 vectors) and a plane.\n"
    "   Returns a vector for the intersection or None.\n"
    "\n"
    "   :arg line_a: First point of the first line\n"
    "   :type line_a: :class:`mathutils.Vector`\n"
    "   :arg line_b: Second point of the first line\n"
    "   :type line_b: :class:`mathutils.Vector`\n"
    "   :arg plane_co: A point on the plane\n"
    "   :type plane_co: :class:`mathutils.Vector`\n"
    "   :arg plane_no: The direction the plane is facing\n"
    "   :type plane_no: :class:`mathutils.Vector`\n"
    "   :arg no_flip: Not implemented\n"
    "   :type no_flip: bool\n"
    "   :return: The point of intersection or None when not found\n"
    "   :rtype: :class:`mathutils.Vector` | None\n");
static PyObject *M_Geometry_intersect_line_plane(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "intersect_line_plane";
  PyObject *py_line_a, *py_line_b, *py_plane_co, *py_plane_no;
  float line_a[3], line_b[3], plane_co[3], plane_no[3];
  float isect[3];
  const bool no_flip = false;

  if (!PyArg_ParseTuple(args,
                        "OOOO|O&:intersect_line_plane",
                        &py_line_a,
                        &py_line_b,
                        &py_plane_co,
                        &py_plane_no,
                        PyC_ParseBool,
                        &no_flip))
  {
    return nullptr;
  }

  if (((mathutils_array_parse(line_a, 3, 3 | MU_ARRAY_SPILL, py_line_a, error_prefix) != -1) &&
       (mathutils_array_parse(line_b, 3, 3 | MU_ARRAY_SPILL, py_line_b, error_prefix) != -1) &&
       (mathutils_array_parse(plane_co, 3, 3 | MU_ARRAY_SPILL, py_plane_co, error_prefix) != -1) &&
       (mathutils_array_parse(plane_no, 3, 3 | MU_ARRAY_SPILL, py_plane_no, error_prefix) !=
        -1)) == 0)
  {
    return nullptr;
  }

  /* TODO: implements no_flip */
  if (isect_line_plane_v3(isect, line_a, line_b, plane_co, plane_no) == 1) {
    return Vector_CreatePyObject(isect, 3, nullptr);
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_intersect_plane_plane_doc,
    ".. function:: intersect_plane_plane(plane_a_co, plane_a_no, plane_b_co, plane_b_no, /)\n"
    "\n"
    "   Return the intersection between two planes\n"
    "\n"
    "   :arg plane_a_co: Point on the first plane\n"
    "   :type plane_a_co: :class:`mathutils.Vector`\n"
    "   :arg plane_a_no: Normal of the first plane\n"
    "   :type plane_a_no: :class:`mathutils.Vector`\n"
    "   :arg plane_b_co: Point on the second plane\n"
    "   :type plane_b_co: :class:`mathutils.Vector`\n"
    "   :arg plane_b_no: Normal of the second plane\n"
    "   :type plane_b_no: :class:`mathutils.Vector`\n"
    "   :return: The line of the intersection represented as a point and a vector or None if the "
    "intersection can't be calculated\n"
    "   :rtype: tuple[:class:`mathutils.Vector`, :class:`mathutils.Vector`] | "
    "tuple[None, None]\n");
static PyObject *M_Geometry_intersect_plane_plane(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "intersect_plane_plane";
  PyObject *ret, *ret_co, *ret_no;
  PyObject *py_plane_a_co, *py_plane_a_no, *py_plane_b_co, *py_plane_b_no;
  float plane_a_co[3], plane_a_no[3], plane_b_co[3], plane_b_no[3];
  float plane_a[4], plane_b[4];

  float isect_co[3];
  float isect_no[3];

  if (!PyArg_ParseTuple(args,
                        "OOOO:intersect_plane_plane",
                        &py_plane_a_co,
                        &py_plane_a_no,
                        &py_plane_b_co,
                        &py_plane_b_no))
  {
    return nullptr;
  }

  if (((mathutils_array_parse(plane_a_co, 3, 3 | MU_ARRAY_SPILL, py_plane_a_co, error_prefix) !=
        -1) &&
       (mathutils_array_parse(plane_a_no, 3, 3 | MU_ARRAY_SPILL, py_plane_a_no, error_prefix) !=
        -1) &&
       (mathutils_array_parse(plane_b_co, 3, 3 | MU_ARRAY_SPILL, py_plane_b_co, error_prefix) !=
        -1) &&
       (mathutils_array_parse(plane_b_no, 3, 3 | MU_ARRAY_SPILL, py_plane_b_no, error_prefix) !=
        -1)) == 0)
  {
    return nullptr;
  }

  plane_from_point_normal_v3(plane_a, plane_a_co, plane_a_no);
  plane_from_point_normal_v3(plane_b, plane_b_co, plane_b_no);

  if (isect_plane_plane_v3(plane_a, plane_b, isect_co, isect_no)) {
    normalize_v3(isect_no);

    ret_co = Vector_CreatePyObject(isect_co, 3, nullptr);
    ret_no = Vector_CreatePyObject(isect_no, 3, nullptr);
  }
  else {
    ret_co = Py_NewRef(Py_None);
    ret_no = Py_NewRef(Py_None);
  }

  ret = PyTuple_New(2);
  PyTuple_SET_ITEMS(ret, ret_co, ret_no);
  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_intersect_line_sphere_doc,
    ".. function:: intersect_line_sphere(line_a, line_b, sphere_co, sphere_radius, clip=True, /)\n"
    "\n"
    "   Takes a line (as 2 points) and a sphere (as a point and a radius) and\n"
    "   returns the intersection\n"
    "\n"
    "   :arg line_a: First point of the line\n"
    "   :type line_a: :class:`mathutils.Vector`\n"
    "   :arg line_b: Second point of the line\n"
    "   :type line_b: :class:`mathutils.Vector`\n"
    "   :arg sphere_co: The center of the sphere\n"
    "   :type sphere_co: :class:`mathutils.Vector`\n"
    "   :arg sphere_radius: Radius of the sphere\n"
    "   :type sphere_radius: float\n"
    "   :arg clip: When False, don't restrict the intersection to the area of the "
    "sphere.\n"
    "   :type clip: bool\n"
    "   :return: The intersection points as a pair of vectors or None when there is no "
    "intersection\n"
    "   :rtype: tuple[:class:`mathutils.Vector` | None, :class:`mathutils.Vector` | None]\n");
static PyObject *M_Geometry_intersect_line_sphere(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "intersect_line_sphere";
  PyObject *py_line_a, *py_line_b, *py_sphere_co;
  float line_a[3], line_b[3], sphere_co[3];
  float sphere_radius;
  bool clip = true;

  float isect_a[3];
  float isect_b[3];

  if (!PyArg_ParseTuple(args,
                        "OOOf|O&:intersect_line_sphere",
                        &py_line_a,
                        &py_line_b,
                        &py_sphere_co,
                        &sphere_radius,
                        PyC_ParseBool,
                        &clip))
  {
    return nullptr;
  }

  if (((mathutils_array_parse(line_a, 3, 3 | MU_ARRAY_SPILL, py_line_a, error_prefix) != -1) &&
       (mathutils_array_parse(line_b, 3, 3 | MU_ARRAY_SPILL, py_line_b, error_prefix) != -1) &&
       (mathutils_array_parse(sphere_co, 3, 3 | MU_ARRAY_SPILL, py_sphere_co, error_prefix) !=
        -1)) == 0)
  {
    return nullptr;
  }

  bool use_a = true;
  bool use_b = true;
  float lambda;

  PyObject *ret = PyTuple_New(2);

  switch (isect_line_sphere_v3(line_a, line_b, sphere_co, sphere_radius, isect_a, isect_b)) {
    case 1:
      if (!(!clip || (((lambda = line_point_factor_v3(isect_a, line_a, line_b)) >= 0.0f) &&
                      (lambda <= 1.0f))))
      {
        use_a = false;
      }
      use_b = false;
      break;
    case 2:
      if (!(!clip || (((lambda = line_point_factor_v3(isect_a, line_a, line_b)) >= 0.0f) &&
                      (lambda <= 1.0f))))
      {
        use_a = false;
      }
      if (!(!clip || (((lambda = line_point_factor_v3(isect_b, line_a, line_b)) >= 0.0f) &&
                      (lambda <= 1.0f))))
      {
        use_b = false;
      }
      break;
    default:
      use_a = false;
      use_b = false;
      break;
  }

  PyTuple_SET_ITEMS(ret,
                    use_a ? Vector_CreatePyObject(isect_a, 3, nullptr) : Py_NewRef(Py_None),
                    use_b ? Vector_CreatePyObject(isect_b, 3, nullptr) : Py_NewRef(Py_None));

  return ret;
}

/* keep in sync with M_Geometry_intersect_line_sphere */
PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_intersect_line_sphere_2d_doc,
    ".. function:: intersect_line_sphere_2d(line_a, line_b, sphere_co, "
    "sphere_radius, clip=True, /)\n"
    "\n"
    "   Takes a line (as 2 points) and a sphere (as a point and a radius) and\n"
    "   returns the intersection\n"
    "\n"
    "   :arg line_a: First point of the line\n"
    "   :type line_a: :class:`mathutils.Vector`\n"
    "   :arg line_b: Second point of the line\n"
    "   :type line_b: :class:`mathutils.Vector`\n"
    "   :arg sphere_co: The center of the sphere\n"
    "   :type sphere_co: :class:`mathutils.Vector`\n"
    "   :arg sphere_radius: Radius of the sphere\n"
    "   :type sphere_radius: float\n"
    "   :arg clip: When False, don't restrict the intersection to the area of the "
    "sphere.\n"
    "   :type clip: bool\n"
    "   :return: The intersection points as a pair of vectors or None when there is no "
    "intersection\n"
    "   :rtype: tuple[:class:`mathutils.Vector` | None, :class:`mathutils.Vector` | None]\n");
static PyObject *M_Geometry_intersect_line_sphere_2d(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "intersect_line_sphere_2d";
  PyObject *py_line_a, *py_line_b, *py_sphere_co;
  float line_a[2], line_b[2], sphere_co[2];
  float sphere_radius;
  bool clip = true;

  float isect_a[2];
  float isect_b[2];

  if (!PyArg_ParseTuple(args,
                        "OOOf|O&:intersect_line_sphere_2d",
                        &py_line_a,
                        &py_line_b,
                        &py_sphere_co,
                        &sphere_radius,
                        PyC_ParseBool,
                        &clip))
  {
    return nullptr;
  }

  if (((mathutils_array_parse(line_a, 2, 2 | MU_ARRAY_SPILL, py_line_a, error_prefix) != -1) &&
       (mathutils_array_parse(line_b, 2, 2 | MU_ARRAY_SPILL, py_line_b, error_prefix) != -1) &&
       (mathutils_array_parse(sphere_co, 2, 2 | MU_ARRAY_SPILL, py_sphere_co, error_prefix) !=
        -1)) == 0)
  {
    return nullptr;
  }

  bool use_a = true;
  bool use_b = true;
  float lambda;

  PyObject *ret = PyTuple_New(2);

  switch (isect_line_sphere_v2(line_a, line_b, sphere_co, sphere_radius, isect_a, isect_b)) {
    case 1:
      if (!(!clip || (((lambda = line_point_factor_v2(isect_a, line_a, line_b)) >= 0.0f) &&
                      (lambda <= 1.0f))))
      {
        use_a = false;
      }
      use_b = false;
      break;
    case 2:
      if (!(!clip || (((lambda = line_point_factor_v2(isect_a, line_a, line_b)) >= 0.0f) &&
                      (lambda <= 1.0f))))
      {
        use_a = false;
      }
      if (!(!clip || (((lambda = line_point_factor_v2(isect_b, line_a, line_b)) >= 0.0f) &&
                      (lambda <= 1.0f))))
      {
        use_b = false;
      }
      break;
    default:
      use_a = false;
      use_b = false;
      break;
  }

  PyTuple_SET_ITEMS(ret,
                    use_a ? Vector_CreatePyObject(isect_a, 2, nullptr) : Py_NewRef(Py_None),
                    use_b ? Vector_CreatePyObject(isect_b, 2, nullptr) : Py_NewRef(Py_None));

  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_intersect_point_line_doc,
    ".. function:: intersect_point_line(pt, line_p1, line_p2, /)\n"
    "\n"
    "   Takes a point and a line and returns the closest point on the line and its "
    "distance from the first point of the line as a percentage of the length of the line.\n"
    "\n"
    "   :arg pt: Point\n"
    "   :type pt: :class:`mathutils.Vector`\n"
    "   :arg line_p1: First point of the line\n"
    "   :type line_p1: :class:`mathutils.Vector`\n"
    "   :arg line_p2: Second point of the line\n"
    "   :type line_p2: :class:`mathutils.Vector`\n"
    "   :rtype: tuple[:class:`mathutils.Vector`, float]\n");
static PyObject *M_Geometry_intersect_point_line(PyObject * /*self*/,
                                                 PyObject *const *args,
                                                 Py_ssize_t nargs)
{
  const char *error_prefix = "intersect_point_line";
  float pt[3], pt_out[3], line_a[3], line_b[3];
  int pt_num = 2;

  if (!_PyArg_CheckPositional(error_prefix, nargs, 3, 3)) {
    return nullptr;
  }

  PyObject *py_pt = args[0];
  PyObject *py_line_a = args[1];
  PyObject *py_line_b = args[2];

  /* Accept 2D verts. */
  if ((((pt_num = mathutils_array_parse(
             pt, 2, 3 | MU_ARRAY_SPILL | MU_ARRAY_ZERO, py_pt, error_prefix)) != -1) &&
       (mathutils_array_parse(
            line_a, 2, 3 | MU_ARRAY_SPILL | MU_ARRAY_ZERO, py_line_a, error_prefix) != -1) &&
       (mathutils_array_parse(
            line_b, 2, 3 | MU_ARRAY_SPILL | MU_ARRAY_ZERO, py_line_b, error_prefix) != -1)) == 0)
  {
    return nullptr;
  }

  /* Do the calculation. */
  const float lambda = closest_to_line_v3(pt_out, pt, line_a, line_b);

  PyObject *ret = PyTuple_New(2);
  PyTuple_SET_ITEMS(
      ret, Vector_CreatePyObject(pt_out, pt_num, nullptr), PyFloat_FromDouble(lambda));
  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_intersect_point_line_segment_doc,
    ".. function:: intersect_point_line_segment(pt, seg_p1, seg_p2, /)\n"
    "\n"
    "   Takes a point and a segment and returns the closest point on the segment "
    "and the distance to the segment.\n"
    "\n"
    "   :arg pt: Point\n"
    "   :type pt: :class:`mathutils.Vector`\n"
    "   :arg seg_p1: First point of the segment\n"
    "   :type seg_p1: :class:`mathutils.Vector`\n"
    "   :arg seg_p2: Second point of the segment\n"
    "   :type seg_p2: :class:`mathutils.Vector`\n"
    "   :rtype: tuple[:class:`mathutils.Vector`, float]\n");
static PyObject *M_Geometry_intersect_point_line_segment(PyObject * /*self*/,
                                                         PyObject *const *args,
                                                         Py_ssize_t nargs)
{
  const char *error_prefix = "intersect_point_line_segment";
  float pt[3], pt_out[3], seg_a[3], seg_b[3];
  int pt_num = 2;

  if (!_PyArg_CheckPositional(error_prefix, nargs, 3, 3)) {
    return nullptr;
  }

  PyObject *py_pt = args[0];
  PyObject *py_seq_a = args[1];
  PyObject *py_seg_b = args[2];

  /* Accept 2D verts. */
  if ((((pt_num = mathutils_array_parse(
             pt, 2, 3 | MU_ARRAY_SPILL | MU_ARRAY_ZERO, py_pt, error_prefix)) != -1) &&
       (mathutils_array_parse(
            seg_a, 2, 3 | MU_ARRAY_SPILL | MU_ARRAY_ZERO, py_seq_a, error_prefix) != -1) &&
       (mathutils_array_parse(
            seg_b, 2, 3 | MU_ARRAY_SPILL | MU_ARRAY_ZERO, py_seg_b, error_prefix) != -1)) == 0)
  {
    return nullptr;
  }

  /* Do the calculation. */
  closest_to_line_segment_v3(pt_out, pt, seg_a, seg_b);
  const float lambda = len_v3v3(pt_out, pt);

  PyObject *ret = PyTuple_New(2);
  PyTuple_SET_ITEMS(
      ret, Vector_CreatePyObject(pt_out, pt_num, nullptr), PyFloat_FromDouble(lambda));
  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_intersect_point_tri_doc,
    ".. function:: intersect_point_tri(pt, tri_p1, tri_p2, tri_p3, /)\n"
    "\n"
    "   Takes 4 vectors: one is the point and the next 3 define the triangle. Projects "
    "the point onto the triangle plane and checks if it is within the triangle.\n"
    "\n"
    "   :arg pt: Point\n"
    "   :type pt: :class:`mathutils.Vector`\n"
    "   :arg tri_p1: First point of the triangle\n"
    "   :type tri_p1: :class:`mathutils.Vector`\n"
    "   :arg tri_p2: Second point of the triangle\n"
    "   :type tri_p2: :class:`mathutils.Vector`\n"
    "   :arg tri_p3: Third point of the triangle\n"
    "   :type tri_p3: :class:`mathutils.Vector`\n"
    "   :return: Point on the triangles plane or None if its outside the triangle\n"
    "   :rtype: :class:`mathutils.Vector` | None\n");
static PyObject *M_Geometry_intersect_point_tri(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "intersect_point_tri";
  PyObject *py_pt, *py_tri[3];
  float pt[3], tri[3][3];
  float vi[3];
  int i;

  if (!PyArg_ParseTuple(args, "OOOO:intersect_point_tri", &py_pt, UNPACK3_EX(&, py_tri, ))) {
    return nullptr;
  }

  if (mathutils_array_parse(pt, 2, 3 | MU_ARRAY_SPILL | MU_ARRAY_ZERO, py_pt, error_prefix) == -1)
  {
    return nullptr;
  }
  for (i = 0; i < ARRAY_SIZE(tri); i++) {
    if (mathutils_array_parse(
            tri[i], 2, 3 | MU_ARRAY_SPILL | MU_ARRAY_ZERO, py_tri[i], error_prefix) == -1)
    {
      return nullptr;
    }
  }

  if (isect_point_tri_v3(pt, UNPACK3(tri), vi)) {
    return Vector_CreatePyObject(vi, 3, nullptr);
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_closest_point_on_tri_doc,
    ".. function:: closest_point_on_tri(pt, tri_p1, tri_p2, tri_p3, /)\n"
    "\n"
    "   Takes 4 vectors: one is the point and the next 3 define the triangle.\n"
    "\n"
    "   :arg pt: Point\n"
    "   :type pt: :class:`mathutils.Vector`\n"
    "   :arg tri_p1: First point of the triangle\n"
    "   :type tri_p1: :class:`mathutils.Vector`\n"
    "   :arg tri_p2: Second point of the triangle\n"
    "   :type tri_p2: :class:`mathutils.Vector`\n"
    "   :arg tri_p3: Third point of the triangle\n"
    "   :type tri_p3: :class:`mathutils.Vector`\n"
    "   :return: The closest point of the triangle.\n"
    "   :rtype: :class:`mathutils.Vector`\n");
static PyObject *M_Geometry_closest_point_on_tri(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "closest_point_on_tri";
  PyObject *py_pt, *py_tri[3];
  float pt[3], tri[3][3];
  float vi[3];
  int i;

  if (!PyArg_ParseTuple(args, "OOOO:closest_point_on_tri", &py_pt, UNPACK3_EX(&, py_tri, ))) {
    return nullptr;
  }

  if (mathutils_array_parse(pt, 2, 3 | MU_ARRAY_SPILL | MU_ARRAY_ZERO, py_pt, error_prefix) == -1)
  {
    return nullptr;
  }
  for (i = 0; i < ARRAY_SIZE(tri); i++) {
    if (mathutils_array_parse(
            tri[i], 2, 3 | MU_ARRAY_SPILL | MU_ARRAY_ZERO, py_tri[i], error_prefix) == -1)
    {
      return nullptr;
    }
  }

  closest_on_tri_to_point_v3(vi, pt, UNPACK3(tri));

  return Vector_CreatePyObject(vi, 3, nullptr);
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_intersect_point_tri_2d_doc,
    ".. function:: intersect_point_tri_2d(pt, tri_p1, tri_p2, tri_p3, /)\n"
    "\n"
    "   Takes 4 vectors (using only the x and y coordinates): one is the point and the next 3 "
    "define the triangle. Returns 1 if the point is within the triangle, otherwise 0.\n"
    "\n"
    "   :arg pt: Point\n"
    "   :type pt: :class:`mathutils.Vector`\n"
    "   :arg tri_p1: First point of the triangle\n"
    "   :type tri_p1: :class:`mathutils.Vector`\n"
    "   :arg tri_p2: Second point of the triangle\n"
    "   :type tri_p2: :class:`mathutils.Vector`\n"
    "   :arg tri_p3: Third point of the triangle\n"
    "   :type tri_p3: :class:`mathutils.Vector`\n"
    "   :rtype: int\n");
static PyObject *M_Geometry_intersect_point_tri_2d(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "intersect_point_tri_2d";
  PyObject *py_pt, *py_tri[3];
  float pt[2], tri[3][2];
  int i;

  if (!PyArg_ParseTuple(args, "OOOO:intersect_point_tri_2d", &py_pt, UNPACK3_EX(&, py_tri, ))) {
    return nullptr;
  }

  if (mathutils_array_parse(pt, 2, 2 | MU_ARRAY_SPILL, py_pt, error_prefix) == -1) {
    return nullptr;
  }
  for (i = 0; i < ARRAY_SIZE(tri); i++) {
    if (mathutils_array_parse(tri[i], 2, 2 | MU_ARRAY_SPILL, py_tri[i], error_prefix) == -1) {
      return nullptr;
    }
  }

  return PyLong_FromLong(isect_point_tri_v2(pt, UNPACK3(tri)));
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_intersect_point_quad_2d_doc,
    ".. function:: intersect_point_quad_2d(pt, quad_p1, quad_p2, quad_p3, quad_p4, /)\n"
    "\n"
    "   Takes 5 vectors (using only the x and y coordinates): one is the point and the "
    "next 4 define the quad,\n"
    "   only the x and y are used from the vectors. Returns 1 if the point is within the "
    "quad, otherwise 0.\n"
    "   Works only with convex quads without singular edges.\n"
    "\n"
    "   :arg pt: Point\n"
    "   :type pt: :class:`mathutils.Vector`\n"
    "   :arg quad_p1: First point of the quad\n"
    "   :type quad_p1: :class:`mathutils.Vector`\n"
    "   :arg quad_p2: Second point of the quad\n"
    "   :type quad_p2: :class:`mathutils.Vector`\n"
    "   :arg quad_p3: Third point of the quad\n"
    "   :type quad_p3: :class:`mathutils.Vector`\n"
    "   :arg quad_p4: Fourth point of the quad\n"
    "   :type quad_p4: :class:`mathutils.Vector`\n"
    "   :rtype: int\n");
static PyObject *M_Geometry_intersect_point_quad_2d(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "intersect_point_quad_2d";
  PyObject *py_pt, *py_quad[4];
  float pt[2], quad[4][2];
  int i;

  if (!PyArg_ParseTuple(args, "OOOOO:intersect_point_quad_2d", &py_pt, UNPACK4_EX(&, py_quad, ))) {
    return nullptr;
  }

  if (mathutils_array_parse(pt, 2, 2 | MU_ARRAY_SPILL, py_pt, error_prefix) == -1) {
    return nullptr;
  }
  for (i = 0; i < ARRAY_SIZE(quad); i++) {
    if (mathutils_array_parse(quad[i], 2, 2 | MU_ARRAY_SPILL, py_quad[i], error_prefix) == -1) {
      return nullptr;
    }
  }

  return PyLong_FromLong(isect_point_quad_v2(pt, UNPACK4(quad)));
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_distance_point_to_plane_doc,
    ".. function:: distance_point_to_plane(pt, plane_co, plane_no, /)\n"
    "\n"
    "   Returns the signed distance between a point and a plane "
    "   (negative when below the normal).\n"
    "\n"
    "   :arg pt: Point\n"
    "   :type pt: :class:`mathutils.Vector`\n"
    "   :arg plane_co: A point on the plane\n"
    "   :type plane_co: :class:`mathutils.Vector`\n"
    "   :arg plane_no: The direction the plane is facing\n"
    "   :type plane_no: :class:`mathutils.Vector`\n"
    "   :rtype: float\n");
static PyObject *M_Geometry_distance_point_to_plane(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "distance_point_to_plane";
  PyObject *py_pt, *py_plane_co, *py_plane_no;
  float pt[3], plane_co[3], plane_no[3];
  float plane[4];

  if (!PyArg_ParseTuple(args, "OOO:distance_point_to_plane", &py_pt, &py_plane_co, &py_plane_no)) {
    return nullptr;
  }

  if (((mathutils_array_parse(pt, 3, 3 | MU_ARRAY_SPILL, py_pt, error_prefix) != -1) &&
       (mathutils_array_parse(plane_co, 3, 3 | MU_ARRAY_SPILL, py_plane_co, error_prefix) != -1) &&
       (mathutils_array_parse(plane_no, 3, 3 | MU_ARRAY_SPILL, py_plane_no, error_prefix) !=
        -1)) == 0)
  {
    return nullptr;
  }

  plane_from_point_normal_v3(plane, plane_co, plane_no);
  return PyFloat_FromDouble(dist_signed_to_plane_v3(pt, plane));
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_barycentric_transform_doc,
    ".. function:: barycentric_transform(point, tri_a1, tri_a2, tri_a3, tri_b1, tri_b2, tri_b3, "
    "/)\n"
    "\n"
    "   Return a transformed point, the transformation is defined by 2 triangles.\n"
    "\n"
    "   :arg point: The point to transform.\n"
    "   :type point: :class:`mathutils.Vector`\n"
    "   :arg tri_a1: source triangle vertex.\n"
    "   :type tri_a1: :class:`mathutils.Vector`\n"
    "   :arg tri_a2: source triangle vertex.\n"
    "   :type tri_a2: :class:`mathutils.Vector`\n"
    "   :arg tri_a3: source triangle vertex.\n"
    "   :type tri_a3: :class:`mathutils.Vector`\n"
    "   :arg tri_b1: target triangle vertex.\n"
    "   :type tri_b1: :class:`mathutils.Vector`\n"
    "   :arg tri_b2: target triangle vertex.\n"
    "   :type tri_b2: :class:`mathutils.Vector`\n"
    "   :arg tri_b3: target triangle vertex.\n"
    "   :type tri_b3: :class:`mathutils.Vector`\n"
    "   :return: The transformed point\n"
    "   :rtype: :class:`mathutils.Vector`\n");
static PyObject *M_Geometry_barycentric_transform(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "barycentric_transform";
  PyObject *py_pt_src, *py_tri_src[3], *py_tri_dst[3];
  float pt_src[3], pt_dst[3], tri_src[3][3], tri_dst[3][3];
  int i;

  if (!PyArg_ParseTuple(args,
                        "OOOOOOO:barycentric_transform",
                        &py_pt_src,
                        UNPACK3_EX(&, py_tri_src, ),
                        UNPACK3_EX(&, py_tri_dst, )))
  {
    return nullptr;
  }

  if (mathutils_array_parse(pt_src, 3, 3 | MU_ARRAY_SPILL, py_pt_src, error_prefix) == -1) {
    return nullptr;
  }
  for (i = 0; i < ARRAY_SIZE(tri_src); i++) {
    if (((mathutils_array_parse(tri_src[i], 3, 3 | MU_ARRAY_SPILL, py_tri_src[i], error_prefix) !=
          -1) &&
         (mathutils_array_parse(tri_dst[i], 3, 3 | MU_ARRAY_SPILL, py_tri_dst[i], error_prefix) !=
          -1)) == 0)
    {
      return nullptr;
    }
  }

  transform_point_by_tri_v3(pt_dst, pt_src, UNPACK3(tri_dst), UNPACK3(tri_src));

  return Vector_CreatePyObject(pt_dst, 3, nullptr);
}

struct PointsInPlanes_UserData {
  PyObject *py_verts;
  char *planes_used;
};

static void points_in_planes_fn(const float co[3], int i, int j, int k, void *user_data_p)
{
  PointsInPlanes_UserData *user_data = static_cast<PointsInPlanes_UserData *>(user_data_p);
  PyList_APPEND(user_data->py_verts, Vector_CreatePyObject(co, 3, nullptr));
  user_data->planes_used[i] = true;
  user_data->planes_used[j] = true;
  user_data->planes_used[k] = true;
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_points_in_planes_doc,
    ".. function:: points_in_planes(planes, epsilon_coplanar=1e-4, epsilon_isect=1e-6, /)\n"
    "\n"
    "   Returns a list of points inside all planes given and a list of index values for "
    "the planes used.\n"
    "\n"
    "   :arg planes: List of planes (4D vectors).\n"
    "   :type planes: list[:class:`mathutils.Vector`]\n"
    "   :arg epsilon_coplanar: Epsilon value for interpreting plane pairs as co-plannar.\n"
    "   :type epsilon_coplanar: float\n"
    "   :arg epsilon_isect: Epsilon value for intersection.\n"
    "   :type epsilon_isect: float\n"
    "   :return: Two lists, once containing the 3D coordinates inside the planes, "
    "another containing the plane indices used.\n"
    "   :rtype: tuple[list[:class:`mathutils.Vector`], list[int]]\n");
static PyObject *M_Geometry_points_in_planes(PyObject * /*self*/, PyObject *args)
{
  PyObject *py_planes;
  float (*planes)[4];
  float eps_coplanar = 1e-4f;
  float eps_isect = 1e-6f;
  uint planes_len;

  if (!PyArg_ParseTuple(args, "O|ff:points_in_planes", &py_planes, &eps_coplanar, &eps_isect)) {
    return nullptr;
  }

  if ((planes_len = mathutils_array_parse_alloc_v(
           (float **)&planes, 4, py_planes, "points_in_planes")) == -1)
  {
    return nullptr;
  }

  /* NOTE: this could be refactored into plain C easy - py bits are noted. */

  PointsInPlanes_UserData user_data{};
  user_data.py_verts = PyList_New(0);
  user_data.planes_used = static_cast<char *>(PyMem_Malloc(sizeof(char) * planes_len));

  /* python */
  PyObject *py_plane_index = PyList_New(0);

  memset(user_data.planes_used, 0, sizeof(char) * planes_len);

  const bool has_isect = isect_planes_v3_fn(
      planes, planes_len, eps_coplanar, eps_isect, points_in_planes_fn, &user_data);
  PyMem_Free(planes);

  /* Now make user_data list of used planes. */
  if (has_isect) {
    for (int i = 0; i < planes_len; i++) {
      if (user_data.planes_used[i]) {
        PyList_APPEND(py_plane_index, PyLong_FromLong(i));
      }
    }
  }
  PyMem_Free(user_data.planes_used);

  {
    PyObject *ret = PyTuple_New(2);
    PyTuple_SET_ITEMS(ret, user_data.py_verts, py_plane_index);
    return ret;
  }
}

#ifndef MATH_STANDALONE

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_interpolate_bezier_doc,
    ".. function:: interpolate_bezier(knot1, handle1, handle2, knot2, resolution, /)\n"
    "\n"
    "   Interpolate a bezier spline segment.\n"
    "\n"
    "   :arg knot1: First bezier spline point.\n"
    "   :type knot1: :class:`mathutils.Vector`\n"
    "   :arg handle1: First bezier spline handle.\n"
    "   :type handle1: :class:`mathutils.Vector`\n"
    "   :arg handle2: Second bezier spline handle.\n"
    "   :type handle2: :class:`mathutils.Vector`\n"
    "   :arg knot2: Second bezier spline point.\n"
    "   :type knot2: :class:`mathutils.Vector`\n"
    "   :arg resolution: Number of points to return.\n"
    "   :type resolution: int\n"
    "   :return: The interpolated points.\n"
    "   :rtype: list[:class:`mathutils.Vector`]\n");
static PyObject *M_Geometry_interpolate_bezier(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "interpolate_bezier";
  PyObject *py_data[4];
  float data[4][4] = {{0.0f}};
  int resolu;
  int dims = 0;
  int i;
  float *coord_array, *fp;
  PyObject *list;

  if (!PyArg_ParseTuple(args, "OOOOi:interpolate_bezier", UNPACK4_EX(&, py_data, ), &resolu)) {
    return nullptr;
  }

  for (i = 0; i < 4; i++) {
    int dims_tmp;
    if ((dims_tmp = mathutils_array_parse(
             data[i], 2, 3 | MU_ARRAY_SPILL | MU_ARRAY_ZERO, py_data[i], error_prefix)) == -1)
    {
      return nullptr;
    }
    dims = max_ii(dims, dims_tmp);
  }

  if (resolu <= 1) {
    PyErr_SetString(PyExc_ValueError, "resolution must be 2 or over");
    return nullptr;
  }

  coord_array = MEM_calloc_arrayN<float>(size_t(dims) * size_t(resolu), error_prefix);
  for (i = 0; i < dims; i++) {
    BKE_curve_forward_diff_bezier(
        UNPACK4_EX(, data, [i]), coord_array + i, resolu - 1, sizeof(float) * dims);
  }

  list = PyList_New(resolu);
  fp = coord_array;
  for (i = 0; i < resolu; i++, fp = fp + dims) {
    PyList_SET_ITEM(list, i, Vector_CreatePyObject(fp, dims, nullptr));
  }
  MEM_freeN(coord_array);
  return list;
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_tessellate_polygon_doc,
    ".. function:: tessellate_polygon(polylines, /)\n"
    "\n"
    "   Takes a list of polylines (each point a pair or triplet of numbers) and returns "
    "the point indices for a polyline filled with triangles. Does not handle degenerate "
    "geometry (such as zero-length lines due to consecutive identical points).\n"
    "\n"
    "   :arg polylines: Polygons where each polygon is a sequence of 2D or 3D points.\n"
    "   :type polylines: Sequence[Sequence[Sequence[float]]]"
    "   :return: A list of triangles.\n"
    "   :rtype: list[tuple[int, int, int]]\n");
/* PolyFill function, uses Blenders scan-fill to fill multiple poly lines. */
static PyObject *M_Geometry_tessellate_polygon(PyObject * /*self*/, PyObject *polyLineSeq)
{
  PyObject *tri_list; /* Return this list of triangles. */
  PyObject *polyLine, *polyVec;
  int i, len_polylines, len_polypoints;
  bool list_parse_error = false;
  bool is_2d = true;

  /* Display #ListBase. */
  ListBase dispbase = {nullptr, nullptr};
  DispList *dl;
  float *fp; /* Pointer to the array of malloced dl->verts to set the points from the vectors. */
  int totpoints = 0;

  if (!PySequence_Check(polyLineSeq)) {
    PyErr_SetString(PyExc_TypeError, "expected a sequence of poly lines");
    return nullptr;
  }

  len_polylines = PySequence_Size(polyLineSeq);

  for (i = 0; i < len_polylines; i++) {
    polyLine = PySequence_GetItem(polyLineSeq, i);
    if (!PySequence_Check(polyLine)) {
      BKE_displist_free(&dispbase);
      Py_XDECREF(polyLine); /* May be null so use #Py_XDECREF. */
      PyErr_SetString(PyExc_TypeError,
                      "One or more of the polylines is not a sequence of mathutils.Vector's");
      return nullptr;
    }

    len_polypoints = PySequence_Size(polyLine);
    if (len_polypoints > 0) { /* don't bother adding edges as polylines */
      dl = MEM_callocN<DispList>("poly disp");
      BLI_addtail(&dispbase, dl);
      dl->nr = len_polypoints;
      dl->type = DL_POLY;
      dl->parts = 1; /* no faces, 1 edge loop */
      dl->col = 0;   /* no material */
      dl->verts = fp = MEM_malloc_arrayN<float>(3 * size_t(len_polypoints), "dl verts");
      dl->index = MEM_calloc_arrayN<int>(3 * size_t(len_polypoints), "dl index");

      for (int index = 0; index < len_polypoints; index++, fp += 3) {
        polyVec = PySequence_GetItem(polyLine, index);
        const int polyVec_len = mathutils_array_parse(
            fp, 2, 3 | MU_ARRAY_SPILL, polyVec, "tessellate_polygon: parse coord");
        Py_DECREF(polyVec);

        if (UNLIKELY(polyVec_len == -1)) {
          list_parse_error = true;
        }
        else if (polyVec_len == 2) {
          fp[2] = 0.0f;
        }
        else if (polyVec_len == 3) {
          is_2d = false;
        }

        totpoints++;
      }
    }
    Py_DECREF(polyLine);
  }

  if (list_parse_error) {
    BKE_displist_free(&dispbase); /* possible some dl was allocated */
    return nullptr;
  }
  if (totpoints) {
    /* now make the list to return */
    float down_vec[3] = {0, 0, -1};
    BKE_displist_fill(&dispbase, &dispbase, is_2d ? down_vec : nullptr, false);

    /* The faces are stored in a new DisplayList
     * that's added to the head of the #ListBase. */
    dl = static_cast<DispList *>(dispbase.first);

    tri_list = PyList_New(dl->parts);
    if (!tri_list) {
      BKE_displist_free(&dispbase);
      PyErr_SetString(PyExc_RuntimeError, "failed to make a new list");
      return nullptr;
    }

    int *dl_face = dl->index;
    for (int index = 0; index < dl->parts; index++) {
      PyList_SET_ITEM(tri_list, index, PyC_Tuple_Pack_I32({dl_face[0], dl_face[1], dl_face[2]}));
      dl_face += 3;
    }
    BKE_displist_free(&dispbase);
  }
  else {
    /* no points, do this so scripts don't barf */
    BKE_displist_free(&dispbase); /* possible some dl was allocated */
    tri_list = PyList_New(0);
  }

  return tri_list;
}

static int boxPack_FromPyObject(PyObject *value, BoxPack **r_boxarray)
{
  Py_ssize_t len, i;
  PyObject *list_item, *item_1, *item_2;
  BoxPack *boxarray;

  /* Error checking must already be done */
  if (!PyList_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "can only back a list of [x, y, w, h]");
    return -1;
  }

  len = PyList_GET_SIZE(value);

  boxarray = MEM_malloc_arrayN<BoxPack>(size_t(len), __func__);

  for (i = 0; i < len; i++) {
    list_item = PyList_GET_ITEM(value, i);
    if (!PyList_Check(list_item) || PyList_GET_SIZE(list_item) < 4) {
      MEM_freeN(boxarray);
      PyErr_SetString(PyExc_TypeError, "can only pack a list of [x, y, w, h]");
      return -1;
    }

    BoxPack *box = &boxarray[i];

    item_1 = PyList_GET_ITEM(list_item, 2);
    item_2 = PyList_GET_ITEM(list_item, 3);

    box->w = float(PyFloat_AsDouble(item_1));
    box->h = float(PyFloat_AsDouble(item_2));
    box->index = i;

    /* accounts for error case too and overwrites with own error */
    if (box->w < 0.0f || box->h < 0.0f) {
      MEM_freeN(boxarray);
      PyErr_SetString(PyExc_TypeError,
                      "error parsing width and height values from list: "
                      "[x, y, w, h], not numbers or below zero");
      return -1;
    }

    /* verts will be added later */
  }

  *r_boxarray = boxarray;
  return 0;
}

static void boxPack_ToPyObject(PyObject *value, const BoxPack *boxarray)
{
  Py_ssize_t len, i;
  PyObject *list_item;

  len = PyList_GET_SIZE(value);

  for (i = 0; i < len; i++) {
    const BoxPack *box = &boxarray[i];
    list_item = PyList_GET_ITEM(value, box->index);
    PyList_SetItem(list_item, 0, PyFloat_FromDouble(box->x));
    PyList_SetItem(list_item, 1, PyFloat_FromDouble(box->y));
  }
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_box_pack_2d_doc,
    ".. function:: box_pack_2d(boxes, /)\n"
    "\n"
    "   Returns a tuple with the width and height of the packed bounding box.\n"
    "\n"
    "   :arg boxes: list of boxes, each box is a list where the first 4 items are "
    "[X, Y, width, height, ...] other items are ignored. "
    "The X & Y values in this list are modified to set the packed positions.\n"
    "   :type boxes: list[list[float]]\n"
    "   :return: The width and height of the packed bounding box.\n"
    "   :rtype: tuple[float, float]\n");
static PyObject *M_Geometry_box_pack_2d(PyObject * /*self*/, PyObject *boxlist)
{
  float tot_width = 0.0f, tot_height = 0.0f;
  Py_ssize_t len;

  PyObject *ret;

  if (!PyList_Check(boxlist)) {
    PyErr_SetString(PyExc_TypeError, "expected a list of boxes [[x, y, w, h], ... ]");
    return nullptr;
  }

  len = PyList_GET_SIZE(boxlist);
  if (len) {
    BoxPack *boxarray = nullptr;
    if (boxPack_FromPyObject(boxlist, &boxarray) == -1) {
      return nullptr; /* exception set */
    }

    const bool sort_boxes = true; /* Caution: BLI_box_pack_2d sorting is non-deterministic. */
    /* Non Python function */
    BLI_box_pack_2d(boxarray, len, sort_boxes, &tot_width, &tot_height);

    boxPack_ToPyObject(boxlist, boxarray);
    MEM_freeN(boxarray);
  }

  ret = PyTuple_New(2);
  PyTuple_SET_ITEMS(ret, PyFloat_FromDouble(tot_width), PyFloat_FromDouble(tot_height));
  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_box_fit_2d_doc,
    ".. function:: box_fit_2d(points, /)\n"
    "\n"
    "   Returns an angle that best fits the points to an axis aligned rectangle\n"
    "\n"
    "   :arg points: Sequence of 2D points.\n"
    "   :type points: Sequence[Sequence[float]]\n"
    "   :return: angle\n"
    "   :rtype: float\n");
static PyObject *M_Geometry_box_fit_2d(PyObject * /*self*/, PyObject *pointlist)
{
  float (*points)[2];
  Py_ssize_t len;

  float angle = 0.0f;

  len = mathutils_array_parse_alloc_v(((float **)&points), 2, pointlist, "box_fit_2d");
  if (len == -1) {
    return nullptr;
  }

  if (len) {
    /* Non Python function */
    angle = BLI_convexhull_aabb_fit_points_2d({reinterpret_cast<blender::float2 *>(points), len});

    PyMem_Free(points);
  }

  return PyFloat_FromDouble(angle);
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_convex_hull_2d_doc,
    ".. function:: convex_hull_2d(points)\n"
    "\n"
    "   Returns a list of indices into the list given\n"
    "\n"
    "   :arg points: Sequence of 2D points.\n"
    "   :type points: Sequence[Sequence[float]]\n"
    "   :return: a list of indices\n"
    "   :rtype: list[int]\n");
static PyObject *M_Geometry_convex_hull_2d(PyObject * /*self*/, PyObject *pointlist)
{
  float (*points)[2];
  Py_ssize_t len;

  PyObject *ret;

  len = mathutils_array_parse_alloc_v(((float **)&points), 2, pointlist, "convex_hull_2d");
  if (len == -1) {
    return nullptr;
  }

  if (len) {
    int *index_map;
    Py_ssize_t len_ret, i;

    index_map = MEM_malloc_arrayN<int>(size_t(len), __func__);

    /* Non Python function */
    len_ret = BLI_convexhull_2d({reinterpret_cast<blender::float2 *>(points), len}, index_map);

    ret = PyList_New(len_ret);
    for (i = 0; i < len_ret; i++) {
      PyList_SET_ITEM(ret, i, PyLong_FromLong(index_map[i]));
    }

    MEM_freeN(index_map);

    PyMem_Free(points);
  }
  else {
    ret = PyList_New(0);
  }

  return ret;
}

/* Return a PyObject that is a list of lists, using the flattened list array
 * to fill values, with start_table and len_table giving the start index
 * and length of the toplevel_len sub-lists.
 */
static PyObject *list_of_lists_from_arrays(const blender::Span<blender::Vector<int>> data)
{
  if (data.is_empty()) {
    return PyList_New(0);
  }
  PyObject *ret = PyList_New(data.size());
  for (const int i : data.index_range()) {
    const blender::Span<int> group = data[i];
    PyObject *sublist = PyList_New(group.size());
    for (const int j : group.index_range()) {
      PyList_SET_ITEM(sublist, j, PyLong_FromLong(group[j]));
    }
    PyList_SET_ITEM(ret, i, sublist);
  }
  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_delaunay_2d_cdt_doc,
    ".. function:: delaunay_2d_cdt(vert_coords, edges, faces, output_type, epsilon, "
    "need_ids=True, /)\n"
    "\n"
    "   Computes the Constrained Delaunay Triangulation of a set of vertices,\n"
    "   with edges and faces that must appear in the triangulation.\n"
    "   Some triangles may be eaten away, or combined with other triangles,\n"
    "   according to output type.\n"
    "   The returned verts may be in a different order from input verts, may be moved\n"
    "   slightly, and may be merged with other nearby verts.\n"
    "   The three returned orig lists give, for each of verts, edges, and faces, the list of\n"
    "   input element indices corresponding to the positionally same output element.\n"
    "   For edges, the orig indices start with the input edges and then continue\n"
    "   with the edges implied by each of the faces (n of them for an n-gon).\n"
    "   If the need_ids argument is supplied, and False, then the code skips the preparation\n"
    "   of the orig arrays, which may save some time.\n"
    "\n"
    "   :arg vert_coords: Vertex coordinates (2d)\n"
    "   :type vert_coords: Sequence[:class:`mathutils.Vector`]\n"
    "   :arg edges: Edges, as pairs of indices in ``vert_coords``\n"
    "   :type edges: Sequence[Sequence[int, int]]\n"
    "   :arg faces: Faces, each sublist is a face, as indices in ``vert_coords`` (CCW oriented).\n"
    "   :type faces: Sequence[Sequence[int]]\n"
    "   :arg output_type: What output looks like. 0 => triangles with convex hull. "
    "1 => triangles inside constraints. "
    "2 => the input constraints, intersected. "
    "3 => like 2 but detect holes and omit them from output. "
    "4 => like 2 but with extra edges to make valid BMesh faces. "
    "5 => like 4 but detect holes and omit them from output.\n"
    "   :type output_type: int\n"
    "   :arg epsilon: For nearness tests; should not be zero\n"
    "   :type epsilon: float\n"
    "   :arg need_ids: are the orig output arrays needed?\n"
    "   :type need_ids: bool\n"
    "   :return: Output tuple, (vert_coords, edges, faces, orig_verts, orig_edges, orig_faces)\n"
    "   :rtype: tuple["
    "list[:class:`mathutils.Vector`], "
    "list[tuple[int, int]], "
    "list[list[int]], "
    "list[list[int]], "
    "list[list[int]], "
    "list[list[int]]]\n");
static PyObject *M_Geometry_delaunay_2d_cdt(PyObject * /*self*/, PyObject *args)
{
  using namespace blender;
  const char *error_prefix = "delaunay_2d_cdt";
  PyObject *vert_coords, *edges, *faces;
  int output_type;
  float epsilon;
  bool need_ids = true;
  float (*in_coords)[2] = nullptr;
  int (*in_edges)[2] = nullptr;
  Py_ssize_t vert_coords_len, edges_len;
  PyObject *out_vert_coords = nullptr;
  PyObject *out_edges = nullptr;
  PyObject *out_faces = nullptr;
  PyObject *out_orig_verts = nullptr;
  PyObject *out_orig_edges = nullptr;
  PyObject *out_orig_faces = nullptr;
  PyObject *ret_value = nullptr;

  if (!PyArg_ParseTuple(args,
                        "OOOif|p:delaunay_2d_cdt",
                        &vert_coords,
                        &edges,
                        &faces,
                        &output_type,
                        &epsilon,
                        &need_ids))
  {
    return nullptr;
  }

  BLI_SCOPED_DEFER([&]() {
    if (in_coords != nullptr) {
      PyMem_Free(in_coords);
    }
    if (in_edges != nullptr) {
      PyMem_Free(in_edges);
    }
  });

  vert_coords_len = mathutils_array_parse_alloc_v(
      (float **)&in_coords, 2, vert_coords, error_prefix);
  if (vert_coords_len == -1) {
    return nullptr;
  }

  edges_len = mathutils_array_parse_alloc_vi((int **)&in_edges, 2, edges, error_prefix);
  if (edges_len == -1) {
    return nullptr;
  }

  Array<Vector<int>> in_faces;
  if (!mathutils_array_parse_alloc_viseq(faces, error_prefix, in_faces)) {
    return nullptr;
  }

  Array<double2> verts(vert_coords_len);
  for (const int i : verts.index_range()) {
    verts[i] = {double(in_coords[i][0]), double(in_coords[i][1])};
  }

  meshintersect::CDT_input<double> in;
  in.vert = std::move(verts);
  in.edge = Span(reinterpret_cast<std::pair<int, int> *>(in_edges), edges_len);
  in.face = std::move(in_faces);
  in.epsilon = epsilon;
  in.need_ids = need_ids;

  const meshintersect::CDT_result<double> res = meshintersect::delaunay_2d_calc(
      in, CDT_output_type(output_type));

  ret_value = PyTuple_New(6);

  out_vert_coords = PyList_New(res.vert.size());
  for (const int i : res.vert.index_range()) {
    const float2 vert_float(res.vert[i]);
    PyObject *item = Vector_CreatePyObject(vert_float, 2, nullptr);
    if (item == nullptr) {
      Py_DECREF(ret_value);
      Py_DECREF(out_vert_coords);
      return nullptr;
    }
    PyList_SET_ITEM(out_vert_coords, i, item);
  }
  PyTuple_SET_ITEM(ret_value, 0, out_vert_coords);

  out_edges = PyList_New(res.edge.size());
  for (const int i : res.edge.index_range()) {
    PyObject *item = PyTuple_New(2);
    PyTuple_SET_ITEM(item, 0, PyLong_FromLong(long(res.edge[i].first)));
    PyTuple_SET_ITEM(item, 1, PyLong_FromLong(long(res.edge[i].second)));
    PyList_SET_ITEM(out_edges, i, item);
  }
  PyTuple_SET_ITEM(ret_value, 1, out_edges);

  out_faces = list_of_lists_from_arrays(res.face);
  PyTuple_SET_ITEM(ret_value, 2, out_faces);

  out_orig_verts = list_of_lists_from_arrays(res.vert_orig);
  PyTuple_SET_ITEM(ret_value, 3, out_orig_verts);

  out_orig_edges = list_of_lists_from_arrays(res.edge_orig);
  PyTuple_SET_ITEM(ret_value, 4, out_orig_edges);

  out_orig_faces = list_of_lists_from_arrays(res.face_orig);
  PyTuple_SET_ITEM(ret_value, 5, out_orig_faces);

  return ret_value;
}

#endif /* MATH_STANDALONE */

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef M_Geometry_methods[] = {
    {"intersect_ray_tri",
     (PyCFunction)M_Geometry_intersect_ray_tri,
     METH_VARARGS,
     M_Geometry_intersect_ray_tri_doc},
    {"intersect_point_line",
     (PyCFunction)M_Geometry_intersect_point_line,
     METH_FASTCALL,
     M_Geometry_intersect_point_line_doc},
    {"intersect_point_line_segment",
     (PyCFunction)M_Geometry_intersect_point_line_segment,
     METH_FASTCALL,
     M_Geometry_intersect_point_line_segment_doc},
    {"intersect_point_tri",
     (PyCFunction)M_Geometry_intersect_point_tri,
     METH_VARARGS,
     M_Geometry_intersect_point_tri_doc},
    {"closest_point_on_tri",
     (PyCFunction)M_Geometry_closest_point_on_tri,
     METH_VARARGS,
     M_Geometry_closest_point_on_tri_doc},
    {"intersect_point_tri_2d",
     (PyCFunction)M_Geometry_intersect_point_tri_2d,
     METH_VARARGS,
     M_Geometry_intersect_point_tri_2d_doc},
    {"intersect_point_quad_2d",
     (PyCFunction)M_Geometry_intersect_point_quad_2d,
     METH_VARARGS,
     M_Geometry_intersect_point_quad_2d_doc},
    {"intersect_line_line",
     (PyCFunction)M_Geometry_intersect_line_line,
     METH_VARARGS,
     M_Geometry_intersect_line_line_doc},
    {"intersect_line_line_2d",
     (PyCFunction)M_Geometry_intersect_line_line_2d,
     METH_VARARGS,
     M_Geometry_intersect_line_line_2d_doc},
    {"intersect_line_plane",
     (PyCFunction)M_Geometry_intersect_line_plane,
     METH_VARARGS,
     M_Geometry_intersect_line_plane_doc},
    {"intersect_plane_plane",
     (PyCFunction)M_Geometry_intersect_plane_plane,
     METH_VARARGS,
     M_Geometry_intersect_plane_plane_doc},
    {"intersect_line_sphere",
     (PyCFunction)M_Geometry_intersect_line_sphere,
     METH_VARARGS,
     M_Geometry_intersect_line_sphere_doc},
    {"intersect_line_sphere_2d",
     (PyCFunction)M_Geometry_intersect_line_sphere_2d,
     METH_VARARGS,
     M_Geometry_intersect_line_sphere_2d_doc},
    {"distance_point_to_plane",
     (PyCFunction)M_Geometry_distance_point_to_plane,
     METH_VARARGS,
     M_Geometry_distance_point_to_plane_doc},
    {"intersect_sphere_sphere_2d",
     (PyCFunction)M_Geometry_intersect_sphere_sphere_2d,
     METH_VARARGS,
     M_Geometry_intersect_sphere_sphere_2d_doc},
    {"intersect_tri_tri_2d",
     (PyCFunction)M_Geometry_intersect_tri_tri_2d,
     METH_VARARGS,
     M_Geometry_intersect_tri_tri_2d_doc},
    {"area_tri", (PyCFunction)M_Geometry_area_tri, METH_VARARGS, M_Geometry_area_tri_doc},
    {"volume_tetrahedron",
     (PyCFunction)M_Geometry_volume_tetrahedron,
     METH_VARARGS,
     M_Geometry_volume_tetrahedron_doc},
    {"normal", (PyCFunction)M_Geometry_normal, METH_VARARGS, M_Geometry_normal_doc},
    {"barycentric_transform",
     (PyCFunction)M_Geometry_barycentric_transform,
     METH_VARARGS,
     M_Geometry_barycentric_transform_doc},
    {"points_in_planes",
     (PyCFunction)M_Geometry_points_in_planes,
     METH_VARARGS,
     M_Geometry_points_in_planes_doc},
#ifndef MATH_STANDALONE
    {"interpolate_bezier",
     (PyCFunction)M_Geometry_interpolate_bezier,
     METH_VARARGS,
     M_Geometry_interpolate_bezier_doc},
    {"tessellate_polygon",
     (PyCFunction)M_Geometry_tessellate_polygon,
     METH_O,
     M_Geometry_tessellate_polygon_doc},
    {"convex_hull_2d",
     (PyCFunction)M_Geometry_convex_hull_2d,
     METH_O,
     M_Geometry_convex_hull_2d_doc},
    {"delaunay_2d_cdt",
     (PyCFunction)M_Geometry_delaunay_2d_cdt,
     METH_VARARGS,
     M_Geometry_delaunay_2d_cdt_doc},
    {"box_fit_2d", (PyCFunction)M_Geometry_box_fit_2d, METH_O, M_Geometry_box_fit_2d_doc},
    {"box_pack_2d", (PyCFunction)M_Geometry_box_pack_2d, METH_O, M_Geometry_box_pack_2d_doc},
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

PyDoc_STRVAR(
    /* Wrap. */
    M_Geometry_doc,
    "The Blender geometry module.");
static PyModuleDef M_Geometry_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "mathutils.geometry",
    /*m_doc*/ M_Geometry_doc,
    /*m_size*/ 0,
    /*m_methods*/ M_Geometry_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

/*----------------------------MODULE INIT-------------------------*/

PyMODINIT_FUNC PyInit_mathutils_geometry()
{
  PyObject *submodule = PyModule_Create(&M_Geometry_module_def);
  return submodule;
}
