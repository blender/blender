/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "bpy_ui.h"
#include "bpy_util.h"
#include "bpy_rna.h" /* for rna buttons */
#include "bpy_operator.h" /* for setting button operator properties */

#include "WM_types.h" /* for WM_OP_INVOKE_DEFAULT & friends */

#include "BLI_dynstr.h"

#include "MEM_guardedalloc.h"
#include "BKE_global.h" /* evil G.* */
#include "BKE_context.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h" /* only for SpaceLink */
#include "UI_interface.h"
#include "WM_api.h"

/* Dummy Module, may want to include non RNA UI functions here, else it can be removed */

static struct PyMethodDef ui_methods[] = {
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef ui_module = {
	PyModuleDef_HEAD_INIT,
	"bpy.ui",
	"",
	-1,/* multiple "initialization" just copies the module dict. */
	ui_methods,
	NULL, NULL, NULL, NULL
};

PyObject *BPY_ui_module( void )
{
	PyObject *submodule;
	submodule= PyModule_Create(&ui_module);
	
	/* INCREF since its its assumed that all these functions return the
	 * module with a new ref like PyDict_New, since they are passed to
	  * PyModule_AddObject which steals a ref */
	Py_INCREF(submodule);
	
	return submodule;
}
