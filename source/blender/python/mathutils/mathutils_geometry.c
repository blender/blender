/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Joseph Gilbert, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/mathutils/mathutils_geometry.c
 *  \ingroup pymathutils
 */


#include <Python.h>

#include "mathutils.h"
#include "mathutils_geometry.h"

/* Used for PolyFill */
#ifndef MATH_STANDALONE /* define when building outside blender */
#  include "MEM_guardedalloc.h"
#  include "BLI_blenlib.h"
#  include "BLI_boxpack2d.h"
#  include "BLI_convexhull2d.h"
#  include "BKE_displist.h"
#  include "BKE_curve.h"
#endif

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "../generic/python_utildefines.h"

/*-------------------------DOC STRINGS ---------------------------*/
PyDoc_STRVAR(M_Geometry_doc,
"The Blender geometry module"
);

/* ---------------------------------INTERSECTION FUNCTIONS-------------------- */

PyDoc_STRVAR(M_Geometry_intersect_ray_tri_doc,
".. function:: intersect_ray_tri(v1, v2, v3, ray, orig, clip=True)\n"
"\n"
"   Returns the intersection between a ray and a triangle, if possible, returns None otherwise.\n"
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
"   :arg clip: When False, don't restrict the intersection to the area of the triangle, use the infinite plane defined by the triangle.\n"
"   :type clip: boolean\n"
"   :return: The point of intersection or None if no intersection is found\n"
"   :rtype: :class:`mathutils.Vector` or None\n"
);
static PyObject *M_Geometry_intersect_ray_tri(PyObject *UNUSED(self), PyObject *args)
{
	const char *error_prefix = "intersect_ray_tri";
	PyObject *py_ray, *py_ray_off, *py_tri[3];
	float dir[3], orig[3], tri[3][3], e1[3], e2[3], pvec[3], tvec[3], qvec[3];
	float det, inv_det, u, v, t;
	int clip = 1;
	int i;

	if (!PyArg_ParseTuple(
	        args, "OOOOO|i:intersect_ray_tri",
	        UNPACK3_EX(&, py_tri, ), &py_ray, &py_ray_off, &clip))
	{
		return NULL;
	}

	if (((mathutils_array_parse(dir, 3, 3, py_ray, error_prefix) != -1) &&
	     (mathutils_array_parse(orig, 3, 3, py_ray_off, error_prefix) != -1)) == 0)
	{
		return NULL;
	}

	for (i = 0; i < ARRAY_SIZE(tri); i++) {
		if (mathutils_array_parse(tri[i], 2, 2 | MU_ARRAY_SPILL, py_tri[i], error_prefix) == -1) {
			return NULL;
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

	return Vector_CreatePyObject(pvec, 3, NULL);
}

/* Line-Line intersection using algorithm from mathworld.wolfram.com */

PyDoc_STRVAR(M_Geometry_intersect_line_line_doc,
".. function:: intersect_line_line(v1, v2, v3, v4)\n"
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
"   :rtype: tuple of :class:`mathutils.Vector`'s\n"
);
static PyObject *M_Geometry_intersect_line_line(PyObject *UNUSED(self), PyObject *args)
{
	const char *error_prefix = "intersect_line_line";
	PyObject *tuple;
	PyObject *py_lines[4];
	float lines[4][3], i1[3], i2[3];
	int len;
	int result;

	if (!PyArg_ParseTuple(
	        args, "OOOO:intersect_line_line",
	        UNPACK4_EX(&, py_lines, )))
	{
		return NULL;
	}

	if ((((len = mathutils_array_parse(lines[0], 2, 3, py_lines[0], error_prefix)) != -1) &&
	     (mathutils_array_parse(lines[1], len, len, py_lines[1], error_prefix) != -1) &&
	     (mathutils_array_parse(lines[2], len, len, py_lines[2], error_prefix) != -1) &&
	     (mathutils_array_parse(lines[3], len, len, py_lines[3], error_prefix) != -1)) == 0)
	{
		return NULL;
	}

	if (len == 2) {
		lines[0][2] = lines[1][2] = lines[2][2] = lines[3][2] = 0.0f;
	}

	result = isect_line_line_v3(UNPACK4(lines), i1, i2);
	/* The return-code isnt exposed,
	 * this way we can check know how close the lines are. */
	if (result == 1) {
		closest_to_line_v3(i2, i1, lines[2], lines[3]);
	}

	if (result == 0) {
		/* colinear */
		Py_RETURN_NONE;
	}
	else {
		tuple = PyTuple_New(2);
		PyTuple_SET_ITEMS(tuple,
		        Vector_CreatePyObject(i1, len, NULL),
		        Vector_CreatePyObject(i2, len, NULL));
		return tuple;
	}
}

/* Line-Line intersection using algorithm from mathworld.wolfram.com */

PyDoc_STRVAR(M_Geometry_intersect_sphere_sphere_2d_doc,
".. function:: intersect_sphere_sphere_2d(p_a, radius_a, p_b, radius_b)\n"
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
"   :rtype: tuple of :class:`mathutils.Vector`'s or None when there is no intersection\n"
);
static PyObject *M_Geometry_intersect_sphere_sphere_2d(PyObject *UNUSED(self), PyObject *args)
{
	const char *error_prefix = "intersect_sphere_sphere_2d";
	PyObject *ret;
	PyObject *py_v_a, *py_v_b;
	float v_a[2], v_b[2];
	float rad_a, rad_b;
	float v_ab[2];
	float dist;

	if (!PyArg_ParseTuple(
	        args, "OfOf:intersect_sphere_sphere_2d",
	        &py_v_a, &rad_a,
	        &py_v_b, &rad_b))
	{
		return NULL;
	}

	if (((mathutils_array_parse(v_a, 2, 2, py_v_a, error_prefix) != -1) &&
	     (mathutils_array_parse(v_b, 2, 2, py_v_b, error_prefix) != -1)) == 0)
	{
		return NULL;
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
		PyTuple_SET_ITEMS(ret,
		        Py_INCREF_RET(Py_None),
		        Py_INCREF_RET(Py_None));
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

		PyTuple_SET_ITEMS(ret,
		        Vector_CreatePyObject(i1, 2, NULL),
		        Vector_CreatePyObject(i2, 2, NULL));
	}

	return ret;
}

PyDoc_STRVAR(M_Geometry_normal_doc,
".. function:: normal(vectors)\n"
"\n"
"   Returns the normal of a 3D polygon.\n"
"\n"
"   :arg vectors: Vectors to calculate normals with\n"
"   :type vectors: sequence of 3 or more 3d vector\n"
"   :rtype: :class:`mathutils.Vector`\n"
);
static PyObject *M_Geometry_normal(PyObject *UNUSED(self), PyObject *args)
{
	float (*coords)[3];
	int coords_len;
	float n[3];
	PyObject *ret = NULL;

	/* use */
	if (PyTuple_GET_SIZE(args) == 1) {
		args = PyTuple_GET_ITEM(args, 0);
	}

	if ((coords_len = mathutils_array_parse_alloc_v((float **)&coords, 3 | MU_ARRAY_SPILL, args, "normal")) == -1) {
		return NULL;
	}

	if (coords_len < 3) {
		PyErr_SetString(PyExc_ValueError,
		                "Expected 3 or more vectors");
		goto finally;
	}

	normal_poly_v3(n, (const float (*)[3])coords, coords_len);
	ret = Vector_CreatePyObject(n, 3, NULL);

finally:
	PyMem_Free(coords);
	return ret;
}

/* --------------------------------- AREA FUNCTIONS-------------------- */

PyDoc_STRVAR(M_Geometry_area_tri_doc,
".. function:: area_tri(v1, v2, v3)\n"
"\n"
"   Returns the area size of the 2D or 3D triangle defined.\n"
"\n"
"   :arg v1: Point1\n"
"   :type v1: :class:`mathutils.Vector`\n"
"   :arg v2: Point2\n"
"   :type v2: :class:`mathutils.Vector`\n"
"   :arg v3: Point3\n"
"   :type v3: :class:`mathutils.Vector`\n"
"   :rtype: float\n"
);
static PyObject *M_Geometry_area_tri(PyObject *UNUSED(self), PyObject *args)
{
	const char *error_prefix = "area_tri";
	PyObject *py_tri[3];
	float tri[3][3];
	int len;

	if (!PyArg_ParseTuple(
	        args, "OOO:area_tri",
	        UNPACK3_EX(&, py_tri, )))
	{
		return NULL;
	}

	if ((((len = mathutils_array_parse(tri[0], 2, 3, py_tri[0], error_prefix)) != -1) &&
	     (mathutils_array_parse(tri[1], len, len, py_tri[1], error_prefix) != -1) &&
	     (mathutils_array_parse(tri[2], len, len, py_tri[2], error_prefix) != -1)) == 0)
	{
		return NULL;
	}

	return PyFloat_FromDouble((len == 3 ? area_tri_v3 : area_tri_v2)(UNPACK3(tri)));
}

PyDoc_STRVAR(M_Geometry_volume_tetrahedron_doc,
".. function:: volume_tetrahedron(v1, v2, v3, v4)\n"
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
"   :rtype: float\n"
);
static PyObject *M_Geometry_volume_tetrahedron(PyObject *UNUSED(self), PyObject *args)
{
	const char *error_prefix = "volume_tetrahedron";
	PyObject *py_tet[4];
	float tet[4][3];
	int i;

	if (!PyArg_ParseTuple(
	        args, "OOOO:volume_tetrahedron",
	        UNPACK4_EX(&, py_tet, )))
	{
		return NULL;
	}

	for (i = 0; i < ARRAY_SIZE(tet); i++) {
		if (mathutils_array_parse(tet[i], 3, 3 | MU_ARRAY_SPILL, py_tet[i], error_prefix) == -1) {
			return NULL;
		}
	}

	return PyFloat_FromDouble(volume_tetrahedron_v3(UNPACK4(tet)));
}

PyDoc_STRVAR(M_Geometry_intersect_line_line_2d_doc,
".. function:: intersect_line_line_2d(lineA_p1, lineA_p2, lineB_p1, lineB_p2)\n"
"\n"
"   Takes 2 lines (as 4 vectors) and returns a vector for their point of intersection or None.\n"
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
"   :rtype: :class:`mathutils.Vector` or None\n"
);
static PyObject *M_Geometry_intersect_line_line_2d(PyObject *UNUSED(self), PyObject *args)
{
	const char *error_prefix = "intersect_line_line_2d";
	PyObject *py_lines[4];
	float lines[4][2];
	float vi[2];
	int i;

	if (!PyArg_ParseTuple(
	        args, "OOOO:intersect_line_line_2d",
	        UNPACK4_EX(&, py_lines, )))
	{
		return NULL;
	}

	for (i = 0; i < ARRAY_SIZE(lines); i++) {
		if (mathutils_array_parse(lines[i], 2, 2 | MU_ARRAY_SPILL, py_lines[i], error_prefix) == -1) {
			return NULL;
		}
	}

	if (isect_seg_seg_v2_point(UNPACK4(lines), vi) == 1) {
		return Vector_CreatePyObject(vi, 2, NULL);
	}
	else {
		Py_RETURN_NONE;
	}
}


PyDoc_STRVAR(M_Geometry_intersect_line_plane_doc,
".. function:: intersect_line_plane(line_a, line_b, plane_co, plane_no, no_flip=False)\n"
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
"   :return: The point of intersection or None when not found\n"
"   :rtype: :class:`mathutils.Vector` or None\n"
);
static PyObject *M_Geometry_intersect_line_plane(PyObject *UNUSED(self), PyObject *args)
{
	const char *error_prefix = "intersect_line_plane";
	PyObject *py_line_a, *py_line_b, *py_plane_co, *py_plane_no;
	float line_a[3], line_b[3], plane_co[3], plane_no[3];
	float isect[3];
	int no_flip = false;

	if (!PyArg_ParseTuple(
	        args, "OOOO|i:intersect_line_plane",
	        &py_line_a, &py_line_b, &py_plane_co, &py_plane_no,
	        &no_flip))
	{
		return NULL;
	}

	if (((mathutils_array_parse(line_a, 3, 3 | MU_ARRAY_SPILL, py_line_a, error_prefix) != -1) &&
	     (mathutils_array_parse(line_b, 3, 3 | MU_ARRAY_SPILL, py_line_b, error_prefix) != -1) &&
	     (mathutils_array_parse(plane_co, 3, 3 | MU_ARRAY_SPILL, py_plane_co, error_prefix) != -1) &&
	     (mathutils_array_parse(plane_no, 3, 3 | MU_ARRAY_SPILL, py_plane_no, error_prefix) != -1)) == 0)
	{
		return NULL;
	}

	/* TODO: implements no_flip */
	if (isect_line_plane_v3(isect, line_a, line_b, plane_co, plane_no) == 1) {
		return Vector_CreatePyObject(isect, 3, NULL);
	}
	else {
		Py_RETURN_NONE;
	}
}

PyDoc_STRVAR(M_Geometry_intersect_plane_plane_doc,
".. function:: intersect_plane_plane(plane_a_co, plane_a_no, plane_b_co, plane_b_no)\n"
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
"   :return: The line of the intersection represented as a point and a vector\n"
"   :rtype: tuple pair of :class:`mathutils.Vector` or None if the intersection can't be calculated\n"
);
static PyObject *M_Geometry_intersect_plane_plane(PyObject *UNUSED(self), PyObject *args)
{
	const char *error_prefix = "intersect_plane_plane";
	PyObject *ret, *ret_co, *ret_no;
	PyObject *py_plane_a_co, *py_plane_a_no, *py_plane_b_co, *py_plane_b_no;
	float plane_a_co[3], plane_a_no[3], plane_b_co[3], plane_b_no[3];

	float isect_co[3];
	float isect_no[3];

	if (!PyArg_ParseTuple(
	        args, "OOOO:intersect_plane_plane",
	        &py_plane_a_co, &py_plane_a_no, &py_plane_b_co, &py_plane_b_no))
	{
		return NULL;
	}

	if (((mathutils_array_parse(plane_a_co, 3, 3 | MU_ARRAY_SPILL, py_plane_a_co, error_prefix) != -1) &&
	     (mathutils_array_parse(plane_a_no, 3, 3 | MU_ARRAY_SPILL, py_plane_a_no, error_prefix) != -1) &&
	     (mathutils_array_parse(plane_b_co, 3, 3 | MU_ARRAY_SPILL, py_plane_b_co, error_prefix) != -1) &&
	     (mathutils_array_parse(plane_b_no, 3, 3 | MU_ARRAY_SPILL, py_plane_b_no, error_prefix) != -1)) == 0)
	{
		return NULL;
	}

	if (isect_plane_plane_v3(isect_co, isect_no,
	                         plane_a_co, plane_a_no,
	                         plane_b_co, plane_b_no))
	{
		normalize_v3(isect_no);

		ret_co = Vector_CreatePyObject(isect_co, 3, NULL);
		ret_no = Vector_CreatePyObject(isect_no, 3, NULL);
	}
	else {
		ret_co = Py_INCREF_RET(Py_None);
		ret_no = Py_INCREF_RET(Py_None);
	}

	ret = PyTuple_New(2);
	PyTuple_SET_ITEMS(ret,
	        ret_co,
	        ret_no);
	return ret;
}

PyDoc_STRVAR(M_Geometry_intersect_line_sphere_doc,
".. function:: intersect_line_sphere(line_a, line_b, sphere_co, sphere_radius, clip=True)\n"
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
"   :type sphere_radius: sphere_radius\n"
"   :return: The intersection points as a pair of vectors or None when there is no intersection\n"
"   :rtype: A tuple pair containing :class:`mathutils.Vector` or None\n"
);
static PyObject *M_Geometry_intersect_line_sphere(PyObject *UNUSED(self), PyObject *args)
{
	const char *error_prefix = "intersect_line_sphere";
	PyObject *py_line_a, *py_line_b, *py_sphere_co;
	float line_a[3], line_b[3], sphere_co[3];
	float sphere_radius;
	int clip = true;

	float isect_a[3];
	float isect_b[3];

	if (!PyArg_ParseTuple(
	        args, "OOOf|i:intersect_line_sphere",
	        &py_line_a, &py_line_b, &py_sphere_co, &sphere_radius, &clip))
	{
		return NULL;
	}

	if (((mathutils_array_parse(line_a, 3, 3 | MU_ARRAY_SPILL, py_line_a, error_prefix) != -1) &&
	     (mathutils_array_parse(line_b, 3, 3 | MU_ARRAY_SPILL, py_line_b, error_prefix) != -1) &&
	     (mathutils_array_parse(sphere_co, 3, 3 | MU_ARRAY_SPILL, py_sphere_co, error_prefix) != -1)) == 0)
	{
		return NULL;
	}
	else {
		bool use_a = true;
		bool use_b = true;
		float lambda;

		PyObject *ret = PyTuple_New(2);

		switch (isect_line_sphere_v3(line_a, line_b, sphere_co, sphere_radius, isect_a, isect_b)) {
			case 1:
				if (!(!clip || (((lambda = line_point_factor_v3(isect_a, line_a, line_b)) >= 0.0f) && (lambda <= 1.0f)))) use_a = false;
				use_b = false;
				break;
			case 2:
				if (!(!clip || (((lambda = line_point_factor_v3(isect_a, line_a, line_b)) >= 0.0f) && (lambda <= 1.0f)))) use_a = false;
				if (!(!clip || (((lambda = line_point_factor_v3(isect_b, line_a, line_b)) >= 0.0f) && (lambda <= 1.0f)))) use_b = false;
				break;
			default:
				use_a = false;
				use_b = false;
				break;
		}

		PyTuple_SET_ITEMS(ret,
		        use_a ? Vector_CreatePyObject(isect_a, 3, NULL) : Py_INCREF_RET(Py_None),
		        use_b ? Vector_CreatePyObject(isect_b, 3, NULL) : Py_INCREF_RET(Py_None));

		return ret;
	}
}

/* keep in sync with M_Geometry_intersect_line_sphere */
PyDoc_STRVAR(M_Geometry_intersect_line_sphere_2d_doc,
".. function:: intersect_line_sphere_2d(line_a, line_b, sphere_co, sphere_radius, clip=True)\n"
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
"   :type sphere_radius: sphere_radius\n"
"   :return: The intersection points as a pair of vectors or None when there is no intersection\n"
"   :rtype: A tuple pair containing :class:`mathutils.Vector` or None\n"
);
static PyObject *M_Geometry_intersect_line_sphere_2d(PyObject *UNUSED(self), PyObject *args)
{
	const char *error_prefix = "intersect_line_sphere_2d";
	PyObject *py_line_a, *py_line_b, *py_sphere_co;
	float line_a[2], line_b[2], sphere_co[2];
	float sphere_radius;
	int clip = true;

	float isect_a[2];
	float isect_b[2];

	if (!PyArg_ParseTuple(
	        args, "OOOf|i:intersect_line_sphere_2d",
	        &py_line_a, &py_line_b, &py_sphere_co, &sphere_radius, &clip))
	{
		return NULL;
	}

	if (((mathutils_array_parse(line_a, 2, 2 | MU_ARRAY_SPILL, py_line_a, error_prefix) != -1) &&
	     (mathutils_array_parse(line_b, 2, 2 | MU_ARRAY_SPILL, py_line_b, error_prefix) != -1) &&
	     (mathutils_array_parse(sphere_co, 2, 2 | MU_ARRAY_SPILL, py_sphere_co, error_prefix) != -1)) == 0)
	{
		return NULL;
	}
	else {
		bool use_a = true;
		bool use_b = true;
		float lambda;

		PyObject *ret = PyTuple_New(2);

		switch (isect_line_sphere_v2(line_a, line_b, sphere_co, sphere_radius, isect_a, isect_b)) {
			case 1:
				if (!(!clip || (((lambda = line_point_factor_v2(isect_a, line_a, line_b)) >= 0.0f) && (lambda <= 1.0f)))) use_a = false;
				use_b = false;
				break;
			case 2:
				if (!(!clip || (((lambda = line_point_factor_v2(isect_a, line_a, line_b)) >= 0.0f) && (lambda <= 1.0f)))) use_a = false;
				if (!(!clip || (((lambda = line_point_factor_v2(isect_b, line_a, line_b)) >= 0.0f) && (lambda <= 1.0f)))) use_b = false;
				break;
			default:
				use_a = false;
				use_b = false;
				break;
		}

		PyTuple_SET_ITEMS(ret,
		        use_a ? Vector_CreatePyObject(isect_a, 2, NULL) : Py_INCREF_RET(Py_None),
		        use_b ? Vector_CreatePyObject(isect_b, 2, NULL) : Py_INCREF_RET(Py_None));

		return ret;
	}
}

PyDoc_STRVAR(M_Geometry_intersect_point_line_doc,
".. function:: intersect_point_line(pt, line_p1, line_p2)\n"
"\n"
"   Takes a point and a line and returns a tuple with the closest point on the line and its distance from the first point of the line as a percentage of the length of the line.\n"
"\n"
"   :arg pt: Point\n"
"   :type pt: :class:`mathutils.Vector`\n"
"   :arg line_p1: First point of the line\n"
"   :type line_p1: :class:`mathutils.Vector`\n"
"   :arg line_p1: Second point of the line\n"
"   :type line_p1: :class:`mathutils.Vector`\n"
"   :rtype: (:class:`mathutils.Vector`, float)\n"
);
static PyObject *M_Geometry_intersect_point_line(PyObject *UNUSED(self), PyObject *args)
{
	const char *error_prefix = "intersect_point_line";
	PyObject *py_pt, *py_line_a, *py_line_b;
	float pt[3], pt_out[3], line_a[3], line_b[3];
	float lambda;
	PyObject *ret;
	int size = 2;
	
	if (!PyArg_ParseTuple(
	        args, "OOO:intersect_point_line",
	        &py_pt, &py_line_a, &py_line_b))
	{
		return NULL;
	}

	/* accept 2d verts */
	if ((((size = mathutils_array_parse(pt, 2, 3 | MU_ARRAY_SPILL | MU_ARRAY_ZERO, py_pt, error_prefix)) != -1) &&
	     (mathutils_array_parse(line_a, 2, 3 | MU_ARRAY_SPILL | MU_ARRAY_ZERO, py_line_a, error_prefix) != -1) &&
	     (mathutils_array_parse(line_b, 3, 3 | MU_ARRAY_SPILL | MU_ARRAY_ZERO, py_line_b, error_prefix) != -1)) == 0)
	{
		return NULL;
	}

	/* do the calculation */
	lambda = closest_to_line_v3(pt_out, pt, line_a, line_b);
	
	ret = PyTuple_New(2);
	PyTuple_SET_ITEMS(ret,
	        Vector_CreatePyObject(pt_out, size, NULL),
	        PyFloat_FromDouble(lambda));
	return ret;
}

PyDoc_STRVAR(M_Geometry_intersect_point_tri_doc,
".. function:: intersect_point_tri(pt, tri_p1, tri_p2, tri_p3)\n"
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
"   :return: Point on the triangles plane or None if its outside the triangle\n"
"   :rtype: :class:`mathutils.Vector` or None\n"
);
static PyObject *M_Geometry_intersect_point_tri(PyObject *UNUSED(self), PyObject *args)
{
	const char *error_prefix = "intersect_point_tri";
	PyObject *py_pt, *py_tri[3];
	float pt[3], tri[3][3];
	float vi[3];
	int i;

	if (!PyArg_ParseTuple(
	        args, "OOOO:intersect_point_tri",
	        &py_pt, UNPACK3_EX(&, py_tri, )))
	{
		return NULL;
	}

	if (mathutils_array_parse(pt, 3, 3 | MU_ARRAY_SPILL, py_pt, error_prefix) == -1) {
		return NULL;
	}
	for (i = 0; i < ARRAY_SIZE(tri); i++) {
		if (mathutils_array_parse(tri[i], 3, 3 | MU_ARRAY_SPILL, py_tri[i], error_prefix) == -1) {
			return NULL;
		}
	}

	if (isect_point_tri_v3(pt, UNPACK3(tri), vi)) {
		return Vector_CreatePyObject(vi, 3, NULL);
	}
	else {
		Py_RETURN_NONE;
	}
}

PyDoc_STRVAR(M_Geometry_intersect_point_tri_2d_doc,
".. function:: intersect_point_tri_2d(pt, tri_p1, tri_p2, tri_p3)\n"
"\n"
"   Takes 4 vectors (using only the x and y coordinates): one is the point and the next 3 define the triangle. Returns 1 if the point is within the triangle, otherwise 0.\n"
"\n"
"   :arg pt: Point\n"
"   :type pt: :class:`mathutils.Vector`\n"
"   :arg tri_p1: First point of the triangle\n"
"   :type tri_p1: :class:`mathutils.Vector`\n"
"   :arg tri_p2: Second point of the triangle\n"
"   :type tri_p2: :class:`mathutils.Vector`\n"
"   :arg tri_p3: Third point of the triangle\n"
"   :type tri_p3: :class:`mathutils.Vector`\n"
"   :rtype: int\n"
);
static PyObject *M_Geometry_intersect_point_tri_2d(PyObject *UNUSED(self), PyObject *args)
{
	const char *error_prefix = "intersect_point_tri_2d";
	PyObject *py_pt, *py_tri[3];
	float pt[2], tri[3][2];
	int i;

	if (!PyArg_ParseTuple(
	        args, "OOOO:intersect_point_tri_2d",
	        &py_pt, UNPACK3_EX(&, py_tri, )))
	{
		return NULL;
	}

	if (mathutils_array_parse(pt, 2, 2 | MU_ARRAY_SPILL, py_pt, error_prefix) == -1) {
		return NULL;
	}
	for (i = 0; i < ARRAY_SIZE(tri); i++) {
		if (mathutils_array_parse(tri[i], 2, 2 | MU_ARRAY_SPILL, py_tri[i], error_prefix) == -1) {
			return NULL;
		}
	}

	return PyLong_FromLong(isect_point_tri_v2(pt, UNPACK3(tri)));
}

PyDoc_STRVAR(M_Geometry_intersect_point_quad_2d_doc,
".. function:: intersect_point_quad_2d(pt, quad_p1, quad_p2, quad_p3, quad_p4)\n"
"\n"
"   Takes 5 vectors (using only the x and y coordinates): one is the point and the next 4 define the quad, \n"
"   only the x and y are used from the vectors. Returns 1 if the point is within the quad, otherwise 0.\n"
"   Works only with convex quads without singular edges."
"\n"
"   :arg pt: Point\n"
"   :type pt: :class:`mathutils.Vector`\n"
"   :arg quad_p1: First point of the quad\n"
"   :type quad_p1: :class:`mathutils.Vector`\n"
"   :arg quad_p2: Second point of the quad\n"
"   :type quad_p2: :class:`mathutils.Vector`\n"
"   :arg quad_p3: Third point of the quad\n"
"   :type quad_p3: :class:`mathutils.Vector`\n"
"   :arg quad_p4: Forth point of the quad\n"
"   :type quad_p4: :class:`mathutils.Vector`\n"
"   :rtype: int\n"
);
static PyObject *M_Geometry_intersect_point_quad_2d(PyObject *UNUSED(self), PyObject *args)
{
	const char *error_prefix = "intersect_point_quad_2d";
	PyObject *py_pt, *py_quad[4];
	float pt[2], quad[4][2];
	int i;
	
	if (!PyArg_ParseTuple(
	        args, "OOOOO:intersect_point_quad_2d",
	        &py_pt, UNPACK4_EX(&, py_quad, )))
	{
		return NULL;
	}

	if (mathutils_array_parse(pt, 2, 2 | MU_ARRAY_SPILL, py_pt, error_prefix) == -1) {
		return NULL;
	}
	for (i = 0; i < ARRAY_SIZE(quad); i++) {
		if (mathutils_array_parse(quad[i], 2, 2 | MU_ARRAY_SPILL, py_quad[i], error_prefix) == -1) {
			return NULL;
		}
	}

	return PyLong_FromLong(isect_point_quad_v2(pt, UNPACK4(quad)));
}

PyDoc_STRVAR(M_Geometry_distance_point_to_plane_doc,
".. function:: distance_point_to_plane(pt, plane_co, plane_no)\n"
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
"   :rtype: float\n"
);
static PyObject *M_Geometry_distance_point_to_plane(PyObject *UNUSED(self), PyObject *args)
{
	const char *error_prefix = "distance_point_to_plane";
	PyObject *py_pt, *py_plane_co, *py_plane_no;
	float pt[3], plane_co[3], plane_no[3];
	float plane[4];

	if (!PyArg_ParseTuple(
	        args, "OOO:distance_point_to_plane",
	        &py_pt, &py_plane_co, &py_plane_no))
	{
		return NULL;
	}

	if (((mathutils_array_parse(pt,       3, 3 | MU_ARRAY_SPILL, py_pt,       error_prefix) != -1) &&
	     (mathutils_array_parse(plane_co, 3, 3 | MU_ARRAY_SPILL, py_plane_co, error_prefix) != -1) &&
	     (mathutils_array_parse(plane_no, 3, 3 | MU_ARRAY_SPILL, py_plane_no, error_prefix) != -1)) == 0)
	{
		return NULL;
	}

	plane_from_point_normal_v3(plane, plane_co, plane_no);
	return PyFloat_FromDouble(dist_signed_to_plane_v3(pt, plane));
}

PyDoc_STRVAR(M_Geometry_barycentric_transform_doc,
".. function:: barycentric_transform(point, tri_a1, tri_a2, tri_a3, tri_b1, tri_b2, tri_b3)\n"
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
"   :arg tri_a1: target triangle vertex.\n"
"   :type tri_a1: :class:`mathutils.Vector`\n"
"   :arg tri_a2: target triangle vertex.\n"
"   :type tri_a2: :class:`mathutils.Vector`\n"
"   :arg tri_a3: target triangle vertex.\n"
"   :type tri_a3: :class:`mathutils.Vector`\n"
"   :return: The transformed point\n"
"   :rtype: :class:`mathutils.Vector`'s\n"
);
static PyObject *M_Geometry_barycentric_transform(PyObject *UNUSED(self), PyObject *args)
{
	const char *error_prefix = "barycentric_transform";
	PyObject *py_pt_src, *py_tri_src[3], *py_tri_dst[3];
	float pt_src[3], pt_dst[3], tri_src[3][3], tri_dst[3][3];
	int i;

	if (!PyArg_ParseTuple(
	        args, "OOOOOOO:barycentric_transform",
	        &py_pt_src,
	        UNPACK3_EX(&, py_tri_src, ),
	        UNPACK3_EX(&, py_tri_dst, )))
	{
		return NULL;
	}

	if (mathutils_array_parse(pt_src, 3, 3 | MU_ARRAY_SPILL, py_pt_src, error_prefix) == -1) {
		return NULL;
	}
	for (i = 0; i < ARRAY_SIZE(tri_src); i++) {
		if (((mathutils_array_parse(tri_src[i], 3, 3 | MU_ARRAY_SPILL, py_tri_src[i], error_prefix) != -1) &&
		     (mathutils_array_parse(tri_dst[i], 3, 3 | MU_ARRAY_SPILL, py_tri_dst[i], error_prefix) != -1)) == 0)
		{
			return NULL;
		}
	}

	transform_point_by_tri_v3(
	        pt_dst, pt_src,
	        UNPACK3(tri_dst),
	        UNPACK3(tri_src));

	return Vector_CreatePyObject(pt_dst, 3, NULL);
}

PyDoc_STRVAR(M_Geometry_points_in_planes_doc,
".. function:: points_in_planes(planes)\n"
"\n"
"   Returns a list of points inside all planes given and a list of index values for the planes used.\n"
"\n"
"   :arg planes: List of planes (4D vectors).\n"
"   :type planes: list of :class:`mathutils.Vector`\n"
"   :return: two lists, once containing the vertices inside the planes, another containing the plane indices used\n"
"   :rtype: pair of lists\n"
);
/* note: this function could be optimized by some spatial structure */
static PyObject *M_Geometry_points_in_planes(PyObject *UNUSED(self), PyObject *args)
{
	PyObject *py_planes;
	float (*planes)[4];
	unsigned int planes_len;

	if (!PyArg_ParseTuple(
	        args, "O:points_in_planes",
	        &py_planes))
	{
		return NULL;
	}

	if ((planes_len = mathutils_array_parse_alloc_v((float **)&planes, 4, py_planes, "points_in_planes")) == -1) {
		return NULL;
	}
	else {
		/* note, this could be refactored into plain C easy - py bits are noted */
		const float eps = 0.0001f;
		const unsigned int len = (unsigned int)planes_len;
		unsigned int i, j, k, l;

		float n1n2[3], n2n3[3], n3n1[3];
		float potentialVertex[3];
		char *planes_used = PyMem_Malloc(sizeof(char) * len);

		/* python */
		PyObject *py_verts = PyList_New(0);
		PyObject *py_plane_index = PyList_New(0);

		memset(planes_used, 0, sizeof(char) * len);

		for (i = 0; i < len; i++) {
			const float *N1 = planes[i];
			for (j = i + 1; j < len; j++) {
				const float *N2 = planes[j];
				cross_v3_v3v3(n1n2, N1, N2);
				if (len_squared_v3(n1n2) > eps) {
					for (k = j + 1; k < len; k++) {
						const float *N3 = planes[k];
						cross_v3_v3v3(n2n3, N2, N3);
						if (len_squared_v3(n2n3) > eps) {
							cross_v3_v3v3(n3n1, N3, N1);
							if (len_squared_v3(n3n1) > eps) {
								const float quotient = dot_v3v3(N1, n2n3);
								if (fabsf(quotient) > eps) {
									/* potentialVertex = (n2n3 * N1[3] + n3n1 * N2[3] + n1n2 * N3[3]) * (-1.0 / quotient); */
									const float quotient_ninv = -1.0f / quotient;
									potentialVertex[0] = ((n2n3[0] * N1[3]) + (n3n1[0] * N2[3]) + (n1n2[0] * N3[3])) * quotient_ninv;
									potentialVertex[1] = ((n2n3[1] * N1[3]) + (n3n1[1] * N2[3]) + (n1n2[1] * N3[3])) * quotient_ninv;
									potentialVertex[2] = ((n2n3[2] * N1[3]) + (n3n1[2] * N2[3]) + (n1n2[2] * N3[3])) * quotient_ninv;
									for (l = 0; l < len; l++) {
										const float *NP = planes[l];
										if ((dot_v3v3(NP, potentialVertex) + NP[3]) > 0.000001f) {
											break;
										}
									}

									if (l == len) { /* ok */
										/* python */
										PyList_APPEND(py_verts, Vector_CreatePyObject(potentialVertex, 3, NULL));
										planes_used[i] = planes_used[j] = planes_used[k] = true;
									}
								}
							}
						}
					}
				}
			}
		}

		PyMem_Free(planes);

		/* now make a list of used planes */
		for (i = 0; i < len; i++) {
			if (planes_used[i]) {
				PyList_APPEND(py_plane_index, PyLong_FromLong(i));
			}
		}
		PyMem_Free(planes_used);

		{
			PyObject *ret = PyTuple_New(2);
			PyTuple_SET_ITEMS(ret,
			        py_verts,
			        py_plane_index);
			return ret;
		}
	}
}

#ifndef MATH_STANDALONE

PyDoc_STRVAR(M_Geometry_interpolate_bezier_doc,
".. function:: interpolate_bezier(knot1, handle1, handle2, knot2, resolution)\n"
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
"   :return: The interpolated points\n"
"   :rtype: list of :class:`mathutils.Vector`'s\n"
);
static PyObject *M_Geometry_interpolate_bezier(PyObject *UNUSED(self), PyObject *args)
{
	const char *error_prefix = "interpolate_bezier";
	PyObject *py_data[4];
	float data[4][4] = {{0.0f}};
	int resolu;
	int dims = 0;
	int i;
	float *coord_array, *fp;
	PyObject *list;

	if (!PyArg_ParseTuple(
	        args, "OOOOi:interpolate_bezier",
	        UNPACK4_EX(&, py_data, ), &resolu))
	{
		return NULL;
	}

	for (i = 0; i < 4; i++) {
		int dims_tmp;
		if ((((dims_tmp = mathutils_array_parse(data[i], 2, 2 | MU_ARRAY_SPILL, py_data[i], error_prefix)) == -1))) {
			return NULL;
		}
		dims = max_ii(dims, dims_tmp);
	}

	if (resolu <= 1) {
		PyErr_SetString(PyExc_ValueError,
		                "resolution must be 2 or over");
		return NULL;
	}

	coord_array = MEM_callocN(dims * (resolu) * sizeof(float), error_prefix);
	for (i = 0; i < dims; i++) {
		BKE_curve_forward_diff_bezier(UNPACK4_EX(, data, [i]), coord_array + i, resolu - 1, sizeof(float) * dims);
	}

	list = PyList_New(resolu);
	fp = coord_array;
	for (i = 0; i < resolu; i++, fp = fp + dims) {
		PyList_SET_ITEM(list, i, Vector_CreatePyObject(fp, dims, NULL));
	}
	MEM_freeN(coord_array);
	return list;
}


PyDoc_STRVAR(M_Geometry_tessellate_polygon_doc,
".. function:: tessellate_polygon(veclist_list)\n"
"\n"
"   Takes a list of polylines (each point a vector) and returns the point indices for a polyline filled with triangles.\n"
"\n"
"   :arg veclist_list: list of polylines\n"
"   :rtype: list\n"
);
/* PolyFill function, uses Blenders scanfill to fill multiple poly lines */
static PyObject *M_Geometry_tessellate_polygon(PyObject *UNUSED(self), PyObject *polyLineSeq)
{
	PyObject *tri_list; /*return this list of tri's */
	PyObject *polyLine, *polyVec;
	int i, len_polylines, len_polypoints, ls_error = 0;

	/* display listbase */
	ListBase dispbase = {NULL, NULL};
	DispList *dl;
	float *fp; /*pointer to the array of malloced dl->verts to set the points from the vectors */
	int index, *dl_face, totpoints = 0;

	if (!PySequence_Check(polyLineSeq)) {
		PyErr_SetString(PyExc_TypeError,
		                "expected a sequence of poly lines");
		return NULL;
	}

	len_polylines = PySequence_Size(polyLineSeq);

	for (i = 0; i < len_polylines; i++) {
		polyLine = PySequence_GetItem(polyLineSeq, i);
		if (!PySequence_Check(polyLine)) {
			BKE_displist_free(&dispbase);
			Py_XDECREF(polyLine); /* may be null so use Py_XDECREF*/
			PyErr_SetString(PyExc_TypeError,
			                "One or more of the polylines is not a sequence of mathutils.Vector's");
			return NULL;
		}

		len_polypoints = PySequence_Size(polyLine);
		if (len_polypoints > 0) { /* don't bother adding edges as polylines */
#if 0
			if (EXPP_check_sequence_consistency(polyLine, &vector_Type) != 1) {
				freedisplist(&dispbase);
				Py_DECREF(polyLine);
				PyErr_SetString(PyExc_TypeError,
				                "A point in one of the polylines is not a mathutils.Vector type");
				return NULL;
			}
#endif
			dl = MEM_callocN(sizeof(DispList), "poly disp");
			BLI_addtail(&dispbase, dl);
			dl->type = DL_INDEX3;
			dl->nr = len_polypoints;
			dl->type = DL_POLY;
			dl->parts = 1; /* no faces, 1 edge loop */
			dl->col = 0; /* no material */
			dl->verts = fp = MEM_callocN(sizeof(float) * 3 * len_polypoints, "dl verts");
			dl->index = MEM_callocN(sizeof(int) * 3 * len_polypoints, "dl index");

			for (index = 0; index < len_polypoints; index++, fp += 3) {
				polyVec = PySequence_GetItem(polyLine, index);
				if (VectorObject_Check(polyVec)) {

					if (BaseMath_ReadCallback((VectorObject *)polyVec) == -1)
						ls_error = 1;

					fp[0] = ((VectorObject *)polyVec)->vec[0];
					fp[1] = ((VectorObject *)polyVec)->vec[1];
					if (((VectorObject *)polyVec)->size > 2)
						fp[2] = ((VectorObject *)polyVec)->vec[2];
					else
						fp[2] = 0.0f;  /* if its a 2d vector then set the z to be zero */
				}
				else {
					ls_error = 1;
				}

				totpoints++;
				Py_DECREF(polyVec);
			}
		}
		Py_DECREF(polyLine);
	}

	if (ls_error) {
		BKE_displist_free(&dispbase); /* possible some dl was allocated */
		PyErr_SetString(PyExc_TypeError,
		                "A point in one of the polylines "
		                "is not a mathutils.Vector type");
		return NULL;
	}
	else if (totpoints) {
		/* now make the list to return */
		/* TODO, add normal arg */
		BKE_displist_fill(&dispbase, &dispbase, NULL, false);

		/* The faces are stored in a new DisplayList
		 * thats added to the head of the listbase */
		dl = dispbase.first;

		tri_list = PyList_New(dl->parts);
		if (!tri_list) {
			BKE_displist_free(&dispbase);
			PyErr_SetString(PyExc_RuntimeError,
			                "failed to make a new list");
			return NULL;
		}

		index = 0;
		dl_face = dl->index;
		while (index < dl->parts) {
			PyList_SET_ITEM(tri_list, index, Py_BuildValue("iii", dl_face[0], dl_face[1], dl_face[2]));
			dl_face += 3;
			index++;
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


static int boxPack_FromPyObject(PyObject *value, BoxPack **boxarray)
{
	Py_ssize_t len, i;
	PyObject *list_item, *item_1, *item_2;
	BoxPack *box;


	/* Error checking must already be done */
	if (!PyList_Check(value)) {
		PyErr_SetString(PyExc_TypeError,
		                "can only back a list of [x, y, w, h]");
		return -1;
	}

	len = PyList_GET_SIZE(value);

	*boxarray = MEM_mallocN(len * sizeof(BoxPack), "BoxPack box");


	for (i = 0; i < len; i++) {
		list_item = PyList_GET_ITEM(value, i);
		if (!PyList_Check(list_item) || PyList_GET_SIZE(list_item) < 4) {
			MEM_freeN(*boxarray);
			PyErr_SetString(PyExc_TypeError,
			                "can only pack a list of [x, y, w, h]");
			return -1;
		}

		box = (*boxarray) + i;

		item_1 = PyList_GET_ITEM(list_item, 2);
		item_2 = PyList_GET_ITEM(list_item, 3);

		box->w =  (float)PyFloat_AsDouble(item_1);
		box->h =  (float)PyFloat_AsDouble(item_2);
		box->index = i;

		/* accounts for error case too and overwrites with own error */
		if (box->w < 0.0f || box->h < 0.0f) {
			MEM_freeN(*boxarray);
			PyErr_SetString(PyExc_TypeError,
			                "error parsing width and height values from list: "
			                "[x, y, w, h], not numbers or below zero");
			return -1;
		}

		/* verts will be added later */
	}
	return 0;
}

static void boxPack_ToPyObject(PyObject *value, BoxPack **boxarray)
{
	Py_ssize_t len, i;
	PyObject *list_item;
	BoxPack *box;

	len = PyList_GET_SIZE(value);

	for (i = 0; i < len; i++) {
		box = (*boxarray) + i;
		list_item = PyList_GET_ITEM(value, box->index);
		PyList_SET_ITEM(list_item, 0, PyFloat_FromDouble(box->x));
		PyList_SET_ITEM(list_item, 1, PyFloat_FromDouble(box->y));
	}
	MEM_freeN(*boxarray);
}

PyDoc_STRVAR(M_Geometry_box_pack_2d_doc,
".. function:: box_pack_2d(boxes)\n"
"\n"
"   Returns the normal of the 3D tri or quad.\n"
"\n"
"   :arg boxes: list of boxes, each box is a list where the first 4 items are [x, y, width, height, ...] other items are ignored.\n"
"   :type boxes: list\n"
"   :return: the width and height of the packed bounding box\n"
"   :rtype: tuple, pair of floats\n"
);
static PyObject *M_Geometry_box_pack_2d(PyObject *UNUSED(self), PyObject *boxlist)
{
	float tot_width = 0.0f, tot_height = 0.0f;
	Py_ssize_t len;

	PyObject *ret;

	if (!PyList_Check(boxlist)) {
		PyErr_SetString(PyExc_TypeError,
		                "expected a list of boxes [[x, y, w, h], ... ]");
		return NULL;
	}

	len = PyList_GET_SIZE(boxlist);
	if (len) {
		BoxPack *boxarray = NULL;
		if (boxPack_FromPyObject(boxlist, &boxarray) == -1) {
			return NULL; /* exception set */
		}

		/* Non Python function */
		BLI_box_pack_2d(boxarray, len, &tot_width, &tot_height);

		boxPack_ToPyObject(boxlist, &boxarray);
	}

	ret = PyTuple_New(2);
	PyTuple_SET_ITEMS(ret,
	        PyFloat_FromDouble(tot_width),
	        PyFloat_FromDouble(tot_height));
	return ret;
}

PyDoc_STRVAR(M_Geometry_box_fit_2d_doc,
".. function:: box_fit_2d(points)\n"
"\n"
"   Returns an angle that best fits the points to an axis aligned rectangle\n"
"\n"
"   :arg points: list of 2d points.\n"
"   :type points: list\n"
"   :return: angle\n"
"   :rtype: float\n"
);
static PyObject *M_Geometry_box_fit_2d(PyObject *UNUSED(self), PyObject *pointlist)
{
	float (*points)[2];
	Py_ssize_t len;

	float angle = 0.0f;

	len = mathutils_array_parse_alloc_v(((float **)&points), 2, pointlist, "box_fit_2d");
	if (len == -1) {
		return NULL;
	}

	if (len) {
		/* Non Python function */
		angle = BLI_convexhull_aabb_fit_points_2d((const float (*)[2])points, len);

		PyMem_Free(points);
	}


	return PyFloat_FromDouble(angle);
}

PyDoc_STRVAR(M_Geometry_convex_hull_2d_doc,
".. function:: convex_hull_2d(points)\n"
"\n"
"   Returns a list of indices into the list given\n"
"\n"
"   :arg points: list of 2d points.\n"
"   :type points: list\n"
"   :return: a list of indices\n"
"   :rtype: list of ints\n"
);
static PyObject *M_Geometry_convex_hull_2d(PyObject *UNUSED(self), PyObject *pointlist)
{
	float (*points)[2];
	Py_ssize_t len;

	PyObject *ret;

	len = mathutils_array_parse_alloc_v(((float **)&points), 2, pointlist, "convex_hull_2d");
	if (len == -1) {
		return NULL;
	}

	if (len) {
		int *index_map;
		Py_ssize_t len_ret, i;

		index_map  = MEM_mallocN(sizeof(*index_map) * len * 2, __func__);

		/* Non Python function */
		len_ret = BLI_convexhull_2d((const float (*)[2])points, len, index_map);

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

#endif /* MATH_STANDALONE */


static PyMethodDef M_Geometry_methods[] = {
	{"intersect_ray_tri", (PyCFunction) M_Geometry_intersect_ray_tri, METH_VARARGS, M_Geometry_intersect_ray_tri_doc},
	{"intersect_point_line", (PyCFunction) M_Geometry_intersect_point_line, METH_VARARGS, M_Geometry_intersect_point_line_doc},
	{"intersect_point_tri", (PyCFunction) M_Geometry_intersect_point_tri, METH_VARARGS, M_Geometry_intersect_point_tri_doc},
	{"intersect_point_tri_2d", (PyCFunction) M_Geometry_intersect_point_tri_2d, METH_VARARGS, M_Geometry_intersect_point_tri_2d_doc},
	{"intersect_point_quad_2d", (PyCFunction) M_Geometry_intersect_point_quad_2d, METH_VARARGS, M_Geometry_intersect_point_quad_2d_doc},
	{"intersect_line_line", (PyCFunction) M_Geometry_intersect_line_line, METH_VARARGS, M_Geometry_intersect_line_line_doc},
	{"intersect_line_line_2d", (PyCFunction) M_Geometry_intersect_line_line_2d, METH_VARARGS, M_Geometry_intersect_line_line_2d_doc},
	{"intersect_line_plane", (PyCFunction) M_Geometry_intersect_line_plane, METH_VARARGS, M_Geometry_intersect_line_plane_doc},
	{"intersect_plane_plane", (PyCFunction) M_Geometry_intersect_plane_plane, METH_VARARGS, M_Geometry_intersect_plane_plane_doc},
	{"intersect_line_sphere", (PyCFunction) M_Geometry_intersect_line_sphere, METH_VARARGS, M_Geometry_intersect_line_sphere_doc},
	{"intersect_line_sphere_2d", (PyCFunction) M_Geometry_intersect_line_sphere_2d, METH_VARARGS, M_Geometry_intersect_line_sphere_2d_doc},
	{"distance_point_to_plane", (PyCFunction) M_Geometry_distance_point_to_plane, METH_VARARGS, M_Geometry_distance_point_to_plane_doc},
	{"intersect_sphere_sphere_2d", (PyCFunction) M_Geometry_intersect_sphere_sphere_2d, METH_VARARGS, M_Geometry_intersect_sphere_sphere_2d_doc},
	{"area_tri", (PyCFunction) M_Geometry_area_tri, METH_VARARGS, M_Geometry_area_tri_doc},
	{"volume_tetrahedron", (PyCFunction) M_Geometry_volume_tetrahedron, METH_VARARGS, M_Geometry_volume_tetrahedron_doc},
	{"normal", (PyCFunction) M_Geometry_normal, METH_VARARGS, M_Geometry_normal_doc},
	{"barycentric_transform", (PyCFunction) M_Geometry_barycentric_transform, METH_VARARGS, M_Geometry_barycentric_transform_doc},
	{"points_in_planes", (PyCFunction) M_Geometry_points_in_planes, METH_VARARGS, M_Geometry_points_in_planes_doc},
#ifndef MATH_STANDALONE
	{"interpolate_bezier", (PyCFunction) M_Geometry_interpolate_bezier, METH_VARARGS, M_Geometry_interpolate_bezier_doc},
	{"tessellate_polygon", (PyCFunction) M_Geometry_tessellate_polygon, METH_O, M_Geometry_tessellate_polygon_doc},
	{"convex_hull_2d", (PyCFunction) M_Geometry_convex_hull_2d, METH_O, M_Geometry_convex_hull_2d_doc},
	{"box_fit_2d", (PyCFunction) M_Geometry_box_fit_2d, METH_O, M_Geometry_box_fit_2d_doc},
	{"box_pack_2d", (PyCFunction) M_Geometry_box_pack_2d, METH_O, M_Geometry_box_pack_2d_doc},
#endif
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef M_Geometry_module_def = {
	PyModuleDef_HEAD_INIT,
	"mathutils.geometry",  /* m_name */
	M_Geometry_doc,  /* m_doc */
	0,  /* m_size */
	M_Geometry_methods,  /* m_methods */
	NULL,  /* m_reload */
	NULL,  /* m_traverse */
	NULL,  /* m_clear */
	NULL,  /* m_free */
};

/*----------------------------MODULE INIT-------------------------*/
PyMODINIT_FUNC PyInit_mathutils_geometry(void)
{
	PyObject *submodule = PyModule_Create(&M_Geometry_module_def);
	return submodule;
}
