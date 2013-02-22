#include "BPy_UnaryFunction0DVectorViewShape.h"

#include "../BPy_Convert.h"
#include "../Iterator/BPy_Interface0DIterator.h"

#include "UnaryFunction0D_vector_ViewShape/BPy_GetOccludersF0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------

int UnaryFunction0DVectorViewShape_Init( PyObject *module ) {

	if( module == NULL )
		return -1;

	if( PyType_Ready( &UnaryFunction0DVectorViewShape_Type ) < 0 )
		return -1;
	Py_INCREF( &UnaryFunction0DVectorViewShape_Type );
	PyModule_AddObject(module, "UnaryFunction0DVectorViewShape", (PyObject *)&UnaryFunction0DVectorViewShape_Type);
	
	if( PyType_Ready( &GetOccludersF0D_Type ) < 0 )
		return -1;
	Py_INCREF( &GetOccludersF0D_Type );
	PyModule_AddObject(module, "GetOccludersF0D", (PyObject *)&GetOccludersF0D_Type);

	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char UnaryFunction0DVectorViewShape___doc__[] =
"Class hierarchy: :class:`UnaryFunction0D` > :class:`UnaryFunction0DVectorViewShape`\n"
"\n"
"Base class for unary functions (functors) that work on\n"
":class:`Interface0DIterator` and return a list of :class:`ViewShape`\n"
"objects.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n";

static int UnaryFunction0DVectorViewShape___init__(BPy_UnaryFunction0DVectorViewShape* self, PyObject *args, PyObject *kwds)
{
    if ( !PyArg_ParseTuple(args, "") )
        return -1;
	self->uf0D_vectorviewshape = new UnaryFunction0D< std::vector<ViewShape*> >();
	self->uf0D_vectorviewshape->py_uf0D = (PyObject *)self;
	return 0;
}

static void UnaryFunction0DVectorViewShape___dealloc__(BPy_UnaryFunction0DVectorViewShape* self)
{
	if (self->uf0D_vectorviewshape)
		delete self->uf0D_vectorviewshape;
	UnaryFunction0D_Type.tp_dealloc((PyObject*)self);
}

static PyObject * UnaryFunction0DVectorViewShape___repr__(BPy_UnaryFunction0DVectorViewShape* self)
{
	return PyUnicode_FromFormat("type: %s - address: %p", self->uf0D_vectorviewshape->getName().c_str(), self->uf0D_vectorviewshape );
}

static char UnaryFunction0DVectorViewShape_getName___doc__[] =
".. method:: getName()\n"
"\n"
"   Returns the name of the unary 0D predicate.\n"
"\n"
"   :return: The name of the unary 0D predicate.\n"
"   :rtype: str\n";

static PyObject * UnaryFunction0DVectorViewShape_getName( BPy_UnaryFunction0DVectorViewShape *self )
{
	return PyUnicode_FromString( self->uf0D_vectorviewshape->getName().c_str() );
}

static PyObject * UnaryFunction0DVectorViewShape___call__( BPy_UnaryFunction0DVectorViewShape *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj;

	if( kwds != NULL ) {
		PyErr_SetString(PyExc_TypeError, "keyword argument(s) not supported");
		return NULL;
	}
	if(!PyArg_ParseTuple(args, "O!", &Interface0DIterator_Type, &obj))
		return NULL;

	if( typeid(*(self->uf0D_vectorviewshape)) == typeid(UnaryFunction0D< std::vector<ViewShape*> >) ) {
		PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
		return NULL;
	}
	if (self->uf0D_vectorviewshape->operator()(*( ((BPy_Interface0DIterator *) obj)->if0D_it )) < 0) {
		if (!PyErr_Occurred()) {
			string class_name(Py_TYPE(self)->tp_name);
			PyErr_SetString(PyExc_RuntimeError, (class_name + " __call__ method failed").c_str());
		}
		return NULL;
	}
	PyObject *list = PyList_New(0);
	PyObject *item;
	for( unsigned int i = 0; i < self->uf0D_vectorviewshape->result.size(); i++) {
		ViewShape *v = self->uf0D_vectorviewshape->result[i];
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

/*----------------------UnaryFunction0DVectorViewShape instance definitions ----------------------------*/
static PyMethodDef BPy_UnaryFunction0DVectorViewShape_methods[] = {
	{"getName", ( PyCFunction ) UnaryFunction0DVectorViewShape_getName, METH_NOARGS, UnaryFunction0DVectorViewShape_getName___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_UnaryFunction0DVectorViewShape type definition ------------------------------*/

PyTypeObject UnaryFunction0DVectorViewShape_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"UnaryFunction0DVectorViewShape", /* tp_name */
	sizeof(BPy_UnaryFunction0DVectorViewShape), /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)UnaryFunction0DVectorViewShape___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)UnaryFunction0DVectorViewShape___repr__, /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	(ternaryfunc)UnaryFunction0DVectorViewShape___call__, /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	UnaryFunction0DVectorViewShape___doc__, /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_UnaryFunction0DVectorViewShape_methods, /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryFunction0D_Type,          /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)UnaryFunction0DVectorViewShape___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
