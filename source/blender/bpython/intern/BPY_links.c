/** Helper functions to handle links between Object types,
 * Script links */

/*
 * $Id$
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Python.h"
#include "BPY_macros.h"
#include "b_interface.h"

/* GLOBALS */

PyObject *g_blenderdict;

/* PROTOS */
char *event_to_name(short event);
void set_scriptlinks(ID *id, short event);

#ifndef SHAREDMODULE
PyObject *DataBlock_fromData (void *data);


void set_scriptlinks(ID *id, short event)
{
	PyObject *link;

	if (!g_blenderdict) // not initialized yet; this can happen at first file load
	{
		return;
	}	
	if (GET_ID_TYPE(id) == ID_SCE) {
		Py_INCREF(Py_None);
		link = Py_None;
	} else {
		link = DataBlock_fromData(id);
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
	PyDict_SetItemString(g_blenderdict, "event", Py_BuildValue("s", event_to_name(event)));
}

/* this is just a hack-added function to release a script link reference.
 * The scriptlink concept will be redone later */

void release_scriptlinks(ID *id)
{
	PyObject *link;
	if (!g_blenderdict) return; // return if Blender module was not initialized
	link = PyDict_GetItemString(g_blenderdict, "link");
	Py_DECREF(link);
	Py_INCREF(Py_None);
	PyDict_SetItemString(g_blenderdict, "link", Py_None);
}

#endif
