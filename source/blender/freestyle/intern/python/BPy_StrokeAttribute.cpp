#include "BPy_StrokeAttribute.h"

#include "BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int StrokeAttribute_Init( PyObject *module )
{
	if( module == NULL )
		return -1;

	if( PyType_Ready( &StrokeAttribute_Type ) < 0 )
		return -1;
	Py_INCREF( &StrokeAttribute_Type );
	PyModule_AddObject(module, "StrokeAttribute", (PyObject *)&StrokeAttribute_Type);

	StrokeAttribute_mathutils_register_callback();
	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(StrokeAttribute_doc,
"Class to define a set of attributes associated with a :class:`StrokeVertex`.\n"
"The attribute set stores the color, alpha and thickness values for a Stroke\n"
"Vertex.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(iBrother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg iBrother: A StrokeAttribute object.\n"
"   :type iBrother: :class:`StrokeAttribute`\n"
"\n"
".. method:: __init__(iRColor, iGColor, iBColor, iAlpha, iRThickness, iLThickness)\n"
"\n"
"   Builds a stroke vertex attribute from a set of parameters.\n"
"\n"
"   :arg iRColor: Red component of a stroke color.\n"
"   :type iRColor: float\n"
"   :arg iGColor: Green component of a stroke color.\n"
"   :type iGColor: float\n"
"   :arg iBColor: Blue component of a stroke color.\n"
"   :type iBColor: float\n"
"   :arg iAlpha: Alpha component of a stroke color.\n"
"   :type iAlpha: float\n"
"   :arg iRThickness: Stroke thickness on the right.\n"
"   :type iRThickness: float\n"
"   :arg iLThickness: Stroke thickness on the left.\n"
"   :type iLThickness: float\n"
"\n"
".. method:: __init__(a1, a2, t)\n"
"\n"
"   Interpolation constructor. Builds a StrokeAttribute from two\n"
"   StrokeAttribute objects and an interpolation parameter.\n"
"\n"
"   :arg a1: The first StrokeAttribute object.\n"
"   :type a1: :class:`StrokeAttribute`\n"
"   :arg a2: The second StrokeAttribute object.\n"
"   :type a2: :class:`StrokeAttribute`\n"
"   :arg t: The interpolation parameter.\n"
"   :type t: float\n");

static int StrokeAttribute___init__(BPy_StrokeAttribute *self, PyObject *args, PyObject *kwds)
{

	PyObject *obj1 = 0, *obj2 = 0 , *obj3 = 0, *obj4 = 0, *obj5 = 0 , *obj6 = 0;

	if (! PyArg_ParseTuple(args, "|OOOOOO", &obj1, &obj2, &obj3, &obj4, &obj5, &obj6) )
		return -1;

	if ( !obj1 ) {

		self->sa = new StrokeAttribute();

	} else if ( BPy_StrokeAttribute_Check(obj1) && !obj2 ) {	

		self->sa = new StrokeAttribute(	*( ((BPy_StrokeAttribute *) obj1)->sa ) );

	} else if ( BPy_StrokeAttribute_Check(obj1) && 
				BPy_StrokeAttribute_Check(obj2) &&
				PyFloat_Check(obj3) && !obj4 ) {	

		self->sa = new StrokeAttribute(	*( ((BPy_StrokeAttribute *) obj1)->sa ),
										*( ((BPy_StrokeAttribute *) obj2)->sa ),
										PyFloat_AsDouble( obj3 ) );

	} else if ( obj6 ) {

		self->sa = new StrokeAttribute(	PyFloat_AsDouble( obj1 ),
										PyFloat_AsDouble( obj2 ),
										PyFloat_AsDouble( obj3 ),
										PyFloat_AsDouble( obj4 ),
										PyFloat_AsDouble( obj5 ),
										PyFloat_AsDouble( obj6 ) );

	} else {
		PyErr_SetString(PyExc_TypeError, "invalid arguments");
		return -1;
	}

	self->borrowed = 0;

	return 0;

}

static void StrokeAttribute___dealloc__(BPy_StrokeAttribute* self)
{
	if( self->sa && !self->borrowed )
		delete self->sa;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * StrokeAttribute___repr__(BPy_StrokeAttribute* self)
{
	stringstream repr("StrokeAttribute:");
	repr << " r: " << self->sa->getColorR()
		 << " g: " << self->sa->getColorG()
		 << " b: " << self->sa->getColorB()
		 << " a: " << self->sa->getAlpha()
		 << " - R: " << self->sa->getThicknessR() 
		 << " L: " << self->sa->getThicknessL();

	return PyUnicode_FromString( repr.str().c_str() );
}

PyDoc_STRVAR(StrokeAttribute_get_attribute_real_doc,
".. method:: get_attribute_real(iName)\n"
"\n"
"   Returns an attribute of float type.\n"
"\n"
"   :arg iName: The name of the attribute.\n"
"   :type iName: str\n"
"   :return: The attribute value.\n"
"   :rtype: float\n");

static PyObject *StrokeAttribute_get_attribute_real( BPy_StrokeAttribute *self, PyObject *args ) {
	char *attr;

	if(!( PyArg_ParseTuple(args, "s", &attr) ))
		return NULL;

	double a = self->sa->getAttributeReal( attr );
	return PyFloat_FromDouble( a );
}

PyDoc_STRVAR(StrokeAttribute_get_attribute_vec2_doc,
".. method:: get_attribute_vec2(iName)\n"
"\n"
"   Returns an attribute of two-dimensional vector type.\n"
"\n"
"   :arg iName: The name of the attribute.\n"
"   :type iName: str\n"
"   :return: The attribute value.\n"
"   :rtype: :class:`mathutils.Vector`\n");

static PyObject *StrokeAttribute_get_attribute_vec2( BPy_StrokeAttribute *self, PyObject *args ) {
	char *attr;

	if(!( PyArg_ParseTuple(args, "s", &attr) ))
		return NULL;

	Vec2f a = self->sa->getAttributeVec2f( attr );
	return Vector_from_Vec2f( a );
}

PyDoc_STRVAR(StrokeAttribute_get_attribute_vec3_doc,
".. method:: get_attribute_vec3(iName)\n"
"\n"
"   Returns an attribute of three-dimensional vector type.\n"
"\n"
"   :arg iName: The name of the attribute.\n"
"   :type iName: str\n"
"   :return: The attribute value.\n"
"   :rtype: :class:`mathutils.Vector`\n");

static PyObject *StrokeAttribute_get_attribute_vec3( BPy_StrokeAttribute *self, PyObject *args ) {
	char *attr;

	if(!( PyArg_ParseTuple(args, "s", &attr) ))
		return NULL;

	Vec3f a = self->sa->getAttributeVec3f( attr );
	return Vector_from_Vec3f( a );
}

PyDoc_STRVAR(StrokeAttribute_has_attribute_real_doc,
".. method:: has_attribute_real(iName)\n"
"\n"
"   Checks whether the attribute iName of float type is available.\n"
"\n"
"   :arg iName: The name of the attribute.\n"
"   :type iName: str\n"
"   :return: True if the attribute is availbale.\n"
"   :rtype: bool\n");

static PyObject *StrokeAttribute_has_attribute_real( BPy_StrokeAttribute *self, PyObject *args ) {
	char *attr;

	if(!( PyArg_ParseTuple(args, "s", &attr) ))
		return NULL;

	return PyBool_from_bool( self->sa->isAttributeAvailableReal( attr ) );
}

PyDoc_STRVAR(StrokeAttribute_has_attribute_vec2_doc,
".. method:: has_attribute_vec2(iName)\n"
"\n"
"   Checks whether the attribute iName of two-dimensional vector type\n"
"   is available.\n"
"\n"
"   :arg iName: The name of the attribute.\n"
"   :type iName: str\n"
"   :return: True if the attribute is availbale.\n"
"   :rtype: bool\n");

static PyObject *StrokeAttribute_has_attribute_vec2( BPy_StrokeAttribute *self, PyObject *args ) {
	char *attr;

	if(!( PyArg_ParseTuple(args, "s", &attr) ))
		return NULL;

	return PyBool_from_bool( self->sa->isAttributeAvailableVec2f( attr ) );
}

PyDoc_STRVAR(StrokeAttribute_has_attribute_vec3_doc,
".. method:: has_attribute_vec3(iName)\n"
"\n"
"   Checks whether the attribute iName of three-dimensional vector\n"
"   type is available.\n"
"\n"
"   :arg iName: The name of the attribute.\n"
"   :type iName: str\n"
"   :return: True if the attribute is availbale.\n"
"   :rtype: bool\n");

static PyObject *StrokeAttribute_has_attribute_vec3( BPy_StrokeAttribute *self, PyObject *args ) {
	char *attr;

	if(!( PyArg_ParseTuple(args, "s", &attr) ))
		return NULL;

	return PyBool_from_bool( self->sa->isAttributeAvailableVec3f( attr ) );
}

PyDoc_STRVAR(StrokeAttribute_set_attribute_real_doc,
".. method:: set_attribute_real(iName, att)\n"
"\n"
"   Adds a user-defined attribute of float type.  If there is no\n"
"   attribute of the given name, it is added.  Otherwise, the new value\n"
"   replaces the old one.\n"
"\n"
"   :arg iName: The name of the attribute.\n"
"   :type iName: str\n"
"   :arg att: The attribute value.\n"
"   :type att: float\n");

static PyObject * StrokeAttribute_set_attribute_real( BPy_StrokeAttribute *self, PyObject *args ) {
	char *s = 0;
	double d = 0;

	if(!( PyArg_ParseTuple(args, "sd", &s, &d) ))
		return NULL;

	self->sa->setAttributeReal( s, d );
	Py_RETURN_NONE;
}

PyDoc_STRVAR(StrokeAttribute_set_attribute_vec2_doc,
".. method:: set_attribute_vec2(iName, att)\n"
"\n"
"   Adds a user-defined attribute of two-dimensional vector type.  If\n"
"   there is no attribute of the given name, it is added.  Otherwise,\n"
"   the new value replaces the old one.\n"
"\n"
"   :arg iName: The name of the attribute.\n"
"   :type iName: str\n"
"   :arg att: The attribute value.\n"
"   :type att: :class:`mathutils.Vector`, list or tuple of 2 real numbers\n");

static PyObject * StrokeAttribute_set_attribute_vec2( BPy_StrokeAttribute *self, PyObject *args ) {
	char *s;
	PyObject *obj = 0;

	if(!( PyArg_ParseTuple(args, "sO", &s, &obj) ))
		return NULL;
	Vec2f *v = Vec2f_ptr_from_PyObject(obj);
	if( !v ) {
		PyErr_SetString(PyExc_TypeError, "argument 2 must be a 2D vector (either a list of 2 elements or Vector)");
		return NULL;
	}
	self->sa->setAttributeVec2f( s, *v );
	delete v;

	Py_RETURN_NONE;
}

PyDoc_STRVAR(StrokeAttribute_set_attribute_vec3_doc,
".. method:: set_attribute_vec3(iName, att)\n"
"\n"
"   Adds a user-defined attribute of three-dimensional vector type.\n"
"   If there is no attribute of the given name, it is added.\n"
"   Otherwise, the new value replaces the old one.\n"
"\n"
"   :arg iName: The name of the attribute.\n"
"   :type iName: str\n"
"   :arg att: The attribute value.\n"
"   :type att: :class:`mathutils.Vector`, list or tuple of 3 real numbers\n");

static PyObject * StrokeAttribute_set_attribute_vec3( BPy_StrokeAttribute *self, PyObject *args ) {
	char *s;
	PyObject *obj = 0;

	if(!( PyArg_ParseTuple(args, "sO", &s, &obj) ))
		return NULL;
	Vec3f *v = Vec3f_ptr_from_PyObject(obj);
	if( !v ) {
		PyErr_SetString(PyExc_TypeError, "argument 2 must be a 3D vector (either a list of 3 elements or Vector)");
		return NULL;
	}
	self->sa->setAttributeVec3f( s, *v );
	delete v;

	Py_RETURN_NONE;
}

/*----------------------StrokeAttribute instance definitions ----------------------------*/
static PyMethodDef BPy_StrokeAttribute_methods[] = {
	{"get_attribute_real", ( PyCFunction ) StrokeAttribute_get_attribute_real, METH_VARARGS, StrokeAttribute_get_attribute_real_doc},
	{"get_attribute_vec2", ( PyCFunction ) StrokeAttribute_get_attribute_vec2, METH_VARARGS, StrokeAttribute_get_attribute_vec2_doc},
	{"get_attribute_vec3", ( PyCFunction ) StrokeAttribute_get_attribute_vec3, METH_VARARGS, StrokeAttribute_get_attribute_vec3_doc},
	{"has_attribute_real", ( PyCFunction ) StrokeAttribute_has_attribute_real, METH_VARARGS, StrokeAttribute_has_attribute_real_doc},
	{"has_attribute_vec2", ( PyCFunction ) StrokeAttribute_has_attribute_vec2, METH_VARARGS, StrokeAttribute_has_attribute_vec2_doc},
	{"has_attribute_vec3", ( PyCFunction ) StrokeAttribute_has_attribute_vec3, METH_VARARGS, StrokeAttribute_has_attribute_vec3_doc},
	{"set_attribute_real", ( PyCFunction ) StrokeAttribute_set_attribute_real, METH_VARARGS, StrokeAttribute_set_attribute_real_doc},
	{"set_attribute_vec2", ( PyCFunction ) StrokeAttribute_set_attribute_vec2, METH_VARARGS, StrokeAttribute_set_attribute_vec2_doc},
	{"set_attribute_vec3", ( PyCFunction ) StrokeAttribute_set_attribute_vec3, METH_VARARGS, StrokeAttribute_set_attribute_vec3_doc},
	{NULL, NULL, 0, NULL}
};

/*----------------------mathutils callbacks ----------------------------*/

/* subtype */
#define MATHUTILS_SUBTYPE_COLOR      1
#define MATHUTILS_SUBTYPE_THICKNESS  2

static int StrokeAttribute_mathutils_check(BaseMathObject *bmo)
{
	if (!BPy_StrokeAttribute_Check(bmo->cb_user))
		return -1;
	return 0;
}

static int StrokeAttribute_mathutils_get(BaseMathObject *bmo, int subtype)
{
	BPy_StrokeAttribute *self = (BPy_StrokeAttribute *)bmo->cb_user;
	switch (subtype) {
	case MATHUTILS_SUBTYPE_COLOR:
		bmo->data[0] = self->sa->getColorR();
		bmo->data[1] = self->sa->getColorG();
		bmo->data[2] = self->sa->getColorB();
		break;
	case MATHUTILS_SUBTYPE_THICKNESS:
		bmo->data[0] = self->sa->getThicknessR();
		bmo->data[1] = self->sa->getThicknessL();
		break;
	default:
		return -1;
	}
	return 0;
}

static int StrokeAttribute_mathutils_set(BaseMathObject *bmo, int subtype)
{
	BPy_StrokeAttribute *self = (BPy_StrokeAttribute *)bmo->cb_user;
	switch (subtype) {
	case MATHUTILS_SUBTYPE_COLOR:
		self->sa->setColor(bmo->data[0], bmo->data[1], bmo->data[2]);
		break;
	case MATHUTILS_SUBTYPE_THICKNESS:
		self->sa->setThickness(bmo->data[0], bmo->data[1]);
		break;
	default:
		return -1;
	}
	return 0;
}

static int StrokeAttribute_mathutils_get_index(BaseMathObject *bmo, int subtype, int index)
{
	BPy_StrokeAttribute *self = (BPy_StrokeAttribute *)bmo->cb_user;
	switch (subtype) {
	case MATHUTILS_SUBTYPE_COLOR:
		switch (index) {
		case 0: bmo->data[0] = self->sa->getColorR(); break;
		case 1: bmo->data[1] = self->sa->getColorG(); break;
		case 2: bmo->data[2] = self->sa->getColorB(); break;
		default:
			return -1;
		}
		break;
	case MATHUTILS_SUBTYPE_THICKNESS:
		switch (index) {
		case 0: bmo->data[0] = self->sa->getThicknessR(); break;
		case 1: bmo->data[1] = self->sa->getThicknessL(); break;
		default:
			return -1;
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static int StrokeAttribute_mathutils_set_index(BaseMathObject *bmo, int subtype, int index)
{
	BPy_StrokeAttribute *self = (BPy_StrokeAttribute *)bmo->cb_user;
	switch (subtype) {
	case MATHUTILS_SUBTYPE_COLOR:
		{
			float r = (index == 0) ? bmo->data[0] : self->sa->getColorR();
			float g = (index == 1) ? bmo->data[1] : self->sa->getColorG();
			float b = (index == 2) ? bmo->data[2] : self->sa->getColorB();
			self->sa->setColor(r, g, b);
		}
		break;
	case MATHUTILS_SUBTYPE_THICKNESS:
		{
			float tr = (index == 0) ? bmo->data[0] : self->sa->getThicknessR();
			float tl = (index == 1) ? bmo->data[1] : self->sa->getThicknessL();
			self->sa->setThickness(tr, tl);
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static Mathutils_Callback StrokeAttribute_mathutils_cb = {
	StrokeAttribute_mathutils_check,
	StrokeAttribute_mathutils_get,
	StrokeAttribute_mathutils_set,
	StrokeAttribute_mathutils_get_index,
	StrokeAttribute_mathutils_set_index
};

static unsigned char StrokeAttribute_mathutils_cb_index = -1;

void StrokeAttribute_mathutils_register_callback()
{
	StrokeAttribute_mathutils_cb_index = Mathutils_RegisterCallback(&StrokeAttribute_mathutils_cb);
}

/*----------------------StrokeAttribute get/setters ----------------------------*/

PyDoc_STRVAR(StrokeAttribute_alpha_doc,
"Alpha component of the stroke color.\n"
"\n"
":type: float");

static PyObject *StrokeAttribute_alpha_get(BPy_StrokeAttribute *self, void *UNUSED(closure))
{
	return PyFloat_FromDouble(self->sa->getAlpha());
}

static int StrokeAttribute_alpha_set(BPy_StrokeAttribute *self, PyObject *value, void *UNUSED(closure))
{
	float scalar;
	if ((scalar = PyFloat_AsDouble(value)) == -1.0f && PyErr_Occurred()) { /* parsed item not a number */
		PyErr_SetString(PyExc_TypeError, "value must be a number");
		return -1;
	}
	self->sa->setAlpha(scalar);
	return 0;
}

PyDoc_STRVAR(StrokeAttribute_color_doc,
"RGB components of the stroke color.\n"
"\n"
":type: mathutils.Color");

static PyObject *StrokeAttribute_color_get(BPy_StrokeAttribute *self, void *UNUSED(closure))
{
	return Color_CreatePyObject_cb((PyObject *)self, StrokeAttribute_mathutils_cb_index, MATHUTILS_SUBTYPE_COLOR);
}

static int StrokeAttribute_color_set(BPy_StrokeAttribute *self, PyObject *value, void *UNUSED(closure))
{
	Vec3f *v = Vec3f_ptr_from_PyObject(value);
	if (!v) {
		PyErr_SetString(PyExc_ValueError, "value must be a 3-dimensional vector");
		return -1;
	}
	self->sa->setColor(v->x(), v->y(), v->z());
	return 0;
}

PyDoc_STRVAR(StrokeAttribute_thickness_doc,
"Right and left components of the stroke thickness.\n"
"The right (left) component is the thickness on the right (left) of the vertex\n"
"when following the stroke.\n"
"\n"
":type: mathutils.Vector");

static PyObject *StrokeAttribute_thickness_get(BPy_StrokeAttribute *self, void *UNUSED(closure))
{
	return Vector_CreatePyObject_cb((PyObject *)self, 2, StrokeAttribute_mathutils_cb_index, MATHUTILS_SUBTYPE_THICKNESS);
}

static int StrokeAttribute_thickness_set(BPy_StrokeAttribute *self, PyObject *value, void *UNUSED(closure))
{
	Vec2f *v = Vec2f_ptr_from_PyObject(value);
	if (!v) {
		PyErr_SetString(PyExc_ValueError, "value must be a 2-dimensional vector");
		return -1;
	}
	self->sa->setThickness(v->x(), v->y());
	return 0;
}

PyDoc_STRVAR(StrokeAttribute_visible_doc,
"The visibility flag.  True if the StrokeVertex is visible.\n"
"\n"
":type: bool");

static PyObject *StrokeAttribute_visible_get(BPy_StrokeAttribute *self, void *UNUSED(closure))
{
	return PyBool_from_bool(self->sa->isVisible());
}

static int StrokeAttribute_visible_set(BPy_StrokeAttribute *self, PyObject *value, void *UNUSED(closure))
{
	if (!PyBool_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be boolean");
		return -1;
	}
	self->sa->setVisible(bool_from_PyBool(value));
	return 0;
}

static PyGetSetDef BPy_StrokeAttribute_getseters[] = {
	{(char *)"alpha", (getter)StrokeAttribute_alpha_get, (setter)StrokeAttribute_alpha_set, (char *)StrokeAttribute_alpha_doc, NULL},
	{(char *)"color", (getter)StrokeAttribute_color_get, (setter)StrokeAttribute_color_set, (char *)StrokeAttribute_color_doc, NULL},
	{(char *)"thickness", (getter)StrokeAttribute_thickness_get, (setter)StrokeAttribute_thickness_set, (char *)StrokeAttribute_thickness_doc, NULL},
	{(char *)"visible", (getter)StrokeAttribute_visible_get, (setter)StrokeAttribute_visible_set, (char *)StrokeAttribute_visible_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

/*-----------------------BPy_StrokeAttribute type definition ------------------------------*/

PyTypeObject StrokeAttribute_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"StrokeAttribute",              /* tp_name */
	sizeof(BPy_StrokeAttribute),    /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)StrokeAttribute___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)StrokeAttribute___repr__, /* tp_repr */
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
	StrokeAttribute_doc,            /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_StrokeAttribute_methods,    /* tp_methods */
	0,                              /* tp_members */
	BPy_StrokeAttribute_getseters,  /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)StrokeAttribute___init__, /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

