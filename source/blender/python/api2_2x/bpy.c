/* 
 * $Id: bpy.c 10550 2007-04-18 22:53:20Z campbellbarton $
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Michel Selten, Willian P. Germano, Joseph Gilbert,
 * Campbell Barton
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

/* for open, close in Blender_Load */

#include "BLI_blenlib.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"
#include "BKE_scene.h"
#include "BKE_main.h"

#include "DNA_scene_types.h"

#include "Types.h"
#include "Library.h"

#include "bpy.h"
#include "bpy_data.h"
#include "bpy_config.h"

/*****************************************************************************/
/* Global variables	 */
/*****************************************************************************/
PyObject *g_bpydict;

/*****************************************************************************/
/* Function:		initBlender		 */
/*****************************************************************************/

void m_bpy_init(void)
{
	PyObject *module;
	PyObject *dict;
	
	/* G.scene should only aver be NULL if blender is executed in 
	background mode, not loading a blend file and executing a python script eg.
	blender -P somescript.py -b
	The if below solves the segfaults that happen when python runs and
	G.scene is NULL */
	if(G.background && G.main->scene.first==NULL) {
		Scene *sce= add_scene("1");
		/*set_scene(sce);*/ /* causes a crash */
		G.scene= sce;
	}
	
	module = Py_InitModule3("bpy", NULL, "The main bpy module");

	types_InitAll();	/* set all our pytypes to &PyType_Type */

	dict = PyModule_GetDict(module);
	g_bpydict = dict;
	
	PyModule_AddObject( module, "config", Config_CreatePyObject() );
	PyDict_SetItemString( dict, "data", Data_Init());
	PyDict_SetItemString( dict, "libraries", Library_Init(  ) );
	
}
