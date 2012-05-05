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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Joseph Gilbert, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/mathutils/mathutils_geometry.c
 *  \ingroup pymathutils
 */


#include <Python.h>

#include "mathutils_geometry.h"

/* Used for PolyFill */
#ifndef MATH_STANDALONE /* define when building outside blender */
#  include "MEM_guardedalloc.h"
#  include "BLI_blenlib.h"
#  include "BLI_boxpack2d.h"
#  include "BKE_displist.h"
#  include "BKE_curve.h"
#endif

#include "BLI_math.h"
#include "BLI_utildefines.h"

#define SWAP_FLOAT(a, b, tmp) tmp = a; a = b; b = tmp

/*-------------------------DOC STRINGS ---------------------------*/
PyDoc_STRVAR(M_Geometry_doc,
"The Blender geometry module"
);

//---------------------------------INTERSECTION FUNCTIONS--------------------

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
	VectorObject *ray, *ray_off, *vec1, *vec2, *vec3;
	float dir[3], orig[3], v1[3], v2[3], v3[3], e1[3], e2[3], pvec[3], tvec[3], qvec[3];
	float det, inv_det, u, v, t;
	int clip = 1;

	if (!PyArg_ParseTuple(args,
	                      "O!O!O!O!O!|i:intersect_ray_tri",
	                      &vector_Type, &vec1,
	                      &vector_Type, &vec2,
	                      &vector_Type, &vec3,
	                      &vector_Type, &ray,
	                      &vector_Type, &ray_off, &clip))
	{
		return NULL;
	}
	if (vec1->size != 3 || vec2->size != 3 || vec3->size != 3 || ray->size != 3 || ray_off->size != 3) {
		PyErr_SetString(PyExc_ValueError,
		                "only 3D vectors for all parameters");
		return NULL;
	}

	if (BaseMath_ReadCallback(vec1) == -1 ||
	    BaseMath_ReadCallback(vec2) == -1 ||
	    BaseMath_ReadCallback(vec3) == -1 ||
	    BaseMath_ReadCallback(ray)  == -1 ||
	    BaseMath_ReadCallback(ray_off) == -1)
	{
		return NULL;
	}

	copy_v3_v3(v1, vec1->vec);
	copy_v3_v3(v2, vec2->vec);
	copy_v3_v3(v3, vec3->vec);

	copy_v3_v3(dir, ray->vec);
	normalize_v3(dir);

	copy_v3_v3(orig, ray_off->vec);

	/* find vectors for two edges sharing v1 */
	sub_v3_v3v3(e1, v2, v1);
	sub_v3_v3v3(e2, v3, v1);

	/* begin calculating determinant - also used to calculated U parameter */
	cross_v3_v3v3(pvec, dir, e2);

	/* if determinant is near zero, ray lies in plane of triangle */
	det = dot_v3v3(e1, pvec);

	if (det > -0.000001f && det < 0.000001f) {
		Py_RETURN_NONE;
	}

	inv_det = 1.0f / det;

	/* calculate distance from v1 to ray origin */
	sub_v3_v3v3(tvec, orig, v1);

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

	mul_v3_fl(dir, t);
	add_v3_v3v3(pvec, orig, dir);

	return Vector_CreatePyObject(pvec, 3, Py_NEW, NULL);
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
	PyObject *tuple;
	VectorObject *vec1, *vec2, *vec3, *vec4;
	float v1[3], v2[3], v3[3], v4[3], i1[3], i2[3];

	if (!PyArg_ParseTuple(args, "O!O!O!O!:intersect_line_line",
	                      &vector_Type, &vec1,
	                      &vector_Type, &vec2,
	                      &vector_Type, &vec3,
	                      &vector_Type, &vec4))
	{
		return NULL;
	}

	if (vec1->size != vec2->size || vec1->size != vec3->size || vec3->size != vec2->size) {
		PyErr_SetString(PyExc_ValueError,
		                "vectors must be of the same size");
		return NULL;
	}

	if (BaseMath_ReadCallback(vec1) == -1 ||
	    BaseMath_ReadCallback(vec2) == -1 ||
	    BaseMath_ReadCallback(vec3) == -1 ||
	    BaseMath_ReadCallback(vec4) == -1)
	{
		return NULL;
	}

	if (vec1->size == 3 || vec1->size == 2) {
		int result;

		if (vec1->size == 3) {
			copy_v3_v3(v1, vec1->vec);
			copy_v3_v3(v2, vec2->vec);
			copy_v3_v3(v3, vec3->vec);
			copy_v3_v3(v4, vec4->vec);
		}
		else {
			v1[0] = vec1->vec[0];
			v1[1] = vec1->vec[1];
			v1[2] = 0.0f;

			v2[0] = vec2->vec[0];
			v2[1] = vec2->vec[1];
			v2[2] = 0.0f;

			v3[0] = vec3->vec[0];
			v3[1] = vec3->vec[1];
			v3[2] = 0.0f;

			v4[0] = vec4->vec[0];
			v4[1] = vec4->vec[1];
			v4[2] = 0.0f;
		}

		result = isect_line_line_v3(v1, v2, v3, v4, i1, i2);

		if (result == 0) {
			/* colinear */
			Py_RETURN_NONE;
		}
		else {
			tuple = PyTuple_New(2);
			PyTuple_SET_ITEM(tuple, 0, Vector_CreatePyObject(i1, vec1->size, Py_NEW, NULL));
			PyTuple_SET_ITEM(tuple, 1, Vector_CreatePyObject(i2, vec1->size, Py_NEW, NULL));
			return tuple;
		}
	}
	else {
		PyErr_SetString(PyExc_ValueError,
		                "2D/3D vectors only");
		return NULL;
	}
}




//----------------------------geometry.normal() -------------------
PyDoc_STRVAR(M_Geometry_normal_doc,
".. function:: normal(v1, v2, v3, v4=None)\n"
"\n"
"   Returns the normal of the 3D tri or quad.\n"
"\n"
"   :arg v1: Point1\n"
"   :type v1: :class:`mathutils.Vector`\n"
"   :arg v2: Point2\n"
"   :type v2: :class:`mathutils.Vector`\n"
"   :arg v3: Point3\n"
"   :type v3: :class:`mathutils.Vector`\n"
"   :arg v4: Point4 (optional)\n"
"   :type v4: :class:`mathutils.Vector`\n"
"   :rtype: :class:`mathutils.Vector`\n"
);
static PyObject *M_Geometry_normal(PyObject *UNUSED(self), PyObject *args)
{
	VectorObject *vec1, *vec2, *vec3, *vec4;
	float n[3];

	if (PyTuple_GET_SIZE(args) == 3) {
		if (!PyArg_ParseTuple(args, "O!O!O!:normal",
		                      &vector_Type, &vec1,
		                      &vector_Type, &vec2,
		                      &vector_Type, &vec3))
		{
			return NULL;
		}

		if (vec1->size != vec2->size || vec1->size != vec3->size) {
			PyErr_SetString(PyExc_ValueError,
			                "vectors must be of the same size");
			return NULL;
		}
		if (vec1->size < 3) {
			PyErr_SetString(PyExc_ValueError,
			                "2D vectors unsupported");
			return NULL;
		}

		if (BaseMath_ReadCallback(vec1) == -1 ||
		    BaseMath_ReadCallback(vec2) == -1 ||
		    BaseMath_ReadCallback(vec3) == -1)
		{
			return NULL;
		}

		normal_tri_v3(n, vec1->vec, vec2->vec, vec3->vec);
	}
	else {
		if (!PyArg_ParseTuple(args, "O!O!O!O!:normal",
		                      &vector_Type, &vec1,
		                      &vector_Type, &vec2,
		                      &vector_Type, &vec3,
		                      &vector_Type, &vec4))
		{
			return NULL;
		}
		if (vec1->size != vec2->size || vec1->size != vec3->size || vec1->size != vec4->size) {
			PyErr_SetString(PyExc_ValueError,
			                "vectors must be of the same size");
			return NULL;
		}
		if (vec1->size < 3) {
			PyErr_SetString(PyExc_ValueError,
			                "2D vectors unsupported");
			return NULL;
		}

		if (BaseMath_ReadCallback(vec1) == -1 ||
		    BaseMath_ReadCallback(vec2) == -1 ||
		    BaseMath_ReadCallback(vec3) == -1 ||
		    BaseMath_ReadCallback(vec4) == -1)
		{
			return NULL;
		}

		normal_quad_v3(n, vec1->vec, vec2->vec, vec3->vec, vec4->vec);
	}

	return Vector_CreatePyObject(n, 3, Py_NEW, NULL);
}

//--------------------------------- AREA FUNCTIONS--------------------

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
	VectorObject *vec1, *vec2, *vec3;

	if (!PyArg_ParseTuple(args, "O!O!O!:area_tri",
	                      &vector_Type, &vec1,
	                      &vector_Type, &vec2,
	                      &vector_Type, &vec3))
	{
		return NULL;
	}

	if (vec1->size != vec2->size || vec1->size != vec3->size) {
		PyErr_SetString(PyExc_ValueError,
		                "vectors must be of the same size");
		return NULL;
	}

	if (BaseMath_ReadCallback(vec1) == -1 ||
	    BaseMath_ReadCallback(vec2) == -1 ||
	    BaseMath_ReadCallback(vec3) == -1)
	{
		return NULL;
	}

	if (vec1->size == 3) {
		return PyFloat_FromDouble(area_tri_v3(vec1->vec, vec2->vec, vec3->vec));
	}
	else if (vec1->size == 2) {
		return PyFloat_FromDouble(area_tri_v2(vec1->vec, vec2->vec, vec3->vec));
	}
	else {
		PyErr_SetString(PyExc_ValueError,
		                "only 2D,3D vectors are supported");
		return NULL;
	}
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
	VectorObject *line_a1, *line_a2, *line_b1, *line_b2;
	float vi[2];
	if (!PyArg_ParseTuple(args, "O!O!O!O!:intersect_line_line_2d",
	                      &vector_Type, &line_a1,
	                      &vector_Type, &line_a2,
	                      &vector_Type, &line_b1,
	                      &vector_Type, &line_b2))
	{
		return NULL;
	}
	
	if (BaseMath_ReadCallback(line_a1) == -1 ||
	    BaseMath_ReadCallback(line_a2) == -1 ||
	    BaseMath_ReadCallback(line_b1) == -1 ||
	    BaseMath_ReadCallback(line_b2) == -1)
	{
		return NULL;
	}

	if (isect_seg_seg_v2_point(line_a1->vec, line_a2->vec, line_b1->vec, line_b2->vec, vi) == 1) {
		return Vector_CreatePyObject(vi, 2, Py_NEW, NULL);
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
"   :arg no_flip: Always return an intersection on the directon defined bt line_a -> line_b\n"
"   :type no_flip: :boolean\n"
"   :return: The point of intersection or None when not found\n"
"   :rtype: :class:`mathutils.Vector` or None\n"
);
static PyObject *M_Geometry_intersect_line_plane(PyObject *UNUSED(self), PyObject *args)
{
	VectorObject *line_a, *line_b, *plane_co, *plane_no;
	int no_flip = 0;
	float isect[3];
	if (!PyArg_ParseTuple(args, "O!O!O!O!|i:intersect_line_plane",
	                      &vector_Type, &line_a,
	                      &vector_Type, &line_b,
	                      &vector_Type, &plane_co,
	                      &vector_Type, &plane_no,
	                      &no_flip))
	{
		return NULL;
	}

	if (BaseMath_ReadCallback(line_a) == -1 ||
	    BaseMath_ReadCallback(line_b) == -1 ||
	    BaseMath_ReadCallback(plane_co) == -1 ||
	    BaseMath_ReadCallback(plane_no) == -1)
	{
		return NULL;
	}

	if (ELEM4(2, line_a->size, line_b->size, plane_co->size, plane_no->size)) {
		PyErr_SetString(PyExc_ValueError,
		                "geometry.intersect_line_plane(...): "
		                " can't use 2D Vectors");
		return NULL;
	}

	if (isect_line_plane_v3(isect, line_a->vec, line_b->vec, plane_co->vec, plane_no->vec, no_flip) == 1) {
		return Vector_CreatePyObject(isect, 3, Py_NEW, NULL);
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
"   :rtype: tuple pair of :class:`mathutils.Vector`\n"
);
static PyObject *M_Geometry_intersect_plane_plane(PyObject *UNUSED(self), PyObject *args)
{
	PyObject *ret;
	VectorObject *plane_a_co, *plane_a_no, *plane_b_co, *plane_b_no;

	float isect_co[3];
	float isect_no[3];

	if (!PyArg_ParseTuple(args, "O!O!O!O!|i:intersect_plane_plane",
	                      &vector_Type, &plane_a_co,
	                      &vector_Type, &plane_a_no,
	                      &vector_Type, &plane_b_co,
	                      &vector_Type, &plane_b_no))
	{
		return NULL;
	}

	if (BaseMath_ReadCallback(plane_a_co) == -1 ||
	    BaseMath_ReadCallback(plane_a_no) == -1 ||
	    BaseMath_ReadCallback(plane_b_co) == -1 ||
	    BaseMath_ReadCallback(plane_b_no) == -1)
	{
		return NULL;
	}

	if (ELEM4(2, plane_a_co->size, plane_a_no->size, plane_b_co->size, plane_b_no->size)) {
		PyErr_SetString(PyExc_ValueError,
		                "geometry.intersect_plane_plane(...): "
		                " can't use 2D Vectors");
		return NULL;
	}

	isect_plane_plane_v3(isect_co, isect_no,
	                     plane_a_co->vec, plane_a_no->vec,
	                     plane_b_co->vec, plane_b_no->vec);

	normalize_v3(isect_no);

	ret = PyTuple_New(2);
	PyTuple_SET_ITEM(ret, 0, Vector_CreatePyObject(isect_co, 3, Py_NEW, NULL));
	PyTuple_SET_ITEM(ret, 1, Vector_CreatePyObject(isect_no, 3, Py_NEW, NULL));
	return ret;
}

PyDoc_STRVAR(M_Geometry_intersect_line_sphere_doc,
".. function:: intersect_line_sphere(line_a, line_b, sphere_co, sphere_radius, clip=True)\n"
"\n"
"   Takes a lines (as 2 vectors), a sphere as a point and a radius and\n"
"   returns the intersection\n"
"\n"
"   :arg line_a: First point of the first line\n"
"   :type line_a: :class:`mathutils.Vector`\n"
"   :arg line_b: Second point of the first line\n"
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
	VectorObject *line_a, *line_b, *sphere_co;
	float sphere_radius;
	int clip = TRUE;

	float isect_a[3];
	float isect_b[3];

	if (!PyArg_ParseTuple(args, "O!O!O!f|i:intersect_line_sphere",
	                      &vector_Type, &line_a,
	                      &vector_Type, &line_b,
	                      &vector_Type, &sphere_co,
	                      &sphere_radius, &clip))
	{
		return NULL;
	}

	if (BaseMath_ReadCallback(line_a) == -1 ||
	    BaseMath_ReadCallback(line_b) == -1 ||
	    BaseMath_ReadCallback(sphere_co) == -1)
	{
		return NULL;
	}

	if (ELEM3(2, line_a->size, line_b->size, sphere_co->size)) {
		PyErr_SetString(PyExc_ValueError,
		                "geometry.intersect_line_sphere(...): "
		                " can't use 2D Vectors");
		return NULL;
	}
	else {
		short use_a = TRUE;
		short use_b = TRUE;
		float lambda;

		PyObject *ret = PyTuple_New(2);

		switch (isect_line_sphere_v3(line_a->vec, line_b->vec, sphere_co->vec, sphere_radius, isect_a, isect_b)) {
			case 1:
				if (!(!clip || (((lambda = line_point_factor_v3(isect_a, line_a->vec, line_b->vec)) >= 0.0f) && (lambda <= 1.0f)))) use_a = FALSE;
				use_b = FALSE;
				break;
			case 2:
				if (!(!clip || (((lambda = line_point_factor_v3(isect_a, line_a->vec, line_b->vec)) >= 0.0f) && (lambda <= 1.0f)))) use_a = FALSE;
				if (!(!clip || (((lambda = line_point_factor_v3(isect_b, line_a->vec, line_b->vec)) >= 0.0f) && (lambda <= 1.0f)))) use_b = FALSE;
				break;
			default:
				use_a = FALSE;
				use_b = FALSE;
		}

		if (use_a) { PyTuple_SET_ITEM(ret, 0,  Vector_CreatePyObject(isect_a, 3, Py_NEW, NULL)); }
		else       { PyTuple_SET_ITEM(ret, 0,  Py_None); Py_INCREF(Py_None); }

		if (use_b) { PyTuple_SET_ITEM(ret, 1,  Vector_CreatePyObject(isect_b, 3, Py_NEW, NULL)); }
		else       { PyTuple_SET_ITEM(ret, 1,  Py_None); Py_INCREF(Py_None); }

		return ret;
	}
}

/* keep in sync with M_Geometry_intersect_line_sphere */
PyDoc_STRVAR(M_Geometry_intersect_line_sphere_2d_doc,
".. function:: intersect_line_sphere_2d(line_a, line_b, sphere_co, sphere_radius, clip=True)\n"
"\n"
"   Takes a lines (as 2 vectors), a sphere as a point and a radius and\n"
"   returns the intersection\n"
"\n"
"   :arg line_a: First point of the first line\n"
"   :type line_a: :class:`mathutils.Vector`\n"
"   :arg line_b: Second point of the first line\n"
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
	VectorObject *line_a, *line_b, *sphere_co;
	float sphere_radius;
	int clip = TRUE;

	float isect_a[3];
	float isect_b[3];

	if (!PyArg_ParseTuple(args, "O!O!O!f|i:intersect_line_sphere_2d",
	                      &vector_Type, &line_a,
	                      &vector_Type, &line_b,
	                      &vector_Type, &sphere_co,
	                      &sphere_radius, &clip))
	{
		return NULL;
	}

	if (BaseMath_ReadCallback(line_a) == -1 ||
	    BaseMath_ReadCallback(line_b) == -1 ||
	    BaseMath_ReadCallback(sphere_co) == -1)
	{
		return NULL;
	}
	else {
		short use_a = TRUE;
		short use_b = TRUE;
		float lambda;

		PyObject *ret = PyTuple_New(2);

		switch (isect_line_sphere_v2(line_a->vec, line_b->vec, sphere_co->vec, sphere_radius, isect_a, isect_b)) {
			case 1:
				if (!(!clip || (((lambda = line_point_factor_v2(isect_a, line_a->vec, line_b->vec)) >= 0.0f) && (lambda <= 1.0f)))) use_a = FALSE;
				use_b = FALSE;
				break;
			case 2:
				if (!(!clip || (((lambda = line_point_factor_v2(isect_a, line_a->vec, line_b->vec)) >= 0.0f) && (lambda <= 1.0f)))) use_a = FALSE;
				if (!(!clip || (((lambda = line_point_factor_v2(isect_b, line_a->vec, line_b->vec)) >= 0.0f) && (lambda <= 1.0f)))) use_b = FALSE;
				break;
			default:
				use_a = FALSE;
				use_b = FALSE;
		}

		if (use_a) { PyTuple_SET_ITEM(ret, 0,  Vector_CreatePyObject(isect_a, 2, Py_NEW, NULL)); }
		else       { PyTuple_SET_ITEM(ret, 0,  Py_None); Py_INCREF(Py_None); }

		if (use_b) { PyTuple_SET_ITEM(ret, 1,  Vector_CreatePyObject(isect_b, 2, Py_NEW, NULL)); }
		else       { PyTuple_SET_ITEM(ret, 1,  Py_None); Py_INCREF(Py_None); }

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
	VectorObject *pt, *line_1, *line_2;
	float pt_in[3], pt_out[3], l1[3], l2[3];
	float lambda;
	PyObject *ret;
	
	if (!PyArg_ParseTuple(args, "O!O!O!:intersect_point_line",
	                      &vector_Type, &pt,
	                      &vector_Type, &line_1,
	                      &vector_Type, &line_2))
	{
		return NULL;
	}

	if (BaseMath_ReadCallback(pt) == -1 ||
	    BaseMath_ReadCallback(line_1) == -1 ||
	    BaseMath_ReadCallback(line_2) == -1)
	{
		return NULL;
	}

	/* accept 2d verts */
	if (pt->size == 3) {     copy_v3_v3(pt_in, pt->vec); }
	else { pt_in[2] = 0.0f;  copy_v2_v2(pt_in, pt->vec); }
	
	if (line_1->size == 3) { copy_v3_v3(l1, line_1->vec); }
	else { l1[2] = 0.0f;     copy_v2_v2(l1, line_1->vec); }
	
	if (line_2->size == 3) { copy_v3_v3(l2, line_2->vec); }
	else { l2[2] = 0.0f;     copy_v2_v2(l2, line_2->vec); }
	
	/* do the calculation */
	lambda = closest_to_line_v3(pt_out, pt_in, l1, l2);
	
	ret = PyTuple_New(2);
	PyTuple_SET_ITEM(ret, 0, Vector_CreatePyObject(pt_out, 3, Py_NEW, NULL));
	PyTuple_SET_ITEM(ret, 1, PyFloat_FromDouble(lambda));
	return ret;
}

PyDoc_STRVAR(M_Geometry_intersect_point_tri_2d_doc,
".. function:: intersect_point_tri_2d(pt, tri_p1, tri_p2, tri_p3)\n"
"\n"
"   Takes 4 vectors (using only the x and y coordinates): one is the point and the next 3 define the triangle. Returns 1 if the point is within the triangle, otherwise 0.\n"
"\n"
"   :arg pt: Point\n"
"   :type v1: :class:`mathutils.Vector`\n"
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
	VectorObject *pt_vec, *tri_p1, *tri_p2, *tri_p3;
	
	if (!PyArg_ParseTuple(args, "O!O!O!O!:intersect_point_tri_2d",
	                      &vector_Type, &pt_vec,
	                      &vector_Type, &tri_p1,
	                      &vector_Type, &tri_p2,
	                      &vector_Type, &tri_p3))
	{
		return NULL;
	}
	
	if (BaseMath_ReadCallback(pt_vec) == -1 ||
	    BaseMath_ReadCallback(tri_p1) == -1 ||
	    BaseMath_ReadCallback(tri_p2) == -1 ||
	    BaseMath_ReadCallback(tri_p3) == -1)
	{
		return NULL;
	}

	return PyLong_FromLong(isect_point_tri_v2(pt_vec->vec, tri_p1->vec, tri_p2->vec, tri_p3->vec));
}

PyDoc_STRVAR(M_Geometry_intersect_point_quad_2d_doc,
".. function:: intersect_point_quad_2d(pt, quad_p1, quad_p2, quad_p3, quad_p4)\n"
"\n"
"   Takes 5 vectors (using only the x and y coordinates): one is the point and the next 4 define the quad, \n"
"   only the x and y are used from the vectors. Returns 1 if the point is within the quad, otherwise 0.\n"
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
	VectorObject *pt_vec, *quad_p1, *quad_p2, *quad_p3, *quad_p4;
	
	if (!PyArg_ParseTuple(args, "O!O!O!O!O!:intersect_point_quad_2d",
	                      &vector_Type, &pt_vec,
	                      &vector_Type, &quad_p1,
	                      &vector_Type, &quad_p2,
	                      &vector_Type, &quad_p3,
	                      &vector_Type, &quad_p4))
	{
		return NULL;
	}

	if (BaseMath_ReadCallback(pt_vec)  == -1 ||
	    BaseMath_ReadCallback(quad_p1) == -1 ||
	    BaseMath_ReadCallback(quad_p2) == -1 ||
	    BaseMath_ReadCallback(quad_p3) == -1 ||
	    BaseMath_ReadCallback(quad_p4) == -1)
	{
		return NULL;
	}

	return PyLong_FromLong(isect_point_quad_v2(pt_vec->vec, quad_p1->vec, quad_p2->vec, quad_p3->vec, quad_p4->vec));
}

PyDoc_STRVAR(M_Geometry_distance_point_to_plane_doc,
".. function:: distance_point_to_plane(pt, plane_co, plane_no)\n"
"\n"
"   Returns the signed distance between a point and a plane "
"   (negative when below the normal).\n"
"\n"
"   :arg pt: Point\n"
"   :type pt: :class:`mathutils.Vector`\n"
"   :arg plane_co: First point of the quad\n"
"   :type plane_co: :class:`mathutils.Vector`\n"
"   :arg plane_no: Second point of the quad\n"
"   :type plane_no: :class:`mathutils.Vector`\n"
"   :rtype: float\n"
);
static PyObject *M_Geometry_distance_point_to_plane(PyObject *UNUSED(self), PyObject *args)
{
	VectorObject *pt, *plene_co, *plane_no;

	if (!PyArg_ParseTuple(args, "O!O!O!:distance_point_to_plane",
	                      &vector_Type, &pt,
	                      &vector_Type, &plene_co,
	                      &vector_Type, &plane_no))
	{
		return NULL;
	}

	if (BaseMath_ReadCallback(pt) == -1 ||
	    BaseMath_ReadCallback(plene_co) == -1 ||
	    BaseMath_ReadCallback(plane_no) == -1)
	{
		return NULL;
	}

	return PyFloat_FromDouble(dist_to_plane_v3(pt->vec, plene_co->vec, plane_no->vec));
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
	VectorObject *vec_pt;
	VectorObject *vec_t1_tar, *vec_t2_tar, *vec_t3_tar;
	VectorObject *vec_t1_src, *vec_t2_src, *vec_t3_src;
	float vec[3];

	if (!PyArg_ParseTuple(args, "O!O!O!O!O!O!O!:barycentric_transform",
	                      &vector_Type, &vec_pt,
	                      &vector_Type, &vec_t1_src,
	                      &vector_Type, &vec_t2_src,
	                      &vector_Type, &vec_t3_src,
	                      &vector_Type, &vec_t1_tar,
	                      &vector_Type, &vec_t2_tar,
	                      &vector_Type, &vec_t3_tar))
	{
		return NULL;
	}

	if (vec_pt->size != 3 ||
	    vec_t1_src->size != 3 ||
	    vec_t2_src->size != 3 ||
	    vec_t3_src->size != 3 ||
	    vec_t1_tar->size != 3 ||
	    vec_t2_tar->size != 3 ||
	    vec_t3_tar->size != 3)
	{
		PyErr_SetString(PyExc_ValueError,
		                "One of more of the vector arguments wasn't a 3D vector");
		return NULL;
	}

	barycentric_transform(vec, vec_pt->vec,
	                      vec_t1_tar->vec, vec_t2_tar->vec, vec_t3_tar->vec,
	                      vec_t1_src->vec, vec_t2_src->vec, vec_t3_src->vec);

	return Vector_CreatePyObject(vec, 3, Py_NEW, NULL);
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
	VectorObject *vec_k1, *vec_h1, *vec_k2, *vec_h2;
	int resolu;
	int dims;
	int i;
	float *coord_array, *fp;
	PyObject *list;

	float k1[4] = {0.0, 0.0, 0.0, 0.0};
	float h1[4] = {0.0, 0.0, 0.0, 0.0};
	float k2[4] = {0.0, 0.0, 0.0, 0.0};
	float h2[4] = {0.0, 0.0, 0.0, 0.0};


	if (!PyArg_ParseTuple(args, "O!O!O!O!i:interpolate_bezier",
	                      &vector_Type, &vec_k1,
	                      &vector_Type, &vec_h1,
	                      &vector_Type, &vec_h2,
	                      &vector_Type, &vec_k2, &resolu))
	{
		return NULL;
	}

	if (resolu <= 1) {
		PyErr_SetString(PyExc_ValueError,
		                "resolution must be 2 or over");
		return NULL;
	}

	if (BaseMath_ReadCallback(vec_k1) == -1 ||
	    BaseMath_ReadCallback(vec_h1) == -1 ||
	    BaseMath_ReadCallback(vec_k2) == -1 ||
	    BaseMath_ReadCallback(vec_h2) == -1)
	{
		return NULL;
	}

	dims = MAX4(vec_k1->size, vec_h1->size, vec_h2->size, vec_k2->size);

	for (i = 0; i < vec_k1->size; i++) k1[i] = vec_k1->vec[i];
	for (i = 0; i < vec_h1->size; i++) h1[i] = vec_h1->vec[i];
	for (i = 0; i < vec_k2->size; i++) k2[i] = vec_k2->vec[i];
	for (i = 0; i < vec_h2->size; i++) h2[i] = vec_h2->vec[i];

	coord_array = MEM_callocN(dims * (resolu) * sizeof(float), "interpolate_bezier");
	for (i = 0; i < dims; i++) {
		BKE_curve_forward_diff_bezier(k1[i], h1[i], h2[i], k2[i], coord_array + i, resolu - 1, sizeof(float) * dims);
	}

	list = PyList_New(resolu);
	fp = coord_array;
	for (i = 0; i < resolu; i++, fp = fp + dims) {
		PyList_SET_ITEM(list, i, Vector_CreatePyObject(fp, dims, Py_NEW, NULL));
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
			freedisplist(&dispbase);
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
		freedisplist(&dispbase); /* possible some dl was allocated */
		PyErr_SetString(PyExc_TypeError,
		                "A point in one of the polylines "
		                "is not a mathutils.Vector type");
		return NULL;
	}
	else if (totpoints) {
		/* now make the list to return */
		filldisplist(&dispbase, &dispbase, 0);

		/* The faces are stored in a new DisplayList
		 * thats added to the head of the listbase */
		dl = dispbase.first;

		tri_list = PyList_New(dl->parts);
		if (!tri_list) {
			freedisplist(&dispbase);
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
		freedisplist(&dispbase);
	}
	else {
		/* no points, do this so scripts don't barf */
		freedisplist(&dispbase); /* possible some dl was allocated */
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
		BLI_box_pack_2D(boxarray, len, &tot_width, &tot_height);

		boxPack_ToPyObject(boxlist, &boxarray);
	}

	ret = PyTuple_New(2);
	PyTuple_SET_ITEM(ret, 0, PyFloat_FromDouble(tot_width));
	PyTuple_SET_ITEM(ret, 1, PyFloat_FromDouble(tot_width));
	return ret;
}

#endif /* MATH_STANDALONE */


static PyMethodDef M_Geometry_methods[] = {
	{"intersect_ray_tri", (PyCFunction) M_Geometry_intersect_ray_tri, METH_VARARGS, M_Geometry_intersect_ray_tri_doc},
	{"intersect_point_line", (PyCFunction) M_Geometry_intersect_point_line, METH_VARARGS, M_Geometry_intersect_point_line_doc},
	{"intersect_point_tri_2d", (PyCFunction) M_Geometry_intersect_point_tri_2d, METH_VARARGS, M_Geometry_intersect_point_tri_2d_doc},
	{"intersect_point_quad_2d", (PyCFunction) M_Geometry_intersect_point_quad_2d, METH_VARARGS, M_Geometry_intersect_point_quad_2d_doc},
	{"intersect_line_line", (PyCFunction) M_Geometry_intersect_line_line, METH_VARARGS, M_Geometry_intersect_line_line_doc},
	{"intersect_line_line_2d", (PyCFunction) M_Geometry_intersect_line_line_2d, METH_VARARGS, M_Geometry_intersect_line_line_2d_doc},
	{"intersect_line_plane", (PyCFunction) M_Geometry_intersect_line_plane, METH_VARARGS, M_Geometry_intersect_line_plane_doc},
	{"intersect_plane_plane", (PyCFunction) M_Geometry_intersect_plane_plane, METH_VARARGS, M_Geometry_intersect_plane_plane_doc},
	{"intersect_line_sphere", (PyCFunction) M_Geometry_intersect_line_sphere, METH_VARARGS, M_Geometry_intersect_line_sphere_doc},
	{"intersect_line_sphere_2d", (PyCFunction) M_Geometry_intersect_line_sphere_2d, METH_VARARGS, M_Geometry_intersect_line_sphere_2d_doc},
	{"distance_point_to_plane", (PyCFunction) M_Geometry_distance_point_to_plane, METH_VARARGS, M_Geometry_distance_point_to_plane_doc},
	{"area_tri", (PyCFunction) M_Geometry_area_tri, METH_VARARGS, M_Geometry_area_tri_doc},
	{"normal", (PyCFunction) M_Geometry_normal, METH_VARARGS, M_Geometry_normal_doc},
	{"barycentric_transform", (PyCFunction) M_Geometry_barycentric_transform, METH_VARARGS, M_Geometry_barycentric_transform_doc},
#ifndef MATH_STANDALONE
	{"interpolate_bezier", (PyCFunction) M_Geometry_interpolate_bezier, METH_VARARGS, M_Geometry_interpolate_bezier_doc},
	{"tessellate_polygon", (PyCFunction) M_Geometry_tessellate_polygon, METH_O, M_Geometry_tessellate_polygon_doc},
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
