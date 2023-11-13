/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file defines a singleton py object accessed via 'bpy.utils.previews',
 * which exposes low-level API for custom previews/icons.
 * It is replaced in final API by an higher-level python wrapper, that handles previews by addon,
 * and automatically release them on deletion.
 */

#include <Python.h>
#include <structmember.h>

#include "BLI_utildefines.h"

#include "RNA_access.hh"
#include "RNA_prototypes.h"
#include "RNA_types.hh"

#include "BPY_extern.h"
#include "bpy_rna.h"
#include "bpy_utils_previews.h"

#include "../generic/py_capi_utils.h"

#include "MEM_guardedalloc.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_thumbs.h"

#include "BKE_preview_image.hh"

#include "DNA_ID.h"

#include "../generic/python_utildefines.h"

#define STR_SOURCE_TYPES "'IMAGE', 'MOVIE', 'BLEND', 'FONT'"

PyDoc_STRVAR(
    bpy_utils_previews_new_doc,
    ".. method:: new(name)\n"
    "\n"
    "   Generate a new empty preview.\n"
    "\n"
    "   :arg name: The name (unique id) identifying the preview.\n"
    "   :type name: string\n"
    "   :return: The Preview matching given name, or a new empty one.\n"
    "   :rtype: :class:`bpy.types.ImagePreview`\n"
    /* This is only true when accessed via 'bpy.utils.previews.ImagePreviewCollection.load',
     * however this is the public API, allow this minor difference to the internal version here. */
    "   :raises KeyError: if ``name`` already exists.");
static PyObject *bpy_utils_previews_new(PyObject * /*self*/, PyObject *args)
{
  char *name;
  PreviewImage *prv;

  if (!PyArg_ParseTuple(args, "s:new", &name)) {
    return nullptr;
  }

  prv = BKE_previewimg_cached_ensure(name);
  PointerRNA ptr = RNA_pointer_create(nullptr, &RNA_ImagePreview, prv);

  return pyrna_struct_CreatePyObject(&ptr);
}

PyDoc_STRVAR(
    bpy_utils_previews_load_doc,
    ".. method:: load(name, filepath, filetype, force_reload=False)\n"
    "\n"
    "   Generate a new preview from given file path.\n"
    "\n"
    "   :arg name: The name (unique id) identifying the preview.\n"
    "   :type name: string\n"
    "   :arg filepath: The file path to generate the preview from.\n"
    "   :type filepath: string or bytes\n"
    "   :arg filetype: The type of file, needed to generate the preview in [" STR_SOURCE_TYPES
    "].\n"
    "   :type filetype: string\n"
    "   :arg force_reload: If True, force running thumbnail manager even if preview already "
    "exists in cache.\n"
    "   :type force_reload: bool\n"
    "   :return: The Preview matching given name, or a new empty one.\n"
    "   :rtype: :class:`bpy.types.ImagePreview`\n"
    /* This is only true when accessed via 'bpy.utils.previews.ImagePreviewCollection.load',
     * however this is the public API, allow this minor difference to the internal version here. */
    "   :raises KeyError: if ``name`` already exists.");
static PyObject *bpy_utils_previews_load(PyObject * /*self*/, PyObject *args)
{
  char *name;
  PyC_UnicodeAsBytesAndSize_Data filepath_data = {nullptr};
  const PyC_StringEnumItems path_type_items[] = {
      {THB_SOURCE_IMAGE, "IMAGE"},
      {THB_SOURCE_MOVIE, "MOVIE"},
      {THB_SOURCE_BLEND, "BLEND"},
      {THB_SOURCE_FONT, "FONT"},
      {THB_SOURCE_OBJECT_IO, "OBJECT_IO"},
      {0, nullptr},
  };
  PyC_StringEnum path_type = {
      path_type_items,
      /* The default isn't used. */
      0,
  };
  int force_reload = false;

  if (!PyArg_ParseTuple(args,
                        "s"  /* `name` */
                        "O&" /* `filepath` */
                        "O&" /* `filetype` */
                        "|"  /* Optional arguments. */
                        "p"  /* `force_reload` */
                        ":load",
                        &name,
                        PyC_ParseUnicodeAsBytesAndSize,
                        &filepath_data,
                        PyC_ParseStringEnum,
                        &path_type,
                        &force_reload))
  {
    return nullptr;
  }

  PreviewImage *prv = BKE_previewimg_cached_thumbnail_read(
      name, filepath_data.value, path_type.value_found, force_reload);

  Py_XDECREF(filepath_data.value_coerce);

  PointerRNA ptr = RNA_pointer_create(nullptr, &RNA_ImagePreview, prv);
  return pyrna_struct_CreatePyObject(&ptr);
}

PyDoc_STRVAR(bpy_utils_previews_release_doc,
             ".. method:: release(name)\n"
             "\n"
             "   Release (free) a previously created preview.\n"
             "\n"
             "\n"
             "   :arg name: The name (unique id) identifying the preview.\n"
             "   :type name: string\n");
static PyObject *bpy_utils_previews_release(PyObject * /*self*/, PyObject *args)
{
  char *name;

  if (!PyArg_ParseTuple(args, "s:release", &name)) {
    return nullptr;
  }

  BKE_previewimg_cached_release(name);

  Py_RETURN_NONE;
}

static PyMethodDef bpy_utils_previews_methods[] = {
    /* Can't use METH_KEYWORDS alone, see http://bugs.python.org/issue11587 */
    {"new", (PyCFunction)bpy_utils_previews_new, METH_VARARGS, bpy_utils_previews_new_doc},
    {"load", (PyCFunction)bpy_utils_previews_load, METH_VARARGS, bpy_utils_previews_load_doc},
    {"release",
     (PyCFunction)bpy_utils_previews_release,
     METH_VARARGS,
     bpy_utils_previews_release_doc},
    {nullptr, nullptr, 0, nullptr},
};

PyDoc_STRVAR(
    bpy_utils_previews_doc,
    "This object contains basic static methods to handle cached (non-ID) previews in Blender\n"
    "(low-level API, not exposed to final users).");
static PyModuleDef bpy_utils_previews_module = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "bpy._utils_previews",
    /*m_doc*/ bpy_utils_previews_doc,
    /*m_size*/ 0,
    /*m_methods*/ bpy_utils_previews_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *BPY_utils_previews_module()
{
  PyObject *submodule;

  submodule = PyModule_Create(&bpy_utils_previews_module);

  return submodule;
}
