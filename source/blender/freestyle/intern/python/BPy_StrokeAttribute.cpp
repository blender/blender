/* SPDX-FileCopyrightText: 2004-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_StrokeAttribute.h"

#include "BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int StrokeAttribute_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&StrokeAttribute_Type) < 0) {
    return -1;
  }
  Py_INCREF(&StrokeAttribute_Type);
  PyModule_AddObject(module, "StrokeAttribute", (PyObject *)&StrokeAttribute_Type);

  StrokeAttribute_mathutils_register_callback();
  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(StrokeAttribute_doc,
             "Class to define a set of attributes associated with a :class:`StrokeVertex`.\n"
             "The attribute set stores the color, alpha and thickness values for a Stroke\n"
             "Vertex.\n"
             "\n"
             ".. method:: __init__()\n"
             "            __init__(brother)\n"
             "            __init__(red, green, blue, alpha, thickness_right, thickness_left)\n"
             "            __init__(attribute1, attribute2, t)\n"
             "\n"
             "   Creates a :class:`StrokeAttribute` object using either a default constructor,\n"
             "   copy constructor, overloaded constructor, or and interpolation constructor\n"
             "   to interpolate between two :class:`StrokeAttribute` objects.\n"
             "\n"
             "   :arg brother: A StrokeAttribute object to be used as a copy constructor.\n"
             "   :type brother: :class:`StrokeAttribute`\n"
             "   :arg red: Red component of a stroke color.\n"
             "   :type red: float\n"
             "   :arg green: Green component of a stroke color.\n"
             "   :type green: float\n"
             "   :arg blue: Blue component of a stroke color.\n"
             "   :type blue: float\n"
             "   :arg alpha: Alpha component of a stroke color.\n"
             "   :type alpha: float\n"
             "   :arg thickness_right: Stroke thickness on the right.\n"
             "   :type thickness_right: float\n"
             "   :arg thickness_left: Stroke thickness on the left.\n"
             "   :type thickness_left: float\n"
             "   :arg attribute1: The first StrokeAttribute object.\n"
             "   :type attribute1: :class:`StrokeAttribute`\n"
             "   :arg attribute2: The second StrokeAttribute object.\n"
             "   :type attribute2: :class:`StrokeAttribute`\n"
             "   :arg t: The interpolation parameter (0 <= t <= 1).\n"
             "   :type t: float\n");

static int StrokeAttribute_init(BPy_StrokeAttribute *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", nullptr};
  static const char *kwlist_2[] = {"attribute1", "attribute2", "t", nullptr};
  static const char *kwlist_3[] = {
      "red", "green", "blue", "alpha", "thickness_right", "thickness_left", nullptr};
  PyObject *obj1 = nullptr, *obj2 = nullptr;
  float red, green, blue, alpha, thickness_right, thickness_left, t;

  if (PyArg_ParseTupleAndKeywords(
          args, kwds, "|O!", (char **)kwlist_1, &StrokeAttribute_Type, &obj1))
  {
    if (!obj1) {
      self->sa = new StrokeAttribute();
    }
    else {
      self->sa = new StrokeAttribute(*(((BPy_StrokeAttribute *)obj1)->sa));
    }
  }
  else if ((void)PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(args,
                                       kwds,
                                       "O!O!f",
                                       (char **)kwlist_2,
                                       &StrokeAttribute_Type,
                                       &obj1,
                                       &StrokeAttribute_Type,
                                       &obj2,
                                       &t))
  {
    self->sa = new StrokeAttribute(
        *(((BPy_StrokeAttribute *)obj1)->sa), *(((BPy_StrokeAttribute *)obj2)->sa), t);
  }
  else if ((void)PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(args,
                                       kwds,
                                       "ffffff",
                                       (char **)kwlist_3,
                                       &red,
                                       &green,
                                       &blue,
                                       &alpha,
                                       &thickness_right,
                                       &thickness_left))
  {
    self->sa = new StrokeAttribute(red, green, blue, alpha, thickness_right, thickness_left);
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
    return -1;
  }
  self->borrowed = false;
  return 0;
}

static void StrokeAttribute_dealloc(BPy_StrokeAttribute *self)
{
  if (self->sa && !self->borrowed) {
    delete self->sa;
  }
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *StrokeAttribute_repr(BPy_StrokeAttribute *self)
{
  stringstream repr("StrokeAttribute:");
  repr << " r: " << self->sa->getColorR() << " g: " << self->sa->getColorG()
       << " b: " << self->sa->getColorB() << " a: " << self->sa->getAlpha()
       << " - R: " << self->sa->getThicknessR() << " L: " << self->sa->getThicknessL();

  return PyUnicode_FromString(repr.str().c_str());
}

PyDoc_STRVAR(StrokeAttribute_get_attribute_real_doc,
             ".. method:: get_attribute_real(name)\n"
             "\n"
             "   Returns an attribute of float type.\n"
             "\n"
             "   :arg name: The name of the attribute.\n"
             "   :type name: str\n"
             "   :return: The attribute value.\n"
             "   :rtype: float\n");

static PyObject *StrokeAttribute_get_attribute_real(BPy_StrokeAttribute *self,
                                                    PyObject *args,
                                                    PyObject *kwds)
{
  static const char *kwlist[] = {"name", nullptr};
  char *attr;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &attr)) {
    return nullptr;
  }
  double a = self->sa->getAttributeReal(attr);
  return PyFloat_FromDouble(a);
}

PyDoc_STRVAR(StrokeAttribute_get_attribute_vec2_doc,
             ".. method:: get_attribute_vec2(name)\n"
             "\n"
             "   Returns an attribute of two-dimensional vector type.\n"
             "\n"
             "   :arg name: The name of the attribute.\n"
             "   :type name: str\n"
             "   :return: The attribute value.\n"
             "   :rtype: :class:`mathutils.Vector`\n");

static PyObject *StrokeAttribute_get_attribute_vec2(BPy_StrokeAttribute *self,
                                                    PyObject *args,
                                                    PyObject *kwds)
{
  static const char *kwlist[] = {"name", nullptr};
  char *attr;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &attr)) {
    return nullptr;
  }
  Vec2f a = self->sa->getAttributeVec2f(attr);
  return Vector_from_Vec2f(a);
}

PyDoc_STRVAR(StrokeAttribute_get_attribute_vec3_doc,
             ".. method:: get_attribute_vec3(name)\n"
             "\n"
             "   Returns an attribute of three-dimensional vector type.\n"
             "\n"
             "   :arg name: The name of the attribute.\n"
             "   :type name: str\n"
             "   :return: The attribute value.\n"
             "   :rtype: :class:`mathutils.Vector`\n");

static PyObject *StrokeAttribute_get_attribute_vec3(BPy_StrokeAttribute *self,
                                                    PyObject *args,
                                                    PyObject *kwds)
{
  static const char *kwlist[] = {"name", nullptr};
  char *attr;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &attr)) {
    return nullptr;
  }
  Vec3f a = self->sa->getAttributeVec3f(attr);
  return Vector_from_Vec3f(a);
}

PyDoc_STRVAR(StrokeAttribute_has_attribute_real_doc,
             ".. method:: has_attribute_real(name)\n"
             "\n"
             "   Checks whether the attribute name of float type is available.\n"
             "\n"
             "   :arg name: The name of the attribute.\n"
             "   :type name: str\n"
             "   :return: True if the attribute is available.\n"
             "   :rtype: bool\n");

static PyObject *StrokeAttribute_has_attribute_real(BPy_StrokeAttribute *self,
                                                    PyObject *args,
                                                    PyObject *kwds)
{
  static const char *kwlist[] = {"name", nullptr};
  char *attr;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &attr)) {
    return nullptr;
  }
  return PyBool_from_bool(self->sa->isAttributeAvailableReal(attr));
}

PyDoc_STRVAR(StrokeAttribute_has_attribute_vec2_doc,
             ".. method:: has_attribute_vec2(name)\n"
             "\n"
             "   Checks whether the attribute name of two-dimensional vector type\n"
             "   is available.\n"
             "\n"
             "   :arg name: The name of the attribute.\n"
             "   :type name: str\n"
             "   :return: True if the attribute is available.\n"
             "   :rtype: bool\n");

static PyObject *StrokeAttribute_has_attribute_vec2(BPy_StrokeAttribute *self,
                                                    PyObject *args,
                                                    PyObject *kwds)
{
  static const char *kwlist[] = {"name", nullptr};
  char *attr;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &attr)) {
    return nullptr;
  }
  return PyBool_from_bool(self->sa->isAttributeAvailableVec2f(attr));
}

PyDoc_STRVAR(StrokeAttribute_has_attribute_vec3_doc,
             ".. method:: has_attribute_vec3(name)\n"
             "\n"
             "   Checks whether the attribute name of three-dimensional vector\n"
             "   type is available.\n"
             "\n"
             "   :arg name: The name of the attribute.\n"
             "   :type name: str\n"
             "   :return: True if the attribute is available.\n"
             "   :rtype: bool\n");

static PyObject *StrokeAttribute_has_attribute_vec3(BPy_StrokeAttribute *self,
                                                    PyObject *args,
                                                    PyObject *kwds)
{
  static const char *kwlist[] = {"name", nullptr};
  char *attr;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", (char **)kwlist, &attr)) {
    return nullptr;
  }
  return PyBool_from_bool(self->sa->isAttributeAvailableVec3f(attr));
}

PyDoc_STRVAR(StrokeAttribute_set_attribute_real_doc,
             ".. method:: set_attribute_real(name, value)\n"
             "\n"
             "   Adds a user-defined attribute of float type. If there is no\n"
             "   attribute of the given name, it is added. Otherwise, the new value\n"
             "   replaces the old one.\n"
             "\n"
             "   :arg name: The name of the attribute.\n"
             "   :type name: str\n"
             "   :arg value: The attribute value.\n"
             "   :type value: float\n");

static PyObject *StrokeAttribute_set_attribute_real(BPy_StrokeAttribute *self,
                                                    PyObject *args,
                                                    PyObject *kwds)
{
  static const char *kwlist[] = {"name", "value", nullptr};
  char *s = nullptr;
  double d = 0;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "sd", (char **)kwlist, &s, &d)) {
    return nullptr;
  }
  self->sa->setAttributeReal(s, d);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(StrokeAttribute_set_attribute_vec2_doc,
             ".. method:: set_attribute_vec2(name, value)\n"
             "\n"
             "   Adds a user-defined attribute of two-dimensional vector type. If\n"
             "   there is no attribute of the given name, it is added. Otherwise,\n"
             "   the new value replaces the old one.\n"
             "\n"
             "   :arg name: The name of the attribute.\n"
             "   :type name: str\n"
             "   :arg value: The attribute value.\n"
             "   :type value: :class:`mathutils.Vector`, list or tuple of 2 real numbers\n");

static PyObject *StrokeAttribute_set_attribute_vec2(BPy_StrokeAttribute *self,
                                                    PyObject *args,
                                                    PyObject *kwds)
{
  static const char *kwlist[] = {"name", "value", nullptr};
  char *s;
  PyObject *obj = nullptr;
  Vec2f vec;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "sO", (char **)kwlist, &s, &obj)) {
    return nullptr;
  }
  if (!Vec2f_ptr_from_PyObject(obj, vec)) {
    PyErr_SetString(PyExc_TypeError,
                    "argument 2 must be a 2D vector (either a list of 2 elements or Vector)");
    return nullptr;
  }
  self->sa->setAttributeVec2f(s, vec);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(StrokeAttribute_set_attribute_vec3_doc,
             ".. method:: set_attribute_vec3(name, value)\n"
             "\n"
             "   Adds a user-defined attribute of three-dimensional vector type.\n"
             "   If there is no attribute of the given name, it is added.\n"
             "   Otherwise, the new value replaces the old one.\n"
             "\n"
             "   :arg name: The name of the attribute.\n"
             "   :type name: str\n"
             "   :arg value: The attribute value.\n"
             "   :type value: :class:`mathutils.Vector`, list or tuple of 3 real numbers\n");

static PyObject *StrokeAttribute_set_attribute_vec3(BPy_StrokeAttribute *self,
                                                    PyObject *args,
                                                    PyObject *kwds)
{
  static const char *kwlist[] = {"name", "value", nullptr};
  char *s;
  PyObject *obj = nullptr;
  Vec3f vec;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "sO", (char **)kwlist, &s, &obj)) {
    return nullptr;
  }
  if (!Vec3f_ptr_from_PyObject(obj, vec)) {
    PyErr_SetString(PyExc_TypeError,
                    "argument 2 must be a 3D vector (either a list of 3 elements or Vector)");
    return nullptr;
  }
  self->sa->setAttributeVec3f(s, vec);
  Py_RETURN_NONE;
}

static PyMethodDef BPy_StrokeAttribute_methods[] = {
    {"get_attribute_real",
     (PyCFunction)StrokeAttribute_get_attribute_real,
     METH_VARARGS | METH_KEYWORDS,
     StrokeAttribute_get_attribute_real_doc},
    {"get_attribute_vec2",
     (PyCFunction)StrokeAttribute_get_attribute_vec2,
     METH_VARARGS | METH_KEYWORDS,
     StrokeAttribute_get_attribute_vec2_doc},
    {"get_attribute_vec3",
     (PyCFunction)StrokeAttribute_get_attribute_vec3,
     METH_VARARGS | METH_KEYWORDS,
     StrokeAttribute_get_attribute_vec3_doc},
    {"has_attribute_real",
     (PyCFunction)StrokeAttribute_has_attribute_real,
     METH_VARARGS | METH_KEYWORDS,
     StrokeAttribute_has_attribute_real_doc},
    {"has_attribute_vec2",
     (PyCFunction)StrokeAttribute_has_attribute_vec2,
     METH_VARARGS | METH_KEYWORDS,
     StrokeAttribute_has_attribute_vec2_doc},
    {"has_attribute_vec3",
     (PyCFunction)StrokeAttribute_has_attribute_vec3,
     METH_VARARGS | METH_KEYWORDS,
     StrokeAttribute_has_attribute_vec3_doc},
    {"set_attribute_real",
     (PyCFunction)StrokeAttribute_set_attribute_real,
     METH_VARARGS | METH_KEYWORDS,
     StrokeAttribute_set_attribute_real_doc},
    {"set_attribute_vec2",
     (PyCFunction)StrokeAttribute_set_attribute_vec2,
     METH_VARARGS | METH_KEYWORDS,
     StrokeAttribute_set_attribute_vec2_doc},
    {"set_attribute_vec3",
     (PyCFunction)StrokeAttribute_set_attribute_vec3,
     METH_VARARGS | METH_KEYWORDS,
     StrokeAttribute_set_attribute_vec3_doc},
    {nullptr, nullptr, 0, nullptr},
};

/*----------------------mathutils callbacks ----------------------------*/

/* subtype */
#define MATHUTILS_SUBTYPE_COLOR 1
#define MATHUTILS_SUBTYPE_THICKNESS 2

static int StrokeAttribute_mathutils_check(BaseMathObject *bmo)
{
  if (!BPy_StrokeAttribute_Check(bmo->cb_user)) {
    return -1;
  }
  return 0;
}

static int StrokeAttribute_mathutils_get(BaseMathObject *bmo, int subtype)
{
  BPy_StrokeAttribute *self = (BPy_StrokeAttribute *)bmo->cb_user;
  switch (subtype) {
    case MATHUTILS_SUBTYPE_COLOR:
      bmo->data[0] = self->sa->getColorR();
      bmo->data[1] = self->sa->getColorG();
      bmo->data[2] = self->sa->getColorB();
      break;
    case MATHUTILS_SUBTYPE_THICKNESS:
      bmo->data[0] = self->sa->getThicknessR();
      bmo->data[1] = self->sa->getThicknessL();
      break;
    default:
      return -1;
  }
  return 0;
}

static int StrokeAttribute_mathutils_set(BaseMathObject *bmo, int subtype)
{
  BPy_StrokeAttribute *self = (BPy_StrokeAttribute *)bmo->cb_user;
  switch (subtype) {
    case MATHUTILS_SUBTYPE_COLOR:
      self->sa->setColor(bmo->data[0], bmo->data[1], bmo->data[2]);
      break;
    case MATHUTILS_SUBTYPE_THICKNESS:
      self->sa->setThickness(bmo->data[0], bmo->data[1]);
      break;
    default:
      return -1;
  }
  return 0;
}

static int StrokeAttribute_mathutils_get_index(BaseMathObject *bmo, int subtype, int index)
{
  BPy_StrokeAttribute *self = (BPy_StrokeAttribute *)bmo->cb_user;
  switch (subtype) {
    case MATHUTILS_SUBTYPE_COLOR:
      switch (index) {
        case 0:
          bmo->data[0] = self->sa->getColorR();
          break;
        case 1:
          bmo->data[1] = self->sa->getColorG();
          break;
        case 2:
          bmo->data[2] = self->sa->getColorB();
          break;
        default:
          return -1;
      }
      break;
    case MATHUTILS_SUBTYPE_THICKNESS:
      switch (index) {
        case 0:
          bmo->data[0] = self->sa->getThicknessR();
          break;
        case 1:
          bmo->data[1] = self->sa->getThicknessL();
          break;
        default:
          return -1;
      }
      break;
    default:
      return -1;
  }
  return 0;
}

static int StrokeAttribute_mathutils_set_index(BaseMathObject *bmo, int subtype, int index)
{
  BPy_StrokeAttribute *self = (BPy_StrokeAttribute *)bmo->cb_user;
  switch (subtype) {
    case MATHUTILS_SUBTYPE_COLOR: {
      float r = (index == 0) ? bmo->data[0] : self->sa->getColorR();
      float g = (index == 1) ? bmo->data[1] : self->sa->getColorG();
      float b = (index == 2) ? bmo->data[2] : self->sa->getColorB();
      self->sa->setColor(r, g, b);
    } break;
    case MATHUTILS_SUBTYPE_THICKNESS: {
      float tr = (index == 0) ? bmo->data[0] : self->sa->getThicknessR();
      float tl = (index == 1) ? bmo->data[1] : self->sa->getThicknessL();
      self->sa->setThickness(tr, tl);
    } break;
    default:
      return -1;
  }
  return 0;
}

static Mathutils_Callback StrokeAttribute_mathutils_cb = {
    StrokeAttribute_mathutils_check,
    StrokeAttribute_mathutils_get,
    StrokeAttribute_mathutils_set,
    StrokeAttribute_mathutils_get_index,
    StrokeAttribute_mathutils_set_index,
};

static uchar StrokeAttribute_mathutils_cb_index = -1;

void StrokeAttribute_mathutils_register_callback()
{
  StrokeAttribute_mathutils_cb_index = Mathutils_RegisterCallback(&StrokeAttribute_mathutils_cb);
}

/*----------------------StrokeAttribute get/setters ----------------------------*/

PyDoc_STRVAR(StrokeAttribute_alpha_doc,
             "Alpha component of the stroke color.\n"
             "\n"
             ":type: float");

static PyObject *StrokeAttribute_alpha_get(BPy_StrokeAttribute *self, void * /*closure*/)
{
  return PyFloat_FromDouble(self->sa->getAlpha());
}

static int StrokeAttribute_alpha_set(BPy_StrokeAttribute *self,
                                     PyObject *value,
                                     void * /*closure*/)
{
  float scalar;
  if ((scalar = PyFloat_AsDouble(value)) == -1.0f && PyErr_Occurred()) {
    /* parsed item not a number */
    PyErr_SetString(PyExc_TypeError, "value must be a number");
    return -1;
  }
  self->sa->setAlpha(scalar);
  return 0;
}

PyDoc_STRVAR(StrokeAttribute_color_doc,
             "RGB components of the stroke color.\n"
             "\n"
             ":type: :class:`mathutils.Color`");

static PyObject *StrokeAttribute_color_get(BPy_StrokeAttribute *self, void * /*closure*/)
{
  return Color_CreatePyObject_cb(
      (PyObject *)self, StrokeAttribute_mathutils_cb_index, MATHUTILS_SUBTYPE_COLOR);
}

static int StrokeAttribute_color_set(BPy_StrokeAttribute *self,
                                     PyObject *value,
                                     void * /*closure*/)
{
  float v[3];
  if (mathutils_array_parse(v, 3, 3, value, "value must be a 3-dimensional vector") == -1) {
    return -1;
  }
  self->sa->setColor(v[0], v[1], v[2]);
  return 0;
}

PyDoc_STRVAR(StrokeAttribute_thickness_doc,
             "Right and left components of the stroke thickness.\n"
             "The right (left) component is the thickness on the right (left) of the vertex\n"
             "when following the stroke.\n"
             "\n"
             ":type: :class:`mathutils.Vector`");

static PyObject *StrokeAttribute_thickness_get(BPy_StrokeAttribute *self, void * /*closure*/)
{
  return Vector_CreatePyObject_cb(
      (PyObject *)self, 2, StrokeAttribute_mathutils_cb_index, MATHUTILS_SUBTYPE_THICKNESS);
}

static int StrokeAttribute_thickness_set(BPy_StrokeAttribute *self,
                                         PyObject *value,
                                         void * /*closure*/)
{
  float v[2];
  if (mathutils_array_parse(v, 2, 2, value, "value must be a 2-dimensional vector") == -1) {
    return -1;
  }
  self->sa->setThickness(v[0], v[1]);
  return 0;
}

PyDoc_STRVAR(StrokeAttribute_visible_doc,
             "The visibility flag. True if the StrokeVertex is visible.\n"
             "\n"
             ":type: bool");

static PyObject *StrokeAttribute_visible_get(BPy_StrokeAttribute *self, void * /*closure*/)
{
  return PyBool_from_bool(self->sa->isVisible());
}

static int StrokeAttribute_visible_set(BPy_StrokeAttribute *self,
                                       PyObject *value,
                                       void * /*closure*/)
{
  if (!PyBool_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be boolean");
    return -1;
  }
  self->sa->setVisible(bool_from_PyBool(value));
  return 0;
}

static PyGetSetDef BPy_StrokeAttribute_getseters[] = {
    {"alpha",
     (getter)StrokeAttribute_alpha_get,
     (setter)StrokeAttribute_alpha_set,
     StrokeAttribute_alpha_doc,
     nullptr},
    {"color",
     (getter)StrokeAttribute_color_get,
     (setter)StrokeAttribute_color_set,
     StrokeAttribute_color_doc,
     nullptr},
    {"thickness",
     (getter)StrokeAttribute_thickness_get,
     (setter)StrokeAttribute_thickness_set,
     StrokeAttribute_thickness_doc,
     nullptr},
    {"visible",
     (getter)StrokeAttribute_visible_get,
     (setter)StrokeAttribute_visible_set,
     StrokeAttribute_visible_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_StrokeAttribute type definition ------------------------------*/

PyTypeObject StrokeAttribute_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "StrokeAttribute",
    /*tp_basicsize*/ sizeof(BPy_StrokeAttribute),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)StrokeAttribute_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)StrokeAttribute_repr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ StrokeAttribute_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ BPy_StrokeAttribute_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_StrokeAttribute_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)StrokeAttribute_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ PyType_GenericNew,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
