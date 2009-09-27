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

/*---------------  Python API function prototypes for Stroke instance  -----------*/
static int Stroke___init__(BPy_Stroke *self, PyObject *args, PyObject *kwds);
static PyObject * Stroke___iter__(PyObject *self);

static Py_ssize_t Stroke_length( BPy_Stroke *self );
static PyObject * Stroke_item( BPy_Stroke *self, Py_ssize_t i );
static PyObject * Stroke___getitem__( BPy_Stroke *self, PyObject *item );

static PyObject * Stroke_ComputeSampling( BPy_Stroke *self, PyObject *args );
static PyObject * Stroke_Resample( BPy_Stroke *self, PyObject *args );
static PyObject * Stroke_InsertVertex( BPy_Stroke *self, PyObject *args );
static PyObject * Stroke_RemoveVertex( BPy_Stroke *self, PyObject *args );
static PyObject * Stroke_getMediumType( BPy_Stroke *self );
static PyObject * Stroke_getTextureId( BPy_Stroke *self );
static PyObject * Stroke_hasTips( BPy_Stroke *self );
static PyObject * Stroke_setId( BPy_Stroke *self , PyObject *args);
static PyObject * Stroke_setLength( BPy_Stroke *self , PyObject *args);
static PyObject * Stroke_setMediumType( BPy_Stroke *self , PyObject *args);
static PyObject * Stroke_setTextureId( BPy_Stroke *self , PyObject *args);
static PyObject * Stroke_setTips( BPy_Stroke *self , PyObject *args);
static PyObject * Stroke_strokeVerticesBegin( BPy_Stroke *self , PyObject *args);
static PyObject * Stroke_strokeVerticesEnd( BPy_Stroke *self );
static PyObject * Stroke_strokeVerticesSize( BPy_Stroke *self );
static PyObject * Stroke_verticesBegin( BPy_Stroke *self );
static PyObject * Stroke_verticesEnd( BPy_Stroke *self );
static PyObject * Stroke_pointsBegin( BPy_Stroke *self , PyObject *args);
static PyObject * Stroke_pointsEnd( BPy_Stroke *self , PyObject *args);

/*----------------------Stroke instance definitions ----------------------------*/
static PyMethodDef BPy_Stroke_methods[] = {	
	{"__getitem__", ( PyCFunction ) Stroke___getitem__, METH_O, "(int i) Returns the i-th StrokeVertex constituting the Stroke."},
	{"ComputeSampling", ( PyCFunction ) Stroke_ComputeSampling, METH_VARARGS, "(int nVertices) Compute the sampling needed to get nVertices vertices. If the specified number of vertices is less than the actual number of vertices, the actual sampling value is returned."},
	{"Resample", ( PyCFunction ) Stroke_Resample, METH_VARARGS, "(float f | int n) Resampling method. If the argument is a float, Resamples the curve with a given sampling; if this sampling is < to the actual sampling value, no resampling is done. If the argument is an integer, Resamples the curve so that it eventually has n. That means it is going to add n-vertices_size, if vertices_size is the number of points we already have. Is vertices_size >= n, no resampling is done."},
	{"RemoveVertex", ( PyCFunction ) Stroke_RemoveVertex, METH_VARARGS, "(StrokeVertex sv) Removes the stroke vertex sv from the stroke. The length and curvilinear abscissa are updated consequently."},
	{"InsertVertex", ( PyCFunction ) Stroke_InsertVertex, METH_VARARGS, "(StrokeVertex sv, StrokeVertexIterator next) Inserts the stroke vertex iVertex in the stroke before next. The length, curvilinear abscissa are updated consequently."},
	{"getMediumType", ( PyCFunction ) Stroke_getMediumType, METH_NOARGS, "() Returns the MediumType used for this Stroke."},
	{"getTextureId", ( PyCFunction ) Stroke_getTextureId, METH_NOARGS, "() Returns the id of the texture used to simulate th marks system for this Stroke."},
	{"hasTips", ( PyCFunction ) Stroke_hasTips, METH_NOARGS, "() Returns true if this Stroke uses a texture with tips, false otherwise."},
	{"setId", ( PyCFunction ) Stroke_setId, METH_VARARGS, "(Id id) Sets the Id of the Stroke."},
	{"setLength", ( PyCFunction ) Stroke_setLength, METH_VARARGS, "(float l) Sets the 2D length of the Stroke."},
	{"setMediumType", ( PyCFunction ) Stroke_setMediumType, METH_VARARGS, "(MediumType mt) Sets the medium type that must be used for this Stroke."},
	{"setTextureId", ( PyCFunction ) Stroke_setTextureId, METH_VARARGS, "(unsigned int id) Sets the texture id to be used to simulate the marks system for this Stroke."},
	{"setTips", ( PyCFunction ) Stroke_setTips, METH_VARARGS, "(bool b) Sets the flag telling whether this stroke is using a texture with tips or not."},
	{"strokeVerticesBegin", ( PyCFunction ) Stroke_strokeVerticesBegin, METH_VARARGS, "(float t=0.f) Returns a StrokeVertexIterator pointing on the first StrokeVertex of the Stroke. One can specifly a sampling value t to resample the Stroke on the fly if needed. "},
	{"strokeVerticesEnd", ( PyCFunction ) Stroke_strokeVerticesEnd, METH_NOARGS, "() Returns a StrokeVertexIterator pointing after the last StrokeVertex of the Stroke."},
	{"strokeVerticesSize", ( PyCFunction ) Stroke_strokeVerticesSize, METH_NOARGS, "() Returns the number of StrokeVertex constituing the Stroke."},
	{"verticesBegin", ( PyCFunction ) Stroke_verticesBegin, METH_NOARGS, "() Returns an Interface0DIterator pointing on the first StrokeVertex of the Stroke. "},
	{"verticesEnd", ( PyCFunction ) Stroke_verticesEnd, METH_NOARGS, "() Returns an Interface0DIterator pointing after the last StrokeVertex of the Stroke. "},
	{"pointsBegin", ( PyCFunction ) Stroke_pointsBegin, METH_VARARGS, "(float t=0.f) Returns an iterator over the Interface1D points, pointing to the first point. The difference with verticesBegin() is that here we can iterate over points of the 1D element at a any given sampling t. Indeed, for each iteration, a virtual point is created. "},
	{"pointsEnd", ( PyCFunction ) Stroke_pointsEnd, METH_VARARGS, "(float t=0.f) Returns an iterator over the Interface1D points, pointing after the last point. The difference with verticesEnd() is that here we can iterate over points of the 1D element at a any given sampling t. Indeed, for each iteration, a virtual point is created. "},

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
	PyObject_HEAD_INIT(NULL)
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
	"Stroke objects",               /* tp_doc */
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

//------------------------INSTANCE METHODS ----------------------------------

// Stroke ()
// template<class InputVertexIterator> Stroke (InputVertexIterator iBegin, InputVertexIterator iEnd)
//
// pb: - need to be able to switch representation: InputVertexIterator <=> position
//     - is it even used ? not even in SWIG version

int Stroke___init__(BPy_Stroke *self, PyObject *args, PyObject *kwds)
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

PyObject * Stroke___iter__( PyObject *self ) {
	StrokeInternal::StrokeVertexIterator sv_it( ((BPy_Stroke *)self)->s->strokeVerticesBegin() );
	return BPy_StrokeVertexIterator_from_StrokeVertexIterator( sv_it, 0 );
}

Py_ssize_t Stroke_length( BPy_Stroke *self ) {
	return self->s->strokeVerticesSize();
}

PyObject * Stroke_item( BPy_Stroke *self, Py_ssize_t i ) {
	if (i < 0 || i >= (Py_ssize_t)self->s->strokeVerticesSize()) {
		PyErr_SetString(PyExc_IndexError, "subscript index out of range");
		return NULL;
	}
	return BPy_StrokeVertex_from_StrokeVertex( self->s->strokeVerticeAt(i) );
}

PyObject * Stroke___getitem__( BPy_Stroke *self, PyObject *item ) {
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

PyObject * Stroke_ComputeSampling( BPy_Stroke *self, PyObject *args ) {	
	int i;

	if(!( PyArg_ParseTuple(args, "i", &i)  ))
		return NULL;

	return PyFloat_FromDouble( self->s->ComputeSampling( i ) );
}

PyObject * Stroke_Resample( BPy_Stroke *self, PyObject *args ) {	
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

PyObject * Stroke_InsertVertex( BPy_Stroke *self, PyObject *args ) {
	PyObject *py_sv = 0, *py_sv_it = 0;

	if(!( PyArg_ParseTuple(args, "O!O!", &StrokeVertex_Type, &py_sv, &StrokeVertexIterator_Type, &py_sv_it) ))
		return NULL;

	StrokeVertex *sv = ((BPy_StrokeVertex *) py_sv)->sv;
	StrokeInternal::StrokeVertexIterator sv_it(*( ((BPy_StrokeVertexIterator *) py_sv_it)->sv_it ));
	self->s->InsertVertex( sv, sv_it );

	Py_RETURN_NONE;
}

PyObject * Stroke_RemoveVertex( BPy_Stroke *self, PyObject *args ) {	
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

PyObject * Stroke_getMediumType( BPy_Stroke *self ) {	
	return BPy_MediumType_from_MediumType( self->s->getMediumType() );
}

PyObject * Stroke_getTextureId( BPy_Stroke *self ) {	
	return PyLong_FromLong( self->s->getTextureId() );
}

PyObject * Stroke_hasTips( BPy_Stroke *self ) {	
	return PyBool_from_bool( self->s->hasTips() );
}


PyObject *Stroke_setId( BPy_Stroke *self , PyObject *args) {
	PyObject *py_id;

	if(!( PyArg_ParseTuple(args, "O!", &Id_Type, &py_id) ))
		return NULL;

	self->s->setId(*( ((BPy_Id *) py_id)->id ));

	Py_RETURN_NONE;
}

PyObject *Stroke_setLength( BPy_Stroke *self , PyObject *args) {
	float f;

	if(!( PyArg_ParseTuple(args, "f", &f) ))
		return NULL;

	self->s->setLength( f );

	Py_RETURN_NONE;
}

PyObject *Stroke_setMediumType( BPy_Stroke *self , PyObject *args) {
	PyObject *py_mt;

	if(!( PyArg_ParseTuple(args, "O!", &MediumType_Type, &py_mt) ))
		return NULL;

	self->s->setMediumType( MediumType_from_BPy_MediumType(py_mt) );

	Py_RETURN_NONE;
}

PyObject *Stroke_setTextureId( BPy_Stroke *self , PyObject *args) {
	unsigned int i;

	if(!( PyArg_ParseTuple(args, "I", &i) ))
		return NULL;

	self->s->setTextureId( i );

	Py_RETURN_NONE;
}

PyObject *Stroke_setTips( BPy_Stroke *self , PyObject *args) {
	PyObject *py_b;

	if(!( PyArg_ParseTuple(args, "O", &py_b) ))
		return NULL;

	self->s->setTips( bool_from_PyBool(py_b) );

	Py_RETURN_NONE;
}

PyObject * Stroke_strokeVerticesBegin( BPy_Stroke *self , PyObject *args) {
	float f = 0;

	if(!( PyArg_ParseTuple(args, "|f", &f)  ))
		return NULL;

	StrokeInternal::StrokeVertexIterator sv_it( self->s->strokeVerticesBegin(f) );
	return BPy_StrokeVertexIterator_from_StrokeVertexIterator( sv_it, 0 );
}

PyObject * Stroke_strokeVerticesEnd( BPy_Stroke *self ) {
	StrokeInternal::StrokeVertexIterator sv_it( self->s->strokeVerticesEnd() );
	return BPy_StrokeVertexIterator_from_StrokeVertexIterator( sv_it, 1 );
}

PyObject * Stroke_strokeVerticesSize( BPy_Stroke *self ) {
	return PyLong_FromLong( self->s->strokeVerticesSize() );
}

PyObject * Stroke_verticesBegin( BPy_Stroke *self ) {
	Interface0DIterator if0D_it( self->s->verticesBegin() );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 0 );
}

PyObject * Stroke_verticesEnd( BPy_Stroke *self ) {
	Interface0DIterator if0D_it( self->s->verticesEnd() );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 1 );	
}

PyObject * Stroke_pointsBegin( BPy_Stroke *self , PyObject *args) {
	float f = 0;

	if(!( PyArg_ParseTuple(args, "|f", &f)  ))
		return NULL;

	Interface0DIterator if0D_it( self->s->pointsBegin(f) );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 0 );
}

PyObject * Stroke_pointsEnd( BPy_Stroke *self , PyObject *args) {
	float f = 0;

	if(!( PyArg_ParseTuple(args, "|f", &f)  ))
		return NULL;
	
	Interface0DIterator if0D_it( self->s->pointsEnd(f) );
	return BPy_Interface0DIterator_from_Interface0DIterator( if0D_it, 1 );
}
	
///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
