/* SPDX-FileCopyrightText: 2004-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_Stroke.h"

#include "../BPy_Convert.h"
#include "../BPy_Id.h"
#include "../BPy_MediumType.h"
#include "../Interface0D/BPy_SVertex.h"
#include "../Interface0D/CurvePoint/BPy_StrokeVertex.h"
#include "../Iterator/BPy_StrokeVertexIterator.h"

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

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
             "Class to define a stroke. A stroke is made of a set of 2D vertices\n"
             "(:class:`StrokeVertex`), regularly spaced out. This set of vertices\n"
             "defines the stroke's backbone geometry. Each of these stroke vertices\n"
             "defines the stroke's shape and appearance at this vertex position.\n"
             "\n"
             ".. method:: Stroke()\n"
             "            Stroke(brother)\n"
             "\n"
             "   Creates a :class:`Stroke` using the default constructor or copy constructor\n");

static int Stroke_init(BPy_Stroke *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"brother", nullptr};
  PyObject *brother = nullptr;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist, &Stroke_Type, &brother)) {
    return -1;
  }
  if (!brother) {
    self->s = new Stroke();
  }
  else {
    self->s = new Stroke(*(((BPy_Stroke *)brother)->s));
  }
  self->py_if1D.if1D = self->s;
  self->py_if1D.borrowed = false;
  return 0;
}

static PyObject *Stroke_iter(PyObject *self)
{
  StrokeInternal::StrokeVertexIterator sv_it(((BPy_Stroke *)self)->s->strokeVerticesBegin());
  return BPy_StrokeVertexIterator_from_StrokeVertexIterator(sv_it, false);
}

static Py_ssize_t Stroke_sq_length(BPy_Stroke *self)
{
  return self->s->strokeVerticesSize();
}

static PyObject *Stroke_sq_item(BPy_Stroke *self, Py_ssize_t keynum)
{
  if (keynum < 0) {
    keynum += Stroke_sq_length(self);
  }
  if (keynum < 0 || keynum >= Stroke_sq_length(self)) {
    PyErr_Format(PyExc_IndexError, "Stroke[index]: index %d out of range", keynum);
    return nullptr;
  }
  return BPy_StrokeVertex_from_StrokeVertex(self->s->strokeVerticeAt(keynum));
}

PyDoc_STRVAR(Stroke_compute_sampling_doc,
             ".. method:: compute_sampling(n)\n"
             "\n"
             "   Compute the sampling needed to get N vertices. If the\n"
             "   specified number of vertices is less than the actual number of\n"
             "   vertices, the actual sampling value is returned. (To remove Vertices,\n"
             "   use the RemoveVertex() method of this class.)\n"
             "\n"
             "   :arg n: The number of stroke vertices we eventually want\n"
             "      in our Stroke.\n"
             "   :type n: int\n"
             "   :return: The sampling that must be used in the Resample(float)\n"
             "      method.\n"
             "   :rtype: float");

static PyObject *Stroke_compute_sampling(BPy_Stroke *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"n", nullptr};
  int i;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", (char **)kwlist, &i)) {
    return nullptr;
  }
  return PyFloat_FromDouble(self->s->ComputeSampling(i));
}

PyDoc_STRVAR(Stroke_resample_doc,
             ".. method:: resample(n)\n"
             "            resample(sampling)\n"
             "\n"
             "   Resamples the stroke so using one of two methods with the goal\n"
             "   of creating a stroke with fewer points and the same shape.\n"
             "\n"
             "   :arg n: Resamples the stroke so that it eventually has N points. That means\n"
             "      it is going to add N-vertices_size, where vertices_size is the\n"
             "      number of points we already have. If vertices_size >= N, no\n"
             "      resampling is done.\n"
             "   :type n: int\n"
             "   :arg sampling: Resamples the stroke with a given sampling value. If the\n"
             "      sampling is smaller than the actual sampling value, no resampling is done.\n"
             "   :type sampling: float");

static PyObject *Stroke_resample(BPy_Stroke *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist_1[] = {"n", nullptr};
  static const char *kwlist_2[] = {"sampling", nullptr};
  int i;
  float f;

  if (PyArg_ParseTupleAndKeywords(args, kwds, "i", (char **)kwlist_1, &i)) {
    if (self->s->Resample(i) < 0) {
      PyErr_SetString(PyExc_RuntimeError, "Stroke resampling (by vertex count) failed");
      return nullptr;
    }
  }
  else if ((void)PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(args, kwds, "f", (char **)kwlist_2, &f)) {
    if (self->s->Resample(f) < 0) {
      PyErr_SetString(PyExc_RuntimeError, "Stroke resampling (by vertex interval) failed");
      return nullptr;
    }
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument");
    return nullptr;
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(Stroke_insert_vertex_doc,
             ".. method:: insert_vertex(vertex, next)\n"
             "\n"
             "   Inserts the StrokeVertex given as argument into the Stroke before the\n"
             "   point specified by next. The length and curvilinear abscissa are\n"
             "   updated consequently.\n"
             "\n"
             "   :arg vertex: The StrokeVertex to insert in the Stroke.\n"
             "   :type vertex: :class:`StrokeVertex`\n"
             "   :arg next: A StrokeVertexIterator pointing to the StrokeVertex\n"
             "      before which vertex must be inserted.\n"
             "   :type next: :class:`StrokeVertexIterator`");

static PyObject *Stroke_insert_vertex(BPy_Stroke *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"vertex", "next", nullptr};
  PyObject *py_sv = nullptr, *py_sv_it = nullptr;

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwds,
                                   "O!O!",
                                   (char **)kwlist,
                                   &StrokeVertex_Type,
                                   &py_sv,
                                   &StrokeVertexIterator_Type,
                                   &py_sv_it))
  {
    return nullptr;
  }

  /* Make the wrapped StrokeVertex internal. */
  ((BPy_StrokeVertex *)py_sv)->py_cp.py_if0D.borrowed = true;

  StrokeVertex *sv = ((BPy_StrokeVertex *)py_sv)->sv;
  StrokeInternal::StrokeVertexIterator sv_it(*(((BPy_StrokeVertexIterator *)py_sv_it)->sv_it));
  self->s->InsertVertex(sv, sv_it);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(Stroke_remove_vertex_doc,
             ".. method:: remove_vertex(vertex)\n"
             "\n"
             "   Removes the StrokeVertex given as argument from the Stroke. The length\n"
             "   and curvilinear abscissa are updated consequently.\n"
             "\n"
             "   :arg vertex: the StrokeVertex to remove from the Stroke.\n"
             "   :type vertex: :class:`StrokeVertex`");

static PyObject *Stroke_remove_vertex(BPy_Stroke *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"vertex", nullptr};
  PyObject *py_sv = nullptr;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist, &StrokeVertex_Type, &py_sv))
  {
    return nullptr;
  }
  if (((BPy_StrokeVertex *)py_sv)->sv) {
    self->s->RemoveVertex(((BPy_StrokeVertex *)py_sv)->sv);
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument");
    return nullptr;
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(Stroke_remove_all_vertices_doc,
             ".. method:: remove_all_vertices()\n"
             "\n"
             "   Removes all vertices from the Stroke.");

static PyObject *Stroke_remove_all_vertices(BPy_Stroke *self)
{
  self->s->RemoveAllVertices();
  Py_RETURN_NONE;
}

PyDoc_STRVAR(Stroke_update_length_doc,
             ".. method:: update_length()\n"
             "\n"
             "   Updates the 2D length of the Stroke.");

static PyObject *Stroke_update_length(BPy_Stroke *self)
{
  self->s->UpdateLength();
  Py_RETURN_NONE;
}

PyDoc_STRVAR(Stroke_stroke_vertices_begin_doc,
             ".. method:: stroke_vertices_begin(t=0.0)\n"
             "\n"
             "   Returns a StrokeVertexIterator pointing on the first StrokeVertex of\n"
             "   the Stroke. One can specify a sampling value to re-sample the Stroke\n"
             "   on the fly if needed.\n"
             "\n"
             "   :arg t: The resampling value with which we want our Stroke to be\n"
             "      resampled. If 0 is specified, no resampling is done.\n"
             "   :type t: float\n"
             "   :return: A StrokeVertexIterator pointing on the first StrokeVertex.\n"
             "   :rtype: :class:`StrokeVertexIterator`");

static PyObject *Stroke_stroke_vertices_begin(BPy_Stroke *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"t", nullptr};
  float f = 0.0f;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|f", (char **)kwlist, &f)) {
    return nullptr;
  }
  StrokeInternal::StrokeVertexIterator sv_it(self->s->strokeVerticesBegin(f));
  return BPy_StrokeVertexIterator_from_StrokeVertexIterator(sv_it, false);
}

PyDoc_STRVAR(Stroke_stroke_vertices_end_doc,
             ".. method:: stroke_vertices_end()\n"
             "\n"
             "   Returns a StrokeVertexIterator pointing after the last StrokeVertex\n"
             "   of the Stroke.\n"
             "\n"
             "   :return: A StrokeVertexIterator pointing after the last StrokeVertex.\n"
             "   :rtype: :class:`StrokeVertexIterator`");

static PyObject *Stroke_stroke_vertices_end(BPy_Stroke *self)
{
  StrokeInternal::StrokeVertexIterator sv_it(self->s->strokeVerticesEnd());
  return BPy_StrokeVertexIterator_from_StrokeVertexIterator(sv_it, true);
}

PyDoc_STRVAR(Stroke_reversed_doc,
             ".. method:: __reversed__()\n"
             "\n"
             "   Returns a StrokeVertexIterator iterating over the vertices of the Stroke\n"
             "   in the reversed order (from the last to the first).\n"
             "\n"
             "   :return: A StrokeVertexIterator pointing after the last StrokeVertex.\n"
             "   :rtype: :class:`StrokeVertexIterator`");

static PyObject *Stroke_reversed(BPy_Stroke *self)
{
  StrokeInternal::StrokeVertexIterator sv_it(self->s->strokeVerticesEnd());
  return BPy_StrokeVertexIterator_from_StrokeVertexIterator(sv_it, true);
}

PyDoc_STRVAR(Stroke_stroke_vertices_size_doc,
             ".. method:: stroke_vertices_size()\n"
             "\n"
             "   Returns the number of StrokeVertex constituting the Stroke.\n"
             "\n"
             "   :return: The number of stroke vertices.\n"
             "   :rtype: int");

static PyObject *Stroke_stroke_vertices_size(BPy_Stroke *self)
{
  return PyLong_FromLong(self->s->strokeVerticesSize());
}

static PyMethodDef BPy_Stroke_methods[] = {
    {"compute_sampling",
     (PyCFunction)Stroke_compute_sampling,
     METH_VARARGS | METH_KEYWORDS,
     Stroke_compute_sampling_doc},
    {"resample", (PyCFunction)Stroke_resample, METH_VARARGS | METH_KEYWORDS, Stroke_resample_doc},
    {"remove_all_vertices",
     (PyCFunction)Stroke_remove_all_vertices,
     METH_NOARGS,
     Stroke_remove_all_vertices_doc},
    {"remove_vertex",
     (PyCFunction)Stroke_remove_vertex,
     METH_VARARGS | METH_KEYWORDS,
     Stroke_remove_vertex_doc},
    {"insert_vertex",
     (PyCFunction)Stroke_insert_vertex,
     METH_VARARGS | METH_KEYWORDS,
     Stroke_insert_vertex_doc},
    {"update_length", (PyCFunction)Stroke_update_length, METH_NOARGS, Stroke_update_length_doc},
    {"stroke_vertices_begin",
     (PyCFunction)Stroke_stroke_vertices_begin,
     METH_VARARGS | METH_KEYWORDS,
     Stroke_stroke_vertices_begin_doc},
    {"stroke_vertices_end",
     (PyCFunction)Stroke_stroke_vertices_end,
     METH_NOARGS,
     Stroke_stroke_vertices_end_doc},
    {"__reversed__", (PyCFunction)Stroke_reversed, METH_NOARGS, Stroke_reversed_doc},
    {"stroke_vertices_size",
     (PyCFunction)Stroke_stroke_vertices_size,
     METH_NOARGS,
     Stroke_stroke_vertices_size_doc},
    {nullptr, nullptr, 0, nullptr},
};

/*----------------------Stroke get/setters ----------------------------*/

PyDoc_STRVAR(Stroke_medium_type_doc,
             "The MediumType used for this Stroke.\n"
             "\n"
             ":type: :class:`MediumType`");

static PyObject *Stroke_medium_type_get(BPy_Stroke *self, void * /*closure*/)
{
  return BPy_MediumType_from_MediumType(self->s->getMediumType());
}

static int Stroke_medium_type_set(BPy_Stroke *self, PyObject *value, void * /*closure*/)
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

static PyObject *Stroke_texture_id_get(BPy_Stroke *self, void * /*closure*/)
{
  return PyLong_FromLong(self->s->getTextureId());
}

static int Stroke_texture_id_set(BPy_Stroke *self, PyObject *value, void * /*closure*/)
{
  uint i = PyLong_AsUnsignedLong(value);
  if (PyErr_Occurred()) {
    return -1;
  }
  self->s->setTextureId(i);
  return 0;
}

PyDoc_STRVAR(Stroke_tips_doc,
             "True if this Stroke uses a texture with tips, and false otherwise.\n"
             "\n"
             ":type: bool");

static PyObject *Stroke_tips_get(BPy_Stroke *self, void * /*closure*/)
{
  return PyBool_from_bool(self->s->hasTips());
}

static int Stroke_tips_set(BPy_Stroke *self, PyObject *value, void * /*closure*/)
{
  if (!PyBool_Check(value)) {
    return -1;
  }
  self->s->setTips(bool_from_PyBool(value));
  return 0;
}

PyDoc_STRVAR(Stroke_length_2d_doc,
             "The 2D length of the Stroke.\n"
             "\n"
             ":type: float");

static PyObject *Stroke_length_2d_get(BPy_Stroke *self, void * /*closure*/)
{
  return PyFloat_FromDouble(self->s->getLength2D());
}

static int Stroke_length_2d_set(BPy_Stroke *self, PyObject *value, void * /*closure*/)
{
  float scalar;
  if ((scalar = PyFloat_AsDouble(value)) == -1.0f && PyErr_Occurred()) {
    /* parsed item not a number */
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

static PyObject *Stroke_id_get(BPy_Stroke *self, void * /*closure*/)
{
  Id id(self->s->getId());
  return BPy_Id_from_Id(id);  // return a copy
}

static int Stroke_id_set(BPy_Stroke *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_Id_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be an Id");
    return -1;
  }
  self->s->setId(*(((BPy_Id *)value)->id));
  return 0;
}

static PyGetSetDef BPy_Stroke_getseters[] = {
    {"medium_type",
     (getter)Stroke_medium_type_get,
     (setter)Stroke_medium_type_set,
     Stroke_medium_type_doc,
     nullptr},
    {"texture_id",
     (getter)Stroke_texture_id_get,
     (setter)Stroke_texture_id_set,
     Stroke_texture_id_doc,
     nullptr},
    {"tips", (getter)Stroke_tips_get, (setter)Stroke_tips_set, Stroke_tips_doc, nullptr},
    {"length_2d",
     (getter)Stroke_length_2d_get,
     (setter)Stroke_length_2d_set,
     Stroke_length_2d_doc,
     nullptr},
    {"id", (getter)Stroke_id_get, (setter)Stroke_id_set, Stroke_id_doc, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_Stroke type definition ------------------------------*/

static PySequenceMethods BPy_Stroke_as_sequence = {
    /*sq_length*/ (lenfunc)Stroke_sq_length,
    /*sq_concat*/ nullptr,
    /*sq_repeat*/ nullptr,
    /*sq_item*/ (ssizeargfunc)Stroke_sq_item,
    /*was_sq_slice*/ nullptr, /* DEPRECATED. */
    /*sq_ass_item*/ nullptr,
    /*was_sq_ass_slice*/ nullptr, /* DEPRECATED. */
    /*sq_contains*/ nullptr,
    /*sq_inplace_concat*/ nullptr,
    /*sq_inplace_repeat*/ nullptr,
};

PyTypeObject Stroke_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "Stroke",
    /*tp_basicsize*/ sizeof(BPy_Stroke),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ nullptr,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ nullptr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ &BPy_Stroke_as_sequence,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ Stroke_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ (getiterfunc)Stroke_iter,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ BPy_Stroke_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_Stroke_getseters,
    /*tp_base*/ &Interface1D_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)Stroke_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
