#include "BPy_UnaryFunction1DVoid.h"

#include "../BPy_Convert.h"
#include "../BPy_Interface1D.h"
#include "../BPy_IntegrationType.h"

#include "UnaryFunction1D_void/BPy_ChainingTimeStampF1D.h"
#include "UnaryFunction1D_void/BPy_IncrementChainingTimeStampF1D.h"
#include "UnaryFunction1D_void/BPy_TimeStampF1D.h"


#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------

int UnaryFunction1DVoid_Init( PyObject *module ) {

	if( module == NULL )
		return -1;

	if( PyType_Ready( &UnaryFunction1DVoid_Type ) < 0 )
		return -1;
	Py_INCREF( &UnaryFunction1DVoid_Type );
	PyModule_AddObject(module, "UnaryFunction1DVoid", (PyObject *)&UnaryFunction1DVoid_Type);
	
	if( PyType_Ready( &ChainingTimeStampF1D_Type ) < 0 )
		return -1;
	Py_INCREF( &ChainingTimeStampF1D_Type );
	PyModule_AddObject(module, "ChainingTimeStampF1D", (PyObject *)&ChainingTimeStampF1D_Type);
	
	if( PyType_Ready( &IncrementChainingTimeStampF1D_Type ) < 0 )
		return -1;
	Py_INCREF( &IncrementChainingTimeStampF1D_Type );
	PyModule_AddObject(module, "IncrementChainingTimeStampF1D", (PyObject *)&IncrementChainingTimeStampF1D_Type);
	
	if( PyType_Ready( &TimeStampF1D_Type ) < 0 )
		return -1;
	Py_INCREF( &TimeStampF1D_Type );
	PyModule_AddObject(module, "TimeStampF1D", (PyObject *)&TimeStampF1D_Type);

	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char UnaryFunction1DVoid___doc__[] =
"Base class for unary functions (functors) working on\n"
":class:`Interface1D`.\n"
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

static int UnaryFunction1DVoid___init__(BPy_UnaryFunction1DVoid* self, PyObject *args)
{
	PyObject *obj = 0;

	if( !PyArg_ParseTuple(args, "|O!", &IntegrationType_Type, &obj) )
		return -1;
	
	if( !obj )
		self->uf1D_void = new UnaryFunction1D_void();
	else {
		self->uf1D_void = new UnaryFunction1D_void( IntegrationType_from_BPy_IntegrationType(obj) );
	}
	
	self->uf1D_void->py_uf1D = (PyObject *)self;
	
	return 0;
}

static void UnaryFunction1DVoid___dealloc__(BPy_UnaryFunction1DVoid* self)
{
	if (self->uf1D_void)
		delete self->uf1D_void;
	UnaryFunction1D_Type.tp_dealloc((PyObject*)self);
}

static PyObject * UnaryFunction1DVoid___repr__(BPy_UnaryFunction1DVoid* self)
{
	return PyUnicode_FromFormat("type: %s - address: %p", self->uf1D_void->getName().c_str(), self->uf1D_void );
}

static char UnaryFunction1DVoid_getName___doc__[] =
".. method:: getName()\n"
"\n"
"   Returns the name of the unary 1D function.\n"
"\n"
"   :return: The name of the unary 1D function.\n"
"   :rtype: string\n";

static PyObject * UnaryFunction1DVoid_getName( BPy_UnaryFunction1DVoid *self )
{
	return PyUnicode_FromFormat( self->uf1D_void->getName().c_str() );
}

static PyObject * UnaryFunction1DVoid___call__( BPy_UnaryFunction1DVoid *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj;

	if( kwds != NULL ) {
		PyErr_SetString(PyExc_TypeError, "keyword argument(s) not supported");
		return NULL;
	}
	if( !PyArg_ParseTuple(args, "O!", &Interface1D_Type, &obj) )
		return NULL;
	
	if( typeid(*(self->uf1D_void)) == typeid(UnaryFunction1D_void) ) {
		PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
		return NULL;
	}
	if (self->uf1D_void->operator()(*( ((BPy_Interface1D *) obj)->if1D )) < 0) {
		if (!PyErr_Occurred()) {
			string msg(self->uf1D_void->getName() + " __call__ method failed");
			PyErr_SetString(PyExc_RuntimeError, msg.c_str());
		}
		return NULL;
	}
	Py_RETURN_NONE;
}

static char UnaryFunction1DVoid_setIntegrationType___doc__[] =
".. method:: setIntegrationType(integration)\n"
"\n"
"   Sets the integration method.\n"
"\n"
"   :arg integration: An integration method.\n"
"   :type integration: :class:`IntegrationType`\n";

static PyObject * UnaryFunction1DVoid_setIntegrationType(BPy_UnaryFunction1DVoid* self, PyObject *args)
{
	PyObject *obj;

	if( !PyArg_ParseTuple(args, "O!", &IntegrationType_Type, &obj) )
		return NULL;
	
	self->uf1D_void->setIntegrationType( IntegrationType_from_BPy_IntegrationType(obj) );
	Py_RETURN_NONE;
}

static char UnaryFunction1DVoid_getIntegrationType___doc__[] =
".. method:: getIntegrationType(integration)\n"
"\n"
"   Returns the integration method.\n"
"\n"
"   :return: The integration method.\n"
"   :rtype: :class:`IntegrationType`\n";

static PyObject * UnaryFunction1DVoid_getIntegrationType(BPy_UnaryFunction1DVoid* self) {
	return BPy_IntegrationType_from_IntegrationType( self->uf1D_void->getIntegrationType() );
}

/*----------------------UnaryFunction1DVoid instance definitions ----------------------------*/
static PyMethodDef BPy_UnaryFunction1DVoid_methods[] = {
	{"getName", ( PyCFunction ) UnaryFunction1DVoid_getName, METH_NOARGS, UnaryFunction1DVoid_getName___doc__},
	{"setIntegrationType", ( PyCFunction ) UnaryFunction1DVoid_setIntegrationType, METH_VARARGS, UnaryFunction1DVoid_setIntegrationType___doc__},
	{"getIntegrationType", ( PyCFunction ) UnaryFunction1DVoid_getIntegrationType, METH_NOARGS, UnaryFunction1DVoid_getIntegrationType___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_UnaryFunction1DVoid type definition ------------------------------*/

PyTypeObject UnaryFunction1DVoid_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"UnaryFunction1DVoid",          /* tp_name */
	sizeof(BPy_UnaryFunction1DVoid), /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)UnaryFunction1DVoid___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)UnaryFunction1DVoid___repr__, /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	(ternaryfunc)UnaryFunction1DVoid___call__, /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	UnaryFunction1DVoid___doc__,    /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_UnaryFunction1DVoid_methods, /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryFunction1D_Type,          /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)UnaryFunction1DVoid___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
