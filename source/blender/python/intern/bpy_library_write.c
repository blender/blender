/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup pythonintern
 *
 * Python API for writing a set of data-blocks into a file.
 * Useful for writing out asset-libraries, defines: `bpy.data.libraries.write(...)`.
 */

#include <Python.h>
#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_blendfile.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "BLO_writefile.h"

#include "RNA_types.h"

#include "bpy_capi_utils.h"
#include "bpy_library.h"
#include "bpy_rna.h"

#include "../generic/py_capi_utils.h"

PyDoc_STRVAR(
    bpy_lib_write_doc,
    ".. method:: write(filepath, datablocks, path_remap=False, fake_user=False, compress=False)\n"
    "\n"
    "   Write data-blocks into a blend file.\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      Indirectly referenced data-blocks will be expanded and written too.\n"
    "\n"
    "   :arg filepath: The path to write the blend-file.\n"
    "   :type filepath: string\n"
    "   :arg datablocks: set of data-blocks (:class:`bpy.types.ID` instances).\n"
    "   :type datablocks: set\n"
    "   :arg path_remap: Optionally remap paths when writing the file:\n"
    "\n"
    "      - ``NONE`` No path manipulation (default).\n"
    "      - ``RELATIVE`` Remap paths that are already relative to the new location.\n"
    "      - ``RELATIVE_ALL`` Remap all paths to be relative to the new location.\n"
    "      - ``ABSOLUTE`` Make all paths absolute on writing.\n"
    "\n"
    "   :type path_remap: string\n"
    "   :arg fake_user: When True, data-blocks will be written with fake-user flag enabled.\n"
    "   :type fake_user: bool\n"
    "   :arg compress: When True, write a compressed blend file.\n"
    "   :type compress: bool\n");
static PyObject *bpy_lib_write(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
  /* args */
  const char *filepath;
  char filepath_abs[FILE_MAX];
  PyObject *datablocks = NULL;

  const struct PyC_StringEnumItems path_remap_items[] = {
      {BLO_WRITE_PATH_REMAP_NONE, "NONE"},
      {BLO_WRITE_PATH_REMAP_RELATIVE, "RELATIVE"},
      {BLO_WRITE_PATH_REMAP_RELATIVE_ALL, "RELATIVE_ALL"},
      {BLO_WRITE_PATH_REMAP_ABSOLUTE, "ABSOLUTE"},
      {0, NULL},
  };
  struct PyC_StringEnum path_remap = {path_remap_items, BLO_WRITE_PATH_REMAP_NONE};

  bool use_fake_user = false, use_compress = false;

  static const char *_keywords[] = {
      "filepath",
      "datablocks",
      /* optional */
      "path_remap",
      "fake_user",
      "compress",
      NULL,
  };
  static _PyArg_Parser _parser = {"sO!|$O&O&O&:write", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kw,
                                        &_parser,
                                        &filepath,
                                        &PySet_Type,
                                        &datablocks,
                                        PyC_ParseStringEnum,
                                        &path_remap,
                                        PyC_ParseBool,
                                        &use_fake_user,
                                        PyC_ParseBool,
                                        &use_compress)) {
    return NULL;
  }

  Main *bmain_src = G_MAIN;
  int write_flags = 0;

  if (use_compress) {
    write_flags |= G_FILE_COMPRESS;
  }

  BLI_strncpy(filepath_abs, filepath, FILE_MAX);
  BLI_path_abs(filepath_abs, BKE_main_blendfile_path_from_global());

  BKE_blendfile_write_partial_begin(bmain_src);

  /* array of ID's and backup any data we modify */
  struct {
    ID *id;
    /* original values */
    short id_flag;
    short id_us;
  } * id_store_array, *id_store;
  int id_store_len = 0;

  PyObject *ret;

  /* collect all id data from the set and store in 'id_store_array' */
  {
    Py_ssize_t pos, hash;
    PyObject *key;

    id_store_array = MEM_mallocN(sizeof(*id_store_array) * PySet_Size(datablocks), __func__);
    id_store = id_store_array;

    pos = hash = 0;
    while (_PySet_NextEntry(datablocks, &pos, &key, &hash)) {

      if (!pyrna_id_FromPyObject(key, &id_store->id)) {
        PyErr_Format(PyExc_TypeError, "Expected an ID type, not %.200s", Py_TYPE(key)->tp_name);
        ret = NULL;
        goto finally;
      }
      else {
        id_store->id_flag = id_store->id->flag;
        id_store->id_us = id_store->id->us;

        if (use_fake_user) {
          id_store->id->flag |= LIB_FAKEUSER;
        }
        id_store->id->us = 1;

        BKE_blendfile_write_partial_tag_ID(id_store->id, true);

        id_store_len += 1;
        id_store++;
      }
    }
  }

  /* write blend */
  int retval = 0;
  ReportList reports;

  BKE_reports_init(&reports, RPT_STORE);
  retval = BKE_blendfile_write_partial(
      bmain_src, filepath_abs, write_flags, path_remap.value_found, &reports);

  /* cleanup state */
  BKE_blendfile_write_partial_end(bmain_src);

  if (retval) {
    BKE_reports_print(&reports, RPT_ERROR_ALL);
    BKE_reports_clear(&reports);
    ret = Py_None;
    Py_INCREF(ret);
  }
  else {
    if (BPy_reports_to_error(&reports, PyExc_IOError, true) == 0) {
      PyErr_SetString(PyExc_IOError, "Unknown error writing library data");
    }
    ret = NULL;
  }

finally:

  /* clear all flags for ID's added to the store (may run on error too) */
  id_store = id_store_array;

  for (int i = 0; i < id_store_len; id_store++, i++) {

    if (use_fake_user) {
      if ((id_store->id_flag & LIB_FAKEUSER) == 0) {
        id_store->id->flag &= ~LIB_FAKEUSER;
      }
    }

    id_store->id->us = id_store->id_us;

    BKE_blendfile_write_partial_tag_ID(id_store->id, false);
  }

  MEM_freeN(id_store_array);

  return ret;
}

PyMethodDef BPY_library_write_method_def = {
    "write",
    (PyCFunction)bpy_lib_write,
    METH_STATIC | METH_VARARGS | METH_KEYWORDS,
    bpy_lib_write_doc,
};
