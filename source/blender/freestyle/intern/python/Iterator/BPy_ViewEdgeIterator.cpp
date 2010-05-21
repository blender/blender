#include "BPy_ViewEdgeIterator.h"

#include "../BPy_Convert.h"
#include "../Interface1D/BPy_ViewEdge.h"


#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char ViewEdgeIterator___doc__[] =
"Base class for iterators over ViewEdges of the :class:`ViewMap` Graph.\n"
"Basically the increment() operator of this class should be able to\n"
"take the decision of \"where\" (on which ViewEdge) to go when pointing\n"
"on a given ViewEdge.\n"
"\n"
".. method:: __init__(begin=None, orientation=True)\n"
"\n"
"   Builds a ViewEdgeIterator from a starting ViewEdge and its\n"
"   orientation.\n"
"\n"
"   :arg begin: The ViewEdge from where to start the iteration.\n"
"   :type begin: :class:`ViewEdge` or None\n"
"   :arg orientation: If true, we'll look for the next ViewEdge among\n"
"      the ViewEdges that surround the ending ViewVertex of begin.  If\n"
"      false, we'll search over the ViewEdges surrounding the ending\n"
"      ViewVertex of begin.\n"
"   :type orientation: bool\n"
"\n"
".. method:: __init__(it)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg it: A ViewEdgeIterator object.\n"
"   :type it: :class:`ViewEdgeIterator`\n";

static int ViewEdgeIterator___init__(BPy_ViewEdgeIterator *self, PyObject *args )
{	
	PyObject *obj1 = 0, *obj2 = 0;

	if (!( PyArg_ParseTuple(args, "O|O", &obj1, &obj2) ))
	    return -1;

	if( obj1 && BPy_ViewEdgeIterator_Check(obj1)  ) {
		self->ve_it = new ViewEdgeInternal::ViewEdgeIterator(*( ((BPy_ViewEdgeIterator *) obj1)->ve_it ));
	
	} else {
		ViewEdge *begin;
		if ( !obj1 || obj1 == Py_None )
			begin = NULL;
		else if ( BPy_ViewEdge_Check(obj1) )
			begin = ((BPy_ViewEdge *) obj1)->ve;
		else {
			PyErr_SetString(PyExc_TypeError, "1st argument must be either a ViewEdge object or None");
			return -1;
		}
		bool orientation = ( obj2 ) ? bool_from_PyBool(obj2) : true;
		
		self->ve_it = new ViewEdgeInternal::ViewEdgeIterator( begin, orientation);
		
	}
		
	self->py_it.it = self->ve_it;

	return 0;
}

static char ViewEdgeIterator_getCurrentEdge___doc__[] =
".. method:: getCurrentEdge()\n"
"\n"
"   Returns the current pointed ViewEdge.\n"
"\n"
"   :return: The current pointed ViewEdge.\n"
"   :rtype: :class:`ViewEdge`\n";

static PyObject *ViewEdgeIterator_getCurrentEdge( BPy_ViewEdgeIterator *self ) {
	ViewEdge *ve = self->ve_it->getCurrentEdge();
	if( ve )
		return BPy_ViewEdge_from_ViewEdge( *ve );
		
	Py_RETURN_NONE;
}

static char ViewEdgeIterator_setCurrentEdge___doc__[] =
".. method:: setCurrentEdge(edge)\n"
"\n"
"   Sets the current pointed ViewEdge.\n"
"\n"
"   :arg edge: The current pointed ViewEdge.\n"
"   :type edge: :class:`ViewEdge`\n";

static PyObject *ViewEdgeIterator_setCurrentEdge( BPy_ViewEdgeIterator *self, PyObject *args ) {
	PyObject *py_ve;

	if(!( PyArg_ParseTuple(args, "O!", &ViewEdge_Type, &py_ve) ))
		return NULL;

	self->ve_it->setCurrentEdge( ((BPy_ViewEdge *) py_ve)->ve );
		
	Py_RETURN_NONE;
}

static char ViewEdgeIterator_getBegin___doc__[] =
".. method:: getBegin()\n"
"\n"
"   Returns the first ViewEdge used for the iteration.\n"
"\n"
"   :return: The first ViewEdge used for the iteration.\n"
"   :rtype: :class:`ViewEdge`\n";

static PyObject *ViewEdgeIterator_getBegin( BPy_ViewEdgeIterator *self ) {
	ViewEdge *ve = self->ve_it->getBegin();
	if( ve )
		return BPy_ViewEdge_from_ViewEdge( *ve );
		
	Py_RETURN_NONE;
}

static char ViewEdgeIterator_setBegin___doc__[] =
".. method:: setBegin(begin)\n"
"\n"
"   Sets the first ViewEdge used for the iteration.\n"
"\n"
"   :arg begin: The first ViewEdge used for the iteration.\n"
"   :type begin: :class:`ViewEdge`\n";

static PyObject *ViewEdgeIterator_setBegin( BPy_ViewEdgeIterator *self, PyObject *args ) {
	PyObject *py_ve;

	if(!( PyArg_ParseTuple(args, "O!", &ViewEdge_Type, &py_ve) ))
		return NULL;

	self->ve_it->setBegin( ((BPy_ViewEdge *) py_ve)->ve );
		
	Py_RETURN_NONE;
}

static char ViewEdgeIterator_getOrientation___doc__[] =
".. method:: getOrientation()\n"
"\n"
"   Returns the orientation of the pointed ViewEdge in the iteration.\n"
"\n"
"   :return: The orientation of the pointed ViewEdge in the iteration.\n"
"   :rtype: bool\n";

static PyObject *ViewEdgeIterator_getOrientation( BPy_ViewEdgeIterator *self ) {
	return PyBool_from_bool( self->ve_it->getOrientation() );
}

static char ViewEdgeIterator_setOrientation___doc__[] =
".. method:: setOrientation(orientation)\n"
"\n"
"   Sets the orientation of the pointed ViewEdge in the iteration.\n"
"\n"
"   :arg orientation: If true, we'll look for the next ViewEdge among\n"
"      the ViewEdges that surround the ending ViewVertex of begin.  If\n"
"      false, we'll search over the ViewEdges surrounding the ending\n"
"      ViewVertex of begin.\n"
"   :type orientation: bool\n";

static PyObject *ViewEdgeIterator_setOrientation( BPy_ViewEdgeIterator *self, PyObject *args ) {
	PyObject *py_b;

	if(!( PyArg_ParseTuple(args, "O", &py_b) ))
		return NULL;

	self->ve_it->setOrientation( bool_from_PyBool(py_b) );
		
	Py_RETURN_NONE;
}

static char ViewEdgeIterator_changeOrientation___doc__[] =
".. method:: changeOrientation()\n"
"\n"
"   Changes the current orientation.\n";

static PyObject *ViewEdgeIterator_changeOrientation( BPy_ViewEdgeIterator *self ) {
	self->ve_it->changeOrientation();
	
	Py_RETURN_NONE;
}

static char ViewEdgeIterator_getObject___doc__[] =
".. method:: getObject()\n"
"\n"
"   Returns the pointed ViewEdge.\n"
"\n"
"   :return: The pointed ViewEdge.\n"
"   :rtype: :class:`ViewEdge`\n";

static PyObject * ViewEdgeIterator_getObject( BPy_ViewEdgeIterator *self) {

	ViewEdge *ve = self->ve_it->operator*();
	if( ve )
		return BPy_ViewEdge_from_ViewEdge( *ve );

	Py_RETURN_NONE;
}

/*----------------------ViewEdgeIterator instance definitions ----------------------------*/
static PyMethodDef BPy_ViewEdgeIterator_methods[] = {
	{"getCurrentEdge", ( PyCFunction ) ViewEdgeIterator_getCurrentEdge, METH_NOARGS, ViewEdgeIterator_getCurrentEdge___doc__},
	{"setCurrentEdge", ( PyCFunction ) ViewEdgeIterator_setCurrentEdge, METH_VARARGS, ViewEdgeIterator_setCurrentEdge___doc__},
	{"getBegin", ( PyCFunction ) ViewEdgeIterator_getBegin, METH_NOARGS, ViewEdgeIterator_getBegin___doc__},
	{"setBegin", ( PyCFunction ) ViewEdgeIterator_setBegin, METH_VARARGS, ViewEdgeIterator_setBegin___doc__},
	{"getOrientation", ( PyCFunction ) ViewEdgeIterator_getOrientation, METH_NOARGS, ViewEdgeIterator_getOrientation___doc__},
	{"setOrientation", ( PyCFunction ) ViewEdgeIterator_setOrientation, METH_VARARGS, ViewEdgeIterator_setOrientation___doc__},
	{"changeOrientation", ( PyCFunction ) ViewEdgeIterator_changeOrientation, METH_NOARGS, ViewEdgeIterator_changeOrientation___doc__},
	{"getObject", ( PyCFunction ) ViewEdgeIterator_getObject, METH_NOARGS, ViewEdgeIterator_getObject___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_ViewEdgeIterator type definition ------------------------------*/

PyTypeObject ViewEdgeIterator_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"ViewEdgeIterator",             /* tp_name */
	sizeof(BPy_ViewEdgeIterator),   /* tp_basicsize */
	0,                              /* tp_itemsize */
	0,                              /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	0,                              /* tp_repr */
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
	ViewEdgeIterator___doc__,       /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_ViewEdgeIterator_methods,   /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&Iterator_Type,                 /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)ViewEdgeIterator___init__, /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
