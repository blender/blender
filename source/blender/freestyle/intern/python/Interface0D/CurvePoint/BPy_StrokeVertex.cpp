#include "BPy_StrokeVertex.h"

#include "../../BPy_Freestyle.h"
#include "../../BPy_Convert.h"
#include "../../BPy_StrokeAttribute.h"
#include "../../Interface0D/BPy_SVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "../../../python/mathutils/mathutils.h" /* for Vector callbacks */

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char StrokeVertex___doc__[] =
"Class hierarchy: :class:`Interface0D` > :class:`CurvePoint` > :class:`StrokeVertex`\n"
"\n"
"Class to define a stroke vertex.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(iBrother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg iBrother: A StrokeVertex object.\n"
"   :type iBrother: :class:`StrokeVertex`\n"
"\n"
".. method:: __init__(iA, iB, t3)\n"
"\n"
"   Builds a stroke vertex from 2 stroke vertices and an interpolation\n"
"   parameter.\n"
"\n"
"   :arg iA: The first StrokeVertex.\n"
"   :type iA: :class:`StrokeVertex`\n"
"   :arg iB: The second StrokeVertex.\n"
"   :type iB: :class:`StrokeVertex`\n"
"   :arg t3: An interpolation parameter.\n"
"   :type t3: float\n"
"\n"
".. method:: __init__(iPoint)\n"
"\n"
"   Builds a stroke vertex from a CurvePoint\n"
"\n"
"   :arg iPoint: A CurvePoint object.\n"
"   :type iPoint: :class:`CurvePoint`\n"
"\n"
".. method:: __init__(iSVertex)\n"
"\n"
"   Builds a stroke vertex from a SVertex\n"
"\n"
"   :arg iSVertex: An SVertex object.\n"
"   :type iSVertex: :class:`SVertex`\n"
"\n"
".. method:: __init__(iSVertex, iAttribute)\n"
"\n"
"   Builds a stroke vertex from an SVertex and a StrokeAttribute object.\n"
"\n"
"   :arg iSVertex: An SVertex object.\n"
"   :type iSVertex: :class:`SVertex`\n"
"   :arg iAttribute: A StrokeAttribute object.\n"
"   :type iAttribute: :class:`StrokeAttribute`\n";

static int StrokeVertex___init__(BPy_StrokeVertex *self, PyObject *args, PyObject *kwds)
{

	PyObject *obj1 = 0, *obj2 = 0 , *obj3 = 0;

    if (! PyArg_ParseTuple(args, "|OOO!", &obj1, &obj2, &PyFloat_Type, &obj3) )
        return -1;

	if( !obj1 ){
		self->sv = new StrokeVertex();
		
	} else if( !obj2 && BPy_StrokeVertex_Check(obj1) && ((BPy_StrokeVertex *) obj1)->sv ) {
		self->sv = new StrokeVertex( *(((BPy_StrokeVertex *) obj1)->sv) );

	} else if( !obj2 && BPy_CurvePoint_Check(obj1) && ((BPy_CurvePoint *) obj1)->cp ) {
		self->sv = new StrokeVertex( ((BPy_CurvePoint *) obj1)->cp );
	
	} else if( !obj2 && BPy_SVertex_Check(obj1) && ((BPy_SVertex *) obj1)->sv ) {
		self->sv = new StrokeVertex( ((BPy_SVertex *) obj1)->sv );
	
	} else if( obj3 && BPy_StrokeVertex_Check(obj1) && BPy_StrokeVertex_Check(obj2) ) {
		StrokeVertex *sv1 = ((BPy_StrokeVertex *) obj1)->sv;
		StrokeVertex *sv2 = ((BPy_StrokeVertex *) obj2)->sv;
		if( !sv1 || ( sv1->A() == 0 && sv1->B() == 0 ) ) {
			PyErr_SetString(PyExc_TypeError, "argument 1 is an invalid StrokeVertex object");
			return -1;
		}
		if( !sv2 || ( sv2->A() == 0 && sv2->B() == 0 ) ) {
			PyErr_SetString(PyExc_TypeError, "argument 2 is an invalid StrokeVertex object");
			return -1;
		}
		self->sv = new StrokeVertex( sv1, sv2, PyFloat_AsDouble( obj3 ) );

	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
		return -1;
	}

	self->py_cp.cp = self->sv;
	self->py_cp.py_if0D.if0D = self->sv;
	self->py_cp.py_if0D.borrowed = 0;

	return 0;
}

// real 	operator[] (const int i) const
// real & 	operator[] (const int i)

/*----------------------StrokeVertex instance definitions ----------------------------*/
static PyMethodDef BPy_StrokeVertex_methods[] = {	
	{NULL, NULL, 0, NULL}
};

/*----------------------mathutils callbacks ----------------------------*/

static int StrokeVertex_mathutils_check(BaseMathObject *bmo)
{
	if (!BPy_StrokeVertex_Check(bmo->cb_user))
		return -1;
	return 0;
}

static int StrokeVertex_mathutils_get(BaseMathObject *bmo, int subtype)
{
	BPy_StrokeVertex *self = (BPy_StrokeVertex *)bmo->cb_user;
	bmo->data[0] = (float)self->sv->x();
	bmo->data[1] = (float)self->sv->y();
	return 0;
}

static int StrokeVertex_mathutils_set(BaseMathObject *bmo, int subtype)
{
	BPy_StrokeVertex *self = (BPy_StrokeVertex *)bmo->cb_user;
	self->sv->setX((real)bmo->data[0]);
	self->sv->setY((real)bmo->data[1]);
	return 0;
}

static int StrokeVertex_mathutils_get_index(BaseMathObject *bmo, int subtype, int index)
{
	BPy_StrokeVertex *self = (BPy_StrokeVertex *)bmo->cb_user;
	switch (index) {
	case 0: bmo->data[0] = (float)self->sv->x(); break;
	case 1: bmo->data[1] = (float)self->sv->y(); break;
	default:
		return -1;
	}
	return 0;
}

static int StrokeVertex_mathutils_set_index(BaseMathObject *bmo, int subtype, int index)
{
	BPy_StrokeVertex *self = (BPy_StrokeVertex *)bmo->cb_user;
	switch (index) {
	case 0: self->sv->setX((real)bmo->data[0]); break;
	case 1: self->sv->setY((real)bmo->data[1]); break;
	default:
		return -1;
	}
	return 0;
}

static Mathutils_Callback StrokeVertex_mathutils_cb = {
	StrokeVertex_mathutils_check,
	StrokeVertex_mathutils_get,
	StrokeVertex_mathutils_set,
	StrokeVertex_mathutils_get_index,
	StrokeVertex_mathutils_set_index
};

static unsigned char StrokeVertex_mathutils_cb_index = -1;

void StrokeVertex_mathutils_register_callback()
{
	StrokeVertex_mathutils_cb_index = Mathutils_RegisterCallback(&StrokeVertex_mathutils_cb);
}

/*----------------------StrokeVertex get/setters ----------------------------*/

PyDoc_STRVAR(StrokeVertex_attribute_doc,
"StrokeAttribute for this StrokeVertex.\n"
"\n"
":type: StrokeAttribute");

static PyObject *StrokeVertex_attribute_get(BPy_StrokeVertex *self, void *UNUSED(closure))
{
	return BPy_StrokeAttribute_from_StrokeAttribute(self->sv->attribute());
}

static int StrokeVertex_attribute_set(BPy_StrokeVertex *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_StrokeAttribute_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be a StrokeAttribute object");
		return -1;
	}
	self->sv->setAttribute(*(((BPy_StrokeAttribute *)value)->sa));
	return 0;
}

PyDoc_STRVAR(StrokeVertex_curvilinear_abscissa_doc,
"Curvilinear abscissa of this StrokeVertex in the Stroke.\n"
"\n"
":type: float");

static PyObject *StrokeVertex_curvilinear_abscissa_get(BPy_StrokeVertex *self, void *UNUSED(closure))
{
	return PyFloat_FromDouble(self->sv->curvilinearAbscissa());
}

static int StrokeVertex_curvilinear_abscissa_set(BPy_StrokeVertex *self, PyObject *value, void *UNUSED(closure))
{
	float scalar;
	if ((scalar = PyFloat_AsDouble(value)) == -1.0f && PyErr_Occurred()) { /* parsed item not a number */
		PyErr_SetString(PyExc_TypeError, "value must be a number");
		return -1;
	}
	self->sv->setCurvilinearAbscissa(scalar);
	return 0;
}

PyDoc_STRVAR(StrokeVertex_point_doc,
"2D point coordinates.\n"
"\n"
":type: mathutils.Vector");

static PyObject *StrokeVertex_point_get(BPy_StrokeVertex *self, void *UNUSED(closure))
{
	return Vector_CreatePyObject_cb((PyObject *)self, 2, StrokeVertex_mathutils_cb_index, 0);
}

static int StrokeVertex_point_set(BPy_StrokeVertex *self, PyObject *value, void *UNUSED(closure))
{
	float v[2];
	if (!float_array_from_PyObject(value, v, 2)) {
		PyErr_SetString(PyExc_ValueError, "value must be a 2-dimensional vector");
		return -1;
	}
	self->sv->setX(v[0]);
	self->sv->setY(v[1]);
	return 0;
}

PyDoc_STRVAR(StrokeVertex_stroke_length_doc,
"Stroke length (it is only a value retained by the StrokeVertex,\n"
"and it won't change the real stroke length).\n"
"\n"
":type: float");

static PyObject *StrokeVertex_stroke_length_get(BPy_StrokeVertex *self, void *UNUSED(closure))
{
	return PyFloat_FromDouble(self->sv->strokeLength());
}

static int StrokeVertex_stroke_length_set(BPy_StrokeVertex *self, PyObject *value, void *UNUSED(closure))
{
	float scalar;
	if ((scalar = PyFloat_AsDouble(value)) == -1.0f && PyErr_Occurred()) { /* parsed item not a number */
		PyErr_SetString(PyExc_TypeError, "value must be a number");
		return -1;
	}
	self->sv->setStrokeLength(scalar);
	return 0;
}

PyDoc_STRVAR(StrokeVertex_u_doc,
"Curvilinear abscissa of this StrokeVertex in the Stroke.\n"
"\n"
":type: float");

static PyObject *StrokeVertex_u_get(BPy_StrokeVertex *self, void *UNUSED(closure))
{
	return PyFloat_FromDouble(self->sv->u());
}

static PyGetSetDef BPy_StrokeVertex_getseters[] = {
	{(char *)"attribute", (getter)StrokeVertex_attribute_get, (setter)StrokeVertex_attribute_set, (char *)StrokeVertex_attribute_doc, NULL},
	{(char *)"curvilinear_abscissa", (getter)StrokeVertex_curvilinear_abscissa_get, (setter)StrokeVertex_curvilinear_abscissa_set, (char *)StrokeVertex_curvilinear_abscissa_doc, NULL},
	{(char *)"point", (getter)StrokeVertex_point_get, (setter)StrokeVertex_point_set, (char *)StrokeVertex_point_doc, NULL},
	{(char *)"stroke_length", (getter)StrokeVertex_stroke_length_get, (setter)StrokeVertex_stroke_length_set, (char *)StrokeVertex_stroke_length_doc, NULL},
	{(char *)"u", (getter)StrokeVertex_u_get, (setter)NULL, (char *)StrokeVertex_u_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

/*-----------------------BPy_StrokeVertex type definition ------------------------------*/
PyTypeObject StrokeVertex_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"StrokeVertex",                 /* tp_name */
	sizeof(BPy_StrokeVertex),       /* tp_basicsize */
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
	StrokeVertex___doc__,           /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_StrokeVertex_methods,       /* tp_methods */
	0,                              /* tp_members */
	BPy_StrokeVertex_getseters,     /* tp_getset */
	&CurvePoint_Type,               /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)StrokeVertex___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
