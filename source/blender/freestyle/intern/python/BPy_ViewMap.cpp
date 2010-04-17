#include "BPy_ViewMap.h"

#include "BPy_Convert.h"
#include "BPy_BBox.h"
#include "Interface1D/BPy_FEdge.h"
#include "Interface1D/BPy_ViewEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int ViewMap_Init( PyObject *module )
{
	if( module == NULL )
		return -1;

	if( PyType_Ready( &ViewMap_Type ) < 0 )
		return -1;

	Py_INCREF( &ViewMap_Type );
	PyModule_AddObject(module, "ViewMap", (PyObject *)&ViewMap_Type);
	return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char ViewMap___doc__[] =
"Class defining the ViewMap.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n";

static int ViewMap___init__(BPy_ViewMap *self, PyObject *args, PyObject *kwds)
{
	self->vm = new ViewMap();
    return 0;
}

static void ViewMap___dealloc__(BPy_ViewMap *self)
{
	if( self->vm )
		delete self->vm;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * ViewMap___repr__(BPy_ViewMap *self)
{
    return PyUnicode_FromFormat("ViewMap - address: %p", self->vm );
}

static char ViewMap_getClosestViewEdge___doc__[] =
".. method:: getClosestViewEdge(x, y)\n"
"\n"
"   Gets the ViewEdge nearest to the 2D point specified as arguments.\n"
"\n"
"   :arg x: X coordinate of a 2D point.\n"
"   :type x: float\n"
"   :arg y: Y coordinate of a 2D point.\n"
"   :type y: float\n"
"   :return: The ViewEdge nearest to the specified 2D point.\n"
"   :rtype: :class:`ViewEdge`\n";

static PyObject * ViewMap_getClosestViewEdge( BPy_ViewMap *self , PyObject *args) {
	double x, y;

	if(!( PyArg_ParseTuple(args, "dd", &x, &y) ))
		return NULL;

	ViewEdge *ve = const_cast<ViewEdge *>( self->vm->getClosestViewEdge(x,y) );
	if( ve )
		return BPy_ViewEdge_from_ViewEdge(*ve);

	Py_RETURN_NONE;
}

static char ViewMap_getClosestFEdge___doc__[] =
".. method:: getClosestFEdge(x, y)\n"
"\n"
"   Gets the FEdge nearest to the 2D point specified as arguments.\n"
"\n"
"   :arg x: X coordinate of a 2D point.\n"
"   :type x: float\n"
"   :arg y: Y coordinate of a 2D point.\n"
"   :type y: float\n"
"   :return: The FEdge nearest to the specified 2D point.\n"
"   :rtype: :class:`FEdge`\n";

static PyObject * ViewMap_getClosestFEdge( BPy_ViewMap *self , PyObject *args) {
	double x, y;

	if(!( PyArg_ParseTuple(args, "dd", &x, &y) ))
		return NULL;

	FEdge *fe = const_cast<FEdge *>( self->vm->getClosestFEdge(x,y) );
	if( fe )
		return Any_BPy_FEdge_from_FEdge(*fe);

	Py_RETURN_NONE;
}

static char ViewMap_getScene3dBBox___doc__[] =
".. method:: getScene3dBBox()\n"
"\n"
"   Returns the scene 3D bounding box.\n"
"\n"
"   :return: The scene 3D bounding box.\n"
"   :rtype: :class:`BBox`\n";

static PyObject * ViewMap_getScene3dBBox( BPy_ViewMap *self , PyObject *args) {
	BBox<Vec3r> bb( self->vm->getScene3dBBox() );
	return BPy_BBox_from_BBox( bb );
}

static char ViewMap_setScene3dBBox___doc__[] =
".. method:: setScene3dBBox(bbox)\n"
"\n"
"   Sets the scene 3D bounding box.\n"
"\n"
"   :arg bbox: The scene 3D bounding box.\n"
"   :type bbox: :class:`BBox`\n";

static PyObject * ViewMap_setScene3dBBox( BPy_ViewMap *self , PyObject *args) {
	PyObject *py_bb = 0;

	if(!( PyArg_ParseTuple(args, "O!", &BBox_Type, &py_bb) ))
		return NULL;

	self->vm->setScene3dBBox(*( ((BPy_BBox *) py_bb)->bb ));

	Py_RETURN_NONE;
}

// static ViewMap *getInstance ();

/*---------------------- BPy_ViewShape instance definitions ----------------------------*/
static PyMethodDef BPy_ViewMap_methods[] = {
	{"getClosestViewEdge", ( PyCFunction ) ViewMap_getClosestViewEdge, METH_VARARGS, ViewMap_getClosestViewEdge___doc__},
	{"getClosestFEdge", ( PyCFunction ) ViewMap_getClosestFEdge, METH_VARARGS, ViewMap_getClosestFEdge___doc__},
	{"getScene3dBBox", ( PyCFunction ) ViewMap_getScene3dBBox, METH_NOARGS, ViewMap_getScene3dBBox___doc__},
	{"setScene3dBBox", ( PyCFunction ) ViewMap_setScene3dBBox, METH_VARARGS, ViewMap_setScene3dBBox___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_ViewMap type definition ------------------------------*/

PyTypeObject ViewMap_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"ViewMap",                      /* tp_name */
	sizeof(BPy_ViewMap),            /* tp_basicsize */
	0,                              /* tp_itemsize */
	(destructor)ViewMap___dealloc__, /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	(reprfunc)ViewMap___repr__,     /* tp_repr */
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
	ViewMap___doc__,                /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_ViewMap_methods,            /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	0,                              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)ViewMap___init__,     /* tp_init */
	0,                              /* tp_alloc */
	PyType_GenericNew,              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
