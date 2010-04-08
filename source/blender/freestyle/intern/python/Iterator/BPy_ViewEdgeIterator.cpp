#include "BPy_ViewEdgeIterator.h"

#include "../BPy_Convert.h"
#include "../Interface1D/BPy_ViewEdge.h"


#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

/*---------------  Python API function prototypes for ViewEdgeIterator instance  -----------*/
static int ViewEdgeIterator___init__(BPy_ViewEdgeIterator *self, PyObject *args);

static PyObject * ViewEdgeIterator_getCurrentEdge( BPy_ViewEdgeIterator *self );
static PyObject * ViewEdgeIterator_setCurrentEdge( BPy_ViewEdgeIterator *self, PyObject *args );
static PyObject * ViewEdgeIterator_getBegin( BPy_ViewEdgeIterator *self );
static PyObject * ViewEdgeIterator_setBegin( BPy_ViewEdgeIterator *self, PyObject *args );
static PyObject * ViewEdgeIterator_getOrientation( BPy_ViewEdgeIterator *self );
static PyObject * ViewEdgeIterator_setOrientation( BPy_ViewEdgeIterator *self, PyObject *args );
static PyObject * ViewEdgeIterator_changeOrientation( BPy_ViewEdgeIterator *self );

static PyObject * ViewEdgeIterator_getObject(BPy_ViewEdgeIterator *self);


/*----------------------ViewEdgeIterator instance definitions ----------------------------*/
static PyMethodDef BPy_ViewEdgeIterator_methods[] = {
	{"getCurrentEdge", ( PyCFunction ) ViewEdgeIterator_getCurrentEdge, METH_NOARGS, "() Returns the current pointed ViewEdge."},
	{"setCurrentEdge", ( PyCFunction ) ViewEdgeIterator_setCurrentEdge, METH_VARARGS, "(ViewEdge ve) Sets the current pointed ViewEdge. "},	
	{"getBegin", ( PyCFunction ) ViewEdgeIterator_getBegin, METH_NOARGS, "() Returns the first ViewEdge used for the iteration."},
	{"setBegin", ( PyCFunction ) ViewEdgeIterator_setBegin, METH_VARARGS, "(ViewEdge ve) Sets the first ViewEdge used for the iteration."},
	{"getOrientation", ( PyCFunction ) ViewEdgeIterator_getOrientation, METH_NOARGS, "() Gets the orientation of the pointed ViewEdge in the iteration. "},
	{"setOrientation", ( PyCFunction ) ViewEdgeIterator_setOrientation, METH_VARARGS, "(bool b) Sets the orientation of the pointed ViewEdge in the iteration. "},
	{"changeOrientation", ( PyCFunction ) ViewEdgeIterator_changeOrientation, METH_NOARGS, "() Changes the current orientation."},
	{"getObject", ( PyCFunction ) ViewEdgeIterator_getObject, METH_NOARGS, "() Get object referenced by the iterator"},
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
	"ViewEdgeIterator objects",     /* tp_doc */
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

//------------------------INSTANCE METHODS ----------------------------------

int ViewEdgeIterator___init__(BPy_ViewEdgeIterator *self, PyObject *args )
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


PyObject *ViewEdgeIterator_getCurrentEdge( BPy_ViewEdgeIterator *self ) {
	ViewEdge *ve = self->ve_it->getCurrentEdge();
	if( ve )
		return BPy_ViewEdge_from_ViewEdge( *ve );
		
	Py_RETURN_NONE;
}

PyObject *ViewEdgeIterator_setCurrentEdge( BPy_ViewEdgeIterator *self, PyObject *args ) {
	PyObject *py_ve;

	if(!( PyArg_ParseTuple(args, "O!", &ViewEdge_Type, &py_ve) ))
		return NULL;

	self->ve_it->setCurrentEdge( ((BPy_ViewEdge *) py_ve)->ve );
		
	Py_RETURN_NONE;
}


PyObject *ViewEdgeIterator_getBegin( BPy_ViewEdgeIterator *self ) {
	ViewEdge *ve = self->ve_it->getBegin();
	if( ve )
		return BPy_ViewEdge_from_ViewEdge( *ve );
		
	Py_RETURN_NONE;
}

PyObject *ViewEdgeIterator_setBegin( BPy_ViewEdgeIterator *self, PyObject *args ) {
	PyObject *py_ve;

	if(!( PyArg_ParseTuple(args, "O!", &ViewEdge_Type, &py_ve) ))
		return NULL;

	self->ve_it->setBegin( ((BPy_ViewEdge *) py_ve)->ve );
		
	Py_RETURN_NONE;
}

PyObject *ViewEdgeIterator_getOrientation( BPy_ViewEdgeIterator *self ) {
	return PyBool_from_bool( self->ve_it->getOrientation() );
}

PyObject *ViewEdgeIterator_setOrientation( BPy_ViewEdgeIterator *self, PyObject *args ) {
	PyObject *py_b;

	if(!( PyArg_ParseTuple(args, "O", &py_b) ))
		return NULL;

	self->ve_it->setOrientation( bool_from_PyBool(py_b) );
		
	Py_RETURN_NONE;
}

PyObject *ViewEdgeIterator_changeOrientation( BPy_ViewEdgeIterator *self ) {
	self->ve_it->changeOrientation();
	
	Py_RETURN_NONE;
}

PyObject * ViewEdgeIterator_getObject( BPy_ViewEdgeIterator *self) {

	ViewEdge *ve = self->ve_it->operator*();
	if( ve )
		return BPy_ViewEdge_from_ViewEdge( *ve );

	Py_RETURN_NONE;
}


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
