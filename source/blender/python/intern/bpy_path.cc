/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file defines '_bpy_path' module, Some 'C' functionality used by 'bpy.path'
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "bpy_path.h"

#include "../generic/py_capi_utils.h"

/* #include "IMB_imbuf_types.h" */
extern "C" const char *imb_ext_image[];
extern "C" const char *imb_ext_movie[];
extern "C" const char *imb_ext_audio[];

/*----------------------------MODULE INIT-------------------------*/
static PyModuleDef _bpy_path_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "_bpy_path",
    /*m_doc*/ nullptr,
    /*m_size*/ 0,
    /*m_methods*/ nullptr,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *BPyInit__bpy_path()
{
  PyObject *submodule;

  submodule = PyModule_Create(&_bpy_path_module_def);

  PyModule_AddObject(submodule, "extensions_image", PyC_FrozenSetFromStrings(imb_ext_image));
  PyModule_AddObject(submodule, "extensions_movie", PyC_FrozenSetFromStrings(imb_ext_movie));
  PyModule_AddObject(submodule, "extensions_audio", PyC_FrozenSetFromStrings(imb_ext_audio));

  return submodule;
}
