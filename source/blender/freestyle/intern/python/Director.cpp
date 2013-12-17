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

/** \file source/blender/freestyle/intern/python/Director.cpp
 *  \ingroup freestyle
 */

#include "Director.h"

#include "BPy_Convert.h"

#include "BPy_BinaryPredicate0D.h"
#include "BPy_BinaryPredicate1D.h"
#include "BPy_FrsMaterial.h"
#include "BPy_Id.h"
#include "BPy_UnaryFunction0D.h"
#include "BPy_UnaryFunction1D.h"
#include "BPy_UnaryPredicate0D.h"
#include "BPy_UnaryPredicate1D.h"
#include "BPy_StrokeShader.h"
#include "Iterator/BPy_ChainingIterator.h"
#include "Iterator/BPy_Interface0DIterator.h"
#include "Interface1D/BPy_Stroke.h"
#include "Interface1D/BPy_ViewEdge.h"
#include "BPy_ViewShape.h"

#include "UnaryFunction0D/BPy_UnaryFunction0DDouble.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DEdgeNature.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DFloat.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DId.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DMaterial.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DUnsigned.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DVec2f.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DVec3f.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DVectorViewShape.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DViewShape.h"

#include "UnaryFunction1D/BPy_UnaryFunction1DDouble.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DEdgeNature.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DFloat.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DUnsigned.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DVec2f.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DVec3f.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DVectorViewShape.h"

// BinaryPredicate0D: __call__
int Director_BPy_BinaryPredicate0D___call__(BinaryPredicate0D *bp0D, Interface0D& i1, Interface0D& i2)
{
	if (!bp0D->py_bp0D) { // internal error
		PyErr_SetString(PyExc_RuntimeError, "Reference to Python object (py_bp0D) not initialized");
		return -1;
	}
	PyObject *arg1 = Any_BPy_Interface0D_from_Interface0D(i1);
	PyObject *arg2 = Any_BPy_Interface0D_from_Interface0D(i2);
	if (!arg1 || !arg2) {
		Py_XDECREF(arg1);
		Py_XDECREF(arg2);
		return -1;
	}
	PyObject *result = PyObject_CallMethod(bp0D->py_bp0D, (char *)"__call__", (char *)"OO", arg1, arg2);
	Py_DECREF(arg1);
	Py_DECREF(arg2);
	if (!result)
		return -1;
	int ret = PyObject_IsTrue(result);
	Py_DECREF(result);
	if (ret < 0)
		return -1;
	bp0D->result = ret;
	return 0;
}

// BinaryPredicate1D: __call__
int Director_BPy_BinaryPredicate1D___call__(BinaryPredicate1D *bp1D, Interface1D& i1, Interface1D& i2)
{
	if (!bp1D->py_bp1D) { // internal error
		PyErr_SetString(PyExc_RuntimeError, "Reference to Python object (py_bp1D) not initialized");
		return -1;
	}
	PyObject *arg1 = Any_BPy_Interface1D_from_Interface1D(i1);
	PyObject *arg2 = Any_BPy_Interface1D_from_Interface1D(i2);
	if (!arg1 || !arg2) {
		Py_XDECREF(arg1);
		Py_XDECREF(arg2);
		return -1;
	}
	PyObject *result = PyObject_CallMethod(bp1D->py_bp1D, (char *)"__call__", (char *)"OO", arg1, arg2);
	Py_DECREF(arg1);
	Py_DECREF(arg2);
	if (!result)
		return -1;
	int ret = PyObject_IsTrue(result);
	Py_DECREF(result);
	if (ret < 0)
		return -1;
	bp1D->result = ret;
	return 0;
}

// UnaryPredicate0D: __call__
int Director_BPy_UnaryPredicate0D___call__(UnaryPredicate0D *up0D, Interface0DIterator& if0D_it)
{
	if (!up0D->py_up0D) { // internal error
		PyErr_SetString(PyExc_RuntimeError, "Reference to Python object (py_up0D) not initialized");
		return -1;
	}
	PyObject *arg = BPy_Interface0DIterator_from_Interface0DIterator(if0D_it, 0);
	if (!arg)
		return -1;
	PyObject *result = PyObject_CallMethod(up0D->py_up0D, (char *)"__call__", (char *)"O", arg);
	Py_DECREF(arg);
	if (!result)
		return -1;
	int ret = PyObject_IsTrue(result);
	Py_DECREF(result);
	if (ret < 0)
		return -1;
	up0D->result = ret;
	return 0;
}

// UnaryPredicate1D: __call__
int Director_BPy_UnaryPredicate1D___call__(UnaryPredicate1D *up1D, Interface1D& if1D)
{
	if (!up1D->py_up1D) { // internal error
		PyErr_SetString(PyExc_RuntimeError, "Reference to Python object (py_up1D) not initialized");
		return -1;
	}
	PyObject *arg = Any_BPy_Interface1D_from_Interface1D(if1D);
	if (!arg)
		return -1;
	PyObject *result = PyObject_CallMethod(up1D->py_up1D, (char *)"__call__", (char *)"O", arg);
	Py_DECREF(arg);
	if (!result)
		return -1;
	int ret = PyObject_IsTrue(result);
	Py_DECREF(result);
	if (ret < 0)
		return -1;
	up1D->result = ret;
	return 0;
}

// StrokeShader: shade
int Director_BPy_StrokeShader_shade(StrokeShader *ss, Stroke& s)
{
	if (!ss->py_ss) { // internal error
		PyErr_SetString(PyExc_RuntimeError, "Reference to Python object (py_ss) not initialized");
		return -1;
	}
	PyObject *arg = BPy_Stroke_from_Stroke(s);
	if (!arg)
		return -1;
	PyObject *result = PyObject_CallMethod(ss->py_ss, (char *)"shade", (char *)"O", arg);
	Py_DECREF(arg);
	if (!result)
		return -1;
	Py_DECREF(result);
	return 0;
}

// ChainingIterator: init, traverse
int Director_BPy_ChainingIterator_init(ChainingIterator *c_it)
{
	if (!c_it->py_c_it) { // internal error
		PyErr_SetString(PyExc_RuntimeError, "Reference to Python object (py_c_it) not initialized");
		return -1;
	}
	PyObject *result = PyObject_CallMethod(c_it->py_c_it, (char *)"init", NULL);
	if (!result)
		return -1;
	Py_DECREF(result);
	return 0;
}

int Director_BPy_ChainingIterator_traverse(ChainingIterator *c_it, AdjacencyIterator& a_it)
{
	if (!c_it->py_c_it) { // internal error
		PyErr_SetString(PyExc_RuntimeError, "Reference to Python object (py_c_it) not initialized");
		return -1;
	}
	PyObject *arg = BPy_AdjacencyIterator_from_AdjacencyIterator(a_it);
	if (!arg)
		return -1;
	PyObject *result = PyObject_CallMethod(c_it->py_c_it, (char *)"traverse", (char *)"O", arg);
	Py_DECREF(arg);
	if (!result)
		return -1;
	if (BPy_ViewEdge_Check(result)) {
		c_it->result = ((BPy_ViewEdge *)result)->ve;
	}
	else if (result == Py_None) {
		c_it->result = NULL;
	}
	else {
		PyErr_SetString(PyExc_RuntimeError, "traverse method returned a wrong value");
		Py_DECREF(result);
		return -1;
	}
	Py_DECREF(result);
	return 0;
}

// BPy_UnaryFunction{0D,1D}: __call__
int Director_BPy_UnaryFunction0D___call__(void *uf0D, PyObject *obj, Interface0DIterator& if0D_it)
{
	if (!obj) { // internal error
		PyErr_SetString(PyExc_RuntimeError, "Reference to Python object (py_uf0D) not initialized");
		return -1;
	}
	PyObject *arg = BPy_Interface0DIterator_from_Interface0DIterator(if0D_it, 0);
	if (!arg)
		return -1;
	PyObject *result = PyObject_CallMethod(obj, (char *)"__call__", (char *)"O", arg);
	Py_DECREF(arg);
	if (!result)
		return -1;
	if (BPy_UnaryFunction0DDouble_Check(obj)) {
		((UnaryFunction0D<double> *)uf0D)->result = PyFloat_AsDouble(result);
	}
	else if (BPy_UnaryFunction0DEdgeNature_Check(obj)) {
		((UnaryFunction0D<Nature::EdgeNature> *)uf0D)->result = EdgeNature_from_BPy_Nature(result);
	}
	else if (BPy_UnaryFunction0DFloat_Check(obj)) {
		((UnaryFunction0D<float> *)uf0D)->result = PyFloat_AsDouble(result);
	}
	else if (BPy_UnaryFunction0DId_Check(obj)) {
		((UnaryFunction0D<Id> *)uf0D)->result = *(((BPy_Id *)result)->id);
	}
	else if (BPy_UnaryFunction0DMaterial_Check(obj)) {
		((UnaryFunction0D<FrsMaterial> *)uf0D)->result = *(((BPy_FrsMaterial *)result)->m);
	}
	else if (BPy_UnaryFunction0DUnsigned_Check(obj)) {
		((UnaryFunction0D<unsigned> *)uf0D)->result = PyLong_AsLong(result);
	}
	else if (BPy_UnaryFunction0DVec2f_Check(obj)) {
		Vec2f vec;
		if (!Vec2f_ptr_from_Vector(result, vec))
			return -1;
		((UnaryFunction0D<Vec2f> *)uf0D)->result = vec;
	}
	else if (BPy_UnaryFunction0DVec3f_Check(obj)) {
		Vec3f vec;
		if (!Vec3f_ptr_from_Vector(result, vec))
			return -1;
		((UnaryFunction0D<Vec3f> *)uf0D)->result = vec;
	}
	else if (BPy_UnaryFunction0DVectorViewShape_Check(obj)) {
		vector<ViewShape*> vec;
		for (int i = 0; i < PyList_Size(result); i++) {
			ViewShape *b = ((BPy_ViewShape *)PyList_GetItem(result, i))->vs;
			vec.push_back(b);
		}
		((UnaryFunction0D< vector<ViewShape*> > *)uf0D)->result = vec;
	}
	else if (BPy_UnaryFunction0DViewShape_Check(obj)) {
		((UnaryFunction0D<ViewShape*> *)uf0D)->result = ((BPy_ViewShape *)result)->vs;
	}
	Py_DECREF(result);
	return 0;
}

int Director_BPy_UnaryFunction1D___call__(void *uf1D, PyObject *obj, Interface1D& if1D)
{
	if (!obj) { // internal error
		PyErr_SetString(PyExc_RuntimeError, "Reference to Python object (py_uf1D) not initialized");
		return -1;
	}
	PyObject *arg = Any_BPy_Interface1D_from_Interface1D(if1D);
	if (!arg)
		return -1;
	PyObject *result = PyObject_CallMethod(obj, (char *)"__call__", (char *)"O", arg);
	Py_DECREF(arg);
	if (!result)
		return -1;
	if (BPy_UnaryFunction1DDouble_Check(obj)) {
		((UnaryFunction1D<double> *)uf1D)->result = PyFloat_AsDouble(result);
	}
	else if (BPy_UnaryFunction1DEdgeNature_Check(obj)) {
		((UnaryFunction1D<Nature::EdgeNature> *)uf1D)->result = EdgeNature_from_BPy_Nature(result);
	}
	else if (BPy_UnaryFunction1DFloat_Check(obj)) {
		((UnaryFunction1D<float> *)uf1D)->result = PyFloat_AsDouble(result);
	}
	else if (BPy_UnaryFunction1DUnsigned_Check(obj)) {
		((UnaryFunction1D<unsigned> *)uf1D)->result = PyLong_AsLong(result);
	}
	else if (BPy_UnaryFunction1DVec2f_Check(obj)) {
		Vec2f vec;
		if (!Vec2f_ptr_from_Vector(result, vec))
			return -1;
		((UnaryFunction1D<Vec2f> *)uf1D)->result = vec;
	}
	else if (BPy_UnaryFunction1DVec3f_Check(obj)) {
		Vec3f vec;
		if (!Vec3f_ptr_from_Vector(result, vec))
			return -1;
		((UnaryFunction1D<Vec3f> *)uf1D)->result = vec;
	}
	else if (BPy_UnaryFunction1DVectorViewShape_Check(obj)) {
		vector<ViewShape*> vec;
		for (int i = 1; i < PyList_Size(result); i++) {
			ViewShape *b = ((BPy_ViewShape *)PyList_GetItem(result, i))->vs;
			vec.push_back(b);
		}
		((UnaryFunction1D< vector<ViewShape*> > *)uf1D)->result = vec;
	} 
	Py_DECREF(result);
	return 0;
}
