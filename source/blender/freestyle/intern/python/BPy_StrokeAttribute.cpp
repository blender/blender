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
	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char StrokeAttribute___doc__[] =
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
"   :type t: float\n";

static int StrokeAttribute___init__(BPy_StrokeAttribute *self, PyObject *args, PyObject *kwds)
{

	PyObject *obj1 = 0, *obj2 = 0 , *obj3 = 0, *obj4 = 0, *obj5 = 0 , *obj6 = 0;

    if (! PyArg_ParseTuple(args, "|OOOOOO", &obj1, &obj2, &obj3, &obj4, &obj5, &obj6) )
        return -1;

	if( !obj1 || !obj2 || !obj3 ){
		
		self->sa = new StrokeAttribute();
		
	} else if( 	BPy_StrokeAttribute_Check(obj1) && 
				BPy_StrokeAttribute_Check(obj2) &&
				PyFloat_Check(obj3) ) {	
		
			self->sa = new StrokeAttribute(	*( ((BPy_StrokeAttribute *) obj1)->sa ),
											*( ((BPy_StrokeAttribute *) obj2)->sa ),
											PyFloat_AsDouble( obj3 ) );	
										
	} else if( 	obj4 && obj5 && obj6 ) {
	
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


static char StrokeAttribute_getColorR___doc__[] =
".. method:: getColorR()\n"
"\n"
"   Returns the red component of the stroke color.\n"
"\n"
"   :return: Red component of the stroke color.\n"
"   :rtype: float\n";

static PyObject *StrokeAttribute_getColorR( BPy_StrokeAttribute *self ) {
	return PyFloat_FromDouble( self->sa->getColorR() );	
}

static char StrokeAttribute_getColorG___doc__[] =
".. method:: getColorG()\n"
"\n"
"   Returns the green component of the stroke color.\n"
"\n"
"   :return: Green component of the stroke color.\n"
"   :rtype: float\n";

static PyObject *StrokeAttribute_getColorG( BPy_StrokeAttribute *self ) {
	return PyFloat_FromDouble( self->sa->getColorG() );	
}

static char StrokeAttribute_getColorB___doc__[] =
".. method:: getColorB()\n"
"\n"
"   Returns the blue component of the stroke color.\n"
"\n"
"   :return: Blue component of the stroke color.\n"
"   :rtype: float\n";

static PyObject *StrokeAttribute_getColorB( BPy_StrokeAttribute *self ) {
	return PyFloat_FromDouble( self->sa->getColorB() );	
}

static char StrokeAttribute_getColorRGB___doc__[] =
".. method:: getColorRGB()\n"
"\n"
"   Returns the RGB components of the stroke color.\n"
"\n"
"   :return: RGB components of the stroke color.\n"
"   :rtype: :class:`mathutils.Vector`\n";

static PyObject *StrokeAttribute_getColorRGB( BPy_StrokeAttribute *self ) {
	Vec3f v( self->sa->getColorRGB() );
	return Vector_from_Vec3f( v );	
}

static char StrokeAttribute_getAlpha___doc__[] =
".. method:: getAlpha()\n"
"\n"
"   Returns the alpha component of the stroke color.\n"
"\n"
"   :return: Alpha component of the stroke color.\n"
"   :rtype: float\n";

static PyObject *StrokeAttribute_getAlpha( BPy_StrokeAttribute *self ) {
	return PyFloat_FromDouble( self->sa->getAlpha() );	
}

static char StrokeAttribute_getThicknessR___doc__[] =
".. method:: getThicknessR()\n"
"\n"
"   Returns the thickness on the right of the vertex when following\n"
"   the stroke.\n"
"\n"
"   :return: The thickness on the right of the vertex.\n"
"   :rtype: float\n";

static PyObject *StrokeAttribute_getThicknessR( BPy_StrokeAttribute *self ) {
	return PyFloat_FromDouble( self->sa->getThicknessR() );	
}

static char StrokeAttribute_getThicknessL___doc__[] =
".. method:: getThicknessL()\n"
"\n"
"   Returns the thickness on the left of the vertex when following\n"
"   the stroke.\n"
"\n"
"   :return: The thickness on the left of the vertex.\n"
"   :rtype: float\n";

static PyObject *StrokeAttribute_getThicknessL( BPy_StrokeAttribute *self ) {
	return PyFloat_FromDouble( self->sa->getThicknessL() );	
}

static char StrokeAttribute_getThicknessRL___doc__[] =
".. method:: getThicknessRL()\n"
"\n"
"   Returns the thickness on the right and on the left of the vertex\n"
"   when following the stroke.\n"
"\n"
"   :return: A two-dimensional vector.  The first value is the\n"
"      thickness on the right of the vertex when following the stroke,\n"
"      and the second one is the thickness on the left.\n"
"   :rtype: :class:`mathutils.Vector`\n";

static PyObject *StrokeAttribute_getThicknessRL( BPy_StrokeAttribute *self ) {
	Vec2f v( self->sa->getThicknessRL() );
	return Vector_from_Vec2f( v );
}

static char StrokeAttribute_isVisible___doc__[] =
".. method:: isVisible()\n"
"\n"
"   Returns true if the StrokeVertex is visible, false otherwise.\n"
"\n"
"   :return: True if the StrokeVertex is visible, false otherwise.\n"
"   :rtype: bool\n";

static PyObject *StrokeAttribute_isVisible( BPy_StrokeAttribute *self ) {
	return PyBool_from_bool( self->sa->isVisible() );	
}

static char StrokeAttribute_getAttributeReal___doc__[] =
".. method:: getAttributeReal(iName)\n"
"\n"
"   Returns an attribute of float type.\n"
"\n"
"   :arg iName: The name of the attribute.\n"
"   :type iName: string\n"
"   :return: The attribute value.\n"
"   :rtype: float\n";

static PyObject *StrokeAttribute_getAttributeReal( BPy_StrokeAttribute *self, PyObject *args ) {
	char *attr;

	if(!( PyArg_ParseTuple(args, "s", &attr) ))
		return NULL;

	double a = self->sa->getAttributeReal( attr );
	return PyFloat_FromDouble( a );
}

static char StrokeAttribute_getAttributeVec2f___doc__[] =
".. method:: getAttributeVec2f(iName)\n"
"\n"
"   Returns an attribute of two-dimensional vector type.\n"
"\n"
"   :arg iName: The name of the attribute.\n"
"   :type iName: string\n"
"   :return: The attribute value.\n"
"   :rtype: :class:`mathutils.Vector`\n";

static PyObject *StrokeAttribute_getAttributeVec2f( BPy_StrokeAttribute *self, PyObject *args ) {
	char *attr;

	if(!( PyArg_ParseTuple(args, "s", &attr) ))
		return NULL;

	Vec2f a = self->sa->getAttributeVec2f( attr );
	return Vector_from_Vec2f( a );
}

static char StrokeAttribute_getAttributeVec3f___doc__[] =
".. method:: getAttributeVec3f(iName)\n"
"\n"
"   Returns an attribute of three-dimensional vector type.\n"
"\n"
"   :arg iName: The name of the attribute.\n"
"   :type iName: string\n"
"   :return: The attribute value.\n"
"   :rtype: :class:`mathutils.Vector`\n";

static PyObject *StrokeAttribute_getAttributeVec3f( BPy_StrokeAttribute *self, PyObject *args ) {
	char *attr;

	if(!( PyArg_ParseTuple(args, "s", &attr) ))
		return NULL;

	Vec3f a = self->sa->getAttributeVec3f( attr );
	return Vector_from_Vec3f( a );
}

static char StrokeAttribute_isAttributeAvailableReal___doc__[] =
".. method:: isAttributeAvailableReal(iName)\n"
"\n"
"   Checks whether the attribute iName of float type is available.\n"
"\n"
"   :arg iName: The name of the attribute.\n"
"   :type iName: string\n"
"   :return: True if the attribute is availbale.\n"
"   :rtype: bool\n";

static PyObject *StrokeAttribute_isAttributeAvailableReal( BPy_StrokeAttribute *self, PyObject *args ) {
	char *attr;

	if(!( PyArg_ParseTuple(args, "s", &attr) ))
		return NULL;

	return PyBool_from_bool( self->sa->isAttributeAvailableReal( attr ) );
}

static char StrokeAttribute_isAttributeAvailableVec2f___doc__[] =
".. method:: isAttributeAvailableVec2f(iName)\n"
"\n"
"   Checks whether the attribute iName of two-dimensional vector type\n"
"   is available.\n"
"\n"
"   :arg iName: The name of the attribute.\n"
"   :type iName: string\n"
"   :return: True if the attribute is availbale.\n"
"   :rtype: bool\n";

static PyObject *StrokeAttribute_isAttributeAvailableVec2f( BPy_StrokeAttribute *self, PyObject *args ) {
	char *attr;

	if(!( PyArg_ParseTuple(args, "s", &attr) ))
		return NULL;

	return PyBool_from_bool( self->sa->isAttributeAvailableVec2f( attr ) );
}

static char StrokeAttribute_isAttributeAvailableVec3f___doc__[] =
".. method:: isAttributeAvailableVec3f(iName)\n"
"\n"
"   Checks whether the attribute iName of three-dimensional vector\n"
"   type is available.\n"
"\n"
"   :arg iName: The name of the attribute.\n"
"   :type iName: string\n"
"   :return: True if the attribute is availbale.\n"
"   :rtype: bool\n";

static PyObject *StrokeAttribute_isAttributeAvailableVec3f( BPy_StrokeAttribute *self, PyObject *args ) {
	char *attr;

	if(!( PyArg_ParseTuple(args, "s", &attr) ))
		return NULL;

	return PyBool_from_bool( self->sa->isAttributeAvailableVec3f( attr ) );
}


static char StrokeAttribute_setColor___doc__[] =
".. method:: setColor(r, g, b)\n"
"\n"
"   Sets the stroke color.\n"
"\n"
"   :arg r: Red component of the stroke color.\n"
"   :type r: float\n"
"   :arg g: Green component of the stroke color.\n"
"   :type g: float\n"
"   :arg b: Blue component of the stroke color.\n"
"   :type b: float\n"
"\n"
".. method:: setColor(iRGB)\n"
"\n"
"   Sets the stroke color.\n"
"\n"
"   :arg iRGB: The new RGB values.\n"
"   :type iRGB: :class:`mathutils.Vector`, list or tuple of 3 real numbers\n";

static PyObject * StrokeAttribute_setColor( BPy_StrokeAttribute *self, PyObject *args ) {
	PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0 ;

	if(!( PyArg_ParseTuple(args, "O|OO", &obj1, &obj2, &obj3) ))
		return NULL;

	if( obj1 && !obj2 && !obj3 ){

		Vec3f *v = Vec3f_ptr_from_PyObject(obj1);
		if( !v ) {
			PyErr_SetString(PyExc_TypeError, "argument 1 must be a 3D vector (either a list of 3 elements or Vector)");
			return NULL;
		}
		self->sa->setColor( *v );
		delete v;
		
	} else if( 	obj1 && obj2 && obj3 ){

		self->sa->setColor(	PyFloat_AsDouble(obj1),
							PyFloat_AsDouble(obj2),
							PyFloat_AsDouble(obj3) );

	} else {
		PyErr_SetString(PyExc_TypeError, "invalid arguments");
		return NULL;
	}
	
	Py_RETURN_NONE;
}

static char StrokeAttribute_setAlpha___doc__[] =
".. method:: setAlpha(alpha)\n"
"\n"
"   Sets the alpha component of the stroke color.\n"
"\n"
"   :arg alpha: The new alpha value.\n"
"   :type alpha: float\n";

static PyObject * StrokeAttribute_setAlpha( BPy_StrokeAttribute *self, PyObject *args ){
	float f = 0;

	if(!( PyArg_ParseTuple(args, "f", &f) ))
		return NULL;
	
	self->sa->setAlpha( f );
	Py_RETURN_NONE;
}

static char StrokeAttribute_setThickness___doc__[] =
".. method:: setThickness(tr, tl)\n"
"\n"
"   Sets the stroke thickness.\n"
"\n"
"   :arg tr: The thickness on the right of the vertex when following\n"
"      the stroke.\n"
"   :type tr: float\n"
"   :arg tl: The thickness on the left of the vertex when following\n"
"      the stroke.\n"
"   :type tl: float\n"
"\n"
".. method:: setThickness(tRL)\n"
"\n"
"   Sets the stroke thickness.\n"
"\n"
"   :arg tRL: The thickness on the right and on the left of the vertex\n"
"      when following the stroke.\n"
"   :type tRL: :class:`mathutils.Vector`, list or tuple of 2 real numbers\n";

static PyObject * StrokeAttribute_setThickness( BPy_StrokeAttribute *self, PyObject *args )  {
	PyObject *obj1 = 0, *obj2 = 0;

	if(!( PyArg_ParseTuple(args, "O|O", &obj1, &obj2) ))
		return NULL;

	if( obj1 && !obj2 ){
		
		Vec2f *v = Vec2f_ptr_from_PyObject(obj1);
		if( !v ) {
			PyErr_SetString(PyExc_TypeError, "argument 1 must be a 2D vector (either a list of 2 elements or Vector)");
			return NULL;
		}
		self->sa->setThickness( *v );
		delete v;
				
	} else if( 	obj1 && obj2 ){
					
		self->sa->setThickness(	PyFloat_AsDouble(obj1),
								PyFloat_AsDouble(obj2) );

	} else {
		PyErr_SetString(PyExc_TypeError, "invalid arguments");
		return NULL;
	}
	
	Py_RETURN_NONE;
}

static char StrokeAttribute_setVisible___doc__[] =
".. method:: setVisible(iVisible)\n"
"\n"
"   Sets the visibility flag.  True means the StrokeVertex is visible.\n"
"\n"
"   :arg iVisible: True if the StrokeVertex is visible.\n"
"   :type iVisible: bool\n";

static PyObject * StrokeAttribute_setVisible( BPy_StrokeAttribute *self, PyObject *args ) {
	PyObject *py_b;

	if(!( PyArg_ParseTuple(args, "O", &py_b) ))
		return NULL;

	self->sa->setVisible( bool_from_PyBool(py_b) );

	Py_RETURN_NONE;
}

static char StrokeAttribute_setAttributeReal___doc__[] =
".. method:: setAttributeReal(iName, att)\n"
"\n"
"   Adds a user-defined attribute of float type.  If there is no\n"
"   attribute of the given name, it is added.  Otherwise, the new value\n"
"   replaces the old one.\n"
"\n"
"   :arg iName: The name of the attribute.\n"
"   :type iName: string\n"
"   :arg att: The attribute value.\n"
"   :type att: float\n";

static PyObject * StrokeAttribute_setAttributeReal( BPy_StrokeAttribute *self, PyObject *args ) {
	char *s = 0;
	double d = 0;

	if(!( PyArg_ParseTuple(args, "sd", &s, &d) ))
		return NULL;

	self->sa->setAttributeReal( s, d );
	Py_RETURN_NONE;
}

static char StrokeAttribute_setAttributeVec2f___doc__[] =
".. method:: setAttributeVec2f(iName, att)\n"
"\n"
"   Adds a user-defined attribute of two-dimensional vector type.  If\n"
"   there is no attribute of the given name, it is added.  Otherwise,\n"
"   the new value replaces the old one.\n"
"\n"
"   :arg iName: The name of the attribute.\n"
"   :type iName: string\n"
"   :arg att: The attribute value.\n"
"   :type att: :class:`mathutils.Vector`, list or tuple of 2 real numbers\n";

static PyObject * StrokeAttribute_setAttributeVec2f( BPy_StrokeAttribute *self, PyObject *args ) {
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

static char StrokeAttribute_setAttributeVec3f___doc__[] =
".. method:: setAttributeVec3f(iName, att)\n"
"\n"
"   Adds a user-defined attribute of three-dimensional vector type.\n"
"   If there is no attribute of the given name, it is added.\n"
"   Otherwise, the new value replaces the old one.\n"
"\n"
"   :arg iName: The name of the attribute.\n"
"   :type iName: string\n"
"   :arg att: The attribute value.\n"
"   :type att: :class:`mathutils.Vector`, list or tuple of 3 real numbers\n";

static PyObject * StrokeAttribute_setAttributeVec3f( BPy_StrokeAttribute *self, PyObject *args ) {
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
	{"getColorR", ( PyCFunction ) StrokeAttribute_getColorR, METH_NOARGS, StrokeAttribute_getColorR___doc__},
	{"getColorG", ( PyCFunction ) StrokeAttribute_getColorG, METH_NOARGS, StrokeAttribute_getColorG___doc__},
	{"getColorB", ( PyCFunction ) StrokeAttribute_getColorB, METH_NOARGS, StrokeAttribute_getColorB___doc__},
	{"getColorRGB", ( PyCFunction ) StrokeAttribute_getColorRGB, METH_NOARGS, StrokeAttribute_getColorRGB___doc__},
	{"getAlpha", ( PyCFunction ) StrokeAttribute_getAlpha, METH_NOARGS, StrokeAttribute_getAlpha___doc__},
	{"getThicknessR", ( PyCFunction ) StrokeAttribute_getThicknessR, METH_NOARGS, StrokeAttribute_getThicknessR___doc__},
	{"getThicknessL", ( PyCFunction ) StrokeAttribute_getThicknessL, METH_NOARGS, StrokeAttribute_getThicknessL___doc__},
	{"getThicknessRL", ( PyCFunction ) StrokeAttribute_getThicknessRL, METH_NOARGS, StrokeAttribute_getThicknessRL___doc__},
	{"isVisible", ( PyCFunction ) StrokeAttribute_isVisible, METH_NOARGS, StrokeAttribute_isVisible___doc__},
	{"getAttributeReal", ( PyCFunction ) StrokeAttribute_getAttributeReal, METH_VARARGS, StrokeAttribute_getAttributeReal___doc__},
	{"getAttributeVec2f", ( PyCFunction ) StrokeAttribute_getAttributeVec2f, METH_VARARGS, StrokeAttribute_getAttributeVec2f___doc__},
	{"getAttributeVec3f", ( PyCFunction ) StrokeAttribute_getAttributeVec3f, METH_VARARGS, StrokeAttribute_getAttributeVec3f___doc__},
	{"isAttributeAvailableReal", ( PyCFunction ) StrokeAttribute_isAttributeAvailableReal, METH_VARARGS, StrokeAttribute_isAttributeAvailableReal___doc__},
	{"isAttributeAvailableVec2f", ( PyCFunction ) StrokeAttribute_isAttributeAvailableVec2f, METH_VARARGS, StrokeAttribute_isAttributeAvailableVec2f___doc__},
	{"isAttributeAvailableVec3f", ( PyCFunction ) StrokeAttribute_isAttributeAvailableVec3f, METH_VARARGS, StrokeAttribute_isAttributeAvailableVec3f___doc__},
	{"setColor", ( PyCFunction ) StrokeAttribute_setColor, METH_VARARGS, StrokeAttribute_setColor___doc__},
	{"setAlpha", ( PyCFunction ) StrokeAttribute_setAlpha, METH_VARARGS, StrokeAttribute_setAlpha___doc__},
	{"setThickness", ( PyCFunction ) StrokeAttribute_setThickness, METH_VARARGS, StrokeAttribute_setThickness___doc__},
	{"setVisible", ( PyCFunction ) StrokeAttribute_setVisible, METH_VARARGS, StrokeAttribute_setVisible___doc__},
	{"setAttributeReal", ( PyCFunction ) StrokeAttribute_setAttributeReal, METH_VARARGS, StrokeAttribute_setAttributeReal___doc__},
	{"setAttributeVec2f", ( PyCFunction ) StrokeAttribute_setAttributeVec2f, METH_VARARGS, StrokeAttribute_setAttributeVec2f___doc__},
	{"setAttributeVec3f", ( PyCFunction ) StrokeAttribute_setAttributeVec3f, METH_VARARGS, StrokeAttribute_setAttributeVec3f___doc__},
	{NULL, NULL, 0, NULL}
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
	StrokeAttribute___doc__,        /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_StrokeAttribute_methods,    /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
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

