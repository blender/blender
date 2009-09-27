#include "BPy_UnaryFunction0DMaterial.h"

#include "../BPy_Convert.h"
#include "../Iterator/BPy_Interface0DIterator.h"

#include "UnaryFunction0D_Material/BPy_MaterialF0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for UnaryFunction0DMaterial instance  -----------*/
static int UnaryFunction0DMaterial___init__(BPy_UnaryFunction0DMaterial* self, PyObject *args, PyObject *kwds);
static void UnaryFunction0DMaterial___dealloc__(BPy_UnaryFunction0DMaterial* self);
static PyObject * UnaryFunction0DMaterial___repr__(BPy_UnaryFunction0DMaterial* self);

static PyObject * UnaryFunction0DMaterial_getName( BPy_UnaryFunction0DMaterial *self);
static PyObject * UnaryFunction0DMaterial___call__( BPy_UnaryFunction0DMaterial *self, PyObject *args, PyObject *kwds);

/*----------------------UnaryFunction0DMaterial instance definitions ----------------------------*/
static PyMethodDef BPy_UnaryFunction0DMaterial_methods[] = {
	{"getName", ( PyCFunction ) UnaryFunction0DMaterial_getName, METH_NOARGS, "() Returns the string of the name of the unary 0D function."},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_UnaryFunction0DMaterial type definition ------------------------------*/

PyTypeObject UnaryFunction0DMaterial_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"UnaryFunction0DMaterial",      /* tp_name */
	sizeof(BPy_UnaryFunction0DMaterial), /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)UnaryFunction0DMaterial___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)UnaryFunction0DMaterial___repr__, /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	(ternaryfunc)UnaryFunction0DMaterial___call__, /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	"UnaryFunction0DMaterial objects", /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_UnaryFunction0DMaterial_methods, /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryFunction0D_Type,          /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)UnaryFunction0DMaterial___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

//-------------------MODULE INITIALIZATION--------------------------------

int UnaryFunction0DMaterial_Init( PyObject *module ) {

	if( module == NULL )
		return -1;

	if( PyType_Ready( &UnaryFunction0DMaterial_Type ) < 0 )
		return -1;
	Py_INCREF( &UnaryFunction0DMaterial_Type );
	PyModule_AddObject(module, "UnaryFunction0DMaterial", (PyObject *)&UnaryFunction0DMaterial_Type);
	
	if( PyType_Ready( &MaterialF0D_Type ) < 0 )
		return -1;
	Py_INCREF( &MaterialF0D_Type );
	PyModule_AddObject(module, "MaterialF0D", (PyObject *)&MaterialF0D_Type);

	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

int UnaryFunction0DMaterial___init__(BPy_UnaryFunction0DMaterial* self, PyObject *args, PyObject *kwds)
{
    if ( !PyArg_ParseTuple(args, "") )
        return -1;
	self->uf0D_material = new UnaryFunction0D<FrsMaterial>();
	self->uf0D_material->py_uf0D = (PyObject *)self;
	return 0;
}

void UnaryFunction0DMaterial___dealloc__(BPy_UnaryFunction0DMaterial* self)
{
	if (self->uf0D_material)
		delete self->uf0D_material;
	UnaryFunction0D_Type.tp_dealloc((PyObject*)self);
}


PyObject * UnaryFunction0DMaterial___repr__(BPy_UnaryFunction0DMaterial* self)
{
	return PyUnicode_FromFormat("type: %s - address: %p", self->uf0D_material->getName().c_str(), self->uf0D_material );
}

PyObject * UnaryFunction0DMaterial_getName( BPy_UnaryFunction0DMaterial *self )
{
	return PyUnicode_FromFormat( self->uf0D_material->getName().c_str() );
}

PyObject * UnaryFunction0DMaterial___call__( BPy_UnaryFunction0DMaterial *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj;

	if( kwds != NULL ) {
		PyErr_SetString(PyExc_TypeError, "keyword argument(s) not supported");
		return NULL;
	}
	if(!PyArg_ParseTuple(args, "O!", &Interface0DIterator_Type, &obj))
		return NULL;
	
	if( typeid(*(self->uf0D_material)) == typeid(UnaryFunction0D<FrsMaterial>) ) {
		PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
		return NULL;
	}
	if (self->uf0D_material->operator()(*( ((BPy_Interface0DIterator *) obj)->if0D_it )) < 0) {
		if (!PyErr_Occurred()) {
			string msg(self->uf0D_material->getName() + " __call__ method failed");
			PyErr_SetString(PyExc_RuntimeError, msg.c_str());
		}
		return NULL;
	}
	return BPy_FrsMaterial_from_FrsMaterial( self->uf0D_material->result );

}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
