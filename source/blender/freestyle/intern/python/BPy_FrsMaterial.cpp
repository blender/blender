/* SPDX-FileCopyrightText: 2004-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_FrsMaterial.h"

#include "BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_hash_mm2a.h"

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int FrsMaterial_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&FrsMaterial_Type) < 0) {
    return -1;
  }
  Py_INCREF(&FrsMaterial_Type);
  PyModule_AddObject(module, "Material", (PyObject *)&FrsMaterial_Type);

  FrsMaterial_mathutils_register_callback();

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(
    FrsMaterial_doc,
    "Class defining a material.\n"
    "\n"
    ".. method:: __init__()\n"
    "            __init__(brother)\n"
    "            __init__(line, diffuse, ambient, specular, emission, shininess, priority)\n"
    "\n"
    "   Creates a :class:`FrsMaterial` using either default constructor,\n"
    "   copy constructor, or an overloaded constructor\n"
    "\n"
    "   :arg brother: A Material object to be used as a copy constructor.\n"
    "   :type brother: :class:`Material`\n"
    "   :arg line: The line color.\n"
    "   :type line: :class:`mathutils.Vector`, list or tuple of 4 float values\n"
    "   :arg diffuse: The diffuse color.\n"
    "   :type diffuse: :class:`mathutils.Vector`, list or tuple of 4 float values\n"
    "   :arg ambient: The ambient color.\n"
    "   :type ambient: :class:`mathutils.Vector`, list or tuple of 4 float values\n"
    "   :arg specular: The specular color.\n"
    "   :type specular: :class:`mathutils.Vector`, list or tuple of 4 float values\n"
    "   :arg emission: The emissive color.\n"
    "   :type emission: :class:`mathutils.Vector`, list or tuple of 4 float values\n"
    "   :arg shininess: The shininess coefficient.\n"
    "   :type shininess: float\n"
    "   :arg priority: The line color priority.\n"
    "   :type priority: int");

static int FrsMaterial_init(BPy_FrsMaterial *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", nullptr};
  static const char *kwlist_2[] = {
      "line", "diffuse", "ambient", "specular", "emission", "shininess", "priority", nullptr};
  PyObject *brother = nullptr;
  float line[4], diffuse[4], ambient[4], specular[4], emission[4], shininess;
  int priority;

  if (PyArg_ParseTupleAndKeywords(
          args, kwds, "|O!", (char **)kwlist_1, &FrsMaterial_Type, &brother)) {
    if (!brother) {
      self->m = new FrsMaterial();
    }
    else {
      FrsMaterial *m = ((BPy_FrsMaterial *)brother)->m;
      if (!m) {
        PyErr_SetString(PyExc_RuntimeError, "invalid Material object");
        return -1;
      }
      self->m = new FrsMaterial(*m);
    }
  }
  else if ((void)PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(args,
                                       kwds,
                                       "O&O&O&O&O&fi",
                                       (char **)kwlist_2,
                                       convert_v4,
                                       line,
                                       convert_v4,
                                       diffuse,
                                       convert_v4,
                                       ambient,
                                       convert_v4,
                                       specular,
                                       convert_v4,
                                       emission,
                                       &shininess,
                                       &priority))
  {
    self->m = new FrsMaterial(line, diffuse, ambient, specular, emission, shininess, priority);
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
    return -1;
  }
  return 0;
}

static void FrsMaterial_dealloc(BPy_FrsMaterial *self)
{
  delete self->m;
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *FrsMaterial_repr(BPy_FrsMaterial *self)
{
  return PyUnicode_FromFormat("Material - address: %p", self->m);
}

/*----------------------mathutils callbacks ----------------------------*/

/* subtype */
#define MATHUTILS_SUBTYPE_DIFFUSE 1
#define MATHUTILS_SUBTYPE_SPECULAR 2
#define MATHUTILS_SUBTYPE_AMBIENT 3
#define MATHUTILS_SUBTYPE_EMISSION 4
#define MATHUTILS_SUBTYPE_LINE 5

static int FrsMaterial_mathutils_check(BaseMathObject *bmo)
{
  if (!BPy_FrsMaterial_Check(bmo->cb_user)) {
    return -1;
  }
  return 0;
}

static int FrsMaterial_mathutils_get(BaseMathObject *bmo, int subtype)
{
  BPy_FrsMaterial *self = (BPy_FrsMaterial *)bmo->cb_user;
  switch (subtype) {
    case MATHUTILS_SUBTYPE_LINE:
      bmo->data[0] = self->m->lineR();
      bmo->data[1] = self->m->lineG();
      bmo->data[2] = self->m->lineB();
      bmo->data[3] = self->m->lineA();
      break;
    case MATHUTILS_SUBTYPE_DIFFUSE:
      bmo->data[0] = self->m->diffuseR();
      bmo->data[1] = self->m->diffuseG();
      bmo->data[2] = self->m->diffuseB();
      bmo->data[3] = self->m->diffuseA();
      break;
    case MATHUTILS_SUBTYPE_SPECULAR:
      bmo->data[0] = self->m->specularR();
      bmo->data[1] = self->m->specularG();
      bmo->data[2] = self->m->specularB();
      bmo->data[3] = self->m->specularA();
      break;
    case MATHUTILS_SUBTYPE_AMBIENT:
      bmo->data[0] = self->m->ambientR();
      bmo->data[1] = self->m->ambientG();
      bmo->data[2] = self->m->ambientB();
      bmo->data[3] = self->m->ambientA();
      break;
    case MATHUTILS_SUBTYPE_EMISSION:
      bmo->data[0] = self->m->emissionR();
      bmo->data[1] = self->m->emissionG();
      bmo->data[2] = self->m->emissionB();
      bmo->data[3] = self->m->emissionA();
      break;
    default:
      return -1;
  }
  return 0;
}

static int FrsMaterial_mathutils_set(BaseMathObject *bmo, int subtype)
{
  BPy_FrsMaterial *self = (BPy_FrsMaterial *)bmo->cb_user;
  switch (subtype) {
    case MATHUTILS_SUBTYPE_LINE:
      self->m->setLine(bmo->data[0], bmo->data[1], bmo->data[2], bmo->data[3]);
      break;
    case MATHUTILS_SUBTYPE_DIFFUSE:
      self->m->setDiffuse(bmo->data[0], bmo->data[1], bmo->data[2], bmo->data[3]);
      break;
    case MATHUTILS_SUBTYPE_SPECULAR:
      self->m->setSpecular(bmo->data[0], bmo->data[1], bmo->data[2], bmo->data[3]);
      break;
    case MATHUTILS_SUBTYPE_AMBIENT:
      self->m->setAmbient(bmo->data[0], bmo->data[1], bmo->data[2], bmo->data[3]);
      break;
    case MATHUTILS_SUBTYPE_EMISSION:
      self->m->setEmission(bmo->data[0], bmo->data[1], bmo->data[2], bmo->data[3]);
      break;
    default:
      return -1;
  }
  return 0;
}

static int FrsMaterial_mathutils_get_index(BaseMathObject *bmo, int subtype, int index)
{
  BPy_FrsMaterial *self = (BPy_FrsMaterial *)bmo->cb_user;
  switch (subtype) {
    case MATHUTILS_SUBTYPE_LINE: {
      const float *color = self->m->line();
      bmo->data[index] = color[index];
    } break;
    case MATHUTILS_SUBTYPE_DIFFUSE: {
      const float *color = self->m->diffuse();
      bmo->data[index] = color[index];
    } break;
    case MATHUTILS_SUBTYPE_SPECULAR: {
      const float *color = self->m->specular();
      bmo->data[index] = color[index];
    } break;
    case MATHUTILS_SUBTYPE_AMBIENT: {
      const float *color = self->m->ambient();
      bmo->data[index] = color[index];
    } break;
    case MATHUTILS_SUBTYPE_EMISSION: {
      const float *color = self->m->emission();
      bmo->data[index] = color[index];
    } break;
    default:
      return -1;
  }
  return 0;
}

static int FrsMaterial_mathutils_set_index(BaseMathObject *bmo, int subtype, int index)
{
  BPy_FrsMaterial *self = (BPy_FrsMaterial *)bmo->cb_user;
  float color[4];
  switch (subtype) {
    case MATHUTILS_SUBTYPE_LINE:
      copy_v4_v4(color, self->m->line());
      color[index] = bmo->data[index];
      self->m->setLine(color[0], color[1], color[2], color[3]);
      break;
    case MATHUTILS_SUBTYPE_DIFFUSE:
      copy_v4_v4(color, self->m->diffuse());
      color[index] = bmo->data[index];
      self->m->setDiffuse(color[0], color[1], color[2], color[3]);
      break;
    case MATHUTILS_SUBTYPE_SPECULAR:
      copy_v4_v4(color, self->m->specular());
      color[index] = bmo->data[index];
      self->m->setSpecular(color[0], color[1], color[2], color[3]);
      break;
    case MATHUTILS_SUBTYPE_AMBIENT:
      copy_v4_v4(color, self->m->ambient());
      color[index] = bmo->data[index];
      self->m->setAmbient(color[0], color[1], color[2], color[3]);
      break;
    case MATHUTILS_SUBTYPE_EMISSION:
      copy_v4_v4(color, self->m->emission());
      color[index] = bmo->data[index];
      self->m->setEmission(color[0], color[1], color[2], color[3]);
      break;
    default:
      return -1;
  }
  return 0;
}

static Mathutils_Callback FrsMaterial_mathutils_cb = {
    FrsMaterial_mathutils_check,
    FrsMaterial_mathutils_get,
    FrsMaterial_mathutils_set,
    FrsMaterial_mathutils_get_index,
    FrsMaterial_mathutils_set_index,
};

static uchar FrsMaterial_mathutils_cb_index = -1;

void FrsMaterial_mathutils_register_callback()
{
  FrsMaterial_mathutils_cb_index = Mathutils_RegisterCallback(&FrsMaterial_mathutils_cb);
}

/*----------------------FrsMaterial get/setters ----------------------------*/

PyDoc_STRVAR(FrsMaterial_line_doc,
             "RGBA components of the line color of the material.\n"
             "\n"
             ":type: :class:`mathutils.Vector`");

static PyObject *FrsMaterial_line_get(BPy_FrsMaterial *self, void * /*closure*/)
{
  return Vector_CreatePyObject_cb(
      (PyObject *)self, 4, FrsMaterial_mathutils_cb_index, MATHUTILS_SUBTYPE_LINE);
}

static int FrsMaterial_line_set(BPy_FrsMaterial *self, PyObject *value, void * /*closure*/)
{
  float color[4];
  if (mathutils_array_parse(color, 4, 4, value, "value must be a 4-dimensional vector") == -1) {
    return -1;
  }
  self->m->setLine(color[0], color[1], color[2], color[3]);
  return 0;
}

PyDoc_STRVAR(FrsMaterial_diffuse_doc,
             "RGBA components of the diffuse color of the material.\n"
             "\n"
             ":type: :class:`mathutils.Vector`");

static PyObject *FrsMaterial_diffuse_get(BPy_FrsMaterial *self, void * /*closure*/)
{
  return Vector_CreatePyObject_cb(
      (PyObject *)self, 4, FrsMaterial_mathutils_cb_index, MATHUTILS_SUBTYPE_DIFFUSE);
}

static int FrsMaterial_diffuse_set(BPy_FrsMaterial *self, PyObject *value, void * /*closure*/)
{
  float color[4];
  if (mathutils_array_parse(color, 4, 4, value, "value must be a 4-dimensional vector") == -1) {
    return -1;
  }
  self->m->setDiffuse(color[0], color[1], color[2], color[3]);
  return 0;
}

PyDoc_STRVAR(FrsMaterial_specular_doc,
             "RGBA components of the specular color of the material.\n"
             "\n"
             ":type: :class:`mathutils.Vector`");

static PyObject *FrsMaterial_specular_get(BPy_FrsMaterial *self, void * /*closure*/)
{
  return Vector_CreatePyObject_cb(
      (PyObject *)self, 4, FrsMaterial_mathutils_cb_index, MATHUTILS_SUBTYPE_SPECULAR);
}

static int FrsMaterial_specular_set(BPy_FrsMaterial *self, PyObject *value, void * /*closure*/)
{
  float color[4];
  if (mathutils_array_parse(color, 4, 4, value, "value must be a 4-dimensional vector") == -1) {
    return -1;
  }
  self->m->setSpecular(color[0], color[1], color[2], color[3]);
  return 0;
}

PyDoc_STRVAR(FrsMaterial_ambient_doc,
             "RGBA components of the ambient color of the material.\n"
             "\n"
             ":type: :class:`mathutils.Color`");

static PyObject *FrsMaterial_ambient_get(BPy_FrsMaterial *self, void * /*closure*/)
{
  return Vector_CreatePyObject_cb(
      (PyObject *)self, 4, FrsMaterial_mathutils_cb_index, MATHUTILS_SUBTYPE_AMBIENT);
}

static int FrsMaterial_ambient_set(BPy_FrsMaterial *self, PyObject *value, void * /*closure*/)
{
  float color[4];
  if (mathutils_array_parse(color, 4, 4, value, "value must be a 4-dimensional vector") == -1) {
    return -1;
  }
  self->m->setAmbient(color[0], color[1], color[2], color[3]);
  return 0;
}

PyDoc_STRVAR(FrsMaterial_emission_doc,
             "RGBA components of the emissive color of the material.\n"
             "\n"
             ":type: :class:`mathutils.Color`");

static PyObject *FrsMaterial_emission_get(BPy_FrsMaterial *self, void * /*closure*/)
{
  return Vector_CreatePyObject_cb(
      (PyObject *)self, 4, FrsMaterial_mathutils_cb_index, MATHUTILS_SUBTYPE_EMISSION);
}

static int FrsMaterial_emission_set(BPy_FrsMaterial *self, PyObject *value, void * /*closure*/)
{
  float color[4];
  if (mathutils_array_parse(color, 4, 4, value, "value must be a 4-dimensional vector") == -1) {
    return -1;
  }
  self->m->setEmission(color[0], color[1], color[2], color[3]);
  return 0;
}

PyDoc_STRVAR(FrsMaterial_shininess_doc,
             "Shininess coefficient of the material.\n"
             "\n"
             ":type: float");

static PyObject *FrsMaterial_shininess_get(BPy_FrsMaterial *self, void * /*closure*/)
{
  return PyFloat_FromDouble(self->m->shininess());
}

static int FrsMaterial_shininess_set(BPy_FrsMaterial *self, PyObject *value, void * /*closure*/)
{
  float scalar;
  if ((scalar = PyFloat_AsDouble(value)) == -1.0f && PyErr_Occurred()) {
    /* parsed item not a number */
    PyErr_SetString(PyExc_TypeError, "value must be a number");
    return -1;
  }
  self->m->setShininess(scalar);
  return 0;
}

PyDoc_STRVAR(FrsMaterial_priority_doc,
             "Line color priority of the material.\n"
             "\n"
             ":type: int");

static PyObject *FrsMaterial_priority_get(BPy_FrsMaterial *self, void * /*closure*/)
{
  return PyLong_FromLong(self->m->priority());
}

static int FrsMaterial_priority_set(BPy_FrsMaterial *self, PyObject *value, void * /*closure*/)
{
  int scalar;
  if ((scalar = PyLong_AsLong(value)) == -1 && PyErr_Occurred()) {
    PyErr_SetString(PyExc_TypeError, "value must be an integer");
    return -1;
  }
  self->m->setPriority(scalar);
  return 0;
}

static PyGetSetDef BPy_FrsMaterial_getseters[] = {
    {"line",
     (getter)FrsMaterial_line_get,
     (setter)FrsMaterial_line_set,
     FrsMaterial_line_doc,
     nullptr},
    {"diffuse",
     (getter)FrsMaterial_diffuse_get,
     (setter)FrsMaterial_diffuse_set,
     FrsMaterial_diffuse_doc,
     nullptr},
    {"specular",
     (getter)FrsMaterial_specular_get,
     (setter)FrsMaterial_specular_set,
     FrsMaterial_specular_doc,
     nullptr},
    {"ambient",
     (getter)FrsMaterial_ambient_get,
     (setter)FrsMaterial_ambient_set,
     FrsMaterial_ambient_doc,
     nullptr},
    {"emission",
     (getter)FrsMaterial_emission_get,
     (setter)FrsMaterial_emission_set,
     FrsMaterial_emission_doc,
     nullptr},
    {"shininess",
     (getter)FrsMaterial_shininess_get,
     (setter)FrsMaterial_shininess_set,
     FrsMaterial_shininess_doc,
     nullptr},
    {"priority",
     (getter)FrsMaterial_priority_get,
     (setter)FrsMaterial_priority_set,
     FrsMaterial_priority_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

static PyObject *BPy_FrsMaterial_richcmpr(PyObject *objectA,
                                          PyObject *objectB,
                                          int comparison_type)
{
  const BPy_FrsMaterial *matA = nullptr, *matB = nullptr;
  bool result = false;

  if (!BPy_FrsMaterial_Check(objectA) || !BPy_FrsMaterial_Check(objectB)) {
    if (comparison_type == Py_NE) {
      Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
  }

  matA = (BPy_FrsMaterial *)objectA;
  matB = (BPy_FrsMaterial *)objectB;

  switch (comparison_type) {
    case Py_NE:
      result = (*matA->m) != (*matB->m);
      break;
    case Py_EQ:
      result = (*matA->m) == (*matB->m);
      break;
    default:
      PyErr_SetString(PyExc_TypeError, "Material does not support this comparison type");
      return nullptr;
  }

  if (result == true) {
    Py_RETURN_TRUE;
  }

  Py_RETURN_FALSE;
}

static Py_hash_t FrsMaterial_hash(PyObject *self)
{
  return (Py_uhash_t)BLI_hash_mm2((const uchar *)self, sizeof(*self), 0);
}
/*-----------------------BPy_FrsMaterial type definition ------------------------------*/

PyTypeObject FrsMaterial_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "Material",
    /*tp_basicsize*/ sizeof(BPy_FrsMaterial),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)FrsMaterial_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)FrsMaterial_repr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ (hashfunc)FrsMaterial_hash,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ FrsMaterial_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ (richcmpfunc)BPy_FrsMaterial_richcmpr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_FrsMaterial_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)FrsMaterial_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ PyType_GenericNew,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
