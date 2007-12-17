/* 
 * $Id: EXPP_interface.c 7338 2006-04-30 16:22:31Z ianwill $
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
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

#include <Python.h>

#include "EXPP_interface.h" 
#include "BLI_blenlib.h"
#include "MEM_guardedalloc.h"
#include "BLI_linklist.h" /* linked list: LinkNode struct and functions */
#include "DNA_object_types.h"
#include "DNA_space_types.h" /* for FILE_MAXDIR, FILE_MAXFILE */
#include "Blender.h"

extern char bprogname[]; /* argv[0] from creator.c */

/* this makes sure BLI_gethome() returns a path with '.blender' appended
 * Besides, this function now either returns userhome/.blender (if it exists)
 * or blenderInstallDir/.blender/ otherwise (can also be cvs dir).
 * If append_scriptsdir is non NULL, "scripts/" is appended to the dir, to
 * get the path to the scripts folder ("release/scripts/" if cvs  dir).
 * Finally, if all else fails BLI_gethome() is returned
 * (or NULL if append_scriptdir != 0).
*/
char *bpy_gethome(int append_scriptsdir)
{
	static char homedir[FILE_MAXDIR];
	static char scriptsdir[FILE_MAXDIR];
	char tmpdir[FILE_MAXDIR];
	char bprogdir[FILE_MAXDIR];
	char *s;
	int i;

	if (append_scriptsdir) {
		if (scriptsdir[0] != '\0')
			return scriptsdir;
	}
	else if (homedir[0] != '\0')
		return homedir;

	/* BLI_gethome() can return NULL if env vars are not set */
	s = BLI_gethome();

	if( !s )  /* bail if no $HOME */
	{
		printf("$HOME is NOT set\n");
		return NULL;
	}

	if( strstr( s, ".blender" ) )
		PyOS_snprintf( homedir, FILE_MAXDIR, s );
	else
		BLI_make_file_string( "/", homedir, s, ".blender" );

	/* if userhome/.blender/ exists, return it */
	if( BLI_exists( homedir ) ) {
		if (append_scriptsdir) {
			BLI_make_file_string("/", scriptsdir, homedir, "scripts");
			if (BLI_exists (scriptsdir)) return scriptsdir;
		}
		else return homedir;
	}
	else homedir[0] = '\0';

	/* if either:
	 * no homedir was found or
	 * append_scriptsdir = 1 but there's no scripts/ inside homedir,
	 * use argv[0] (bprogname) to get .blender/ in
	 * Blender's installation dir */
	s = BLI_last_slash( bprogname );

	i = s - bprogname + 1;

	PyOS_snprintf( bprogdir, i, "%s", bprogname );

	/* using tmpdir to preserve homedir (if) found above:
	 * the ideal is to have a home dir with scripts dir inside
	 * it, but if that isn't available, it's possible to
	 * have a 'broken' home dir somewhere and a scripts dir in the
	 * cvs sources */
	BLI_make_file_string( "/", tmpdir, bprogdir, ".blender" );

	if (BLI_exists(tmpdir)) {
		if (append_scriptsdir) {
			BLI_make_file_string("/", scriptsdir, tmpdir, "scripts");
			if (BLI_exists(scriptsdir)) {
				PyOS_snprintf(homedir, FILE_MAXDIR, "%s", tmpdir);
				return scriptsdir;
			}
			else {
				homedir[0] = '\0';
				scriptsdir[0] = '\0';
			}
		}
		else return homedir;
	}

	/* last try for scripts dir: blender in cvs dir, scripts/ inside release/: */
	if (append_scriptsdir) {
		BLI_make_file_string("/", scriptsdir, bprogdir, "release/scripts");
		if (BLI_exists(scriptsdir)) return scriptsdir;
		else scriptsdir[0] = '\0';
	}

	return NULL;
}

/* PyDrivers */

/*
 * Pydrivers are Blender Ipo Drivers defined by Python expressions.
 * We need to tell DAG about objects used in these expressions, so we
 * eval each expression to collect the ob refs. in it.
 */

/* these are checked for example in Object.c: M_Object_Get (Object.Get())
 * to collect the refs. */
static int pydriver_running = 0;

int bpy_during_pydriver(void)
{
	return pydriver_running;
}

void bpy_pydriver_running(int state)
{
	pydriver_running = state;
}

/* Obj references are collected in this extern linked list: */
LinkNode *bpy_pydriver_oblist = NULL;

void bpy_pydriver_freeList(void)
{
	BLI_linklist_free(bpy_pydriver_oblist, NULL);
	bpy_pydriver_oblist = NULL;
}

void bpy_pydriver_appendToList(struct Object *ob)
{
	LinkNode *ln = bpy_pydriver_oblist;

	/* check that the expression is not referencing its owner object */

/* XXX COMMENTED OUT TO TEST IF WE REALLY NEED TO IMPOSE THIS RESTRICTION
	if (ln && ln->link) {
		if (ob == (Object *)ln->link) {
			PyErr_SetString(PyExc_AttributeError,
				"Python driver expression can't reference its own object");
			return;
		}
		else
			ln = ln->next;
	}
*/
	while (ln) { /* is ob already in list? ... */
		if (ob == (Object *)ln->link)
			break;
		ln = ln->next;
	}

	if (!ln) /* ... not yet, append it */
		BLI_linklist_append(&bpy_pydriver_oblist, (void *)ob);

	return;
}

/* Get an array from our linked list of objs referenced in the
 * current pydriver. The first node in the list is discarded,
 * since it is the actual pydriver owner, which shouldn't be
 * passed to the depsgraph (no self references). */
struct Object **bpy_pydriver_obArrayFromList(void)
{
	Object **obarray = NULL;
	
	if (bpy_pydriver_oblist) {
		int i;
		short len = BLI_linklist_length(bpy_pydriver_oblist);

		if (len > 1) {

			obarray = (Object **)MEM_mallocN(sizeof(Object*)*len,
					"pydriver array");

			if (obarray) {
				LinkNode *ln = bpy_pydriver_oblist;
				ln = ln->next; /* skip first ob, which is the pydriver owner */	

				for (i = 0; i < len-1; i++) {
					obarray[i] = (Object *)ln->link;
					ln = ln->next;
				}

				obarray[len-1] = NULL; /* NULL-terminated array */
			}
		}
		bpy_pydriver_freeList();
	}

	return obarray;
}

