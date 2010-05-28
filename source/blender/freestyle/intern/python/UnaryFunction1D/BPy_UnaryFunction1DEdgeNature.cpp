#include "BPy_UnaryFunction1DEdgeNature.h"

#include "../BPy_Convert.h"
#include "../BPy_Interface1D.h"
#include "../BPy_IntegrationType.h"

#include "UnaryFunction1D_Nature_EdgeNature/BPy_CurveNatureF1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------

int UnaryFunction1DEdgeNature_Init( PyObject *module ) {

	if( module == NULL )
		return -1;

	if( PyType_Ready( &UnaryFunction1DEdgeNature_Type ) < 0 )
		return -1;
	Py_INCREF( &UnaryFunction1DEdgeNature_Type );
	PyModule_AddObject(module, "UnaryFunction1DEdgeNature", (PyObject *)&UnaryFunction1DEdgeNature_Type);
	
	if( PyType_Ready( &CurveNatureF1D_Type ) < 0 )
		return -1;
	Py_INCREF( &CurveNatureF1D_Type );
	PyModule_AddObject(module, "CurveNatureF1D", (PyObject *)&CurveNatureF1D_Type);

	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char UnaryFunction1DEdgeNature___doc__[] =
"Base class for unary functions (functors) that work on\n"
":class:`Interface1D` and return a :class:`Nature` object.\n"
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

static int UnaryFunction1DEdgeNature___init__(BPy_UnaryFunction1DEdgeNature* self, PyObject *args)
{
	PyObject *obj = 0;

	if( !PyArg_ParseTuple(args, "|O!", &IntegrationType_Type, &obj) )		
		return -1;
	
	if( !obj )
		self->uf1D_edgenature = new UnaryFunction1D<Nature::EdgeNature>();
	else {
		self->uf1D_edgenature = new UnaryFunction1D<Nature::EdgeNature>( IntegrationType_from_BPy_IntegrationType(obj) );
	}
	
	self->uf1D_edgenature->py_uf1D = (PyObject *)self;
	
	return 0;
}

static void UnaryFunction1DEdgeNature___dealloc__(BPy_UnaryFunction1DEdgeNature* self)
{
	if (self->uf1D_edgenature)
		delete self->uf1D_edgenature;
	UnaryFunction1D_Type.tp_dealloc((PyObject*)self);
}

static PyObject * UnaryFunction1DEdgeNature___repr__(BPy_UnaryFunction1DEdgeNature* self)
{
	return PyUnicode_FromFormat("type: %s - address: %p", self->uf1D_edgenature->getName().c_str(), self->uf1D_edgenature );
}

static char UnaryFunction1DEdgeNature_getName___doc__[] =
".. method:: getName()\n"
"\n"
"   Returns the name of the unary 1D function.\n"
"\n"
"   :return: The name of the unary 1D function.\n"
"   :rtype: str\n";

static PyObject * UnaryFunction1DEdgeNature_getName( BPy_UnaryFunction1DEdgeNature *self )
{
	return PyUnicode_FromString( self->uf1D_edgenature->getName().c_str() );
}

static PyObject * UnaryFunction1DEdgeNature___call__( BPy_UnaryFunction1DEdgeNature *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj;

	if( kwds != NULL ) {
		PyErr_SetString(PyExc_TypeError, "keyword argument(s) not supported");
		return NULL;
	}
	if( !PyArg_ParseTuple(args, "O!", &Interface1D_Type, &obj) )
		return NULL;
	
	if( typeid(*(self->uf1D_edgenature)) == typeid(UnaryFunction1D<Nature::EdgeNature>) ) {
		PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
		return NULL;
	}
	if (self->uf1D_edgenature->operator()(*( ((BPy_Interface1D *) obj)->if1D )) < 0) {
		if (!PyErr_Occurred()) {
			string msg(self->uf1D_edgenature->getName() + " __call__ method failed");
			PyErr_SetString(PyExc_RuntimeError, msg.c_str());
		}
		return NULL;
	}
	return BPy_Nature_from_Nature( self->uf1D_edgenature->result );

}

static char UnaryFunction1DEdgeNature_setIntegrationType___doc__[] =
".. method:: setIntegrationType(integration)\n"
"\n"
"   Sets the integration method.\n"
"\n"
"   :arg integration: An integration method.\n"
"   :type integration: :class:`IntegrationType`\n";

static PyObject * UnaryFunction1DEdgeNature_setIntegrationType(BPy_UnaryFunction1DEdgeNature* self, PyObject *args)
{
	PyObject *obj;

	if( !PyArg_ParseTuple(args, "O!", &IntegrationType_Type, &obj) )
		return NULL;
	
	self->uf1D_edgenature->setIntegrationType( IntegrationType_from_BPy_IntegrationType(obj) );
	Py_RETURN_NONE;
}

static char UnaryFunction1DEdgeNature_getIntegrationType___doc__[] =
".. method:: getIntegrationType(integration)\n"
"\n"
"   Returns the integration method.\n"
"\n"
"   :return: The integration method.\n"
"   :rtype: :class:`IntegrationType`\n";

static PyObject * UnaryFunction1DEdgeNature_getIntegrationType(BPy_UnaryFunction1DEdgeNature* self) {
	return BPy_IntegrationType_from_IntegrationType( self->uf1D_edgenature->getIntegrationType() );
}

/*----------------------UnaryFunction1DEdgeNature instance definitions ----------------------------*/
static PyMethodDef BPy_UnaryFunction1DEdgeNature_methods[] = {
	{"getName", ( PyCFunction ) UnaryFunction1DEdgeNature_getName, METH_NOARGS, UnaryFunction1DEdgeNature_getName___doc__},
	{"setIntegrationType", ( PyCFunction ) UnaryFunction1DEdgeNature_setIntegrationType, METH_VARARGS, UnaryFunction1DEdgeNature_setIntegrationType___doc__},
	{"getIntegrationType", ( PyCFunction ) UnaryFunction1DEdgeNature_getIntegrationType, METH_NOARGS, UnaryFunction1DEdgeNature_getIntegrationType___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_UnaryFunction1DEdgeNature type definition ------------------------------*/

PyTypeObject UnaryFunction1DEdgeNature_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"UnaryFunction1DEdgeNature",    /* tp_name */
	sizeof(BPy_UnaryFunction1DEdgeNature), /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)UnaryFunction1DEdgeNature___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)UnaryFunction1DEdgeNature___repr__, /* tp_repr */
	0,                              /* tp_as_number */
	0,                              /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	(ternaryfunc)UnaryFunction1DEdgeNature___call__, /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	UnaryFunction1DEdgeNature___doc__, /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_UnaryFunction1DEdgeNature_methods, /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&UnaryFunction1D_Type,          /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)UnaryFunction1DEdgeNature___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
