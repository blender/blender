/*
 *
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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/mathutils/mathutils_Color.c
 *  \ingroup pymathutils
 */


#include <Python.h>

#include "mathutils.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_dynstr.h"

#define COLOR_SIZE 3

//----------------------------------mathutils.Color() -------------------
//makes a new color for you to play with
static PyObject *Color_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	float col[3] = {0.0f, 0.0f, 0.0f};

	if (kwds && PyDict_Size(kwds)) {
		PyErr_SetString(PyExc_TypeError,
		                "mathutils.Color(): "
		                "takes no keyword args");
		return NULL;
	}

	switch (PyTuple_GET_SIZE(args)) {
	case 0:
		break;
	case 1:
		if ((mathutils_array_parse(col, COLOR_SIZE, COLOR_SIZE, PyTuple_GET_ITEM(args, 0), "mathutils.Color()")) == -1)
			return NULL;
		break;
	default:
		PyErr_SetString(PyExc_TypeError,
		                "mathutils.Color(): "
		                "more then a single arg given");
		return NULL;
	}
	return Color_CreatePyObject(col, Py_NEW, type);
}

//-----------------------------METHODS----------------------------

/* note: BaseMath_ReadCallback must be called beforehand */
static PyObject *Color_ToTupleExt(ColorObject *self, int ndigits)
{
	PyObject *ret;
	int i;

	ret = PyTuple_New(COLOR_SIZE);

	if (ndigits >= 0) {
		for (i = 0; i < COLOR_SIZE; i++) {
			PyTuple_SET_ITEM(ret, i, PyFloat_FromDouble(double_round((double)self->col[i], ndigits)));
		}
	}
	else {
		for (i = 0; i < COLOR_SIZE; i++) {
			PyTuple_SET_ITEM(ret, i, PyFloat_FromDouble(self->col[i]));
		}
	}

	return ret;
}

PyDoc_STRVAR(Color_copy_doc,
".. function:: copy()\n"
"\n"
"   Returns a copy of this color.\n"
"\n"
"   :return: A copy of the color.\n"
"   :rtype: :class:`Color`\n"
"\n"
"   .. note:: use this to get a copy of a wrapped color with\n"
"      no reference to the original data.\n"
);
static PyObject *Color_copy(ColorObject *self)
{
	if (BaseMath_ReadCallback(self) == -1)
		return NULL;

	return Color_CreatePyObject(self->col, Py_NEW, Py_TYPE(self));
}

//----------------------------print object (internal)--------------
//print the object to screen

static PyObject *Color_repr(ColorObject *self)
{
	PyObject *ret, *tuple;

	if (BaseMath_ReadCallback(self) == -1)
		return NULL;

	tuple = Color_ToTupleExt(self, -1);

	ret = PyUnicode_FromFormat("Color(%R)", tuple);

	Py_DECREF(tuple);
	return ret;
}

static PyObject *Color_str(ColorObject *self)
{
	DynStr *ds;

	if (BaseMath_ReadCallback(self) == -1)
		return NULL;

	ds = BLI_dynstr_new();

	BLI_dynstr_appendf(ds, "<Color (r=%.4f, g=%.4f, b=%.4f)>",
	                   self->col[0], self->col[1], self->col[2]);

	return mathutils_dynstr_to_py(ds); /* frees ds */
}

//------------------------tp_richcmpr
//returns -1 execption, 0 false, 1 true
static PyObject *Color_richcmpr(PyObject *a, PyObject *b, int op)
{
	PyObject *res;
	int ok = -1; /* zero is true */

	if (ColorObject_Check(a) && ColorObject_Check(b)) {
		ColorObject *colA = (ColorObject *)a;
		ColorObject *colB = (ColorObject *)b;

		if (BaseMath_ReadCallback(colA) == -1 || BaseMath_ReadCallback(colB) == -1)
			return NULL;

		ok = EXPP_VectorsAreEqual(colA->col, colB->col, COLOR_SIZE, 1) ? 0 : -1;
	}

	switch (op) {
	case Py_NE:
		ok = !ok; /* pass through */
	case Py_EQ:
		res = ok ? Py_False : Py_True;
		break;

	case Py_LT:
	case Py_LE:
	case Py_GT:
	case Py_GE:
		res = Py_NotImplemented;
		break;
	default:
		PyErr_BadArgument();
		return NULL;
	}

	return Py_INCREF(res), res;
}

//---------------------SEQUENCE PROTOCOLS------------------------
//----------------------------len(object)------------------------
//sequence length
static int Color_len(ColorObject *UNUSED(self))
{
	return COLOR_SIZE;
}
//----------------------------object[]---------------------------
//sequence accessor (get)
static PyObject *Color_item(ColorObject *self, int i)
{
	if (i < 0) i = COLOR_SIZE - i;

	if (i < 0 || i >= COLOR_SIZE) {
		PyErr_SetString(PyExc_IndexError,
		                "color[attribute]: "
		                "array index out of range");
		return NULL;
	}

	if (BaseMath_ReadIndexCallback(self, i) == -1)
		return NULL;

	return PyFloat_FromDouble(self->col[i]);

}
//----------------------------object[]-------------------------
//sequence accessor (set)
static int Color_ass_item(ColorObject *self, int i, PyObject *value)
{
	float f = PyFloat_AsDouble(value);

	if (f == -1 && PyErr_Occurred()) { // parsed item not a number
		PyErr_SetString(PyExc_TypeError,
		                "color[attribute] = x: "
		                "argument not a number");
		return -1;
	}

	if (i < 0) i= COLOR_SIZE - i;

	if (i < 0 || i >= COLOR_SIZE) {
		PyErr_SetString(PyExc_IndexError, "color[attribute] = x: "
		                "array assignment index out of range");
		return -1;
	}

	self->col[i] = f;

	if (BaseMath_WriteIndexCallback(self, i) == -1)
		return -1;

	return 0;
}
//----------------------------object[z:y]------------------------
//sequence slice (get)
static PyObject *Color_slice(ColorObject *self, int begin, int end)
{
	PyObject *tuple;
	int count;

	if (BaseMath_ReadCallback(self) == -1)
		return NULL;

	CLAMP(begin, 0, COLOR_SIZE);
	if (end < 0) end = (COLOR_SIZE + 1) + end;
	CLAMP(end, 0, COLOR_SIZE);
	begin = MIN2(begin, end);

	tuple = PyTuple_New(end - begin);
	for (count = begin; count < end; count++) {
		PyTuple_SET_ITEM(tuple, count - begin, PyFloat_FromDouble(self->col[count]));
	}

	return tuple;
}
//----------------------------object[z:y]------------------------
//sequence slice (set)
static int Color_ass_slice(ColorObject *self, int begin, int end, PyObject *seq)
{
	int i, size;
	float col[COLOR_SIZE];

	if (BaseMath_ReadCallback(self) == -1)
		return -1;

	CLAMP(begin, 0, COLOR_SIZE);
	if (end < 0) end = (COLOR_SIZE + 1) + end;
	CLAMP(end, 0, COLOR_SIZE);
	begin = MIN2(begin, end);

	if ((size = mathutils_array_parse(col, 0, COLOR_SIZE, seq, "mathutils.Color[begin:end] = []")) == -1)
		return -1;

	if (size != (end - begin)) {
		PyErr_SetString(PyExc_ValueError,
		                "color[begin:end] = []: "
		                "size mismatch in slice assignment");
		return -1;
	}

	for (i = 0; i < COLOR_SIZE; i++)
		self->col[begin + i] = col[i];

	(void)BaseMath_WriteCallback(self);
	return 0;
}

static PyObject *Color_subscript(ColorObject *self, PyObject *item)
{
	if (PyIndex_Check(item)) {
		Py_ssize_t i;
		i = PyNumber_AsSsize_t(item, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return NULL;
		if (i < 0)
			i += COLOR_SIZE;
		return Color_item(self, i);
	}
	else if (PySlice_Check(item)) {
		Py_ssize_t start, stop, step, slicelength;

		if (PySlice_GetIndicesEx((void *)item, COLOR_SIZE, &start, &stop, &step, &slicelength) < 0)
			return NULL;

		if (slicelength <= 0) {
			return PyTuple_New(0);
		}
		else if (step == 1) {
			return Color_slice(self, start, stop);
		}
		else {
			PyErr_SetString(PyExc_IndexError,
			                "slice steps not supported with color");
			return NULL;
		}
	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "color indices must be integers, not %.200s",
		             Py_TYPE(item)->tp_name);
		return NULL;
	}
}

static int Color_ass_subscript(ColorObject *self, PyObject *item, PyObject *value)
{
	if (PyIndex_Check(item)) {
		Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return -1;
		if (i < 0)
			i += COLOR_SIZE;
		return Color_ass_item(self, i, value);
	}
	else if (PySlice_Check(item)) {
		Py_ssize_t start, stop, step, slicelength;

		if (PySlice_GetIndicesEx((void *)item, COLOR_SIZE, &start, &stop, &step, &slicelength) < 0)
			return -1;

		if (step == 1)
			return Color_ass_slice(self, start, stop, value);
		else {
			PyErr_SetString(PyExc_IndexError,
			                "slice steps not supported with color");
			return -1;
		}
	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "color indices must be integers, not %.200s",
		             Py_TYPE(item)->tp_name);
		return -1;
	}
}

//-----------------PROTCOL DECLARATIONS--------------------------
static PySequenceMethods Color_SeqMethods = {
	(lenfunc) Color_len,					/* sq_length */
	(binaryfunc) NULL,						/* sq_concat */
	(ssizeargfunc) NULL,					/* sq_repeat */
	(ssizeargfunc) Color_item,				/* sq_item */
	NULL,									/* sq_slice, deprecated */
	(ssizeobjargproc) Color_ass_item,		/* sq_ass_item */
	NULL,									/* sq_ass_slice, deprecated */
	(objobjproc) NULL,						/* sq_contains */
	(binaryfunc) NULL,						/* sq_inplace_concat */
	(ssizeargfunc) NULL,					/* sq_inplace_repeat */
};

static PyMappingMethods Color_AsMapping = {
	(lenfunc)Color_len,
	(binaryfunc)Color_subscript,
	(objobjargproc)Color_ass_subscript
};

/* numeric */


/* addition: obj + obj */
static PyObject *Color_add(PyObject *v1, PyObject *v2)
{
	ColorObject *color1 = NULL, *color2 = NULL;
	float col[COLOR_SIZE];

	if (!ColorObject_Check(v1) || !ColorObject_Check(v2)) {
		PyErr_Format(PyExc_TypeError,
		             "Color addition: (%s + %s) "
		             "invalid type for this operation",
		             Py_TYPE(v1)->tp_name, Py_TYPE(v2)->tp_name);
		return NULL;
	}
	color1 = (ColorObject *)v1;
	color2 = (ColorObject *)v2;

	if (BaseMath_ReadCallback(color1) == -1 || BaseMath_ReadCallback(color2) == -1)
		return NULL;

	add_vn_vnvn(col, color1->col, color2->col, COLOR_SIZE);

	return Color_CreatePyObject(col, Py_NEW, Py_TYPE(v1));
}

/* addition in-place: obj += obj */
static PyObject *Color_iadd(PyObject *v1, PyObject *v2)
{
	ColorObject *color1 = NULL, *color2 = NULL;

	if (!ColorObject_Check(v1) || !ColorObject_Check(v2)) {
		PyErr_Format(PyExc_TypeError,
		             "Color addition: (%s += %s) "
		             "invalid type for this operation",
		             Py_TYPE(v1)->tp_name, Py_TYPE(v2)->tp_name);
		return NULL;
	}
	color1 = (ColorObject *)v1;
	color2 = (ColorObject *)v2;

	if (BaseMath_ReadCallback(color1) == -1 || BaseMath_ReadCallback(color2) == -1)
		return NULL;

	add_vn_vn(color1->col, color2->col, COLOR_SIZE);

	(void)BaseMath_WriteCallback(color1);
	Py_INCREF(v1);
	return v1;
}

/* subtraction: obj - obj */
static PyObject *Color_sub(PyObject *v1, PyObject *v2)
{
	ColorObject *color1 = NULL, *color2 = NULL;
	float col[COLOR_SIZE];

	if (!ColorObject_Check(v1) || !ColorObject_Check(v2)) {
		PyErr_Format(PyExc_TypeError,
		             "Color subtraction: (%s - %s) "
		             "invalid type for this operation",
		             Py_TYPE(v1)->tp_name, Py_TYPE(v2)->tp_name);
		return NULL;
	}
	color1 = (ColorObject *)v1;
	color2 = (ColorObject *)v2;

	if (BaseMath_ReadCallback(color1) == -1 || BaseMath_ReadCallback(color2) == -1)
		return NULL;

	sub_vn_vnvn(col, color1->col, color2->col, COLOR_SIZE);

	return Color_CreatePyObject(col, Py_NEW, Py_TYPE(v1));
}

/* subtraction in-place: obj -= obj */
static PyObject *Color_isub(PyObject *v1, PyObject *v2)
{
	ColorObject *color1 = NULL, *color2 = NULL;

	if (!ColorObject_Check(v1) || !ColorObject_Check(v2)) {
		PyErr_Format(PyExc_TypeError,
		             "Color subtraction: (%s -= %s) "
		             "invalid type for this operation",
		             Py_TYPE(v1)->tp_name, Py_TYPE(v2)->tp_name);
		return NULL;
	}
	color1 = (ColorObject *)v1;
	color2 = (ColorObject *)v2;

	if (BaseMath_ReadCallback(color1) == -1 || BaseMath_ReadCallback(color2) == -1)
		return NULL;

	sub_vn_vn(color1->col, color2->col, COLOR_SIZE);

	(void)BaseMath_WriteCallback(color1);
	Py_INCREF(v1);
	return v1;
}

static PyObject *color_mul_float(ColorObject *color, const float scalar)
{
	float tcol[COLOR_SIZE];
	mul_vn_vn_fl(tcol, color->col, COLOR_SIZE, scalar);
	return Color_CreatePyObject(tcol, Py_NEW, Py_TYPE(color));
}


static PyObject *Color_mul(PyObject *v1, PyObject *v2)
{
	ColorObject *color1 = NULL, *color2 = NULL;
	float scalar;

	if ColorObject_Check(v1) {
		color1 = (ColorObject *)v1;
		if (BaseMath_ReadCallback(color1) == -1)
			return NULL;
	}
	if ColorObject_Check(v2) {
		color2 = (ColorObject *)v2;
		if (BaseMath_ReadCallback(color2) == -1)
			return NULL;
	}


	/* make sure v1 is always the vector */
	if (color1 && color2) {
		/* col * col, dont support yet! */
	}
	else if (color1) {
		if (((scalar = PyFloat_AsDouble(v2)) == -1.0f && PyErr_Occurred()) == 0) { /* COLOR * FLOAT */
			return color_mul_float(color1, scalar);
		}
	}
	else if (color2) {
		if (((scalar = PyFloat_AsDouble(v1)) == -1.0f && PyErr_Occurred()) == 0) { /* FLOAT * COLOR */
			return color_mul_float(color2, scalar);
		}
	}
	else {
		BLI_assert(!"internal error");
	}

	PyErr_Format(PyExc_TypeError,
	             "Color multiplication: not supported between "
	             "'%.200s' and '%.200s' types",
	             Py_TYPE(v1)->tp_name, Py_TYPE(v2)->tp_name);
	return NULL;
}

static PyObject *Color_div(PyObject *v1, PyObject *v2)
{
	ColorObject *color1 = NULL;
	float scalar;

	if ColorObject_Check(v1) {
		color1 = (ColorObject *)v1;
		if (BaseMath_ReadCallback(color1) == -1)
			return NULL;
	}
	else {
		PyErr_SetString(PyExc_TypeError,
		                "Color division not supported in this order");
		return NULL;
	}

	/* make sure v1 is always the vector */
	if (((scalar = PyFloat_AsDouble(v2)) == -1.0f && PyErr_Occurred()) == 0) { /* COLOR * FLOAT */
		if (scalar == 0.0f) {
			PyErr_SetString(PyExc_ZeroDivisionError,
			                "Color division: divide by zero error");
			return NULL;
		}
		return color_mul_float(color1, 1.0f / scalar);
	}

	PyErr_Format(PyExc_TypeError,
	             "Color multiplication: not supported between "
	             "'%.200s' and '%.200s' types",
	             Py_TYPE(v1)->tp_name, Py_TYPE(v2)->tp_name);
	return NULL;
}

/* mulplication in-place: obj *= obj */
static PyObject *Color_imul(PyObject *v1, PyObject *v2)
{
	ColorObject *color = (ColorObject *)v1;
	float scalar;

	if (BaseMath_ReadCallback(color) == -1)
		return NULL;

	/* only support color *= float */
	if (((scalar = PyFloat_AsDouble(v2)) == -1.0f && PyErr_Occurred()) == 0) { /* COLOR *= FLOAT */
		mul_vn_fl(color->col, COLOR_SIZE, scalar);
	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "Color multiplication: (%s *= %s) "
		             "invalid type for this operation",
		             Py_TYPE(v1)->tp_name, Py_TYPE(v2)->tp_name);
		return NULL;
	}

	(void)BaseMath_WriteCallback(color);
	Py_INCREF(v1);
	return v1;
}

/* mulplication in-place: obj *= obj */
static PyObject *Color_idiv(PyObject *v1, PyObject *v2)
{
	ColorObject *color = (ColorObject *)v1;
	float scalar;

	if (BaseMath_ReadCallback(color) == -1)
		return NULL;

	/* only support color /= float */
	if (((scalar = PyFloat_AsDouble(v2)) == -1.0f && PyErr_Occurred()) == 0) { /* COLOR /= FLOAT */
		if (scalar == 0.0f) {
			PyErr_SetString(PyExc_ZeroDivisionError,
			                "Color division: divide by zero error");
			return NULL;
		}

		mul_vn_fl(color->col, COLOR_SIZE, 1.0f / scalar);
	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "Color division: (%s /= %s) "
		             "invalid type for this operation",
		             Py_TYPE(v1)->tp_name, Py_TYPE(v2)->tp_name);
		return NULL;
	}

	(void)BaseMath_WriteCallback(color);
	Py_INCREF(v1);
	return v1;
}

/* -obj
  returns the negative of this object */
static PyObject *Color_neg(ColorObject *self)
{
	float tcol[COLOR_SIZE];

	if (BaseMath_ReadCallback(self) == -1)
		return NULL;

	negate_vn_vn(tcol, self->col, COLOR_SIZE);
	return Color_CreatePyObject(tcol, Py_NEW, Py_TYPE(self));
}


static PyNumberMethods Color_NumMethods = {
	(binaryfunc) Color_add,	/*nb_add*/
	(binaryfunc) Color_sub,	/*nb_subtract*/
	(binaryfunc) Color_mul,	/*nb_multiply*/
	NULL,				/*nb_remainder*/
	NULL,				/*nb_divmod*/
	NULL,				/*nb_power*/
	(unaryfunc) Color_neg, /*nb_negative*/
	(unaryfunc) NULL,	/*tp_positive*/
	(unaryfunc) NULL,	/*tp_absolute*/
	(inquiry)	NULL,	/*tp_bool*/
	(unaryfunc)	NULL,	/*nb_invert*/
	NULL,				/*nb_lshift*/
	(binaryfunc)NULL,	/*nb_rshift*/
	NULL,				/*nb_and*/
	NULL,				/*nb_xor*/
	NULL,				/*nb_or*/
	NULL,				/*nb_int*/
	NULL,				/*nb_reserved*/
	NULL,				/*nb_float*/
	Color_iadd,			/* nb_inplace_add */
	Color_isub,			/* nb_inplace_subtract */
	Color_imul,			/* nb_inplace_multiply */
	NULL,				/* nb_inplace_remainder */
	NULL,				/* nb_inplace_power */
	NULL,				/* nb_inplace_lshift */
	NULL,				/* nb_inplace_rshift */
	NULL,				/* nb_inplace_and */
	NULL,				/* nb_inplace_xor */
	NULL,				/* nb_inplace_or */
	NULL,				/* nb_floor_divide */
	Color_div,			/* nb_true_divide */
	NULL,				/* nb_inplace_floor_divide */
	Color_idiv,			/* nb_inplace_true_divide */
	NULL,				/* nb_index */
};

/* color channel, vector.r/g/b */
PyDoc_STRVAR(Color_channel_r_doc, "Red color channel.\n\n:type: float");
PyDoc_STRVAR(Color_channel_g_doc, "Green color channel.\n\n:type: float");
PyDoc_STRVAR(Color_channel_b_doc, "Blue color channel.\n\n:type: float");

static PyObject *Color_channel_get(ColorObject *self, void *type)
{
	return Color_item(self, GET_INT_FROM_POINTER(type));
}

static int Color_channel_set(ColorObject *self, PyObject *value, void * type)
{
	return Color_ass_item(self, GET_INT_FROM_POINTER(type), value);
}

/* color channel (HSV), color.h/s/v */
PyDoc_STRVAR(Color_channel_hsv_h_doc, "HSV Hue component in [0, 1].\n\n:type: float");
PyDoc_STRVAR(Color_channel_hsv_s_doc, "HSV Saturation component in [0, 1].\n\n:type: float");
PyDoc_STRVAR(Color_channel_hsv_v_doc, "HSV Value component in [0, 1].\n\n:type: float");

static PyObject *Color_channel_hsv_get(ColorObject *self, void *type)
{
	float hsv[3];
	int i = GET_INT_FROM_POINTER(type);

	if (BaseMath_ReadCallback(self) == -1)
		return NULL;

	rgb_to_hsv(self->col[0], self->col[1], self->col[2], &(hsv[0]), &(hsv[1]), &(hsv[2]));

	return PyFloat_FromDouble(hsv[i]);
}

static int Color_channel_hsv_set(ColorObject *self, PyObject *value, void * type)
{
	float hsv[3];
	int i = GET_INT_FROM_POINTER(type);
	float f = PyFloat_AsDouble(value);

	if (f == -1 && PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError,
		                "color.h/s/v = value: "
		                "argument not a number");
		return -1;
	}

	if (BaseMath_ReadCallback(self) == -1)
		return -1;

	rgb_to_hsv(self->col[0], self->col[1], self->col[2], &(hsv[0]), &(hsv[1]), &(hsv[2]));
	CLAMP(f, 0.0f, 1.0f);
	hsv[i] = f;
	hsv_to_rgb(hsv[0], hsv[1], hsv[2], &(self->col[0]), &(self->col[1]), &(self->col[2]));

	if (BaseMath_WriteCallback(self) == -1)
		return -1;

	return 0;
}

/* color channel (HSV), color.h/s/v */
PyDoc_STRVAR(Color_hsv_doc, "HSV Values in [0, 1].\n\n:type: float triplet");
static PyObject *Color_hsv_get(ColorObject *self, void *UNUSED(closure))
{
	float hsv[3];
	PyObject *ret;

	if (BaseMath_ReadCallback(self) == -1)
		return NULL;

	rgb_to_hsv(self->col[0], self->col[1], self->col[2], &(hsv[0]), &(hsv[1]), &(hsv[2]));

	ret = PyTuple_New(3);
	PyTuple_SET_ITEM(ret, 0, PyFloat_FromDouble(hsv[0]));
	PyTuple_SET_ITEM(ret, 1, PyFloat_FromDouble(hsv[1]));
	PyTuple_SET_ITEM(ret, 2, PyFloat_FromDouble(hsv[2]));
	return ret;
}

static int Color_hsv_set(ColorObject *self, PyObject *value, void *UNUSED(closure))
{
	float hsv[3];

	if (mathutils_array_parse(hsv, 3, 3, value, "mathutils.Color.hsv = value") == -1)
		return -1;

	CLAMP(hsv[0], 0.0f, 1.0f);
	CLAMP(hsv[1], 0.0f, 1.0f);
	CLAMP(hsv[2], 0.0f, 1.0f);

	hsv_to_rgb(hsv[0], hsv[1], hsv[2], &(self->col[0]), &(self->col[1]), &(self->col[2]));

	if (BaseMath_WriteCallback(self) == -1)
		return -1;

	return 0;
}

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef Color_getseters[] = {
	{(char *)"r", (getter)Color_channel_get, (setter)Color_channel_set, Color_channel_r_doc, (void *)0},
	{(char *)"g", (getter)Color_channel_get, (setter)Color_channel_set, Color_channel_g_doc, (void *)1},
	{(char *)"b", (getter)Color_channel_get, (setter)Color_channel_set, Color_channel_b_doc, (void *)2},

	{(char *)"h", (getter)Color_channel_hsv_get, (setter)Color_channel_hsv_set, (char *)Color_channel_hsv_h_doc, (void *)0},
	{(char *)"s", (getter)Color_channel_hsv_get, (setter)Color_channel_hsv_set, (char *)Color_channel_hsv_s_doc, (void *)1},
	{(char *)"v", (getter)Color_channel_hsv_get, (setter)Color_channel_hsv_set, (char *)Color_channel_hsv_v_doc, (void *)2},

	{(char *)"hsv", (getter)Color_hsv_get, (setter)Color_hsv_set, (char *)Color_hsv_doc, (void *)0},

	{(char *)"is_wrapped", (getter)BaseMathObject_is_wrapped_get, (setter)NULL, BaseMathObject_is_wrapped_doc, NULL},
	{(char *)"owner", (getter)BaseMathObject_owner_get, (setter)NULL, BaseMathObject_owner_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};


//-----------------------METHOD DEFINITIONS ----------------------
static struct PyMethodDef Color_methods[] = {
	{"__copy__", (PyCFunction) Color_copy, METH_NOARGS, Color_copy_doc},
	{"copy", (PyCFunction) Color_copy, METH_NOARGS, Color_copy_doc},
	{NULL, NULL, 0, NULL}
};

//------------------PY_OBECT DEFINITION--------------------------
PyDoc_STRVAR(color_doc,
"This object gives access to Colors in Blender."
);
PyTypeObject color_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"mathutils.Color",				//tp_name
	sizeof(ColorObject),			//tp_basicsize
	0,								//tp_itemsize
	(destructor)BaseMathObject_dealloc,		//tp_dealloc
	NULL,							//tp_print
	NULL,							//tp_getattr
	NULL,							//tp_setattr
	NULL,							//tp_compare
	(reprfunc) Color_repr,			//tp_repr
	&Color_NumMethods,				//tp_as_number
	&Color_SeqMethods,				//tp_as_sequence
	&Color_AsMapping,				//tp_as_mapping
	NULL,							//tp_hash
	NULL,							//tp_call
	(reprfunc) Color_str,			//tp_str
	NULL,							//tp_getattro
	NULL,							//tp_setattro
	NULL,							//tp_as_buffer
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, //tp_flags
	color_doc, //tp_doc
	(traverseproc)BaseMathObject_traverse,	//tp_traverse
	(inquiry)BaseMathObject_clear,	//tp_clear
	(richcmpfunc)Color_richcmpr,	//tp_richcompare
	0,								//tp_weaklistoffset
	NULL,							//tp_iter
	NULL,							//tp_iternext
	Color_methods,					//tp_methods
	NULL,							//tp_members
	Color_getseters,				//tp_getset
	NULL,							//tp_base
	NULL,							//tp_dict
	NULL,							//tp_descr_get
	NULL,							//tp_descr_set
	0,								//tp_dictoffset
	NULL,							//tp_init
	NULL,							//tp_alloc
	Color_new,						//tp_new
	NULL,							//tp_free
	NULL,							//tp_is_gc
	NULL,							//tp_bases
	NULL,							//tp_mro
	NULL,							//tp_cache
	NULL,							//tp_subclasses
	NULL,							//tp_weaklist
	NULL							//tp_del
};
//------------------------Color_CreatePyObject (internal)-------------
//creates a new color object
/*pass Py_WRAP - if vector is a WRAPPER for data allocated by BLENDER
 (i.e. it was allocated elsewhere by MEM_mallocN())
  pass Py_NEW - if vector is not a WRAPPER and managed by PYTHON
 (i.e. it must be created here with PyMEM_malloc())*/
PyObject *Color_CreatePyObject(float *col, int type, PyTypeObject *base_type)
{
	ColorObject *self;

	self = base_type ?	(ColorObject *)base_type->tp_alloc(base_type, 0) :
						(ColorObject *)PyObject_GC_New(ColorObject, &color_Type);

	if (self) {
		/* init callbacks as NULL */
		self->cb_user = NULL;
		self->cb_type = self->cb_subtype = 0;

		if (type == Py_WRAP) {
			self->col = col;
			self->wrapped = Py_WRAP;
		}
		else if (type == Py_NEW) {
			self->col = PyMem_Malloc(COLOR_SIZE * sizeof(float));
			if (col)
				copy_v3_v3(self->col, col);
			else
				zero_v3(self->col);

			self->wrapped = Py_NEW;
		}
		else {
			Py_FatalError("Color(): invalid type!");
		}
	}

	return (PyObject *)self;
}

PyObject *Color_CreatePyObject_cb(PyObject *cb_user, int cb_type, int cb_subtype)
{
	ColorObject *self = (ColorObject *)Color_CreatePyObject(NULL, Py_NEW, NULL);
	if (self) {
		Py_INCREF(cb_user);
		self->cb_user =			cb_user;
		self->cb_type =			(unsigned char)cb_type;
		self->cb_subtype =		(unsigned char)cb_subtype;
		PyObject_GC_Track(self);
	}

	return (PyObject *)self;
}
