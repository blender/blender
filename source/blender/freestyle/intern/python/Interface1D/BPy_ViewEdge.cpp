#include "BPy_ViewEdge.h"

#include "../BPy_Convert.h"
#include "../BPy_Id.h"
#include "../Interface0D/BPy_ViewVertex.h"
#include "../Interface1D/BPy_FEdge.h"
#include "../Interface1D/BPy_ViewEdge.h"
#include "../BPy_Nature.h"
#include "../BPy_ViewShape.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------
	
static char ViewEdge___doc__[] =
"Class defining a ViewEdge.  A ViewEdge in an edge of the image graph.\n"
"it connnects two :class:`ViewVertex` objects.  It is made by connecting\n"
"a set of FEdges.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(iBrother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg iBrother: A ViewEdge object.\n"
"   :type iBrother: :class:`ViewEdge`\n";

static int ViewEdge___init__(BPy_ViewEdge *self, PyObject *args, PyObject *kwds)
{
    if ( !PyArg_ParseTuple(args, "") )
        return -1;
	self->ve = new ViewEdge();
	self->py_if1D.if1D = self->ve;
	self->py_if1D.borrowed = 0;

	return 0;
}

static char ViewEdge_A___doc__[] =
".. method:: A()\n"
"\n"
"   Returns the first ViewVertex.\n"
"\n"
"   :return: The first ViewVertex.\n"
"   :rtype: :class:`ViewVertex`\n";

static PyObject * ViewEdge_A( BPy_ViewEdge *self ) {	
	ViewVertex *v = self->ve->A();
	if( v ){
		return Any_BPy_ViewVertex_from_ViewVertex( *v );
	}
		
	Py_RETURN_NONE;
}

static char ViewEdge_B___doc__[] =
".. method:: B()\n"
"\n"
"   Returns the second ViewVertex.\n"
"\n"
"   :return: The second ViewVertex.\n"
"   :rtype: :class:`ViewVertex`\n";

static PyObject * ViewEdge_B( BPy_ViewEdge *self ) {	
	ViewVertex *v = self->ve->B();
	if( v ){
		return Any_BPy_ViewVertex_from_ViewVertex( *v );
	}
		
	Py_RETURN_NONE;
}

static char ViewEdge_fedgeA___doc__[] =
".. method:: fedgeA()\n"
"\n"
"   Returns the first FEdge that constitutes this ViewEdge.\n"
"\n"
"   :return: The first FEdge constituting this ViewEdge.\n"
"   :rtype: :class:`FEdge`\n";

static PyObject * ViewEdge_fedgeA( BPy_ViewEdge *self ) {	
	FEdge *A = self->ve->fedgeA();
	if( A ){
		return Any_BPy_FEdge_from_FEdge( *A );
	}
		
	Py_RETURN_NONE;
}

static char ViewEdge_fedgeB___doc__[] =
".. method:: fedgeB()\n"
"\n"
"   Returns the last FEdge that constitutes this ViewEdge.\n"
"\n"
"   :return: The last FEdge constituting this ViewEdge.\n"
"   :rtype: :class:`FEdge`\n";

static PyObject * ViewEdge_fedgeB( BPy_ViewEdge *self ) {	
	FEdge *B = self->ve->fedgeB();
	if( B ){
		return Any_BPy_FEdge_from_FEdge( *B );
	}
		
	Py_RETURN_NONE;
}

static char ViewEdge_viewShape___doc__[] =
".. method:: viewShape()\n"
"\n"
"   Returns the ViewShape to which this ViewEdge belongs to.\n"
"\n"
"   :return: The ViewShape to which this ViewEdge belongs to.\n"
"   :rtype: :class:`ViewShape`\n";

static PyObject * ViewEdge_viewShape( BPy_ViewEdge *self ) {	
	ViewShape *vs = self->ve->viewShape();
	if( vs ){
		return BPy_ViewShape_from_ViewShape( *vs );
	}
		
	Py_RETURN_NONE;
}

static char ViewEdge_aShape___doc__[] =
".. method:: aShape()\n"
"\n"
"   Returns the shape that is occluded by the ViewShape to which this\n"
"   ViewEdge belongs to.  If no object is occluded, None is returned.\n"
"\n"
"   :return: The occluded ViewShape.\n"
"   :rtype: :class:`ViewShape`\n";

static PyObject * ViewEdge_aShape( BPy_ViewEdge *self ) {	
	ViewShape *vs = self->ve->aShape();
	if( vs ){
		return BPy_ViewShape_from_ViewShape( *vs );
	}
		
	Py_RETURN_NONE;
}

static char ViewEdge_isClosed___doc__[] =
".. method:: isClosed()\n"
"\n"
"   Tells whether this ViewEdge forms a closed loop or not.\n"
"\n"
"   :return: True if this ViewEdge forms a closed loop.\n"
"   :rtype: bool\n";

static PyObject * ViewEdge_isClosed( BPy_ViewEdge *self ) {
	return PyBool_from_bool( self->ve->isClosed() );	
}

static char ViewEdge_getChainingTimeStamp___doc__[] =
".. method:: getChainingTimeStamp()\n"
"\n"
"   Returns the time stamp of this ViewEdge.\n"
"\n"
"   :return: The time stamp.\n"
"   :rtype: int\n";

static PyObject * ViewEdge_getChainingTimeStamp( BPy_ViewEdge *self ) {
	return PyLong_FromLong( self->ve->getChainingTimeStamp() );
}

static char ViewEdge_setChainingTimeStamp___doc__[] =
".. method:: setChainingTimeStamp(ts)\n"
"\n"
"   Sets the time stamp value.\n"
"\n"
"   :arg ts: The time stamp.\n"
"   :type ts: int\n";

static PyObject * ViewEdge_setChainingTimeStamp( BPy_ViewEdge *self, PyObject *args) {
	int timestamp = 0 ;

	if( !PyArg_ParseTuple(args, "i", &timestamp) )
		return NULL;
	
	self->ve->setChainingTimeStamp( timestamp );

	Py_RETURN_NONE;
}

static char ViewEdge_setA___doc__[] =
".. method:: setA(iA)\n"
"\n"
"   Sets the first ViewVertex of the ViewEdge.\n"
"\n"
"   :arg iA: The first ViewVertex of the ViewEdge.\n"
"   :type iA: :class:`ViewVertex`\n";

static PyObject *ViewEdge_setA( BPy_ViewEdge *self , PyObject *args) {
	PyObject *py_vv;

	if(!( PyArg_ParseTuple(args, "O!", &ViewVertex_Type, &py_vv) ))
		return NULL;

	self->ve->setA( ((BPy_ViewVertex *) py_vv)->vv );

	Py_RETURN_NONE;
}

static char ViewEdge_setB___doc__[] =
".. method:: setB(iB)\n"
"\n"
"   Sets the last ViewVertex of the ViewEdge.\n"
"\n"
"   :arg iB: The last ViewVertex of the ViewEdge.\n"
"   :type iB: :class:`ViewVertex`\n";

static PyObject *ViewEdge_setB( BPy_ViewEdge *self , PyObject *args) {
	PyObject *py_vv;

	if(!( PyArg_ParseTuple(args, "O!", &ViewVertex_Type, &py_vv) ))
		return NULL;

	self->ve->setB( ((BPy_ViewVertex *) py_vv)->vv );

	Py_RETURN_NONE;
}

static char ViewEdge_setNature___doc__[] =
".. method:: setNature(iNature)\n"
"\n"
"   Sets the nature of the ViewEdge.\n"
"\n"
"   :arg iNature: The nature of the ViewEdge.\n"
"   :type iNature: :class:`Nature`\n";

static PyObject * ViewEdge_setNature( BPy_ViewEdge *self, PyObject *args ) {
	PyObject *py_n;

	if(!( PyArg_ParseTuple(args, "O!", &Nature_Type, &py_n) ))
		return NULL;
	
	PyObject *i = (PyObject *) &( ((BPy_Nature *) py_n)->i );
	self->ve->setNature( PyLong_AsLong(i) );

	Py_RETURN_NONE;
}

static char ViewEdge_setFEdgeA___doc__[] =
".. method:: setFEdgeA(iFEdge)\n"
"\n"
"   Sets the first FEdge of the ViewEdge.\n"
"\n"
"   :arg iFEdge: The first FEdge of the ViewEdge.\n"
"   :type iFEdge: :class:`FEdge`\n";

static PyObject * ViewEdge_setFEdgeA( BPy_ViewEdge *self, PyObject *args ) {
	PyObject *py_fe;

	if(!( PyArg_ParseTuple(args, "O!", &FEdge_Type, &py_fe) ))
		return NULL;

	self->ve->setFEdgeA( ((BPy_FEdge *) py_fe)->fe );

	Py_RETURN_NONE;
}

static char ViewEdge_setFEdgeB___doc__[] =
".. method:: setFEdgeB(iFEdge)\n"
"\n"
"   Sets the last FEdge of the ViewEdge.\n"
"\n"
"   :arg iFEdge: The last FEdge of the ViewEdge.\n"
"   :type iFEdge: :class:`FEdge`\n";

static PyObject * ViewEdge_setFEdgeB( BPy_ViewEdge *self, PyObject *args ) {
	PyObject *py_fe;

	if(!( PyArg_ParseTuple(args, "O!", &FEdge_Type, &py_fe) ))
		return NULL;

	self->ve->setFEdgeB( ((BPy_FEdge *) py_fe)->fe );

	Py_RETURN_NONE;
}

static char ViewEdge_setShape___doc__[] =
".. method:: setShape(iVShape)\n"
"\n"
"   Sets the ViewShape to which this ViewEdge belongs to.\n"
"\n"
"   :arg iVShape: The ViewShape to which this ViewEdge belongs to.\n"
"   :type iVShape: :class:`ViewShape`\n";

static PyObject * ViewEdge_setShape( BPy_ViewEdge *self, PyObject *args ) {
	PyObject *py_vs;

	if(!( PyArg_ParseTuple(args, "O", &ViewShape_Type, &py_vs) ))
		return NULL;

	self->ve->setShape( ((BPy_ViewShape *) py_vs)->vs );

	Py_RETURN_NONE;
}

static char ViewEdge_setId___doc__[] =
".. method:: setId(id)\n"
"\n"
"   Sets the ViewEdge id.\n"
"\n"
"   :arg id: An Id object.\n"
"   :type id: :class:`Id`\n";

static PyObject * ViewEdge_setId( BPy_ViewEdge *self, PyObject *args ) {
	PyObject *py_id;

	if(!( PyArg_ParseTuple(args, "O!", &Id_Type, &py_id) ))
		return NULL;

	Id id(*( ((BPy_Id *) py_id)->id ));
	self->ve->setId( id );

	Py_RETURN_NONE;
}

static char ViewEdge_UpdateFEdges___doc__[] =
".. method:: UpdateFEdges()\n"
"\n"
"   Sets Viewedge to this for all embedded fedges.\n";

static PyObject * ViewEdge_UpdateFEdges( BPy_ViewEdge *self ) {
	self->ve->UpdateFEdges();

	Py_RETURN_NONE;
}

static char ViewEdge_setaShape___doc__[] =
".. method:: setaShape(iShape)\n"
"\n"
"   Sets the occluded ViewShape.\n"
"\n"
"   :arg iShape: A ViewShape object.\n"
"   :type iShape: :class:`ViewShape`\n";

static PyObject * ViewEdge_setaShape( BPy_ViewEdge *self, PyObject *args ) {
	PyObject *py_vs;

	if(!( PyArg_ParseTuple(args, "O!", &ViewShape_Type, &py_vs) ))
		return NULL;

	ViewShape *vs = ((BPy_ViewShape *) py_vs)->vs;
	self->ve->setaShape( vs );

	Py_RETURN_NONE;
}

static char ViewEdge_setQI___doc__[] =
".. method:: setQI(qi)\n"
"\n"
"   Sets the quantitative invisibility value of the ViewEdge.\n"
"\n"
"   :arg qi: The quantitative invisibility.\n"
"   :type qi: int\n";

static PyObject * ViewEdge_setQI( BPy_ViewEdge *self, PyObject *args ) {
	int qi;

	if(!( PyArg_ParseTuple(args, "i", &qi) ))
		return NULL;

	self->ve->setQI( qi );

	Py_RETURN_NONE;
}

static char ViewEdge_qi___doc__[] =
".. method:: getChainingTimeStamp()\n"
"\n"
"   Returns the quantitative invisibility value of the ViewEdge.\n"
"\n"
"   :return: The quantitative invisibility.\n"
"   :rtype: int\n";

static PyObject * ViewEdge_qi( BPy_ViewEdge *self ) {
	return PyLong_FromLong( self->ve->qi() );
}

/*----------------------ViewEdge instance definitions ----------------------------*/
static PyMethodDef BPy_ViewEdge_methods[] = {	
	{"A", ( PyCFunction ) ViewEdge_A, METH_NOARGS, ViewEdge_A___doc__},
	{"B", ( PyCFunction ) ViewEdge_B, METH_NOARGS, ViewEdge_B___doc__},
	{"fedgeA", ( PyCFunction ) ViewEdge_fedgeA, METH_NOARGS, ViewEdge_fedgeA___doc__},
	{"fedgeB", ( PyCFunction ) ViewEdge_fedgeB, METH_NOARGS, ViewEdge_fedgeB___doc__},
	{"viewShape", ( PyCFunction ) ViewEdge_viewShape, METH_NOARGS, ViewEdge_viewShape___doc__},
	{"aShape", ( PyCFunction ) ViewEdge_aShape, METH_NOARGS, ViewEdge_aShape___doc__},
	{"isClosed", ( PyCFunction ) ViewEdge_isClosed, METH_NOARGS, ViewEdge_isClosed___doc__},
	{"getChainingTimeStamp", ( PyCFunction ) ViewEdge_getChainingTimeStamp, METH_NOARGS, ViewEdge_getChainingTimeStamp___doc__},
	{"setChainingTimeStamp", ( PyCFunction ) ViewEdge_setChainingTimeStamp, METH_VARARGS, ViewEdge_setChainingTimeStamp___doc__},
	{"setA", ( PyCFunction ) ViewEdge_setA, METH_VARARGS, ViewEdge_setA___doc__},
	{"setB", ( PyCFunction ) ViewEdge_setB, METH_VARARGS, ViewEdge_setB___doc__},
	{"setNature", ( PyCFunction ) ViewEdge_setNature, METH_VARARGS, ViewEdge_setNature___doc__},
	{"setFEdgeA", ( PyCFunction ) ViewEdge_setFEdgeA, METH_VARARGS, ViewEdge_setFEdgeA___doc__},
	{"setFEdgeB", ( PyCFunction ) ViewEdge_setFEdgeB, METH_VARARGS, ViewEdge_setFEdgeB___doc__},
	{"setShape", ( PyCFunction ) ViewEdge_setShape, METH_VARARGS, ViewEdge_setShape___doc__},
	{"setId", ( PyCFunction ) ViewEdge_setId, METH_VARARGS, ViewEdge_setId___doc__},
	{"UpdateFEdges", ( PyCFunction ) ViewEdge_UpdateFEdges, METH_NOARGS, ViewEdge_UpdateFEdges___doc__},
	{"setaShape", ( PyCFunction ) ViewEdge_setaShape, METH_VARARGS, ViewEdge_setaShape___doc__},
	{"setQI", ( PyCFunction ) ViewEdge_setQI, METH_VARARGS, ViewEdge_setQI___doc__},
	{"qi", ( PyCFunction ) ViewEdge_qi, METH_NOARGS, ViewEdge_qi___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_ViewEdge type definition ------------------------------*/

PyTypeObject ViewEdge_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"ViewEdge",                     /* tp_name */
	sizeof(BPy_ViewEdge),           /* tp_basicsize */
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
	ViewEdge___doc__,               /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_ViewEdge_methods,           /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&Interface1D_Type,              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)ViewEdge___init__,    /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
