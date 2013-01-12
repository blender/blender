/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/bpy_path.c
 *  \ingroup pythonintern
 *
 * This file defines '_bpy_path' module, Some 'C' funtionality used by 'bpy.path'
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "bpy_path.h"

#include "../generic/py_capi_utils.h"

/* #include "IMB_imbuf_types.h" */
extern const char *imb_ext_image[];
extern const char *imb_ext_movie[];
extern const char *imb_ext_audio[];

/*----------------------------MODULE INIT-------------------------*/
static struct PyModuleDef _bpy_path_module_def = {
	PyModuleDef_HEAD_INIT,
	"_bpy_path",  /* m_name */
	NULL,  /* m_doc */
	0,  /* m_size */
	NULL,  /* m_methods */
	NULL,  /* m_reload */
	NULL,  /* m_traverse */
	NULL,  /* m_clear */
	NULL,  /* m_free */
};

PyObject *BPyInit__bpy_path(void)
{
	PyObject *submodule;

	submodule = PyModule_Create(&_bpy_path_module_def);

	PyModule_AddObject(submodule, "extensions_image", PyC_FrozenSetFromStrings(imb_ext_image));
	PyModule_AddObject(submodule, "extensions_movie", PyC_FrozenSetFromStrings(imb_ext_movie));
	PyModule_AddObject(submodule, "extensions_audio", PyC_FrozenSetFromStrings(imb_ext_audio));

	return submodule;
}

