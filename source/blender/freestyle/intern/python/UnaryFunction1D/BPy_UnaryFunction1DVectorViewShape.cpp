#include "BPy_UnaryFunction1DVectorViewShape.h"

#include "../BPy_Convert.h"
#include "../BPy_Interface1D.h"
#include "../BPy_IntegrationType.h"

#include "UnaryFunction1D_vector_ViewShape/BPy_GetOccludeeF1D.h"
#include "UnaryFunction1D_vector_ViewShape/BPy_GetOccludersF1D.h"
#include "UnaryFunction1D_vector_ViewShape/BPy_GetShapeF1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------

int UnaryFunction1DVectorViewShape_Init( PyObject *module ) {

	if( module == NULL )
		return -1;

	if( PyType_Ready( &UnaryFunction1DVectorViewShape_Type ) < 0 )
		return -1;
	Py_INCREF( &UnaryFunction1DVectorViewShape_Type );
	PyModule_AddObject(module, "UnaryFunction1DVectorViewShape", (PyObject *)&UnaryFunction1DVectorViewShape_Type);
	
	if( PyType_Ready( &GetOccludeeF1D_Type ) < 0 )
		return -1;
	Py_INCREF( &GetOccludeeF1D_Type );
	PyModule_AddObject(module, "GetOccludeeF1D", (PyObject *)&GetOccludeeF1D_Type);

	if( PyType_Ready( &GetOccludersF1D_Type ) < 0 )
		return -1;
	Py_INCREF( &GetOccludersF1D_Type );
	PyModule_AddObject(module, "GetOccludersF1D", (PyObject *)&GetOccludersF1D_Type);
	
	if( PyType_Ready( &GetShapeF1D_Type ) < 0 )
		return -1;
	Py_INCREF( &GetShapeF1D_Type );
	PyModule_AddObject(module, "GetShapeF1D", (PyObject *)&GetShapeF1D_Type);

	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char UnaryFunction1DVectorViewShape___doc__[] =
"Base class for unary functions (functors) that work on\n"
":class:`Interface1D` and return a list of :class:`ViewShape`\n"
"objects.\n"
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

static int UnaryFunction1DVectorViewShape___init__(BPy_UnaryFunction1DVectorViewShape* self, PyObject *args)
{
	PyObject *obj = 0;

	if( !PyArg_ParseTuple(args, "|O!", &IntegrationType_Type, &obj) )
		return -1;
	
	if( !obj )
		self->uf1D_vectorviewshape = new UnaryFunction1D< std::vector<ViewShape*> >();
	else {
		self->uf1D_vectorviewshape = new UnaryFunction1D< std::vector<ViewShape*> >( IntegrationType_from_BPy_IntegrationType(obj) );
	}
	
	self->uf1D_vectorviewshape->py_uf1D = (PyObject *)self;
	
	return 0;
}

static void UnaryFunction1DVectorViewShape___dealloc__(BPy_UnaryFunction1DVectorViewShape* self)
{
	if (self->uf1D_vectorviewshape)
		delete self->uf1D_vectorviewshape;
	UnaryFunction1D_Type.tp_dealloc((PyObject*)self);
}

static PyObject * UnaryFunction1DVectorViewShape___repr__(BPy_UnaryFunction1DVectorViewShape* self)
{
	return PyUnicode_FromFormat("type: %s - address: %p", self->uf1D_vectorviewshape->getName().c_str(), self->uf1D_vectorviewshape );
}

static char UnaryFunction1DVectorViewShape_getName___doc__[] =
".. method:: getName()\n"
"\n"
"   Returns the name of the unary 1D function.\n"
"\n"
"   :return: The name of the unary 1D function.\n"
"   :rtype: str\n";

static PyObject * UnaryFunction1DVectorViewShape_getName( BPy_UnaryFunction1DVectorViewShape *self )
{
	return PyUnicode_FromString( self->uf1D_vectorviewshape->getName().c_str() );
}

static PyObject * UnaryFunction1DVectorViewShape___call__( BPy_UnaryFunction1DVectorViewShape *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj;

	if( kwds != NULL ) {
		PyErr_SetString(PyExc_TypeError, "keyword argument(s) not supported");
		return NULL;
	}
	if( !PyArg_ParseTuple(args, "O!", &Interface1D_Type, &obj) )
		return NULL;
	
	if( typeid(*(self->uf1D_vectorviewshape)) == typeid(UnaryFunction1D< std::vector<ViewShape*> >) ) {
		PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
		return NULL;
	}
	if (self->uf1D_vectorviewshape->operator()(*( ((BPy_Interface1D *) obj)->if1D )) < 0) {
		if (!PyErr_Occurred()) {
			string msg(self->uf1D_vectorviewshape->getName() + " __call__ method failed");
			PyErr_SetString(PyExc_RuntimeError, msg.c_str());
		}
		return NULL;
	}
	PyObject *list = PyList_New(0);
	PyObject *item;
	for( unsigned int i = 0; i < self->uf1D_vectorviewshape->result.size(); i++) {
		ViewShape *v = self->uf1D_vectorviewshape->result[i];
		if (v) {
			item = BPy_ViewShape_from_ViewShape(*v);
		} else {
			item = Py_None;
			Py_INCREF(item);
		}
		PyList_Append(list, item);
	}
	
	return list;
}

static char UnaryFunction1DVectorViewShape_setIntegrationType___doc__[] =
".. method:: setIntegrationType(integration)\n"
"\n"
"   Sets the integration method.\n"
"\n"
"   :arg integration: An integration method.\n"
"   :type integration: :class:`IntegrationType`\n";

static PyObject * UnaryFunction1DVectorViewShape_setIntegrationType(BPy_UnaryFunction1DVectorViewShape* self, PyObject *args)
{
	PyObject *obj;

	if( !PyArg_ParseTuple(args, "O!", &IntegrationType_Type, &obj) )
		return NULL;
	
	self->uf1D_vectorviewshape->setIntegrationType( IntegrationType_from_BPy_IntegrationType(obj) );
	Py_RETURN_NONE;
}

static char UnaryFunction1DVectorViewShape_getIntegrationType___doc__[] =
".. method:: getIntegrationType(integration)\n"
"\n"
"   Returns the integration method.\n"
"\n"
"   :return: The integration method.\n"
"   :rtype: :class:`IntegrationType`\n";

static PyObject * UnaryFunction1DVectorViewShape_getIntegrationType(BPy_UnaryFunction1DVectorViewShape* self) {
	return BPy_IntegrationType_from_IntegrationType( self->uf1D_vectorviewshape->getIntegrationType() );
}

/*----------------------UnaryFunction1DVectorViewShape instance definitions ----------------------------*/
static PyMethodDef BPy_UnaryFunction1DVectorViewShape_methods[] = {
	{"getName", ( PyCFunction ) UnaryFunction1DVectorViewShape_getName, METH_NOARGS, UnaryFunction1DVectorViewShape_getName___doc__},
	{"setIntegrationType", ( PyCFunction ) UnaryFunction1DVectorViewShape_setIntegrationType, METH_VARARGS, UnaryFunction1DVectorViewShape_setIntegrationType___doc__},
	{"getIntegrationType", ( PyCFunction ) UnaryFunction1DVectorViewShape_getIntegrationType, METH_NOARGS, UnaryFunction1DVectorViewShape_getIntegrationType___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_UnaryFunction1DVectorViewShape type definition ------------------------------*/

PyTypeObject UnaryFunction1DVectorViewShape_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"UnaryFunction1DVectorViewShape", /* tp_name */
	sizeof(BPy_UnaryFunction1DVectorViewShape), /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)UnaryFunction1DVectorViewShape___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)UnaryFunction1DVectorViewShape___repr__, /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	(ternaryfunc)UnaryFunction1DVectorViewShape___call__, /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	UnaryFunction1DVectorViewShape___doc__, /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_UnaryFunction1DVectorViewShape_methods, /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryFunction1D_Type,          /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)UnaryFunction1DVectorViewShape___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
