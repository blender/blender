/**
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

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_dynstr.h"
#include "BIF_usiblender.h"


#include "BIF_fsmenu.h"  /* include ourselves */


/* FSMENU HANDLING */

	/* FSMenuEntry's without paths indicate seperators */
typedef struct _FSMenuEntry FSMenuEntry;
struct _FSMenuEntry {
	FSMenuEntry *next;

	char *path;
	short save;
};

static FSMenuEntry *fsmenu= 0;

int fsmenu_get_nentries(void)
{
	FSMenuEntry *fsme;
	int count= 0;

	for (fsme= fsmenu; fsme; fsme= fsme->next) 
		count++;

	return count;
}
int fsmenu_is_entry_a_seperator(int idx)
{
	FSMenuEntry *fsme;

	for (fsme= fsmenu; fsme && idx; fsme= fsme->next)
		idx--;

	return (fsme && !fsme->path)?1:0;
}
char *fsmenu_get_entry(int idx)
{
	FSMenuEntry *fsme;

	for (fsme= fsmenu; fsme && idx; fsme= fsme->next)
		idx--;

	return fsme?fsme->path:NULL;
}
char *fsmenu_build_menu(void)
{
	DynStr *ds= BLI_dynstr_new();
	FSMenuEntry *fsme;
	char *menustr;

	for (fsme= fsmenu; fsme; fsme= fsme->next) {
		if (!fsme->path) {
				/* clean consecutive seperators and ignore trailing ones */
			if (fsme->next) {
				if (fsme->next->path) {
					BLI_dynstr_append(ds, "%l|");
				} else {
					FSMenuEntry *next= fsme->next;
					fsme->next= next->next;
					MEM_freeN(next);
				}
			}
		} else {
			if (fsme->save) {
				BLI_dynstr_append(ds, "o ");
			} else {
				BLI_dynstr_append(ds, "  ");
			}
			BLI_dynstr_append(ds, fsme->path);
			if (fsme->next) BLI_dynstr_append(ds, "|");
		}
	}

	menustr= BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);
	return menustr;
}
static FSMenuEntry *fsmenu_get_last_separator(void) 
{
	FSMenuEntry *fsme, *lsep=NULL;

	for (fsme= fsmenu; fsme; fsme= fsme->next)
		if (!fsme->path)
			lsep= fsme;

	return lsep;
}

static FSMenuEntry *fsmenu_get_first_separator(void) 
{
	FSMenuEntry *fsme, *lsep=NULL;

	for (fsme= fsmenu; fsme; fsme= fsme->next)
		if (!fsme->path) {
			lsep= fsme;
			break;
		}

	return lsep;
}

void fsmenu_insert_entry(char *path, int sorted, short save)
{
	FSMenuEntry *prev;
	FSMenuEntry *fsme;

	if (save) {
		prev = fsmenu_get_first_separator();
	} else {
		prev = fsmenu_get_last_separator();
	}
	fsme= prev?prev->next:fsmenu;

	for (; fsme; prev= fsme, fsme= fsme->next) {
		if (fsme->path) {
			if (BLI_streq(path, fsme->path)) {
				return;
			} else if (sorted && strcmp(path, fsme->path)<0) {
				break;
			}
		} else {
			// if we're bookmarking this, file should come 
			// before the last separator, only automatically added
			// current dir go after the last sep.
			if (save) {
				break;
			}
		}
	}
	
	fsme= MEM_mallocN(sizeof(*fsme), "fsme");
	fsme->path= BLI_strdup(path);
	fsme->save = save;

	if (prev) {
		fsme->next= prev->next;
		prev->next= fsme;
	} else {
		fsme->next= fsmenu;
		fsmenu= fsme;
	}
}
void fsmenu_append_separator(void)
{
	if (fsmenu) {
		FSMenuEntry *fsme= fsmenu;

		while (fsme->next) fsme= fsme->next;

		fsme->next= MEM_mallocN(sizeof(*fsme), "fsme");
		fsme->next->next= NULL;
		fsme->next->path= NULL;
	}
}
void fsmenu_remove_entry(int idx)
{
	FSMenuEntry *prev= NULL, *fsme= fsmenu;

	for (fsme= fsmenu; fsme && idx; prev= fsme, fsme= fsme->next)		
		idx--;

	if (fsme) {
		/* you should only be able to remove entries that were 
		   not added by default, like windows drives.
		   also separators (where path == NULL) shouldn't be removed */
		if (fsme->save && fsme->path) {

			/* remove fsme from list */
			if (prev) {
				prev->next= fsme->next;
			} else {
				fsmenu= fsme->next;
			}
			/* free entry */
			MEM_freeN(fsme->path);
			MEM_freeN(fsme);
		}
	}
}

void fsmenu_write_file(const char *filename)
{
	FSMenuEntry *fsme= fsmenu;

	FILE *fp = fopen(filename, "w");
	if (!fp) return;

	for (fsme= fsmenu; fsme; fsme= fsme->next) {
		if (fsme->path && fsme->save) {
			fprintf(fp, "%s\n", fsme->path);
		}
	}
	fclose(fp);
}

void fsmenu_free(void)
{
	FSMenuEntry *fsme= fsmenu;

	while (fsme) {
		FSMenuEntry *n= fsme->next;

		if (fsme->path) MEM_freeN(fsme->path);
		MEM_freeN(fsme);

		fsme= n;
	}
}



