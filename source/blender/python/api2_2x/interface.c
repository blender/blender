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

#include <DNA_ID.h>

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
		link = DataBlockFromID(id);
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
