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
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

/* This is the main file responsible for having bpython scripts accessible
 * from Blender menus.  To know more, please start with its header file.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef WIN32
#include <dirent.h>
#else
#include "BLI_winstuff.h"
#include <io.h>
#include <direct.h>
#endif 

#include "BKE_utildefines.h"
#include "BLI_blenlib.h"
#include "MEM_guardedalloc.h"

#include <DNA_userdef_types.h> /* for U.pythondir */

#include "BPY_extern.h"
#include "BPY_menus.h"

#include <errno.h>

#define BPYMENU_DATAFILE ".Bpymenus"

static int bpymenu_group_atoi (char *str)
{
	if (!strcmp(str, "Import")) return PYMENU_IMPORT;
	else if (!strcmp(str, "Export")) return PYMENU_EXPORT;
	else if (!strcmp(str, "Misc")) return PYMENU_MISC;
	else return -1;
}

char *BPyMenu_group_itoa (short menugroup)
{
	switch (menugroup) {
		case PYMENU_IMPORT:
			return "Import";
			break;
		case PYMENU_EXPORT:
			return "Export";
			break;
		case PYMENU_MISC:
			return "Misc";
			break;
	}
	return NULL;
}

/* BPyMenu_CreatePupmenuStr:
 * build and return a meaninful string to be used by pupmenu().  The
 * string is made of a bpymenu name as title and its submenus as possible
 * choices for the user.
*/
char *BPyMenu_CreatePupmenuStr(BPyMenu *pym, short menugroup)
{
	BPySubMenu *pysm = pym->submenus;
	char str[1024], str2[100];
	int i = 0, rlen;

	if (!pym || !pysm) return NULL;

	str[0] = '\0';

	snprintf(str2, sizeof(str2), "%s: %s%%t",
		BPyMenu_group_itoa(menugroup), pym->name);
	strcat(str, str2);

	while (pysm) {
		snprintf(str2, sizeof(str2), "|%s%%x%d", pysm->name, i);
		rlen = sizeof(str) - strlen(str);
		strncat(str, str2, rlen);
		i++;
		pysm = pysm->next;
	}

	return BLI_strdup(str);
}

static void bpymenu_RemoveAllSubEntries (BPySubMenu *smenu)
{
	BPySubMenu *tmp;

	while (smenu) {
		tmp = smenu->next;
		if (smenu->name) MEM_freeN(smenu->name);
		if (smenu->arg)  MEM_freeN(smenu->arg);
		MEM_freeN(smenu);
		smenu = tmp;
	}
	return;
}

void BPyMenu_RemoveAllEntries (void)
{
	BPyMenu *tmp, *pymenu;
	int i;

	for (i = 0; i < PYMENU_TOTAL; i++) {
		pymenu = BPyMenuTable[i];
		while (pymenu) {
			tmp = pymenu->next;
			if (pymenu->name) MEM_freeN(pymenu->name);
			if (pymenu->filename) MEM_freeN(pymenu->filename);
			if (pymenu->tooltip) MEM_freeN(pymenu->tooltip);
			if (pymenu->submenus) bpymenu_RemoveAllSubEntries(pymenu->submenus);
			MEM_freeN(pymenu);
			pymenu = tmp;
		}
		BPyMenuTable[i] = NULL;
	}
	return;
}

static BPyMenu *bpymenu_FindEntry (short group, char *name)
{
	BPyMenu *pymenu;

	if ((group <0) || (group >= PYMENU_TOTAL)) return NULL;

	pymenu = BPyMenuTable[group];

	while (pymenu) {
		if (!strcmp(pymenu->name, name)) return pymenu;
		pymenu = pymenu->next;
	}

	return NULL;
}

static void bpymenu_set_tooltip (BPyMenu *pymenu, char *tip)
{
	if (!pymenu) return;

	if (pymenu->tooltip) MEM_freeN(pymenu->tooltip);
	pymenu->tooltip = BLI_strdup(tip);

	return;
}

/* bpymenu_AddEntry:
 * try to find an existing pymenu entry with the given type and name;
 * if found, update it with new info, otherwise create a new one and fill it.
 */
static BPyMenu *bpymenu_AddEntry (short group, char *name, char *fname, char *tooltip)
{
	BPyMenu *menu, **iter;

	if ((group < 0) || (group >= PYMENU_TOTAL)) return NULL;
	if (!name || !fname) return NULL;

	menu = bpymenu_FindEntry (group, name); /* already exists? */

	if (menu) {
		printf("\nWarning: script %s's menu name is already in use.\n", fname);
		printf("Edit the script and change its Name: '%s' field, please.\n", name);
		return NULL;
	}

	menu = MEM_mallocN(sizeof(BPyMenu), "pymenu");

	if (!menu) return NULL;

	menu->name = BLI_strdup(name);
	menu->filename = BLI_strdup(fname);
	menu->tooltip = NULL;
	if (tooltip) menu->tooltip = BLI_strdup(tooltip);
	menu->submenus = NULL;
	menu->next = NULL;

	iter = &BPyMenuTable[group];
	while (*iter) iter = &((*iter)->next);

	*iter = menu;
	
	return menu;
}

/* bpymenu_AddSubEntry:
 * add a submenu to an existing python menu.
 */
static int bpymenu_AddSubEntry (BPyMenu *mentry, char *name, char *arg)
{
	BPySubMenu *smenu, **iter;

	smenu = MEM_mallocN(sizeof(BPySubMenu), "pysubmenu");
	if (!smenu) return -1;

	smenu->name = BLI_strdup(name);
	smenu->arg = BLI_strdup(arg);
	smenu->next = NULL;

	if (!smenu->name || !smenu->arg) return -1;

	iter = &(mentry->submenus);
	while (*iter) iter = &((*iter)->next);

	*iter = smenu;

	return 0;
}

/* bpymenu_CreateFromFile:
 * parse the bpymenus data file where Python menu data is stored;
 * based on this data, create and fill the pymenu structs.
 */
static void bpymenu_CreateFromFile (void)
{
	FILE *fp;
	char line[255], w1[255], w2[255], tooltip[255], *tip;
	int parsing;
	short group;
	BPyMenu *pymenu = NULL;

	/* init global bpymenu table (it is a list of pointers to struct BPyMenus
	 * for each available cathegory: import, export, etc.) */
	for (group = 0; group < PYMENU_TOTAL; group++)
		BPyMenuTable[group] = NULL;

	/* let's try to open the file with bpymenu data */
	BLI_make_file_string (NULL, line, BLI_gethome(), BPYMENU_DATAFILE);

	fp = fopen(line, "rb");

	if (!fp) {
		printf("BPyMenus error: couldn't open config file %s.\n", line);
		return;
	}

	while (fgets(line, 255, fp)) { /* parsing file lines */

		switch (line[0]) { /* check first char */
			case '#': /* comment */
				continue;
				break;
			case '\n':
				continue;
				break;
			default:
				parsing = sscanf(line, "%s {\n", w1); /* menu group */
				break;
		}

		if (parsing == 1) { /* got menu group string */
			group = bpymenu_group_atoi(w1);
			if (group < 0) { /* invalid type */
				printf("BPyMenus error parsing config file: wrong group: %s.\n", w1);
				continue;
			}
		}
		else continue;

		while (1) {
			tip = NULL; /* optional tooltip */
			fgets(line, 255, fp);
			if (line[0] == '}') break;
			else if (line[0] == '\n') continue;
			else if (line[0] == '\'') { /* menu entry */
				parsing = sscanf(line, "'%[^']' %s '%[^']'\n", w1, w2, tooltip);

				if (parsing <= 0) { /* invalid line, get rid of it */
					fgets(line, 255, fp); /* add error report later */
				}
				else if (parsing == 3) tip = tooltip; /* has tooltip */

				pymenu = bpymenu_AddEntry(group, w1, w2, tip);
				if (!pymenu) {
					puts("BpyMenus error: couldn't create bpymenu entry.\n");
					return;
				}
			}
			else if (line[0] == '|' && line[1] == '_') { /* menu sub-entry */
				if (!pymenu) continue; /* no menu yet, skip this line */
				sscanf(line, "|_%[^:]: %s\n", w1, w2);
				bpymenu_AddSubEntry(pymenu, w1, w2);
			}
		}
	}
	return;
}

/* bpymenu_WriteDataFile:
 * writes the registered scripts info to the user's home dir, for faster
 * access when the scripts dir hasn't changed.
*/
static void bpymenu_WriteDataFile(void)
{
	BPyMenu *pymenu;
	BPySubMenu *smenu;
	FILE *fp;
	char fname[FILE_MAXDIR+FILE_MAXFILE];
	int i;

	BLI_make_file_string(NULL, fname, BLI_gethome(), BPYMENU_DATAFILE);

	fp = fopen(fname, "w");
	if (!fp) {
		printf("BPyMenus error: couldn't write %s file.", fname);
		return;
	}

	fprintf(fp, "# Blender: registered menu entries for bpython scripts\n");

	for (i = 0; i < PYMENU_TOTAL; i++) {
		pymenu = BPyMenuTable[i];
		if (!pymenu) continue;
		fprintf(fp, "\n%s {\n", BPyMenu_group_itoa(i));
		while (pymenu) {
			fprintf(fp, "'%s' %s", pymenu->name, pymenu->filename);
			if (pymenu->tooltip) fprintf(fp, " '%s'\n", pymenu->tooltip);
			else fprintf(fp, "\n");
			smenu = pymenu->submenus;
			while (smenu) {
				fprintf(fp, "|_%s: %s\n", smenu->name, smenu->arg);
				smenu = smenu->next;
			}
			pymenu = pymenu->next;
		}
		fprintf(fp, "}\n");
	}
}

/* BPyMenu_PrintAllEntries:
 * useful for debugging.
*/
void BPyMenu_PrintAllEntries(void)
{
	BPyMenu *pymenu;
	BPySubMenu *smenu;
	int i;

	printf("# Blender: registered menu entries for bpython scripts\n");

	for (i = 0; i < PYMENU_TOTAL; i++) {
		pymenu = BPyMenuTable[i];
		printf("\n%s {\n", BPyMenu_group_itoa(i));
		while (pymenu) {
			printf("'%s' %s", pymenu->name, pymenu->filename);
			if (pymenu->tooltip) printf(" '%s'\n", pymenu->tooltip);
			else printf("\n");
			smenu = pymenu->submenus;
			while (smenu) {
				printf("|_%s: %s\n", smenu->name, smenu->arg);
				smenu = smenu->next;
			}
			pymenu = pymenu->next;
		}
		printf("}\n");
	}
}

/* bpymenu_CreateFromDir:
 * this function scans the scripts dir looking for .py files with the
 * right header and menu info, using that to fill the bpymenu structs.
 * Speed is important.
*/
static void bpymenu_CreateFromDir (void)
{
	DIR *dir;
	FILE *fp;
	struct dirent *dir_entry;
	BPyMenu *pymenu;
	char *s, *fname, str[FILE_MAXFILE+FILE_MAXDIR];
	char line[100], w[100];
	char name[100], submenu[100], subarg[100], tooltip[100];
	int res = 0;

	dir = opendir(U.pythondir);

	if (!dir) {
		printf("BPyMenus error: couldn't open python dir %s.\n", U.pythondir);
		return;
	}

	/* init global bpymenu table (it is a list of pointers to struct BPyMenus
	 * for each available group: import, export, etc.) */
	for (res = 0; res < PYMENU_TOTAL; res++)
		BPyMenuTable[res] = NULL;

/* we scan the dir for filenames ending with .py and starting with the
 * right 'magic number': '#!BPY'.  All others are ignored. */

	while ((dir_entry = readdir(dir)) != NULL) {
		fname = dir_entry->d_name;
		/* ignore anything starting with a dot */
		if (fname[0] == '.') continue; /* like . and .. */

		/* also skip filenames whose extension isn't '.py' */
		s = strstr(fname, ".py");
		if (!s || *(s+3) != '\0') continue;

		BLI_make_file_string(NULL, str, U.pythondir, fname);

		fp = fopen(str, "rb");

		if (!fp) {
			printf("BPyMenus error: couldn't open %s.\n", str);
			return;
		}

		/* finally, look for the start string '#!BPY', with
		 * or w/o white space(s) between #! and BPY */
		fgets(line, 100, fp);
		if (line[0] != '#' || line[1] != '!') goto discard;

		if (!strstr (line, "BPY")) goto discard;

		/* file passed the tests, look for the three double-quotes */
		while (fgets(line, 100, fp)) {
			if (line[0] == '"' && line[1] == '"' && line[2] == '"') {
				res = 1; /* found */
				break;
			}
		}

		if (!res) goto discard;

		/* Now we're ready to get the registration info.  A little more structure
		 * was imposed to their format, for speed. The registration
		 * lines must appear between the first pair of triple double-quotes and
		 * follow this order (the single-quotes are part of the format):
		 * 
		 * Name: 'script name for the menu'
		 * Group: 'group name' (defines menu)
		 * Submenu: 'submenu name' related_1word_arg
		 * Tooltip: 'tooltip for the menu'
		 *
		 * notes:
		 * - there may be more than one submenu line, or none:
		 * submenus and the tooltip are optional;
		 * - only the first letter of each token is checked, both lower
		 * and upper cases, so that's all that matters for recognition:
		 * n 'script name' is enough for the name line, for example. */ 

		/* first the name: */
		res = fscanf(fp, "%[^']'%[^'\r\n]'\n", w, name);
		if ((res != 2) || (w[0] != 'n' && w[0] != 'N')) {
			printf("BPyMenus error: wrong 'name' line in %s.\n", str);
			printf("%s | %s\n", w, name);
			goto discard;
		}

		line[0] = '\0'; /* used as group for this part */

		/* the group: */
		res = fscanf(fp, "%[^']'%[^'\r\n]'\n", w, line);
		if ((res != 2) || (w[0] != 'g' && w[0] != 'G')) {
			printf("BPyMenus error: wrong 'group' line in %s.\n", str);
			printf("'%s' | '%s'\n", w, name);
			goto discard;
		}

		res = bpymenu_group_atoi(line);
		if (res < 0) {
			printf("BPyMenus error: unknown 'group' %s in %s.\n", line, str);
			goto discard;
		}

		pymenu = bpymenu_AddEntry(res, name, fname, NULL);
		if (!pymenu) {
			printf("BPyMenus error: couldn't create entry for %s.\n", str);
			fclose(fp);
			closedir(dir);
			return;
		}

		/* the (optional) submenu(s): */
		while (fgets (line, 100, fp)) {
			res = sscanf(line, "%[^']'%[^'\r\n]'%s\n", w, submenu, subarg);
			if ((res != 3) || (w[0] != 's' && w[0] != 'S')) break;
			bpymenu_AddSubEntry(pymenu, submenu, subarg); 
		}	

		/* the (optional) tooltip: */
		res = sscanf(line, "%[^']'%[^'\r\n]'\n", w, tooltip);
		if ((res == 2) && (w[0] == 't' || w[0] == 'T')) {
			bpymenu_set_tooltip (pymenu, tooltip);
		}

discard:
		fclose (fp);
		continue;
	}

	closedir(dir);
	return;
}

/* BPyMenu_Init:
 * import the bpython menus data to Blender, either from:
 * - the BPYMENU_DATAFILE file (~/.Bpymenus) or
 * - the scripts dir, case it's newer than the datafile (then update the file).
 * then fill the bpymenu table with this data.
*/
void BPyMenu_Init(void)
{
	char fname[FILE_MAXDIR+FILE_MAXFILE];
	struct stat st;
	time_t tdir, tfile;
	int result = 0;

	result = stat(U.pythondir, &st);

	if (result == -1) {
		/*
		printf ("\nScripts dir: %s\nError: %s\n", U.pythondir, strerror(errno));
		printf ("Please go to 'Info window -> File Paths tab' and set a valid "
						"path for\nthe Blender Python scripts dir.\n");
		*/
		return;
	}

	if (!S_ISDIR(st.st_mode)) {
		/*
		printf ("\nScripts dir: %s is not a directory!", U.pythondir);
		printf ("Please go to 'Info window -> File Paths tab' and set a valid "
						"path for\nthe Blender Python scripts dir.\n");
		*/
		return;
	}

	printf("Registering scripts in Blender menus:\n");

	tdir = st.st_mtime;

	BLI_make_file_string(NULL, fname, BLI_gethome(), BPYMENU_DATAFILE);

	result = stat(fname, &st);
	if ((result == -1) || !S_ISREG(st.st_mode)) tfile = 0;
	else tfile = st.st_mtime;

	/* comparing dates */

	if (tdir > tfile) { /* if so, dir is newer */
		printf("Getting menu data from dir: %s\n", U.pythondir);
		bpymenu_CreateFromDir();
		bpymenu_WriteDataFile(); /* recreate the file */
		return;
	}
	else { /* file is newer */
		printf("Getting menu data from file: %s\n", fname);
		bpymenu_CreateFromFile();
	}

	return;
}


