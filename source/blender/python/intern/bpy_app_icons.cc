/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * Runtime defined icons.
 */

#include <Python.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BKE_icons.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_compat.h"

#include "bpy_app_icons.h"

/* We may want to load direct from file. */
PyDoc_STRVAR(
    /* Wrap. */
    bpy_app_icons_new_triangles_doc,
    ".. function:: new_triangles(range, coords, colors)\n"
    "\n"
    "   Create a new icon from triangle geometry.\n"
    "\n"
    "   :arg range: Pair of ints.\n"
    "   :type range: tuple.\n"
    "   :arg coords: Sequence of bytes (6 floats for one triangle) for (X, Y) coordinates.\n"
    "   :type coords: byte sequence.\n"
    "   :arg colors: Sequence of ints (12 for one triangles) for RGBA.\n"
    "   :type colors: byte sequence.\n"
    "   :return: Unique icon value (pass to interface ``icon_value`` argument).\n"
    "   :rtype: int\n");
static PyObject *bpy_app_icons_new_triangles(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  /* bytes */
  uchar coords_range[2];
  PyObject *py_coords, *py_colors;

  static const char *_keywords[] = {"range", "coords", "colors", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "(BB)" /* `range` */
      "S"    /* `coords` */
      "S"    /* `colors` */
      ":new_triangles",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kw, &_parser, &coords_range[0], &coords_range[1], &py_coords, &py_colors))
  {
    return nullptr;
  }

  const int coords_len = PyBytes_GET_SIZE(py_coords);
  const int tris_len = coords_len / 6;
  if (tris_len * 6 != coords_len) {
    PyErr_SetString(PyExc_ValueError, "coords must be multiple of 6");
    return nullptr;
  }
  if (PyBytes_GET_SIZE(py_colors) != 2 * coords_len) {
    PyErr_SetString(PyExc_ValueError, "colors must be twice size of coords");
    return nullptr;
  }

  const int coords_size = sizeof(uchar[2]) * tris_len * 3;
  const int colors_size = sizeof(uchar[4]) * tris_len * 3;
  uchar(*coords)[2] = static_cast<uchar(*)[2]>(MEM_mallocN(coords_size, __func__));
  uchar(*colors)[4] = static_cast<uchar(*)[4]>(MEM_mallocN(colors_size, __func__));

  memcpy(coords, PyBytes_AS_STRING(py_coords), coords_size);
  memcpy(colors, PyBytes_AS_STRING(py_colors), colors_size);

  Icon_Geom *geom = static_cast<Icon_Geom *>(MEM_mallocN(sizeof(*geom), __func__));
  geom->coords_len = tris_len;
  geom->coords_range[0] = coords_range[0];
  geom->coords_range[1] = coords_range[1];
  geom->coords = coords;
  geom->colors = colors;
  geom->icon_id = 0;
  const int icon_id = BKE_icon_geom_ensure(geom);
  return PyLong_FromLong(icon_id);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_app_icons_new_triangles_from_file_doc,
    ".. function:: new_triangles_from_file(filepath)\n"
    "\n"
    "   Create a new icon from triangle geometry.\n"
    "\n"
    "   :arg filepath: File path.\n"
    "   :type filepath: string or bytes.\n"
    "   :return: Unique icon value (pass to interface ``icon_value`` argument).\n"
    "   :rtype: int\n");
static PyObject *bpy_app_icons_new_triangles_from_file(PyObject * /*self*/,
                                                       PyObject *args,
                                                       PyObject *kw)
{
  PyC_UnicodeAsBytesAndSize_Data filepath_data = {nullptr};

  static const char *_keywords[] = {"filepath", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "O&" /* `filepath` */
      ":new_triangles_from_file",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kw, &_parser, PyC_ParseUnicodeAsBytesAndSize, &filepath_data))
  {
    return nullptr;
  }

  Icon_Geom *geom = BKE_icon_geom_from_file(filepath_data.value);
  Py_XDECREF(filepath_data.value_coerce);

  if (geom == nullptr) {
    PyErr_SetString(PyExc_ValueError, "Unable to load from file");
    return nullptr;
  }
  const int icon_id = BKE_icon_geom_ensure(geom);
  return PyLong_FromLong(icon_id);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_app_icons_release_doc,
    ".. function:: release(icon_id)\n"
    "\n"
    "   Release the icon.\n");
static PyObject *bpy_app_icons_release(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  int icon_id;
  static const char *_keywords[] = {"icon_id", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "i" /* `icon_id` */
      ":release",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kw, &_parser, &icon_id)) {
    return nullptr;
  }

  if (!BKE_icon_delete_unmanaged(icon_id)) {
    PyErr_SetString(PyExc_ValueError, "invalid icon_id");
    return nullptr;
  }
  Py_RETURN_NONE;
}

#if (defined(__GNUC__) && !defined(__clang__))
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

static PyMethodDef M_AppIcons_methods[] = {
    {"new_triangles",
     (PyCFunction)bpy_app_icons_new_triangles,
     METH_VARARGS | METH_KEYWORDS,
     bpy_app_icons_new_triangles_doc},
    {"new_triangles_from_file",
     (PyCFunction)bpy_app_icons_new_triangles_from_file,
     METH_VARARGS | METH_KEYWORDS,
     bpy_app_icons_new_triangles_from_file_doc},
    {"release",
     (PyCFunction)bpy_app_icons_release,
     METH_VARARGS | METH_KEYWORDS,
     bpy_app_icons_release_doc},
    {nullptr, nullptr, 0, nullptr},
};

#if (defined(__GNUC__) && !defined(__clang__))
#  pragma GCC diagnostic pop
#endif

static PyModuleDef M_AppIcons_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "bpy.app.icons",
    /*m_doc*/ nullptr,
    /*m_size*/ 0,
    /*m_methods*/ M_AppIcons_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *BPY_app_icons_module()
{
  PyObject *sys_modules = PyImport_GetModuleDict();

  PyObject *mod = PyModule_Create(&M_AppIcons_module_def);

  PyDict_SetItem(sys_modules, PyModule_GetNameObject(mod), mod);

  return mod;
}
