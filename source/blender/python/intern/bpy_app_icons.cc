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

#include "BKE_icons.hh"

#include "../generic/py_capi_utils.hh"
#include "../generic/python_compat.hh" /* IWYU pragma: keep. */

#include "bpy_app_icons.hh"

/* We may want to load direct from file. */
PyDoc_STRVAR(
    /* Wrap. */
    bpy_app_icons_new_triangles_doc,
    ".. function:: new_triangles(range, coords, colors)\n"
    "\n"
    "   Create a new icon from triangle geometry.\n"
    "\n"
    "   :arg range: Pair of ints.\n"
    "   :type range: tuple[int, int]\n"
    "   :arg coords: Sequence of bytes (6 floats for one triangle) for (X, Y) coordinates.\n"
    "   :type coords: bytes\n"
    "   :arg colors: Sequence of bytes (12 for one triangles) for RGBA.\n"
    "   :type colors: bytes\n"
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

  const size_t items_num = size_t(tris_len) * 3;
  uchar(*coords)[2] = MEM_malloc_arrayN<uchar[2]>(items_num, __func__);
  uchar(*colors)[4] = MEM_malloc_arrayN<uchar[4]>(items_num, __func__);

  memcpy(coords, PyBytes_AS_STRING(py_coords), sizeof(*coords) * items_num);
  memcpy(colors, PyBytes_AS_STRING(py_colors), sizeof(*colors) * items_num);

  Icon_Geom *geom = MEM_mallocN<Icon_Geom>(__func__);
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
    "   :type filepath: str | bytes.\n"
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

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
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

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
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
