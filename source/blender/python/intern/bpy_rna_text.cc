/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file extends the text editor with C/Python API methods and attributes.
 */

#define PY_SSIZE_T_CLEAN

#include <Python.h>

#include "DNA_text_types.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"

#include "BKE_text.h"

#include "bpy_capi_utils.h"
#include "bpy_rna.h"
#include "bpy_rna_text.h"

/* -------------------------------------------------------------------- */
/** \name Data structures.
 * \{ */

/**
 * Struct representing a selection which is extracted from Python arguments.
 */
struct TextRegion {
  int curl;
  int curc;
  int sell;
  int selc;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Editor Get / Set region text API
 * \{ */

PyDoc_STRVAR(bpy_rna_region_as_string_doc,
             ".. method:: region_as_string(range=None)\n"
             "\n"
             "   :arg range: The region of text to be returned, "
             "defaulting to the selection when no range is passed.\n"
             "      Each int pair represents a line and column: "
             "((start_line, start_column), (end_line, end_column))\n"
             "      The values match Python's slicing logic "
             "(negative values count backwards from the end, the end value is not inclusive).\n"
             "   :type range: Two pairs of ints\n"
             "   :return: The specified region as a string.\n"
             "   :rtype: str.\n");
/* Receive a Python Tuple as parameter to represent the region range. */
static PyObject *bpy_rna_region_as_string(PyObject *self, PyObject *args, PyObject *kwds)
{
  BPy_StructRNA *pyrna = (BPy_StructRNA *)self;
  Text *text = static_cast<Text *>(pyrna->ptr.data);
  /* Parse the region range. */
  TextRegion region;

  static const char *_keywords[] = {"range", nullptr};
  static _PyArg_Parser _parser = {
      "|$"         /* Optional keyword only arguments. */
      "((ii)(ii))" /* `range` */
      ":region_as_string",
      _keywords,
      0,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kwds, &_parser, &region.curl, &region.curc, &region.sell, &region.selc))
  {
    return nullptr;
  }

  if (kwds && PyDict_GET_SIZE(kwds) > 0) {
    txt_sel_set(text, region.curl, region.curc, region.sell, region.selc);
  }

  /* Return an empty string if there is no selection. */
  if (!txt_has_sel(text)) {
    return PyUnicode_FromString("");
  }
  char *buf = txt_sel_to_buf(text, nullptr);
  PyObject *sel_text = PyUnicode_FromString(buf);
  MEM_freeN(buf);
  /* Return the selected text. */
  return sel_text;
}

PyMethodDef BPY_rna_region_as_string_method_def = {
    "region_as_string",
    (PyCFunction)bpy_rna_region_as_string,
    METH_VARARGS | METH_KEYWORDS,
    bpy_rna_region_as_string_doc,
};

PyDoc_STRVAR(bpy_rna_region_from_string_doc,
             ".. method:: region_from_string(body, range=None)\n"
             "\n"
             "   :arg body: The text to be inserted.\n"
             "   :type body: str\n"
             "   :arg range: The region of text to be returned, "
             "defaulting to the selection when no range is passed.\n"
             "      Each int pair represents a line and column: "
             "((start_line, start_column), (end_line, end_column))\n"
             "      The values match Python's slicing logic "
             "(negative values count backwards from the end, the end value is not inclusive).\n"
             "   :type range: Two pairs of ints\n");
static PyObject *bpy_rna_region_from_string(PyObject *self, PyObject *args, PyObject *kwds)
{
  BPy_StructRNA *pyrna = (BPy_StructRNA *)self;
  Text *text = static_cast<Text *>(pyrna->ptr.data);

  /* Parse the region range. */
  const char *buf;
  Py_ssize_t buf_len;
  TextRegion region;

  static const char *_keywords[] = {"", "range", nullptr};
  static _PyArg_Parser _parser = {
      "s#"         /* `buf` (positional). */
      "|$"         /* Optional keyword only arguments. */
      "((ii)(ii))" /* `range` */
      ":region_from_string",
      _keywords,
      0,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kwds,
                                        &_parser,
                                        &buf,
                                        &buf_len,
                                        &region.curl,
                                        &region.curc,
                                        &region.sell,
                                        &region.selc))
  {
    return nullptr;
  }

  if (kwds && PyDict_GET_SIZE(kwds) > 0) {
    txt_sel_set(text, region.curl, region.curc, region.sell, region.selc);
  }

  /* Set the selected text. */
  txt_insert_buf(text, buf, buf_len);
  /* Update the text editor. */
  WM_main_add_notifier(NC_TEXT | NA_EDITED, text);

  Py_RETURN_NONE;
}

PyMethodDef BPY_rna_region_from_string_method_def = {
    "region_from_string",
    (PyCFunction)bpy_rna_region_from_string,
    METH_VARARGS | METH_KEYWORDS,
    bpy_rna_region_from_string_doc,
};

/** \} */
