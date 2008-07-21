#include "StrokeAttribute.h"

#include "Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for StrokeAttribute instance  -----------*/
static int StrokeAttribute___init__(BPy_StrokeAttribute *self, PyObject *args, PyObject *kwds);
static void StrokeAttribute___dealloc__(BPy_StrokeAttribute *self);
static PyObject * StrokeAttribute___repr__(BPy_StrokeAttribute *self);

static PyObject * StrokeAttribute_getColorR( BPy_StrokeAttribute *self );
static PyObject * StrokeAttribute_getColorG( BPy_StrokeAttribute *self );
static PyObject * StrokeAttribute_getColorB( BPy_StrokeAttribute *self );
static PyObject * StrokeAttribute_getColorRGB( BPy_StrokeAttribute *self );
static PyObject * StrokeAttribute_getAlpha( BPy_StrokeAttribute *self );
static PyObject * StrokeAttribute_getThicknessR( BPy_StrokeAttribute *self );
static PyObject * StrokeAttribute_getThicknessL( BPy_StrokeAttribute *self );
static PyObject * StrokeAttribute_getThicknessRL( BPy_StrokeAttribute *self );
static PyObject * StrokeAttribute_isVisible( BPy_StrokeAttribute *self );
static PyObject * StrokeAttribute_getAttributeReal( BPy_StrokeAttribute *self, PyObject *args );
static PyObject * StrokeAttribute_getAttributeVec2f( BPy_StrokeAttribute *self, PyObject *args );
static PyObject * StrokeAttribute_getAttributeVec3f( BPy_StrokeAttribute *self, PyObject *args );
static PyObject * StrokeAttribute_isAttributeAvailableReal( BPy_StrokeAttribute *self, PyObject *args );
static PyObject * StrokeAttribute_isAttributeAvailableVec2f( BPy_StrokeAttribute *self, PyObject *args );
static PyObject * StrokeAttribute_isAttributeAvailableVec3f( BPy_StrokeAttribute *self, PyObject *args );
static int StrokeAttribute_setColor( BPy_StrokeAttribute *self, PyObject *args );
static int StrokeAttribute_setAlpha( BPy_StrokeAttribute *self, PyObject *args );
static int StrokeAttribute_setThickness( BPy_StrokeAttribute *self, PyObject *args );
static int StrokeAttribute_setVisible( BPy_StrokeAttribute *self, PyObject *args );
static int StrokeAttribute_setAttributeReal( BPy_StrokeAttribute *self, PyObject *args );
static int StrokeAttribute_setAttributeVec2f( BPy_StrokeAttribute *self, PyObject *args );
static int StrokeAttribute_setAttributeVec3f( BPy_StrokeAttribute *self, PyObject *args );


/*----------------------StrokeAttribute instance definitions ----------------------------*/
static PyMethodDef BPy_StrokeAttribute_methods[] = {
	{"getColorR", ( PyCFunction ) StrokeAttribute_getColorR, METH_NOARGS, "Returns the R color component. "},
	{"getColorG", ( PyCFunction ) StrokeAttribute_getColorG, METH_NOARGS, "Returns the G color component. "},
	{"getColorB", ( PyCFunction ) StrokeAttribute_getColorB, METH_NOARGS, "Returns the B color component. "},
	{"getColorRGB", ( PyCFunction ) StrokeAttribute_getColorRGB, METH_NOARGS, "Returns the RGB color components."},
	{"getAlpha", ( PyCFunction ) StrokeAttribute_getAlpha, METH_NOARGS, "Returns the alpha color component."},
	{"getThicknessR", ( PyCFunction ) StrokeAttribute_getThicknessR, METH_NOARGS, "Returns the thickness on the right of the vertex when following the stroke. "},
	{"getThicknessL", ( PyCFunction ) StrokeAttribute_getThicknessL, METH_NOARGS, "Returns the thickness on the left of the vertex when following the stroke."},
	{"getThicknessRL", ( PyCFunction ) StrokeAttribute_getThicknessRL, METH_NOARGS, "Returns the thickness on the right and on the left of the vertex when following the stroke. "},
	{"isVisible", ( PyCFunction ) StrokeAttribute_isVisible, METH_NOARGS, "Returns true if the strokevertex is visible, false otherwise"},
	{"getAttributeReal", ( PyCFunction ) StrokeAttribute_getAttributeReal, METH_VARARGS, "(name) Returns an attribute of type real specified by name."},
	{"getAttributeVec2f", ( PyCFunction ) StrokeAttribute_getAttributeVec2f, METH_VARARGS, "(name) Returns an attribute of type Vec2f specified by name."},
	{"getAttributeVec3f", ( PyCFunction ) StrokeAttribute_getAttributeVec3f, METH_VARARGS, "(name) Returns an attribute of type Vec3f specified by name."},
	{"isAttributeAvailableReal", ( PyCFunction ) StrokeAttribute_isAttributeAvailableReal, METH_VARARGS, "(name) Checks whether the real attribute specified by name is available"},
	{"isAttributeAvailableVec2f", ( PyCFunction ) StrokeAttribute_isAttributeAvailableVec2f, METH_VARARGS, "(name) Checks whether the Vec2f attribute specified by name is available"},
	{"isAttributeAvailableVec3f", ( PyCFunction ) StrokeAttribute_isAttributeAvailableVec3f, METH_VARARGS, "(name) Checks whether the Vec3f attribute specified by name is available"},
	{"setColor", ( PyCFunction ) StrokeAttribute_setColor, METH_VARARGS, "(float a)Sets the attribute's alpha value. "},
	{"setAlpha", ( PyCFunction ) StrokeAttribute_setAlpha, METH_VARARGS, "(float a) Sets the attribute's alpha value."},
	{"setThickness", ( PyCFunction ) StrokeAttribute_setThickness, METH_VARARGS, ""},
	{"setVisible", ( PyCFunction ) StrokeAttribute_setVisible, METH_VARARGS, ""},
	{"setAttributeReal", ( PyCFunction ) StrokeAttribute_setAttributeReal, METH_VARARGS, "(name, float att) Adds a user defined attribute of type real. If there is no attribute of specified by name, it is added. Otherwise, the new value replaces the old one."},
	{"setAttributeVec2f", ( PyCFunction ) StrokeAttribute_setAttributeVec2f, METH_VARARGS, "(name, float att) Adds a user defined attribute of type Vec2f. If there is no attribute of specified by name, it is added. Otherwise, the new value replaces the old one."},
	{"setAttributeVec3f", ( PyCFunction ) StrokeAttribute_setAttributeVec3f, METH_VARARGS, "(name, float att) Adds a user defined attribute of type Vec4f. If there is no attribute of specified by name, it is added. Otherwise, the new value replaces the old one."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_StrokeAttribute type definition ------------------------------*/

PyTypeObject StrokeAttribute_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"StrokeAttribute",				/* tp_name */
	sizeof( BPy_StrokeAttribute ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)StrokeAttribute___dealloc__,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	(reprfunc)StrokeAttribute___repr__,					/* tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,						/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, 		/* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_StrokeAttribute_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	NULL,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)StrokeAttribute___init__,                       	/* initproc tp_init; */
	NULL,							/* allocfunc tp_alloc; */
	PyType_GenericNew,		/* newfunc tp_new; */
	
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

//-------------------MODULE INITIALIZATION--------------------------------
PyMODINIT_FUNC StrokeAttribute_Init( PyObject *module )
{
	if( module == NULL )
		return;

	if( PyType_Ready( &StrokeAttribute_Type ) < 0 )
		return;
	Py_INCREF( &StrokeAttribute_Type );
	PyModule_AddObject(module, "StrokeAttribute", (PyObject *)&StrokeAttribute_Type);
	
}

//------------------------INSTANCE METHODS ----------------------------------

int StrokeAttribute___init__(BPy_StrokeAttribute *self, PyObject *args, PyObject *kwds)
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
										
	} else if( 	obj4 && obj5 && obj6 &&
				PyFloat_Check(obj1) && PyFloat_Check(obj2) && PyFloat_Check(obj2) &&
				PyFloat_Check(obj4) && PyFloat_Check(obj5) && PyFloat_Check(obj6) ) {
	
			self->sa = new StrokeAttribute(	PyFloat_AsDouble( obj1 ),
											PyFloat_AsDouble( obj2 ),
											PyFloat_AsDouble( obj3 ),
											PyFloat_AsDouble( obj4 ),
											PyFloat_AsDouble( obj5 ),
											PyFloat_AsDouble( obj6 ) );

	} else {
		return -1;
	}


	return 0;

}

void StrokeAttribute___dealloc__(BPy_StrokeAttribute* self)
{
	delete self->sa;
    self->ob_type->tp_free((PyObject*)self);
}

PyObject * StrokeAttribute___repr__(BPy_StrokeAttribute* self)
{
    return PyString_FromFormat("StrokeAttribute: r:%f g:%f b:%f a:%f - R:%f L:%f", 
		self->sa->getColorR(), self->sa->getColorG(), self->sa->getColorB(), self->sa->getAlpha(),
		self->sa->getThicknessR(), self->sa->getThicknessL() );
}


// PyObject *StrokeAttribute_getColor( BPy_StrokeAttribute *self ) {
// 	float *c = self->sa->getColor();
// 	Vec3f v( c[0], c[1], c[2]);
// 	return Vector_from_Vec3f( v );
// }

PyObject *StrokeAttribute_getColorR( BPy_StrokeAttribute *self ) {
	return PyFloat_FromDouble( self->sa->getColorR() );	
}

PyObject *StrokeAttribute_getColorG( BPy_StrokeAttribute *self ) {
	return PyFloat_FromDouble( self->sa->getColorG() );	
}

PyObject *StrokeAttribute_getColorB( BPy_StrokeAttribute *self ) {
	return PyFloat_FromDouble( self->sa->getColorB() );	
}

PyObject *StrokeAttribute_getColorRGB( BPy_StrokeAttribute *self ) {
	Vec3f v( self->sa->getColorRGB() );
	return Vector_from_Vec3f( v );	
}

PyObject *StrokeAttribute_getAlpha( BPy_StrokeAttribute *self ) {
	return PyFloat_FromDouble( self->sa->getAlpha() );	
}

// PyObject *StrokeAttribute_getThickness( BPy_StrokeAttribute *self ) {
// 	// vector
// 	return PyString_FromString( self->sa->getExactTypeName() );	
// }

PyObject *StrokeAttribute_getThicknessR( BPy_StrokeAttribute *self ) {
	return PyFloat_FromDouble( self->sa->getThicknessR() );	
}
PyObject *StrokeAttribute_getThicknessL( BPy_StrokeAttribute *self ) {
	return PyFloat_FromDouble( self->sa->getThicknessL() );	
}
PyObject *StrokeAttribute_getThicknessRL( BPy_StrokeAttribute *self ) {
	Vec2f v( self->sa->getThicknessRL() );
	return Vector_from_Vec2f( v );
}

PyObject *StrokeAttribute_isVisible( BPy_StrokeAttribute *self ) {
	return PyBool_from_bool( self->sa->isVisible() );	
}


PyObject *StrokeAttribute_getAttributeReal( BPy_StrokeAttribute *self, PyObject *args ) {
	char *attr;

	if(!( PyArg_ParseTuple(args, "s", &attr) )) {
		cout << "ERROR: StrokeAttribute_getAttributeReal" << endl;
		Py_RETURN_NONE;
	}

	double a = self->sa->getAttributeReal( attr );
	return PyFloat_FromDouble( a );
}

PyObject *StrokeAttribute_getAttributeVec2f( BPy_StrokeAttribute *self, PyObject *args ) {
	char *attr;

	if(!( PyArg_ParseTuple(args, "s", &attr) )) {
		cout << "ERROR: StrokeAttribute_getAttributeVec2f" << endl;
		Py_RETURN_NONE;
	}

	Vec2f a = self->sa->getAttributeVec2f( attr );
	return Vector_from_Vec2f( a );
}


PyObject *StrokeAttribute_getAttributeVec3f( BPy_StrokeAttribute *self, PyObject *args ) {
	char *attr;

	if(!( PyArg_ParseTuple(args, "s", &attr) )) {
		cout << "ERROR: StrokeAttribute_getAttributeVec3f" << endl;
		Py_RETURN_NONE;
	}

	Vec3f a = self->sa->getAttributeVec3f( attr );
	return Vector_from_Vec3f( a );
}

PyObject *StrokeAttribute_isAttributeAvailableReal( BPy_StrokeAttribute *self, PyObject *args ) {
	char *attr;

	if(!( PyArg_ParseTuple(args, "s", &attr) )) {
		cout << "ERROR: StrokeAttribute_isAttributeAvailableReal" << endl;
		Py_RETURN_NONE;
	}

	return PyBool_from_bool( self->sa->isAttributeAvailableReal( attr ) );
}

PyObject *StrokeAttribute_isAttributeAvailableVec2f( BPy_StrokeAttribute *self, PyObject *args ) {
	char *attr;

	if(!( PyArg_ParseTuple(args, "s", &attr) )) {
		cout << "ERROR: StrokeAttribute_isAttributeAvailableVec2f" << endl;
		Py_RETURN_NONE;
	}

	return PyBool_from_bool( self->sa->isAttributeAvailableVec2f( attr ) );
}

PyObject *StrokeAttribute_isAttributeAvailableVec3f( BPy_StrokeAttribute *self, PyObject *args ) {
	char *attr;

	if(!( PyArg_ParseTuple(args, "s", &attr) )) {
		cout << "ERROR: StrokeAttribute_isAttributeAvailableVec3f" << endl;
		Py_RETURN_NONE;
	}

	return PyBool_from_bool( self->sa->isAttributeAvailableVec3f( attr ) );
}


int StrokeAttribute_setColor( BPy_StrokeAttribute *self, PyObject *args ) {
	PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0 ;

	if(!( PyArg_ParseTuple(args, "O|OO", &obj1, &obj2, &obj3) )) {
		cout << "ERROR: StrokeAttribute_setColor" << endl;
		return -1;
	}

	if( PyList_Check(obj1) && !obj2 && !obj3 ){
		
		Vec3f v( 	PyFloat_AsDouble( PyList_GetItem(obj1, 0) ),
					PyFloat_AsDouble( PyList_GetItem(obj1, 1) ),
					PyFloat_AsDouble( PyList_GetItem(obj1, 2) )  );
		
		self->sa->setColor( v );
		return 0;
		
	} else if( 	obj1 && PyFloat_Check(obj1) &&
				obj2 && PyFloat_Check(obj2) &&
				obj3 && PyFloat_Check(obj3)	   ){
					
		self->sa->setColor(	PyFloat_AsDouble(obj1),
							PyFloat_AsDouble(obj2),
							PyFloat_AsDouble(obj3) );
		return 0;
	}
	
	return -1;
}

int StrokeAttribute_setAlpha( BPy_StrokeAttribute *self, PyObject *args ){
	float f = 0;

	if(!( PyArg_ParseTuple(args, "f", &f) )) {
		cout << "ERROR: StrokeAttribute_setAlpha" << endl;
		return -1;
	}
	
	self->sa->setAlpha( f );
	return 0;
}

int StrokeAttribute_setThickness( BPy_StrokeAttribute *self, PyObject *args )  {
	PyObject *obj1 = 0, *obj2 = 0;

	if(!( PyArg_ParseTuple(args, "O|O", &obj1, &obj2) )) {
		cout << "ERROR: StrokeAttribute_setThickness" << endl;
		return -1;
	}

	if( PyList_Check(obj1) && !obj2 ){
		
		Vec2f v( 	PyFloat_AsDouble( PyList_GetItem(obj1, 0) ),
					PyFloat_AsDouble( PyList_GetItem(obj1, 1) )  );
		
		self->sa->setThickness( v );
		return 0;
		
	} else if( 	obj1 && PyFloat_Check(obj1) &&
				obj2 && PyFloat_Check(obj2)	   ){
					
		self->sa->setThickness(	PyFloat_AsDouble(obj1),
								PyFloat_AsDouble(obj2) );
		return 0;
	}
	
	return -1;
}

int StrokeAttribute_setVisible( BPy_StrokeAttribute *self, PyObject *args ) {
	int i = 0;

	if(!( PyArg_ParseTuple(args, "i", &i) )) {
		cout << "ERROR: StrokeAttribute_setVisible" << endl;
		return -1;
	}

	self->sa->setVisible( i );
	return 0;
}

int StrokeAttribute_setAttributeReal( BPy_StrokeAttribute *self, PyObject *args ) {
	char *s = 0;
	double d = 0;

	if(!( PyArg_ParseTuple(args, "sd", &s, &d) )) {
		cout << "ERROR: StrokeAttribute_setAttributeReal" << endl;
		return -1;
	}

	self->sa->setAttributeReal( s, d );
	return 0;
}

int StrokeAttribute_setAttributeVec2f( BPy_StrokeAttribute *self, PyObject *args ) {
	char *s;
	PyObject *obj = 0;

	if(!( PyArg_ParseTuple(args, "sO", &s, &obj) )) {
		cout << "ERROR: StrokeAttribute_setAttributeVec2f" << endl;
		return -1;
	}

	if( PyList_Check(obj) && PyList_Size(obj) > 1) {

		Vec2f v( 	PyFloat_AsDouble( PyList_GetItem(obj, 0) ),
					PyFloat_AsDouble( PyList_GetItem(obj, 1) )  );

		self->sa->setAttributeVec2f( s, v );
		return 0;
		
	}
	
	return -1;
}

int StrokeAttribute_setAttributeVec3f( BPy_StrokeAttribute *self, PyObject *args ) {
	char *s;
	PyObject *obj = 0;

	if(!( PyArg_ParseTuple(args, "sO", &s, &obj) )) {
		cout << "ERROR: StrokeAttribute_setAttributeVec3f" << endl;
		return -1;
	}

	if( PyList_Check(obj)  && PyList_Size(obj) > 2 ) {

		Vec3f v( 	PyFloat_AsDouble( PyList_GetItem(obj, 0) ),
					PyFloat_AsDouble( PyList_GetItem(obj, 1) ),
					PyFloat_AsDouble( PyList_GetItem(obj, 2) )  );

		self->sa->setAttributeVec3f( s, v );
		return 0;

	}

	return -1;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

