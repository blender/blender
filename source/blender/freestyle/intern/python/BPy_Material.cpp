#include "BPy_Material.h"

#include "BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for Material instance  -----------*/
static int Material___init__(BPy_Material *self, PyObject *args, PyObject *kwds);
static void Material___dealloc__(BPy_Material *self);
static PyObject * Material___repr__(BPy_Material *self);

static PyObject * Material_diffuse( BPy_Material* self);
static PyObject * Material_diffuseR( BPy_Material* self);
static PyObject * Material_diffuseG( BPy_Material* self) ;
static PyObject * Material_diffuseB( BPy_Material* self) ;
static PyObject * Material_diffuseA( BPy_Material* self);
static PyObject * Material_specular( BPy_Material* self);
static PyObject * Material_specularR( BPy_Material* self);
static PyObject * Material_specularG( BPy_Material* self);
static PyObject * Material_specularB( BPy_Material* self) ;
static PyObject * Material_specularA( BPy_Material* self) ;
static PyObject * Material_ambient( BPy_Material* self) ;
static PyObject * Material_ambientR( BPy_Material* self);
static PyObject * Material_ambientG( BPy_Material* self);
static PyObject * Material_ambientB( BPy_Material* self);
static PyObject * Material_ambientA( BPy_Material* self);
static PyObject * Material_emission( BPy_Material* self);
static PyObject * Material_emissionR( BPy_Material* self);
static PyObject * Material_emissionG( BPy_Material* self) ;
static PyObject * Material_emissionB( BPy_Material* self);
static PyObject * Material_emissionA( BPy_Material* self);
static PyObject * Material_shininess( BPy_Material* self);
static PyObject * Material_setDiffuse( BPy_Material *self, PyObject *args );
static PyObject * Material_setSpecular( BPy_Material *self, PyObject *args );
static PyObject * Material_setAmbient( BPy_Material *self, PyObject *args );
static PyObject * Material_setEmission( BPy_Material *self, PyObject *args );
static PyObject * Material_setShininess( BPy_Material *self, PyObject *args );

/*----------------------Material instance definitions ----------------------------*/
static PyMethodDef BPy_Material_methods[] = {
	{"diffuse", ( PyCFunction ) Material_diffuse, METH_NOARGS, "() Returns the diffuse color as a 4 float array"},
	{"diffuseR", ( PyCFunction ) Material_diffuseR, METH_NOARGS, "() Returns the red component of the diffuse color "},
	{"diffuseG", ( PyCFunction ) Material_diffuseG, METH_NOARGS, "() Returns the green component of the diffuse color "},
	{"diffuseB", ( PyCFunction ) Material_diffuseB, METH_NOARGS, "() Returns the blue component of the diffuse color "},
	{"diffuseA", ( PyCFunction ) Material_diffuseA, METH_NOARGS, "() Returns the alpha component of the diffuse color "},
	{"specular", ( PyCFunction ) Material_specular, METH_NOARGS, "() Returns the specular color as a 4 float array"},
	{"specularR", ( PyCFunction ) Material_specularR, METH_NOARGS, "() Returns the red component of the specular color "},
	{"specularG", ( PyCFunction ) Material_specularG, METH_NOARGS, "() Returns the green component of the specular color "},
	{"specularB", ( PyCFunction ) Material_specularB, METH_NOARGS, "() Returns the blue component of the specular color "},
	{"specularA", ( PyCFunction ) Material_specularA, METH_NOARGS, "() Returns the alpha component of the specular color "},
	{"ambient", ( PyCFunction ) Material_ambient, METH_NOARGS, "() Returns the ambient color as a 4 float array"},
	{"ambientR", ( PyCFunction ) Material_ambientR, METH_NOARGS, "() Returns the red component of the ambient color "},
	{"ambientG", ( PyCFunction ) Material_ambientG, METH_NOARGS, "() Returns the green component of the ambient color "},
	{"ambientB", ( PyCFunction ) Material_ambientB, METH_NOARGS, "() Returns the blue component of the ambient color "},
	{"ambientA", ( PyCFunction ) Material_ambientA, METH_NOARGS, "() Returns the alpha component of the ambient color "},
	{"emission", ( PyCFunction ) Material_emission, METH_NOARGS, "() Returns the emission color as a 4 float array"},
	{"emissionR", ( PyCFunction ) Material_emissionR, METH_NOARGS, "() Returns the red component of the emission color "},
	{"emissionG", ( PyCFunction ) Material_emissionG, METH_NOARGS, "() Returns the green component of the emission color "},
	{"emissionB", ( PyCFunction ) Material_emissionB, METH_NOARGS, "() Returns the blue component of the emission color "},
	{"emissionA", ( PyCFunction ) Material_emissionA, METH_NOARGS, "() Returns the alpha component of the emission color "},
	{"shininess", ( PyCFunction ) Material_shininess, METH_NOARGS, "() Returns the shininess coefficient "},
	{"setDiffuse", ( PyCFunction ) Material_setDiffuse, METH_NOARGS, "(float r, float g, float b, float a) Sets the diffuse color"},
	{"setSpecular", ( PyCFunction ) Material_setSpecular, METH_NOARGS, "(float r, float g, float b, float a) Sets the specular color"},
	{"setAmbient", ( PyCFunction ) Material_setAmbient, METH_NOARGS, "(float r, float g, float b, float a) Sets the ambient color"},
	{"setEmission", ( PyCFunction ) Material_setEmission, METH_NOARGS, "(float r, float g, float b, float a) Sets the emission color"},
	{"setShininess", ( PyCFunction ) Material_setShininess, METH_NOARGS, "(float r, float g, float b, float a) Sets the shininess color"},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Material type definition ------------------------------*/

PyTypeObject Material_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,							/* ob_size */
	"Material",				/* tp_name */
	sizeof( BPy_Material ),	/* tp_basicsize */
	0,							/* tp_itemsize */
	
	/* methods */
	(destructor)Material___dealloc__,	/* tp_dealloc */
	NULL,                       				/* printfunc tp_print; */
	NULL,                       				/* getattrfunc tp_getattr; */
	NULL,                       				/* setattrfunc tp_setattr; */
	NULL,										/* tp_compare */
	(reprfunc)Material___repr__,					/* tp_repr */

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
	BPy_Material_methods,	/* struct PyMethodDef *tp_methods; */
	NULL,                       	/* struct PyMemberDef *tp_members; */
	NULL,         					/* struct PyGetSetDef *tp_getset; */
	NULL,							/* struct _typeobject *tp_base; */
	NULL,							/* PyObject *tp_dict; */
	NULL,							/* descrgetfunc tp_descr_get; */
	NULL,							/* descrsetfunc tp_descr_set; */
	0,                          	/* long tp_dictoffset; */
	(initproc)Material___init__, /* initproc tp_init; */
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
PyMODINIT_FUNC Material_Init( PyObject *module )
{
	if( module == NULL )
		return;

	if( PyType_Ready( &Material_Type ) < 0 )
		return;

	Py_INCREF( &Material_Type );
	PyModule_AddObject(module, "Material", (PyObject *)&Material_Type);
}

//------------------------INSTANCE METHODS ----------------------------------

int Material___init__(BPy_Material *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj1 = 0;
	float f1 = 0, f2 = 0., f3 = 0., f4 = 0., f5 = 0.;

    if (! PyArg_ParseTuple(args, "|Offff", &obj1, &f2, &f3, &f4, &f5) )
        return -1;

	if( !obj1 ){
		self->m = new Material();

	} else if( BPy_Material_Check(obj1) ) {
		if( ((BPy_Material *) obj1)->m )
			self->m = new Material(*( ((BPy_Material *) obj1)->m ));
		else
			return -1;

	} else if( PyFloat_Check(obj1) ) {
		f1 = PyFloat_AsDouble(obj1);
		self->m = new Material(&f1, &f2, &f3, &f4, f5);

	} else {
		return -1;
	}

	return 0;
}

void Material___dealloc__( BPy_Material* self)
{
	delete self->m;
    self->ob_type->tp_free((PyObject*)self);
}


PyObject * Material___repr__( BPy_Material* self)
{
    return PyString_FromFormat("Material - address: %p", self->m );
}

PyObject * Material_diffuse( BPy_Material* self) {
	PyObject *tmp;
	
	const float *diffuse = self->m->diffuse();
	PyObject *py_diffuse = PyList_New(4);
	
	tmp = PyFloat_FromDouble( diffuse[0] ); PyList_SetItem( py_diffuse, 0, tmp); Py_DECREF(tmp);
	tmp = PyFloat_FromDouble( diffuse[1] ); PyList_SetItem( py_diffuse, 1, tmp); Py_DECREF(tmp);
	tmp = PyFloat_FromDouble( diffuse[2] ); PyList_SetItem( py_diffuse, 2, tmp); Py_DECREF(tmp);
	tmp = PyFloat_FromDouble( diffuse[3] ); PyList_SetItem( py_diffuse, 3, tmp); Py_DECREF(tmp);
	
	return py_diffuse;
}

PyObject * Material_diffuseR( BPy_Material* self) {
	return PyFloat_FromDouble( self->m->diffuseR() );
}

PyObject * Material_diffuseG( BPy_Material* self) {
	return PyFloat_FromDouble( self->m->diffuseG() );
}

PyObject * Material_diffuseB( BPy_Material* self) {
	return PyFloat_FromDouble( self->m->diffuseB() );
}

PyObject * Material_diffuseA( BPy_Material* self) {
	return PyFloat_FromDouble( self->m->diffuseA() );
}

PyObject * Material_specular( BPy_Material* self) {
	PyObject *tmp;
	
	const float *specular = self->m->specular();
	PyObject *py_specular = PyList_New(4);
	
	tmp = PyFloat_FromDouble( specular[0] ); PyList_SetItem( py_specular, 0, tmp); Py_DECREF(tmp);
	tmp = PyFloat_FromDouble( specular[1] ); PyList_SetItem( py_specular, 1, tmp); Py_DECREF(tmp);
	tmp = PyFloat_FromDouble( specular[2] ); PyList_SetItem( py_specular, 2, tmp); Py_DECREF(tmp);
	tmp = PyFloat_FromDouble( specular[3] ); PyList_SetItem( py_specular, 3, tmp); Py_DECREF(tmp);
	
	return py_specular;
}

PyObject * Material_specularR( BPy_Material* self) {
	return PyFloat_FromDouble( self->m->specularR() );
}

PyObject * Material_specularG( BPy_Material* self) {
	return PyFloat_FromDouble( self->m->specularG() );
}

PyObject * Material_specularB( BPy_Material* self) {
	return PyFloat_FromDouble( self->m->specularB() );
}

PyObject * Material_specularA( BPy_Material* self) {
	return PyFloat_FromDouble( self->m->specularA() );
}

PyObject * Material_ambient( BPy_Material* self) {
	PyObject *tmp;
	
	const float *ambient = self->m->ambient();
	PyObject *py_ambient = PyList_New(4);
	
	tmp = PyFloat_FromDouble( ambient[0] ); PyList_SetItem(	py_ambient, 0, tmp); Py_DECREF(tmp);
	tmp = PyFloat_FromDouble( ambient[1] ); PyList_SetItem(	py_ambient, 1, tmp); Py_DECREF(tmp);
	tmp = PyFloat_FromDouble( ambient[2] ); PyList_SetItem(	py_ambient, 2, tmp); Py_DECREF(tmp);
	tmp = PyFloat_FromDouble( ambient[3] ); PyList_SetItem(	py_ambient, 3, tmp); Py_DECREF(tmp);
	
	return py_ambient;
}

PyObject * Material_ambientR( BPy_Material* self) {
	return PyFloat_FromDouble( self->m->ambientR() );
}

PyObject * Material_ambientG( BPy_Material* self) {
	return PyFloat_FromDouble( self->m->ambientG() );
}

PyObject * Material_ambientB( BPy_Material* self) {
	return PyFloat_FromDouble( self->m->ambientB() );
}

PyObject * Material_ambientA( BPy_Material* self) {
	return PyFloat_FromDouble( self->m->ambientA() );
}

PyObject * Material_emission( BPy_Material* self) {
	PyObject *tmp;
	
	const float *emission = self->m->emission();
	PyObject *py_emission = PyList_New(4);
	
	tmp = PyFloat_FromDouble( emission[0] ); PyList_SetItem( py_emission, 0, tmp); Py_DECREF(tmp);
	tmp = PyFloat_FromDouble( emission[1] ); PyList_SetItem( py_emission, 1, tmp); Py_DECREF(tmp);
	tmp = PyFloat_FromDouble( emission[2] ); PyList_SetItem( py_emission, 2, tmp); Py_DECREF(tmp);
	tmp = PyFloat_FromDouble( emission[3] ); PyList_SetItem( py_emission, 3, tmp); Py_DECREF(tmp);
	
	return py_emission;
}

PyObject * Material_emissionR( BPy_Material* self) {
	return PyFloat_FromDouble( self->m->emissionR() );
}

PyObject * Material_emissionG( BPy_Material* self) {
	return PyFloat_FromDouble( self->m->emissionG() );
}

PyObject * Material_emissionB( BPy_Material* self) {
	return PyFloat_FromDouble( self->m->emissionB() );
}

PyObject * Material_emissionA( BPy_Material* self) {
	return PyFloat_FromDouble( self->m->emissionA() );
}

PyObject * Material_shininess( BPy_Material* self) {
	return PyFloat_FromDouble( self->m->shininess() );
}

PyObject * Material_setDiffuse( BPy_Material *self, PyObject *args ) {
	float f1, f2, f3, f4;

	if(!( PyArg_ParseTuple(args, "ffff", &f1, &f2, &f3, &f4)  )) {
		cout << "ERROR: Material_setDiffuse" << endl;
		Py_RETURN_NONE;
	}

	self->m->setDiffuse(f1, f2, f3, f4);

	Py_RETURN_NONE;
}
 
PyObject * Material_setSpecular( BPy_Material *self, PyObject *args ) {
	float f1, f2, f3, f4;

	if(!( PyArg_ParseTuple(args, "ffff", &f1, &f2, &f3, &f4)  )) {
		cout << "ERROR: Material_setSpecular" << endl;
		Py_RETURN_NONE;
	}

	self->m->setSpecular(f1, f2, f3, f4);

	Py_RETURN_NONE;
}

PyObject * Material_setAmbient( BPy_Material *self, PyObject *args ) {
	float f1, f2, f3, f4;

	if(!( PyArg_ParseTuple(args, "ffff", &f1, &f2, &f3, &f4)  )) {
		cout << "ERROR: Material_setAmbient" << endl;
		Py_RETURN_NONE;
	}

	self->m->setAmbient(f1, f2, f3, f4);

	Py_RETURN_NONE;
}

PyObject * Material_setEmission( BPy_Material *self, PyObject *args ) {
	float f1, f2, f3, f4;

	if(!( PyArg_ParseTuple(args, "ffff", &f1, &f2, &f3, &f4)  )) {
		cout << "ERROR: Material_setEmission" << endl;
		Py_RETURN_NONE;
	}

	self->m->setEmission(f1, f2, f3, f4);

	Py_RETURN_NONE;
}

PyObject * Material_setShininess( BPy_Material *self, PyObject *args ) {
	float f;

	if(!( PyArg_ParseTuple(args, "f", &f)  )) {
		cout << "ERROR: Material_setShininess" << endl;
		Py_RETURN_NONE;
	}

	self->m->setShininess(f);

	Py_RETURN_NONE;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
