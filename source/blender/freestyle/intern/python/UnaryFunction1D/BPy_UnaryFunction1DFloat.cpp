#include "BPy_UnaryFunction1DFloat.h"

#include "../BPy_Convert.h"
#include "../BPy_Interface1D.h"
#include "../BPy_IntegrationType.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------

int UnaryFunction1DFloat_Init( PyObject *module ) {

	if( module == NULL )
		return -1;

	if( PyType_Ready( &UnaryFunction1DFloat_Type ) < 0 )
		return -1;
	Py_INCREF( &UnaryFunction1DFloat_Type );
	PyModule_AddObject(module, "UnaryFunction1DFloat", (PyObject *)&UnaryFunction1DFloat_Type);

	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char UnaryFunction1DFloat___doc__[] =
"Base class for unary functions (functors) that work on\n"
":class:`Interface1D` and return a float value.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(integration)\n"
"\n"
"   Builds a unary 1D function using the integration method given as\n"
"   argument.\n"
"\n"
"   :arg integration: An integration method.\n"
"   :type integration: :class:`IntegrationType`\n";

static int UnaryFunction1DFloat___init__(BPy_UnaryFunction1DFloat* self, PyObject *args)
{
	PyObject *obj = 0;

	if( !PyArg_ParseTuple(args, "|O!", &IntegrationType_Type, &obj) )	
		return -1;
	
	if( !obj )
		self->uf1D_float = new UnaryFunction1D<float>();
	else {
		self->uf1D_float = new UnaryFunction1D<float>( IntegrationType_from_BPy_IntegrationType(obj) );
	}
	
	self->uf1D_float->py_uf1D = (PyObject *)self;
	
	return 0;
}

static void UnaryFunction1DFloat___dealloc__(BPy_UnaryFunction1DFloat* self)
{
	if (self->uf1D_float)
		delete self->uf1D_float;
	UnaryFunction1D_Type.tp_dealloc((PyObject*)self);
}

static PyObject * UnaryFunction1DFloat___repr__(BPy_UnaryFunction1DFloat* self)
{
	return PyUnicode_FromFormat("type: %s - address: %p", self->uf1D_float->getName().c_str(), self->uf1D_float );
}

static char UnaryFunction1DFloat_getName___doc__[] =
".. method:: getName()\n"
"\n"
"   Returns the name of the unary 1D function.\n"
"\n"
"   :return: The name of the unary 1D function.\n"
"   :rtype: string\n";

static PyObject * UnaryFunction1DFloat_getName( BPy_UnaryFunction1DFloat *self )
{
	return PyUnicode_FromFormat( self->uf1D_float->getName().c_str() );
}

static PyObject * UnaryFunction1DFloat___call__( BPy_UnaryFunction1DFloat *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj;

	if( kwds != NULL ) {
		PyErr_SetString(PyExc_TypeError, "keyword argument(s) not supported");
		return NULL;
	}
	if( !PyArg_ParseTuple(args, "O!", &Interface1D_Type, &obj) )
		return NULL;
	
	if( typeid(*(self->uf1D_float)) == typeid(UnaryFunction1D<float>) ) {
		PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
		return NULL;
	}
	if (self->uf1D_float->operator()(*( ((BPy_Interface1D *) obj)->if1D )) < 0) {
		if (!PyErr_Occurred()) {
			string msg(self->uf1D_float->getName() + " __call__ method failed");
			PyErr_SetString(PyExc_RuntimeError, msg.c_str());
		}
		return NULL;
	}
	return PyFloat_FromDouble( self->uf1D_float->result );

}

static char UnaryFunction1DFloat_setIntegrationType___doc__[] =
".. method:: setIntegrationType(integration)\n"
"\n"
"   Sets the integration method.\n"
"\n"
"   :arg integration: An integration method.\n"
"   :type integration: :class:`IntegrationType`\n";

static PyObject * UnaryFunction1DFloat_setIntegrationType(BPy_UnaryFunction1DFloat* self, PyObject *args)
{
	PyObject *obj;

	if( !PyArg_ParseTuple(args, "O!", &IntegrationType_Type, &obj) )
		return NULL;
	
	self->uf1D_float->setIntegrationType( IntegrationType_from_BPy_IntegrationType(obj) );
	Py_RETURN_NONE;
}

static char UnaryFunction1DFloat_getIntegrationType___doc__[] =
".. method:: getIntegrationType(integration)\n"
"\n"
"   Returns the integration method.\n"
"\n"
"   :return: The integration method.\n"
"   :rtype: :class:`IntegrationType`\n";

static PyObject * UnaryFunction1DFloat_getIntegrationType(BPy_UnaryFunction1DFloat* self) {
	return BPy_IntegrationType_from_IntegrationType( self->uf1D_float->getIntegrationType() );
}

/*----------------------UnaryFunction1DFloat instance definitions ----------------------------*/
static PyMethodDef BPy_UnaryFunction1DFloat_methods[] = {
	{"getName", ( PyCFunction ) UnaryFunction1DFloat_getName, METH_NOARGS, UnaryFunction1DFloat_getName___doc__},
	{"setIntegrationType", ( PyCFunction ) UnaryFunction1DFloat_setIntegrationType, METH_VARARGS, UnaryFunction1DFloat_setIntegrationType___doc__},
	{"getIntegrationType", ( PyCFunction ) UnaryFunction1DFloat_getIntegrationType, METH_NOARGS, UnaryFunction1DFloat_getIntegrationType___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_UnaryFunction1DFloat type definition ------------------------------*/

PyTypeObject UnaryFunction1DFloat_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"UnaryFunction1DFloat",         /* tp_name */
	sizeof(BPy_UnaryFunction1DFloat), /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)UnaryFunction1DFloat___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)UnaryFunction1DFloat___repr__, /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	(ternaryfunc)UnaryFunction1DFloat___call__, /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	UnaryFunction1DFloat___doc__,   /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_UnaryFunction1DFloat_methods, /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryFunction1D_Type,          /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)UnaryFunction1DFloat___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
