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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Andrea Weikert (c) 2008 Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_dynstr.h"

#ifdef WIN32
#include <windows.h> /* need to include windows.h so _WIN32_IE is defined  */
#ifndef _WIN32_IE
#define _WIN32_IE 0x0400 /* minimal requirements for SHGetSpecialFolderPath on MINGW MSVC has this defined already */
#endif
#include <shlobj.h> /* for SHGetSpecialFolderPath, has to be done before BLI_winstuff because 'near' is disabled through BLI_windstuff */
#include "BLI_winstuff.h"
#endif

#include "fsmenu.h"  /* include ourselves */


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
int fsmenu_is_entry_a_separator(int idx)
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

void fsmenu_read_file(const char *filename)
{
	char line[256];
	FILE *fp;

	#ifdef WIN32
	/* Add the drive names to the listing */
	{
		__int64 tmp;
		char folder[256];
		char tmps[4];
		int i;
			
		tmp= GetLogicalDrives();
		
		for (i=2; i < 26; i++) {
			if ((tmp>>i) & 1) {
				tmps[0]='a'+i;
				tmps[1]=':';
				tmps[2]='\\';
				tmps[3]=0;
				
				fsmenu_insert_entry(tmps, 0, 0);
			}
		}

		/* Adding Desktop and My Documents */
		fsmenu_append_separator();

		SHGetSpecialFolderPath(0, folder, CSIDL_PERSONAL, 0);
		fsmenu_insert_entry(folder, 0, 0);
		SHGetSpecialFolderPath(0, folder, CSIDL_DESKTOPDIRECTORY, 0);
		fsmenu_insert_entry(folder, 0, 0);

		fsmenu_append_separator();
	}
#endif

	fp = fopen(filename, "w");
	if (!fp) return;

	while ( fgets ( line, 256, fp ) != NULL ) /* read a line */
	{
		int len = strlen(line);
		if (len>0) {
			if (line[len-1] == '\n') {
				line[len-1] = '\0';
			}
			fsmenu_insert_entry(line, 0, 1);
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



