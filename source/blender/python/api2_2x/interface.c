/* 
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Michel Selten
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <stdio.h>

#include <Python.h>

#include <BKE_global.h>
#include <BKE_main.h>
#include <DNA_ID.h>
#include <DNA_camera_types.h>
#include <DNA_lamp_types.h>
#include <DNA_material_types.h>
#include <DNA_object_types.h>
#include <DNA_scene_types.h>
#include <DNA_scriptlink_types.h>
#include <DNA_world_types.h>

#include "gen_utils.h"
#include "modules.h"

void initBlenderApi2_2x (void)
{
	printf ("initBlenderApi2_2x\n");
	g_blenderdict = NULL;
	initBlender ();
}

ScriptLink * setScriptLinks(ID *id, short event)
{
	ScriptLink  * scriptlink;
	PyObject    * link;
	Object      * object;
	int           obj_id;

	obj_id = MAKE_ID2 (id->name[0], id->name[1]);
	printf ("In setScriptLinks (id=%s, event=%d)\n",id->name, event);

	switch (obj_id)
	{
		case ID_OB:
			object = GetObjectByName (GetIdName (id));
			if (object == NULL)
			{
				return NULL;
			}
			link = ObjectCreatePyObject (object);
			scriptlink = &(object->scriptlink);
			break;
		case ID_LA:
			scriptlink = NULL;
			Py_INCREF(Py_None);
			link = Py_None;
			break;
		case ID_CA:
			scriptlink = NULL;
			Py_INCREF(Py_None);
			link = Py_None;
			break;
		case ID_MA:
			scriptlink = NULL;
			Py_INCREF(Py_None);
			link = Py_None;
			break;
		case ID_WO:
			scriptlink = NULL;
			Py_INCREF(Py_None);
			link = Py_None;
			break;
		case ID_SCE:
			scriptlink = NULL;
			Py_INCREF(Py_None);
			link = Py_None;
			break;
		default:
			Py_INCREF(Py_None);
			link = Py_None;
			return NULL;
	}

	if (scriptlink == NULL)
	{
		/* This is probably not an internal error anymore :)
TODO: Check this
		printf ("Internal error, unable to create PyBlock for script link\n");
		*/
		Py_INCREF(Py_False);
		PyDict_SetItemString(g_blenderdict, "bylink", Py_False);
		return NULL;
	}
	else
	{
		Py_INCREF(Py_True);
		PyDict_SetItemString(g_blenderdict, "bylink", Py_True);
	}

	PyDict_SetItemString(g_blenderdict, "link", link);
	PyDict_SetItemString(g_blenderdict, "event",
			Py_BuildValue("s", event_to_name(event)));

	return (scriptlink);
}
