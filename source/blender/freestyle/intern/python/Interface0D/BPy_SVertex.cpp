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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/freestyle/intern/python/Interface0D/BPy_SVertex.cpp
 *  \ingroup freestyle
 */

#include "BPy_SVertex.h"

#include "../BPy_Convert.h"
#include "../BPy_Id.h"
#include "../Interface1D/BPy_FEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*----------------------SVertex methods ----------------------------*/

PyDoc_STRVAR(SVertex_doc,
"Class hierarchy: :class:`Interface0D` > :class:`SVertex`\n"
"\n"
"Class to define a vertex of the embedding.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(brother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg brother: A SVertex object.\n"
"   :type brother: :class:`SVertex`\n"
"\n"
".. method:: __init__(point_3d, id)\n"
"\n"
"   Builds a SVertex from 3D coordinates and an Id.\n"
"\n"
"   :arg point_3d: A three-dimensional vector.\n"
"   :type point_3d: :class:`mathutils.Vector`\n"
"   :arg id: An Id object.\n"
"   :type id: :class:`Id`");

static int convert_v3(PyObject *obj, void *v)
{
	return float_array_from_PyObject(obj, (float *)v, 3);
}

static int SVertex_init(BPy_SVertex *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist_1[] = {"brother", NULL};
	static const char *kwlist_2[] = {"point_3d", "id", NULL};
	PyObject *obj = 0;
	float v[3];

	if (PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist_1, &SVertex_Type, &obj)) {
		if (!obj)
			self->sv = new SVertex();
		else
			self->sv = new SVertex(*(((BPy_SVertex *)obj)->sv));
	}
	else if (PyErr_Clear(),
	         PyArg_ParseTupleAndKeywords(args, kwds, "O&O!", (char **)kwlist_2, convert_v3, v, &Id_Type, &obj))
	{
		Vec3r point_3d(v[0], v[1], v[2]);
		self->sv = new SVertex(point_3d, *(((BPy_Id *)obj)->id));
	}
	else {
		PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
		return -1;
	}
	self->py_if0D.if0D = self->sv;
	self->py_if0D.borrowed = 0;
	return 0;
}

PyDoc_STRVAR(SVertex_add_normal_doc,
".. method:: add_normal(normal)\n"
"\n"
"   Adds a normal to the SVertex's set of normals.  If the same normal\n"
"   is already in the set, nothing changes.\n"
"\n"
"   :arg normal: A three-dimensional vector.\n"
"   :type normal: :class:`mathutils.Vector`, list or tuple of 3 real numbers");

static PyObject *SVertex_add_normal(BPy_SVertex *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"normal", NULL};
	PyObject *py_normal;
	Vec3r n;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", (char **)kwlist, &py_normal))
		return NULL;
	if (!Vec3r_ptr_from_PyObject(py_normal, n)) {
		PyErr_SetString(PyExc_TypeError, "argument 1 must be a 3D vector (either a list of 3 elements or Vector)");
		return NULL;
	}
	self->sv->AddNormal(n);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(SVertex_add_fedge_doc,
".. method:: add_fedge(fedge)\n"
"\n"
"   Add an FEdge to the list of edges emanating from this SVertex.\n"
"\n"
"   :arg fedge: An FEdge.\n"
"   :type fedge: :class:`FEdge`");

static PyObject *SVertex_add_fedge(BPy_SVertex *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"fedge", NULL};
	PyObject *py_fe;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist, &FEdge_Type, &py_fe))
		return NULL;
	self->sv->AddFEdge(((BPy_FEdge *)py_fe)->fe);
	Py_RETURN_NONE;
}

// virtual bool 	operator== (const SVertex &brother)

static PyMethodDef BPy_SVertex_methods[] = {
	{"add_normal", (PyCFunction)SVertex_add_normal, METH_VARARGS | METH_KEYWORDS, SVertex_add_normal_doc},
	{"add_fedge", (PyCFunction)SVertex_add_fedge, METH_VARARGS | METH_KEYWORDS, SVertex_add_fedge_doc},
	{NULL, NULL, 0, NULL}
};

/*----------------------mathutils callbacks ----------------------------*/

/* subtype */
#define MATHUTILS_SUBTYPE_POINT3D  1
#define MATHUTILS_SUBTYPE_POINT2D  2

static int SVertex_mathutils_check(BaseMathObject *bmo)
{
	if (!BPy_SVertex_Check(bmo->cb_user))
		return -1;
	return 0;
}

static int SVertex_mathutils_get(BaseMathObject *bmo, int subtype)
{
	BPy_SVertex *self = (BPy_SVertex *)bmo->cb_user;
	switch (subtype) {
	case MATHUTILS_SUBTYPE_POINT3D:
		bmo->data[0] = self->sv->getX();
		bmo->data[1] = self->sv->getY();
		bmo->data[2] = self->sv->getZ();
		break;
	case MATHUTILS_SUBTYPE_POINT2D:
		bmo->data[0] = self->sv->getProjectedX();
		bmo->data[1] = self->sv->getProjectedY();
		bmo->data[2] = self->sv->getProjectedZ();
		break;
	default:
		return -1;
	}
	return 0;
}

static int SVertex_mathutils_set(BaseMathObject *bmo, int subtype)
{
	BPy_SVertex *self = (BPy_SVertex *)bmo->cb_user;
	switch (subtype) {
	case MATHUTILS_SUBTYPE_POINT3D:
		{
			Vec3r p(bmo->data[0], bmo->data[1], bmo->data[2]);
			self->sv->setPoint3D(p);
		}
		break;
	case MATHUTILS_SUBTYPE_POINT2D:
		{
			Vec3r p(bmo->data[0], bmo->data[1], bmo->data[2]);
			self->sv->setPoint2D(p);
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static int SVertex_mathutils_get_index(BaseMathObject *bmo, int subtype, int index)
{
	BPy_SVertex *self = (BPy_SVertex *)bmo->cb_user;
	switch (subtype) {
	case MATHUTILS_SUBTYPE_POINT3D:
		switch (index) {
		case 0: bmo->data[0] = self->sv->getX(); break;
		case 1: bmo->data[1] = self->sv->getY(); break;
		case 2: bmo->data[2] = self->sv->getZ(); break;
		default:
			return -1;
		}
		break;
	case MATHUTILS_SUBTYPE_POINT2D:
		switch (index) {
		case 0: bmo->data[0] = self->sv->getProjectedX(); break;
		case 1: bmo->data[1] = self->sv->getProjectedY(); break;
		case 2: bmo->data[2] = self->sv->getProjectedZ(); break;
		default:
			return -1;
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static int SVertex_mathutils_set_index(BaseMathObject *bmo, int subtype, int index)
{
	BPy_SVertex *self = (BPy_SVertex *)bmo->cb_user;
	switch (subtype) {
	case MATHUTILS_SUBTYPE_POINT3D:
		{
			Vec3r p(self->sv->point3D());
			p[index] = bmo->data[index];
			self->sv->setPoint3D(p);
		}
		break;
	case MATHUTILS_SUBTYPE_POINT2D:
		{
			Vec3r p(self->sv->point2D());
			p[index] = bmo->data[index];
			self->sv->setPoint2D(p);
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static Mathutils_Callback SVertex_mathutils_cb = {
	SVertex_mathutils_check,
	SVertex_mathutils_get,
	SVertex_mathutils_set,
	SVertex_mathutils_get_index,
	SVertex_mathutils_set_index
};

static unsigned char SVertex_mathutils_cb_index = -1;

void SVertex_mathutils_register_callback()
{
	SVertex_mathutils_cb_index = Mathutils_RegisterCallback(&SVertex_mathutils_cb);
}

/*----------------------SVertex get/setters ----------------------------*/

PyDoc_STRVAR(SVertex_point_3d_doc,
"The 3D coordinates of the SVertex.\n"
"\n"
":type: mathutils.Vector");

static PyObject *SVertex_point_3d_get(BPy_SVertex *self, void *UNUSED(closure))
{
	return Vector_CreatePyObject_cb((PyObject *)self, 3, SVertex_mathutils_cb_index, MATHUTILS_SUBTYPE_POINT3D);
}

static int SVertex_point_3d_set(BPy_SVertex *self, PyObject *value, void *UNUSED(closure))
{
	float v[3];
	if (!float_array_from_PyObject(value, v, 3)) {
		PyErr_SetString(PyExc_ValueError, "value must be a 3-dimensional vector");
		return -1;
	}
	Vec3r p(v[0], v[1], v[2]);
	self->sv->setPoint3D(p);
	return 0;
}

PyDoc_STRVAR(SVertex_point_2d_doc,
"The projected 3D coordinates of the SVertex.\n"
"\n"
":type: mathutils.Vector");

static PyObject *SVertex_point_2d_get(BPy_SVertex *self, void *UNUSED(closure))
{
	return Vector_CreatePyObject_cb((PyObject *)self, 3, SVertex_mathutils_cb_index, MATHUTILS_SUBTYPE_POINT2D);
}

static int SVertex_point_2d_set(BPy_SVertex *self, PyObject *value, void *UNUSED(closure))
{
	float v[3];
	if (!float_array_from_PyObject(value, v, 3)) {
		PyErr_SetString(PyExc_ValueError, "value must be a 3-dimensional vector");
		return -1;
	}
	Vec3r p(v[0], v[1], v[2]);
	self->sv->setPoint2D(p);
	return 0;
}

PyDoc_STRVAR(SVertex_id_doc,
"The Id of this SVertex.\n"
"\n"
":type: :class:`Id`");

static PyObject *SVertex_id_get(BPy_SVertex *self, void *UNUSED(closure))
{
	Id id(self->sv->getId());
	return BPy_Id_from_Id(id); // return a copy
}

static int SVertex_id_set(BPy_SVertex *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_Id_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be an Id");
		return -1;
	}
	self->sv->setId(*(((BPy_Id *)value)->id));
	return 0;
}

PyDoc_STRVAR(SVertex_normals_doc,
"The normals for this Vertex as a list.  In a sharp surface, an SVertex\n"
"has exactly one normal.  In a smooth surface, an SVertex can have any\n"
"number of normals.\n"
"\n"
":type: list of :class:`mathutils.Vector` objects");

static PyObject *SVertex_normals_get(BPy_SVertex *self, void *UNUSED(closure))
{
	PyObject *py_normals; 
	set< Vec3r > normals;
	
	py_normals = PyList_New(0);
	normals = self->sv->normals();
	for (set< Vec3r >::iterator set_iterator = normals.begin(); set_iterator != normals.end(); set_iterator++) {
		Vec3r v(*set_iterator);
		PyList_Append(py_normals, Vector_from_Vec3r(v));
	}
	return py_normals;
}

PyDoc_STRVAR(SVertex_normals_size_doc,
"The number of different normals for this SVertex.\n"
"\n"
":type: int");

static PyObject *SVertex_normals_size_get(BPy_SVertex *self, void *UNUSED(closure))
{
	return PyLong_FromLong(self->sv->normalsSize());
}

PyDoc_STRVAR(SVertex_viewvertex_doc,
"If this SVertex is also a ViewVertex, this property refers to the\n"
"ViewVertex, and None otherwise.\n"
"\n"
":type: :class:`ViewVertex`");

static PyObject *SVertex_viewvertex_get(BPy_SVertex *self, void *UNUSED(closure))
{
	ViewVertex *vv = self->sv->viewvertex();
	if (vv)
		return Any_BPy_ViewVertex_from_ViewVertex(*vv);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(SVertex_curvatures_doc,
"Curvature information expressed in the form of a seven-element tuple\n"
"(K1, e1, K2, e2, Kr, er, dKr), where K1 and K2 are scalar values\n"
"representing the first (maximum) and second (minimum) principal\n"
"curvatures at this SVertex, respectively; e1 and e2 are\n"
"three-dimensional vectors representing the first and second principal\n"
"directions, i.e. the directions of the normal plane where the\n"
"curvature takes its maximum and minimum values, respectively; and Kr,\n"
"er and dKr are the radial curvature, radial direction, and the\n"
"derivative of the radial curvature at this SVertex, repectively.\n"
"\n"
":type: tuple");

static PyObject *SVertex_curvatures_get(BPy_SVertex *self, void *UNUSED(closure))
{
	const CurvatureInfo *info = self->sv->getCurvatureInfo();
	if (!info)
		Py_RETURN_NONE;
	Vec3r e1(info->e1.x(), info->e1.y(), info->e1.z());
	Vec3r e2(info->e2.x(), info->e2.y(), info->e2.z());
	Vec3r er(info->er.x(), info->er.y(), info->er.z());
	PyObject *retval = PyTuple_New(7);
	PyTuple_SET_ITEM(retval, 0, PyFloat_FromDouble(info->K1));
	PyTuple_SET_ITEM(retval, 2, Vector_from_Vec3r(e1));
	PyTuple_SET_ITEM(retval, 1, PyFloat_FromDouble(info->K2));
	PyTuple_SET_ITEM(retval, 3, Vector_from_Vec3r(e2));
	PyTuple_SET_ITEM(retval, 4, PyFloat_FromDouble(info->Kr));
	PyTuple_SET_ITEM(retval, 5, Vector_from_Vec3r(er));
	PyTuple_SET_ITEM(retval, 6, PyFloat_FromDouble(info->dKr));
	return retval;
}

static PyGetSetDef BPy_SVertex_getseters[] = {
	{(char *)"point_3d", (getter)SVertex_point_3d_get, (setter)SVertex_point_3d_set,
	                     (char *)SVertex_point_3d_doc, NULL},
	{(char *)"point_2d", (getter)SVertex_point_2d_get, (setter)SVertex_point_2d_set,
	                     (char *)SVertex_point_2d_doc, NULL},
	{(char *)"id", (getter)SVertex_id_get, (setter)SVertex_id_set, (char *)SVertex_id_doc, NULL},
	{(char *)"normals", (getter)SVertex_normals_get, (setter)NULL, (char *)SVertex_normals_doc, NULL},
	{(char *)"normals_size", (getter)SVertex_normals_size_get, (setter)NULL, (char *)SVertex_normals_size_doc, NULL},
	{(char *)"viewvertex", (getter)SVertex_viewvertex_get, (setter)NULL, (char *)SVertex_viewvertex_doc, NULL},
	{(char *)"curvatures", (getter)SVertex_curvatures_get, (setter)NULL, (char *)SVertex_curvatures_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

/*-----------------------BPy_SVertex type definition ------------------------------*/
PyTypeObject SVertex_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SVertex",                      /* tp_name */
	sizeof(BPy_SVertex),            /* tp_basicsize */
	0,                              /* tp_itemsize */
	0,                              /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	0,                              /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	0,                              /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	SVertex_doc,                    /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_SVertex_methods,            /* tp_methods */
	0,                              /* tp_members */
	BPy_SVertex_getseters,          /* tp_getset */
	&Interface0D_Type,              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)SVertex_init,         /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
