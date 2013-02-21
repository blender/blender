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

/*----------------------Stroke methods ----------------------------*/

// Stroke ()
// template<class InputVertexIterator> Stroke (InputVertexIterator begin, InputVertexIterator end)
//
// pb: - need to be able to switch representation: InputVertexIterator <=> position
//     - is it even used ? not even in SWIG version

PyDoc_STRVAR(Stroke_doc,
"Class hierarchy: :class:`Interface1D` > :class:`Stroke`\n"
"\n"
"Class to define a stroke.  A stroke is made of a set of 2D vertices\n"
"(:class:`StrokeVertex`), regularly spaced out.  This set of vertices\n"
"defines the stroke's backbone geometry.  Each of these stroke vertices\n"
"defines the stroke's shape and appearance at this vertex position.\n"
"\n"
".. method:: Stroke()\n"
"\n"
"   Default constructor\n"
"\n"
".. method:: Stroke(brother)\n"
"\n"
"   Copy constructor");

static int Stroke_init(BPy_Stroke *self, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = {"brother", NULL};
	PyObject *brother = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist, &Stroke_Type, &brother))
		return -1;
	if (!brother)
		self->s = new Stroke();
	else
		self->s = new Stroke(*(((BPy_Stroke *)brother)->s));
	self->py_if1D.if1D = self->s;
	self->py_if1D.borrowed = 0;
	return 0;
}

static PyObject * Stroke_iter(PyObject *self)
{
	StrokeInternal::StrokeVertexIterator sv_it( ((BPy_Stroke *)self)->s->strokeVerticesBegin() );
	return BPy_StrokeVertexIterator_from_StrokeVertexIterator( sv_it, 0 );
}

static Py_ssize_t Stroke_sq_length(BPy_Stroke *self)
{
	return self->s->strokeVerticesSize();
}

static PyObject *Stroke_sq_item(BPy_Stroke *self, int keynum)
{
	if (keynum < 0)
		keynum += Stroke_sq_length(self);
	if (keynum < 0 || keynum >= Stroke_sq_length(self)) {
		PyErr_Format(PyExc_IndexError, "Stroke[index]: index %d out of range", keynum);
		return NULL;
	}
	return BPy_StrokeVertex_from_StrokeVertex(self->s->strokeVerticeAt(keynum));
}

PyDoc_STRVAR(Stroke_compute_sampling_doc,
".. method:: compute_sampling(iNVertices)\n"
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
"   :rtype: float");

static PyObject * Stroke_compute_sampling(BPy_Stroke *self, PyObject *args)
{
	int i;

	if (!PyArg_ParseTuple(args, "i", &i))
		return NULL;
	return PyFloat_FromDouble(self->s->ComputeSampling(i));
}

PyDoc_STRVAR(Stroke_resample_doc,
".. method:: resample(iNPoints)\n"
"\n"
"   Resamples the stroke so that it eventually has iNPoints.  That means\n"
"   it is going to add iNPoints-vertices_size, if vertices_size is the\n"
"   number of points we already have.  If vertices_size >= iNPoints, no\n"
"   resampling is done.\n"
"\n"
"   :arg iNPoints: The number of vertices we eventually want in our stroke.\n"
"   :type iNPoints: int\n"
"\n"
".. method:: resample(iSampling)\n"
"\n"
"   Resamples the stroke with a given sampling.  If the sampling is\n"
"   smaller than the actual sampling value, no resampling is done.\n"
"\n"
"   :arg iSampling: The new sampling value.\n"
"   :type iSampling: float");

static PyObject * Stroke_resample(BPy_Stroke *self, PyObject *args)
{
	PyObject *obj;

	if (!PyArg_ParseTuple(args, "O", &obj))
		return NULL;
	if (PyLong_Check(obj)) {
		self->s->Resample((int)PyLong_AsLong(obj));
	} else if (PyFloat_Check(obj)) {
		self->s->Resample((float)PyFloat_AsDouble(obj));
	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument");
		return NULL;
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(Stroke_insert_vertex_doc,
".. method:: insert_vertex(iVertex, next)\n"
"\n"
"   Inserts the stroke vertex iVertex in the stroke before next.  The\n"
"   length, curvilinear abscissa are updated consequently.\n"
"\n"
"   :arg iVertex: The StrokeVertex to insert in the Stroke.\n"
"   :type iVertex: :class:`StrokeVertex`\n"
"   :arg next: A StrokeVertexIterator pointing to the StrokeVertex\n"
"      before which iVertex must be inserted.\n"
"   :type next: :class:`StrokeVertexIterator`");

static PyObject * Stroke_insert_vertex(BPy_Stroke *self, PyObject *args)
{
	PyObject *py_sv = 0, *py_sv_it = 0;

	if (!PyArg_ParseTuple(args, "O!O!", &StrokeVertex_Type, &py_sv, &StrokeVertexIterator_Type, &py_sv_it))
		return NULL;
	StrokeVertex *sv = ((BPy_StrokeVertex *)py_sv)->sv;
	StrokeInternal::StrokeVertexIterator sv_it(*(((BPy_StrokeVertexIterator *)py_sv_it)->sv_it));
	self->s->InsertVertex(sv, sv_it);
	Py_RETURN_NONE;
}

PyDoc_STRVAR(Stroke_remove_vertex_doc,
".. method:: remove_vertex(iVertex)\n"
"\n"
"   Removes the stroke vertex iVertex from the stroke. The length and\n"
"   curvilinear abscissa are updated consequently.\n"
"\n"
"   :arg iVertex: \n"
"   :type iVertex: :class:`StrokeVertex`");

static PyObject * Stroke_remove_vertex( BPy_Stroke *self, PyObject *args )
{
	PyObject *py_sv;

	if (!PyArg_ParseTuple(args, "O!", &StrokeVertex_Type, &py_sv))
		return NULL;
	if (((BPy_StrokeVertex *)py_sv)->sv) {
		self->s->RemoveVertex(((BPy_StrokeVertex *)py_sv)->sv);
	} else {
		PyErr_SetString(PyExc_TypeError, "invalid argument");
		return NULL;
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(Stroke_update_length_doc,
".. method:: update_length()\n"
"\n"
"   Updates the 2D length of the Stroke.");

static PyObject * Stroke_update_length(BPy_Stroke *self)
{
	self->s->UpdateLength();
	Py_RETURN_NONE;
}

PyDoc_STRVAR(Stroke_stroke_vertices_begin_doc,
".. method:: stroke_vertices_begin(t=0.0)\n"
"\n"
"   Returns a StrokeVertexIterator pointing on the first StrokeVertex of\n"
"   the Stroke. O ne can specify a sampling value to resample the Stroke\n"
"   on the fly if needed.\n"
"\n"
"   :arg t: The resampling value with which we want our Stroke to be\n"
"      resampled.  If 0 is specified, no resampling is done.\n"
"   :type t: float\n"
"   :return: A StrokeVertexIterator pointing on the first StrokeVertex.\n"
"   :rtype: :class:`StrokeVertexIterator`");

static PyObject * Stroke_stroke_vertices_begin( BPy_Stroke *self , PyObject *args)
{
	float f = 0;

	if (!PyArg_ParseTuple(args, "|f", &f))
		return NULL;
	StrokeInternal::StrokeVertexIterator sv_it(self->s->strokeVerticesBegin(f));
	return BPy_StrokeVertexIterator_from_StrokeVertexIterator(sv_it, 0);
}

PyDoc_STRVAR(Stroke_stroke_vertices_end_doc,
".. method:: strokeVerticesEnd()\n"
"\n"
"   Returns a StrokeVertexIterator pointing after the last StrokeVertex\n"
"   of the Stroke.\n"
"\n"
"   :return: A StrokeVertexIterator pointing after the last StrokeVertex.\n"
"   :rtype: :class:`StrokeVertexIterator`");

static PyObject * Stroke_stroke_vertices_end(BPy_Stroke *self)
{
	StrokeInternal::StrokeVertexIterator sv_it(self->s->strokeVerticesEnd());
	return BPy_StrokeVertexIterator_from_StrokeVertexIterator(sv_it, 1);
}

PyDoc_STRVAR(Stroke_stroke_vertices_size_doc,
".. method:: stroke_vertices_size()\n"
"\n"
"   Returns the number of StrokeVertex constituing the Stroke.\n"
"\n"
"   :return: The number of stroke vertices.\n"
"   :rtype: int");

static PyObject * Stroke_stroke_vertices_size(BPy_Stroke *self)
{
	return PyLong_FromLong(self->s->strokeVerticesSize());
}

static PyMethodDef BPy_Stroke_methods[] = {	
	{"compute_sampling", (PyCFunction)Stroke_compute_sampling, METH_VARARGS, Stroke_compute_sampling_doc},
	{"resample", (PyCFunction)Stroke_resample, METH_VARARGS, Stroke_resample_doc},
	{"remove_vertex", (PyCFunction)Stroke_remove_vertex, METH_VARARGS, Stroke_remove_vertex_doc},
	{"insert_vertex", (PyCFunction)Stroke_insert_vertex, METH_VARARGS, Stroke_insert_vertex_doc},
	{"update_length", (PyCFunction)Stroke_update_length, METH_NOARGS, Stroke_update_length_doc},
	{"stroke_vertices_begin", (PyCFunction)Stroke_stroke_vertices_begin, METH_VARARGS, Stroke_stroke_vertices_begin_doc},
	{"stroke_vertices_end", (PyCFunction)Stroke_stroke_vertices_end, METH_NOARGS, Stroke_stroke_vertices_end_doc},
	{"stroke_vertices_size", (PyCFunction)Stroke_stroke_vertices_size, METH_NOARGS, Stroke_stroke_vertices_size_doc},
	{NULL, NULL, 0, NULL}
};

/*----------------------Stroke get/setters ----------------------------*/

PyDoc_STRVAR(Stroke_medium_type_doc,
"The MediumType used for this Stroke.\n"
"\n"
":type: :class:`MediumType`");

static PyObject *Stroke_medium_type_get(BPy_Stroke *self, void *UNUSED(closure))
{
	return BPy_MediumType_from_MediumType(self->s->getMediumType());
}

static int Stroke_medium_type_set(BPy_Stroke *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_MediumType_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be a MediumType");
		return -1;
	}
	self->s->setMediumType(MediumType_from_BPy_MediumType(value));
	return 0;
}

PyDoc_STRVAR(Stroke_texture_id_doc,
"The ID of the texture used to simulate th marks system for this Stroke.\n"
"\n"
":type: int");

static PyObject *Stroke_texture_id_get(BPy_Stroke *self, void *UNUSED(closure))
{
	return PyLong_FromLong( self->s->getTextureId() );
}

static int Stroke_texture_id_set(BPy_Stroke *self, PyObject *value, void *UNUSED(closure))
{
	unsigned int i = PyLong_AsUnsignedLong(value);
	if(PyErr_Occurred())
		return -1;
	self->s->setTextureId(i);
	return 0;
}

PyDoc_STRVAR(Stroke_tips_doc,
"True if this Stroke uses a texture with tips, and false otherwise.\n"
"\n"
":type: bool");

static PyObject *Stroke_tips_get(BPy_Stroke *self, void *UNUSED(closure))
{
	return PyBool_from_bool(self->s->hasTips());
}

static int Stroke_tips_set(BPy_Stroke *self, PyObject *value, void *UNUSED(closure))
{
	if (!PyBool_Check(value))
		return -1;
	self->s->setTips(bool_from_PyBool(value));
	return 0;
}

PyDoc_STRVAR(Stroke_length_2d_doc,
"The 2D length of the Stroke.\n"
"\n"
":type: float");

static PyObject *Stroke_length_2d_get(BPy_Stroke *self, void *UNUSED(closure))
{
	return PyFloat_FromDouble(self->s->getLength2D());
}

static int Stroke_length_2d_set(BPy_Stroke *self, PyObject *value, void *UNUSED(closure))
{
	float scalar;
	if ((scalar = PyFloat_AsDouble(value)) == -1.0f && PyErr_Occurred()) { /* parsed item not a number */
		PyErr_SetString(PyExc_TypeError, "value must be a number");
		return -1;
	}
	self->s->setLength(scalar);
	return 0;
}

PyDoc_STRVAR(Stroke_id_doc,
"The Id of this Stroke.\n"
"\n"
":type: :class:`Id`");

static PyObject *Stroke_id_get(BPy_Stroke *self, void *UNUSED(closure))
{
	Id id(self->s->getId());
	return BPy_Id_from_Id(id); // return a copy
}

static int Stroke_id_set(BPy_Stroke *self, PyObject *value, void *UNUSED(closure))
{
	if (!BPy_Id_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "value must be an Id");
		return -1;
	}
	self->s->setId(*(((BPy_Id *)value)->id));
	return 0;
}

static PyGetSetDef BPy_Stroke_getseters[] = {
	{(char *)"medium_type", (getter)Stroke_medium_type_get, (setter)Stroke_medium_type_set, (char *)Stroke_medium_type_doc, NULL},
	{(char *)"texture_id", (getter)Stroke_texture_id_get, (setter)Stroke_texture_id_set, (char *)Stroke_texture_id_doc, NULL},
	{(char *)"tips", (getter)Stroke_tips_get, (setter)Stroke_tips_set, (char *)Stroke_tips_doc, NULL},
	{(char *)"length_2d", (getter)Stroke_length_2d_get, (setter)Stroke_length_2d_set, (char *)Stroke_length_2d_doc, NULL},
	{(char *)"id", (getter)Stroke_id_get, (setter)Stroke_id_set, (char *)Stroke_id_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};

/*-----------------------BPy_Stroke type definition ------------------------------*/

static PySequenceMethods BPy_Stroke_as_sequence = {
	(lenfunc)Stroke_sq_length,     /* sq_length */
	NULL,                          /* sq_concat */
	NULL,                          /* sq_repeat */
	(ssizeargfunc)Stroke_sq_item,  /* sq_item */
	NULL,                          /* sq_slice */
	NULL,                          /* sq_ass_item */
	NULL,                          /* *was* sq_ass_slice */
	NULL,                          /* sq_contains */
	NULL,                          /* sq_inplace_concat */
	NULL,                          /* sq_inplace_repeat */
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
	&BPy_Stroke_as_sequence,        /* tp_as_sequence */
	0,                              /* tp_as_mapping */
	0,                              /* tp_hash  */
	0,                              /* tp_call */
	0,                              /* tp_str */
	0,                              /* tp_getattro */
	0,                              /* tp_setattro */
	0,                              /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	Stroke_doc,                     /* tp_doc */
	0,                              /* tp_traverse */
	0,                              /* tp_clear */
	0,                              /* tp_richcompare */
	0,                              /* tp_weaklistoffset */
	(getiterfunc)Stroke_iter,       /* tp_iter */
	0,                              /* tp_iternext */
	BPy_Stroke_methods,             /* tp_methods */
	0,                              /* tp_members */
	BPy_Stroke_getseters,           /* tp_getset */
	&Interface1D_Type,              /* tp_base */
	0,                              /* tp_dict */
	0,                              /* tp_descr_get */
	0,                              /* tp_descr_set */
	0,                              /* tp_dictoffset */
	(initproc)Stroke_init,          /* tp_init */
	0,                              /* tp_alloc */
	0,                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
