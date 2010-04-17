#include "BPy_Stroke.h"

#include "../BPy_Convert.h"
#include "../BPy_Id.h"
#include "../Interface0D/BPy_SVertex.h"
#include "../Interface0D/CurvePoint/BPy_StrokeVertex.h"
#include "../Iterator/BPy_StrokeVertexIterator.h"
#include "../BPy_MediumType.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

// Stroke ()
// template<class InputVertexIterator> Stroke (InputVertexIterator iBegin, InputVertexIterator iEnd)
//
// pb: - need to be able to switch representation: InputVertexIterator <=> position
//     - is it even used ? not even in SWIG version

static char Stroke___doc__[] =
"Class to define a stroke.  A stroke is made of a set of 2D vertices\n"
"(:class:`StrokeVertex`), regularly spaced out.  This set of vertices\n"
"defines the stroke's backbone geometry.  Each of these stroke vertices\n"
"defines the stroke's shape and appearance at this vertex position.\n"
"\n"
".. method:: Stroke()\n"
"\n"
"   Default constructor\n"
"\n"
".. method:: Stroke(iBrother)\n"
"\n"
"   Copy constructor\n"
"\n"
"   :arg iBrother: \n"
"   :type iBrother: :class:`Stroke`\n"
"\n"
".. method:: Stroke(iBegin, iEnd)\n"
"\n"
"   Builds a stroke from a set of StrokeVertex.  This constructor is\n"
"   templated by an iterator type.  This iterator type must allow the\n"
"   vertices parsing using the ++ operator.\n"
"\n"
"   :arg iBegin: The iterator pointing to the first vertex.\n"
"   :type iBegin: InputVertexIterator\n"
"   :arg iEnd: The iterator pointing to the end of the vertex list.\n"
"   :type iEnd: InputVertexIterator\n";

static int Stroke___init__(BPy_Stroke *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj1 = NULL, *obj2 = NULL;

	if (! PyArg_ParseTuple(args, "|OO", &obj1, &obj2) )
        return -1;

	if( !obj1 ){
		self->s = new Stroke();

	} else if ( !obj2 && BPy_Stroke_Check(obj1) ) {
		self->s = new Stroke(*( ((BPy_Stroke *)obj1)->s ));

	} else if ( obj2 ) {
		PyErr_SetString(PyExc_TypeError,
			"Stroke(InputVertexIterator iBegin, InputVertexIterator iEnd) not implemented");
		return -1;
	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
		return -1;
	}

	self->py_if1D.if1D = self->s;
	self->py_if1D.borrowed = 0;

	return 0;
}

static PyObject * Stroke___iter__( PyObject *self ) {
	StrokeInternal::StrokeVertexIterator sv_it( ((BPy_Stroke *)self)->s->strokeVerticesBegin() );
	return BPy_StrokeVertexIterator_from_StrokeVertexIterator( sv_it, 0 );
}

static Py_ssize_t Stroke_length( BPy_Stroke *self ) {
	return self->s->strokeVerticesSize();
}

static PyObject * Stroke_item( BPy_Stroke *self, Py_ssize_t i ) {
	if (i < 0 || i >= (Py_ssize_t)self->s->strokeVerticesSize()) {
		PyErr_SetString(PyExc_IndexError, "subscript index out of range");
		return NULL;
	}
	return BPy_StrokeVertex_from_StrokeVertex( self->s->strokeVerticeAt(i) );
}

static PyObject * Stroke___getitem__( BPy_Stroke *self, PyObject *item ) {
	long i;

	if (!PyLong_Check(item)) {
		PyErr_SetString(PyExc_TypeError, "subscript indices must be integers");
		return NULL;
	}
	i = PyLong_AsLong(item);
	if (i == -1 && PyErr_Occurred())
		return NULL;
	if (i < 0) {
		i += self->s->strokeVerticesSize();
	}
	return Stroke_item(self, i);
}

static char Stroke_ComputeSampling___doc__[] =
".. method:: ComputeSampling(iNVertices)\n"
"\n"
"   Compute the sampling needed to get iNVertices vertices.  If the\n"
"   specified number of vertices is less than the actual number of\n"
"   vertices, the actual sampling value is returned. (To remove Vertices,\n"
"   use the RemoveVertex() method of this class.)\n"
"\n"
"   :arg iNVertices: The number of stroke vertices we eventually want\n"
"      in our Stroke.\n"
"   :type iNVertices: int\n"
"   :return: The sampling that must be used in the Resample(float)\n"
"      method.\n"
"   :rtype: float\n";

static PyObject * Stroke_ComputeSampling( BPy_Stroke *self, PyObject *args ) {	
	int i;

	if(!( PyArg_ParseTuple(args, "i", &i)  ))
		return NULL;

	return PyFloat_FromDouble( self->s->ComputeSampling( i ) );
}

static char Stroke_Resample___doc__[] =
".. method:: Resample(iNPoints)\n"
"\n"
"   Resamples the stroke so that it eventually has iNPoints.  That means\n"
"   it is going to add iNPoints-vertices_size, if vertices_size is the\n"
"   number of points we already have.  If vertices_size >= iNPoints, no\n"
"   resampling is done.\n"
"\n"
"   :arg iNPoints: The number of vertices we eventually want in our stroke.\n"
"   :type iNPoints: int\n"
"\n"
".. method:: Resample(iSampling)\n"
"\n"
"   Resamples the stroke with a given sampling.  If the sampling is\n"
"   smaller than the actual sampling value, no resampling is done.\n"
"\n"
"   :arg iSampling: The new sampling value.\n"
"   :type iSampling: float\n";

static PyObject * Stroke_Resample( BPy_Stroke *self, PyObject *args ) {	
	PyObject *obj;

	if(!( PyArg_ParseTuple(args, "O", &obj) ))
		return NULL;

	if( PyLong_Check(obj) )
		self->s->Resample( (int) PyLong_AsLong(obj) );
	else if( PyFloat_Check(obj) )
		self->s->Resample( (float) PyFloat_AsDouble(obj) );
	else {
		PyErr_SetString(PyExc_TypeError, "invalid argument");
		return NULL;
	}
		
	Py_RETURN_NONE;
}

static char Stroke_InsertVertex___doc__[] =
".. method:: InsertVertex(iVertex, next)\n"
"\n"
"   Inserts the stroke vertex iVertex in the stroke before next.  The\n"
"   length, curvilinear abscissa are updated consequently.\n"
"\n"
"   :arg iVertex: The StrokeVertex to insert in the Stroke.\n"
"   :type iVertex: :class:`StrokeVertex`\n"
"   :arg next: A StrokeVertexIterator pointing to the StrokeVertex\n"
"      before which iVertex must be inserted.\n"
"   :type next: :class:`StrokeVertexIterator`\n";

static PyObject * Stroke_InsertVertex( BPy_Stroke *self, PyObject *args ) {
	PyObject *py_sv = 0, *py_sv_it = 0;

	if(!( PyArg_ParseTuple(args, "O!O!", &StrokeVertex_Type, &py_sv, &StrokeVertexIterator_Type, &py_sv_it) ))
		return NULL;

	StrokeVertex *sv = ((BPy_StrokeVertex *) py_sv)->sv;
	StrokeInternal::StrokeVertexIterator sv_it(*( ((BPy_StrokeVertexIterator *) py_sv_it)->sv_it ));
	self->s->InsertVertex( sv, sv_it );

	Py_RETURN_NONE;
}

static char Stroke_RemoveVertex___doc__[] =
".. method:: RemoveVertex(iVertex)\n"
"\n"
"   Removes the stroke vertex iVertex from the stroke. The length and\n"
"   curvilinear abscissa are updated consequently.\n"
"\n"
"   :arg iVertex: \n"
"   :type iVertex: :class:`StrokeVertex`\n";

static PyObject * Stroke_RemoveVertex( BPy_Stroke *self, PyObject *args ) {	
	PyObject *py_sv;

	if(!( PyArg_ParseTuple(args, "O!", &StrokeVertex_Type, &py_sv) ))
		return NULL;

	if( ((BPy_StrokeVertex *) py_sv)->sv )
		self->s->RemoveVertex( ((BPy_StrokeVertex *) py_sv)->sv );
	else {
		PyErr_SetString(PyExc_TypeError, "invalid argument");
		return NULL;
	}
		
	Py_RETURN_NONE;
}

static char Stroke_getMediumType___doc__[] =
".. method:: getMediumType()\n"
"\n"
"   Returns the MediumType used for this Stroke.\n"
"\n"
"   :return: the MediumType used for this Stroke.\n"
"   :rtype: :class:`MediumType`\n";

static PyObject * Stroke_getMediumType( BPy_Stroke *self ) {	
	return BPy_MediumType_from_MediumType( self->s->getMediumType() );
}

static char Stroke_getTextureId___doc__[] =
".. method:: getTextureId()\n"
"\n"
"   Returns the ID of the texture used to simulate th marks system for\n"
"   this Stroke\n"
"\n"
"   :return: The texture ID.\n"
"   :rtype: int\n";

static PyObject * Stroke_getTextureId( BPy_Stroke *self ) {	
	return PyLong_FromLong( self->s->getTextureId() );
}

static char Stroke_hasTips___doc__[] =
".. method:: hasTips()\n"
"\n"
"   Returns true if this Stroke uses a texture with tips, false\n"
"   otherwise.\n"
"\n"
"   :return: True if this Stroke uses a texture with tips.\n"
"   :rtype: bool\n";

static PyObject * Stroke_hasTips( BPy_Stroke *self ) {	
	return PyBool_from_bool( self->s->hasTips() );
}

static char Stroke_setId___doc__[] =
".. method:: setId(id)\n"
"\n"
"   Sets the Id of the Stroke.\n"
"\n"
"   :arg id: the Id of the Stroke.\n"
"   :type id: :class:`Id`\n";

static PyObject *Stroke_setId( BPy_Stroke *self , PyObject *args) {
	PyObject *py_id;

	if(!( PyArg_ParseTuple(args, "O!", &Id_Type, &py_id) ))
		return NULL;

	self->s->setId(*( ((BPy_Id *) py_id)->id ));

	Py_RETURN_NONE;
}

static char Stroke_setLength___doc__[] =
".. method:: setLength(iLength)\n"
"\n"
"   Sets the 2D length of the Stroke.\n"
"\n"
"   :arg iLength: The 2D length of the Stroke.\n"
"   :type iLength: float\n";

static PyObject *Stroke_setLength( BPy_Stroke *self , PyObject *args) {
	float f;

	if(!( PyArg_ParseTuple(args, "f", &f) ))
		return NULL;

	self->s->setLength( f );

	Py_RETURN_NONE;
}

static char Stroke_setMediumType___doc__[] =
".. method:: setMediumType(iType)\n"
"\n"
"   Sets the medium type that must be used for this Stroke.\n"
"\n"
"   :arg iType: A MediumType object.\n"
"   :type iType: :class:`MediumType`\n";

static PyObject *Stroke_setMediumType( BPy_Stroke *self , PyObject *args) {
	PyObject *py_mt;

	if(!( PyArg_ParseTuple(args, "O!", &MediumType_Type, &py_mt) ))
		return NULL;

	self->s->setMediumType( MediumType_from_BPy_MediumType(py_mt) );

	Py_RETURN_NONE;
}

static char Stroke_setTextureId___doc__[] =
".. method:: setTextureId(iId)\n"
"\n"
"   Sets the texture ID to be used to simulate the marks system for this\n"
"   Stroke.\n"
"\n"
"   :arg iId: A texture ID.\n"
"   :type iId: int\n";

static PyObject *Stroke_setTextureId( BPy_Stroke *self , PyObject *args) {
	unsigned int i;

	if(!( PyArg_ParseTuple(args, "I", &i) ))
		return NULL;

	self->s->setTextureId( i );

	Py_RETURN_NONE;
}

static char Stroke_setTips___doc__[] =
".. method:: setTips(iTips)\n"
"\n"
"   Sets the flag telling whether this stroke is using a texture with\n"
"   tips or not.\n"
"\n"
"   :arg iTips: True if this stroke uses a texture with tips.\n"
"   :type iTips: bool\n";

static PyObject *Stroke_setTips( BPy_Stroke *self , PyObject *args) {
	PyObject *py_b;

	if(!( PyArg_ParseTuple(args, "O", &py_b) ))
		return NULL;

	self->s->setTips( bool_from_PyBool(py_b) );

	Py_RETURN_NONE;
}

static char Stroke_strokeVerticesBegin___doc__[] =
".. method:: strokeVerticesBegin(t=0.0)\n"
"\n"
"   Returns a StrokeVertexIterator pointing on the first StrokeVertex of\n"
"   the Stroke. O ne can specify a sampling value to resample the Stroke\n"
"   on the fly if needed.\n"
"\n"
"   :arg t: The resampling value with which we want our Stroke to be\n"
"      resampled.  If 0 is specified, no resampling is done.\n"
"   :type t: float\n"
"   :return: A StrokeVertexIterator pointing on the first StrokeVertex.\n"
"   :rtype: :class:`StrokeVertexIterator`\n";

static PyObject * Stroke_strokeVerticesBegin( BPy_Stroke *self , PyObject *args) {
	float f = 0;

	if(!( PyArg_ParseTuple(args, "|f", &f)  ))
		return NULL;

	StrokeInternal::StrokeVertexIterator sv_it( self->s->strokeVerticesBegin(f) );
	return BPy_StrokeVertexIterator_from_StrokeVertexIterator( sv_it, 0 );
}

static char Stroke_strokeVerticesEnd___doc__[] =
".. method:: strokeVerticesEnd()\n"
"\n"
"   Returns a StrokeVertexIterator pointing after the last StrokeVertex\n"
"   of the Stroke.\n"
"\n"
"   :return: A StrokeVertexIterator pointing after the last StrokeVertex.\n"
"   :rtype: :class:`StrokeVertexIterator`\n";

static PyObject * Stroke_strokeVerticesEnd( BPy_Stroke *self ) {
	StrokeInternal::StrokeVertexIterator sv_it( self->s->strokeVerticesEnd() );
	return BPy_StrokeVertexIterator_from_StrokeVertexIterator( sv_it, 1 );
}

static char Stroke_strokeVerticesSize___doc__[] =
".. method:: strokeVerticesSize()\n"
"\n"
"   Returns the number of StrokeVertex constituing the Stroke.\n"
"\n"
"   :return: The number of stroke vertices.\n"
"   :rtype: int\n";

static PyObject * Stroke_strokeVerticesSize( BPy_Stroke *self ) {
	return PyLong_FromLong( self->s->strokeVerticesSize() );
}
	
/*----------------------Stroke instance definitions ----------------------------*/

static PyMethodDef BPy_Stroke_methods[] = {	
	{"__getitem__", ( PyCFunction ) Stroke___getitem__, METH_O, "(int i) Returns the i-th StrokeVertex constituting the Stroke."},
	{"ComputeSampling", ( PyCFunction ) Stroke_ComputeSampling, METH_VARARGS, Stroke_ComputeSampling___doc__},
	{"Resample", ( PyCFunction ) Stroke_Resample, METH_VARARGS, Stroke_Resample___doc__},
	{"RemoveVertex", ( PyCFunction ) Stroke_RemoveVertex, METH_VARARGS, Stroke_RemoveVertex___doc__},
	{"InsertVertex", ( PyCFunction ) Stroke_InsertVertex, METH_VARARGS, Stroke_InsertVertex___doc__},
	{"getMediumType", ( PyCFunction ) Stroke_getMediumType, METH_NOARGS, Stroke_getMediumType___doc__},
	{"getTextureId", ( PyCFunction ) Stroke_getTextureId, METH_NOARGS, Stroke_getTextureId___doc__},
	{"hasTips", ( PyCFunction ) Stroke_hasTips, METH_NOARGS, Stroke_hasTips___doc__},
	{"setId", ( PyCFunction ) Stroke_setId, METH_VARARGS, Stroke_setId___doc__},
	{"setLength", ( PyCFunction ) Stroke_setLength, METH_VARARGS, Stroke_setLength___doc__},
	{"setMediumType", ( PyCFunction ) Stroke_setMediumType, METH_VARARGS, Stroke_setMediumType___doc__},
	{"setTextureId", ( PyCFunction ) Stroke_setTextureId, METH_VARARGS, Stroke_setTextureId___doc__},
	{"setTips", ( PyCFunction ) Stroke_setTips, METH_VARARGS, Stroke_setTips___doc__},
	{"strokeVerticesBegin", ( PyCFunction ) Stroke_strokeVerticesBegin, METH_VARARGS, Stroke_strokeVerticesBegin___doc__},
	{"strokeVerticesEnd", ( PyCFunction ) Stroke_strokeVerticesEnd, METH_NOARGS, Stroke_strokeVerticesEnd___doc__},
	{"strokeVerticesSize", ( PyCFunction ) Stroke_strokeVerticesSize, METH_NOARGS, Stroke_strokeVerticesSize___doc__},
	{NULL, NULL, 0, NULL}
};

/*-----------------------BPy_Stroke type definition ------------------------------*/

static PySequenceMethods Stroke_as_sequence = {
	(lenfunc)Stroke_length,		/* sq_length */
	NULL,						/* sq_concat */
	NULL,						/* sq_repeat */
	(ssizeargfunc)Stroke_item,	/* sq_item */
	NULL,						/* sq_slice */
	NULL,						/* sq_ass_item */
	NULL,						/* sq_ass_slice */
	NULL,						/* sq_contains */
	NULL,						/* sq_inplace_concat */
	NULL,						/* sq_inplace_repeat */
};

PyTypeObject Stroke_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"Stroke",                       /* tp_name */
	sizeof(BPy_Stroke),             /* tp_basicsize */
	0,                              /* tp_itemsize */
	0,                              /* tp_dealloc */
	0,                              /* tp_print */
	0,                              /* tp_getattr */
	0,                              /* tp_setattr */
	0,                              /* tp_reserved */
	0,                              /* tp_repr */
	0,                              /* tp_as_number */
	&Stroke_as_sequence,            /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	0,                              /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	Stroke___doc__,                 /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	(getiterfunc)Stroke___iter__,   /* tp_iter */
	0,                              /* tp_iternext */
	BPy_Stroke_methods,             /* tp_methods */
	0,                              /* tp_members */
	0,                              /* tp_getset */
	&Interface1D_Type,              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)Stroke___init__,      /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
