#include "BPy_SVertex.h"

#include "../BPy_Convert.h"
#include "../BPy_Id.h"
#include "../Interface1D/BPy_FEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

static char SVertex___doc__[] =
"Class hierarchy: :class:`Interface0D` > :class:`SVertex`\n"
"\n"
"Class to define a vertex of the embedding.\n"
"\n"
".. method:: __init__()\n"
"\n"
"   Default constructor.\n"
"\n"
".. method:: __init__(iBrother)\n"
"\n"
"   Copy constructor.\n"
"\n"
"   :arg iBrother: A SVertex object.\n"
"   :type iBrother: :class:`SVertex`\n"
"\n"
".. method:: __init__(iPoint3D, id)\n"
"\n"
"   Builds a SVertex from 3D coordinates and an Id.\n"
"\n"
"   :arg iPoint3D: A three-dimensional vector.\n"
"   :type iPoint3D: :class:`mathutils.Vector`\n"
"   :arg id: An Id object.\n"
"   :type id: :class:`Id`\n";

//------------------------INSTANCE METHODS ----------------------------------

static int SVertex___init__(BPy_SVertex *self, PyObject *args, PyObject *kwds)
{
	PyObject *py_point = 0;
	BPy_Id *py_id = 0;
	

	if (! PyArg_ParseTuple(args, "|OO!", &py_point, &Id_Type, &py_id) )
        return -1;
	
	if( !py_point ) {
		self->sv = new SVertex();

	} else if( !py_id && BPy_SVertex_Check(py_point) ) {
		self->sv = new SVertex( *(((BPy_SVertex *)py_point)->sv) );

	} else if( py_point && py_id ) {
		Vec3r *v = Vec3r_ptr_from_PyObject(py_point);
		if( !v ) {
			PyErr_SetString(PyExc_TypeError, "argument 1 must be a 3D vector (either a list of 3 elements or Vector)");
			return -1;
		}
		self->sv = new SVertex( *v, *(py_id->id) );
		delete v;

	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
		return -1;
	}

	self->py_if0D.if0D = self->sv;
	self->py_if0D.borrowed = 0;
	
	return 0;
}

static char SVertex_normals___doc__[] =
".. method:: normals()\n"
"\n"
"   Returns the normals for this Vertex as a list.  In a smooth surface,\n"
"   a vertex has exactly one normal.  In a sharp surface, a vertex can\n"
"   have any number of normals.\n"
"\n"
"   :return: A list of normals.\n"
"   :rtype: List of :class:`mathutils.Vector` objects\n";

static PyObject * SVertex_normals( BPy_SVertex *self ) {
	PyObject *py_normals; 
	set< Vec3r > normals;
	
	py_normals  = PyList_New(0);
	normals = self->sv->normals();
		
	for( set< Vec3r >::iterator set_iterator = normals.begin(); set_iterator != normals.end(); set_iterator++ ) {
		Vec3r v( *set_iterator );
		PyList_Append( py_normals, Vector_from_Vec3r(v) );
	}
	
	return py_normals;
}

static char SVertex_normalsSize___doc__[] =
".. method:: normalsSize()\n"
"\n"
"   Returns the number of different normals for this vertex.\n"
"\n"
"   :return: The number of normals.\n"
"   :rtype: int\n";

static PyObject * SVertex_normalsSize( BPy_SVertex *self ) {
	return PyLong_FromLong( self->sv->normalsSize() );
}

static char SVertex_viewvertex___doc__[] =
".. method:: viewvertex()\n"
"\n"
"   If this SVertex is also a ViewVertex, this method returns the\n"
"   ViewVertex.  None is returned otherwise.\n"
"\n"
"   :return: The ViewVertex object.\n"
"   :rtype: :class:`ViewVertex`\n";

static PyObject * SVertex_viewvertex( BPy_SVertex *self ) {
	ViewVertex *vv = self->sv->viewvertex();
	if( vv )
		return Any_BPy_ViewVertex_from_ViewVertex( *vv );

	Py_RETURN_NONE;
}

static char SVertex_setPoint3D___doc__[] =
".. method:: setPoint3D(p)\n"
"\n"
"   Sets the 3D coordinates of the SVertex.\n"
"\n"
"   :arg p: A three-dimensional vector.\n"
"   :type p: :class:`mathutils.Vector`, list or tuple of 3 real numbers\n";

static PyObject *SVertex_setPoint3D( BPy_SVertex *self , PyObject *args) {
	PyObject *py_point;

	if(!( PyArg_ParseTuple(args, "O", &py_point) ))
		return NULL;
	Vec3r *v = Vec3r_ptr_from_PyObject(py_point);
	if( !v ) {
		PyErr_SetString(PyExc_TypeError, "argument 1 must be a 3D vector (either a list of 3 elements or Vector)");
		return NULL;
	}
	self->sv->setPoint3D( *v );
	delete v;

	Py_RETURN_NONE;
}

static char SVertex_setPoint2D___doc__[] =
".. method:: setPoint2D(p)\n"
"\n"
"   Sets the 2D projected coordinates of the SVertex.\n"
"\n"
"   :arg p: A three-dimensional vector.\n"
"   :type p: :class:`mathutils.Vector`, list or tuple of 3 real numbers\n";

static PyObject *SVertex_setPoint2D( BPy_SVertex *self , PyObject *args) {
	PyObject *py_point;

	if(!( PyArg_ParseTuple(args, "O", &py_point) ))
		return NULL;
	Vec3r *v = Vec3r_ptr_from_PyObject(py_point);
	if( !v ) {
		PyErr_SetString(PyExc_TypeError, "argument 1 must be a 3D vector (either a list of 3 elements or Vector)");
		return NULL;
	}
	self->sv->setPoint2D( *v );
	delete v;

	Py_RETURN_NONE;
}

static char SVertex_AddNormal___doc__[] =
".. method:: AddNormal(n)\n"
"\n"
"   Adds a normal to the SVertex's set of normals.  If the same normal\n"
"   is already in the set, nothing changes.\n"
"\n"
"   :arg n: A three-dimensional vector.\n"
"   :type n: :class:`mathutils.Vector`, list or tuple of 3 real numbers\n";

static PyObject *SVertex_AddNormal( BPy_SVertex *self , PyObject *args) {
	PyObject *py_normal;

	if(!( PyArg_ParseTuple(args, "O", &py_normal) ))
		return NULL;
	Vec3r *n = Vec3r_ptr_from_PyObject(py_normal);
	if( !n ) {
		PyErr_SetString(PyExc_TypeError, "argument 1 must be a 3D vector (either a list of 3 elements or Vector)");
		return NULL;
	}
	self->sv->AddNormal( *n );
	delete n;

	Py_RETURN_NONE;
}

static char SVertex_setId___doc__[] =
".. method:: setId(id)\n"
"\n"
"   Sets the identifier of the SVertex.\n"
"\n"
"   :arg id: The identifier.\n"
"   :type id: :class:`Id`\n";

static PyObject *SVertex_setId( BPy_SVertex *self , PyObject *args) {
	BPy_Id *py_id;

	if( !PyArg_ParseTuple(args, "O!", &Id_Type, &py_id) )
		return NULL;

	self->sv->setId( *(py_id->id) );

	Py_RETURN_NONE;
}

static char SVertex_AddFEdge___doc__[] =
".. method:: AddFEdge(fe)\n"
"\n"
"   Add an FEdge to the list of edges emanating from this SVertex.\n"
"\n"
"   :arg fe: An FEdge.\n"
"   :type fe: :class:`FEdge`\n";

static PyObject *SVertex_AddFEdge( BPy_SVertex *self , PyObject *args) {
	PyObject *py_fe;

	if(!( PyArg_ParseTuple(args, "O!", &FEdge_Type, &py_fe) ))
		return NULL;
	
	self->sv->AddFEdge( ((BPy_FEdge *) py_fe)->fe );

	Py_RETURN_NONE;
}

static char SVertex_curvatures___doc__[] =
".. method:: curvatures()\n"
"\n"
"   Returns curvature information in the form of a seven-element tuple\n"
"   (K1, e1, K2, e2, Kr, er, dKr), where K1 and K2 are scalar values\n"
"   representing the first (maximum) and second (minimum) principal\n"
"   curvatures at this SVertex, respectively; e1 and e2 are\n"
"   three-dimensional vectors representing the first and second principal\n"
"   directions, i.e. the directions of the normal plane where the\n"
"   curvature takes its maximum and minimum values, respectively; and Kr,\n"
"   er and dKr are the radial curvature, radial direction, and the\n"
"   derivative of the radial curvature at this SVertex, repectively.\n"
"   :return: curvature information expressed by a seven-element tuple\n"
"   (K1, e1, K2, e2, Kr, er, dKr).\n"
"   :rtype: tuple\n";

static PyObject *SVertex_curvatures( BPy_SVertex *self , PyObject *args) {
	const CurvatureInfo *info = self->sv->getCurvatureInfo();
	if (!info)
		Py_RETURN_NONE;
	Vec3r e1(info->e1.x(), info->e1.y(), info->e1.z());
	Vec3r e2(info->e2.x(), info->e2.y(), info->e2.z());
	Vec3r er(info->er.x(), info->er.y(), info->er.z());
	PyObject *retval = PyTuple_New(7);
	PyTuple_SET_ITEM( retval, 0, PyFloat_FromDouble(info->K1));
	PyTuple_SET_ITEM( retval, 2, Vector_from_Vec3r(e1));
	PyTuple_SET_ITEM( retval, 1, PyFloat_FromDouble(info->K2));
	PyTuple_SET_ITEM( retval, 3, Vector_from_Vec3r(e2));
	PyTuple_SET_ITEM( retval, 4, PyFloat_FromDouble(info->Kr));
	PyTuple_SET_ITEM( retval, 5, Vector_from_Vec3r(er));
	PyTuple_SET_ITEM( retval, 6, PyFloat_FromDouble(info->dKr));
	return retval;
}

// virtual bool 	operator== (const SVertex &iBrother)
// ViewVertex * 	viewvertex ()

/*----------------------SVertex instance definitions ----------------------------*/
static PyMethodDef BPy_SVertex_methods[] = {
	{"normals", ( PyCFunction ) SVertex_normals, METH_NOARGS, SVertex_normals___doc__},
	{"normalsSize", ( PyCFunction ) SVertex_normalsSize, METH_NOARGS, SVertex_normalsSize___doc__},
	{"viewvertex", ( PyCFunction ) SVertex_viewvertex, METH_NOARGS, SVertex_viewvertex___doc__},
	{"setPoint3D", ( PyCFunction ) SVertex_setPoint3D, METH_VARARGS, SVertex_setPoint3D___doc__},
	{"setPoint2D", ( PyCFunction ) SVertex_setPoint2D, METH_VARARGS, SVertex_setPoint2D___doc__},
	{"AddNormal", ( PyCFunction ) SVertex_AddNormal, METH_VARARGS, SVertex_AddNormal___doc__},
	{"setId", ( PyCFunction ) SVertex_setId, METH_VARARGS, SVertex_setId___doc__},
	{"AddFEdge", ( PyCFunction ) SVertex_AddFEdge, METH_VARARGS, SVertex_AddFEdge___doc__},
	{"curvatures", ( PyCFunction ) SVertex_curvatures, METH_NOARGS, SVertex_curvatures___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_SVertex type definition ------------------------------*/
PyTypeObject SVertex_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SVertex",                      /* tp_name */
	sizeof(BPy_SVertex),            /* tp_basicsize */
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
	SVertex___doc__,                /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	0,                              /* tp_iter */
	0,                              /* tp_iternext */
	BPy_SVertex_methods,            /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&Interface0D_Type,              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)SVertex___init__,     /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

