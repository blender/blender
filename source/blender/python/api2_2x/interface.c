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

#include <BKE_global.h>
#include <BKE_main.h>
#include <DNA_ID.h>
#include <DNA_camera_types.h>
#include <DNA_lamp_types.h>
#include <DNA_material_types.h>
#include <DNA_object_types.h>
#include <DNA_scene_types.h>
#include <DNA_world_types.h>

#include "datablock.h"
#include "gen_utils.h"
#include "modules.h"

void initBlenderApi2_2x (void)
{
	printf ("initBlenderApi2_2x\n");
	initBlender ();
}

void setScriptLinks(ID *id, short event)
{
	PyObject *link;
	int       obj_id;

	printf ("In setScriptLinks (id=?, event=%d)\n", event);
	if (!g_blenderdict)
	{
		/* Not initialized yet. This can happen at first file load. */
		return;
	}

	obj_id = MAKE_ID2 (id->name[0], id->name[1]);
	if (obj_id == ID_SCE)
	{
		Py_INCREF(Py_None);
		link = Py_None;
	}
	else
	{
		link = Py_None;
		switch (obj_id)
		{
			case ID_OB:
				/* Create a new datablock of type: Object */
				/*
				link = ObjectCreatePyObject (G.main->object);
				*/
				break;
			case ID_ME:
				/* Create a new datablock of type: Mesh */
				break;
			case ID_LA:
				/* Create a new datablock of type: Lamp */
				break;
			case ID_CA:
				/* Create a new datablock of type: Camera */
				break;
			case ID_MA:
				/* Create a new datablock of type: Material */
				break;
			case ID_WO:
				/* Create a new datablock of type: World */
				break;
			case ID_IP:
				/* Create a new datablock of type: Ipo */
				break;
			case ID_IM:
				/* Create a new datablock of type: Image */
				break;
			case ID_TXT:
				/* Create a new datablock of type: Text */
				break;
			default:
				PythonReturnErrorObject (PyExc_SystemError,
							"Unable to create block for data");
				return;
		}
		/* link = DataBlockFromID(id); */
	}

	if (!link)
	{
		printf ("Internal error, unable to create PyBlock for script link\n");
		printf ("This is a bug; please report to bugs@blender.nl");
		Py_INCREF(Py_False);
		PyDict_SetItemString(g_blenderdict, "bylink", Py_False);
		return;
	} else {
		Py_INCREF(Py_True);
		PyDict_SetItemString(g_blenderdict, "bylink", Py_True);
	}

	PyDict_SetItemString(g_blenderdict, "link", link);
	PyDict_SetItemString(g_blenderdict, "event",
			Py_BuildValue("s", event_to_name(event)));
}
