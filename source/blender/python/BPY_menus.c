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
#include <Python.h>

#ifndef WIN32
#include <dirent.h>
#else
#include "BLI_winstuff.h"
#include <io.h>
#include <direct.h>
#endif 

#include "BKE_global.h"
#include "BKE_utildefines.h"
#include "BLI_blenlib.h"
#include "MEM_guardedalloc.h"

#include <DNA_userdef_types.h> /* for U.pythondir */

#include "BPY_extern.h"
#include "BPY_menus.h"

#include <errno.h>

#define BPYMENU_DATAFILE "Bpymenus"

static int DEBUG;

/* BPyMenuTable holds all registered pymenus, as linked lists for each menu
 * where they can appear (see PYMENUHOOKS enum in BPY_menus.h).
*/
BPyMenu *BPyMenuTable[PYMENU_TOTAL];

/* we can't be sure if BLI_gethome() returned a path
 * with '.blender' appended or not.  Besides, this function now
 * either returns userhome/.blender (if it exists) or
 * blenderInstallDir/.blender/ otherwise */
char *bpymenu_gethome()
{
	static char homedir[FILE_MAXDIR];
	char bprogdir[FILE_MAXDIR];
	char *s;
	int i;

	if (homedir[0] != '\0') return homedir; /* no need to search twice */

	s = BLI_gethome();

	if (strstr(s, ".blender")) PyOS_snprintf(homedir, FILE_MAXDIR, s);
	else BLI_make_file_string ("/", homedir, s, ".blender/");

	/* if userhome/.blender/ exists, return it */
	if (BLI_exists(homedir)) return homedir;

	/* otherwise, use argv[0] (bprogname) to get .blender/ in
	 * Blender's installation dir */
	s = BLI_last_slash(bprogname);

	i = s - bprogname + 1;

	PyOS_snprintf(bprogdir, i, bprogname);
	BLI_make_file_string ("/", homedir, bprogdir, ".blender/");

	return homedir;
}

static int bpymenu_group_atoi (char *str)
{
	if (!strcmp(str, "Import")) return PYMENU_IMPORT;
	else if (!strcmp(str, "Export")) return PYMENU_EXPORT;
	/* "Misc" or an inexistent group name: use misc */
	else return PYMENU_MISC;
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

	PyOS_snprintf(str2, sizeof(str2), "%s: %s%%t",
		BPyMenu_group_itoa(menugroup), pym->name);
	strcat(str, str2);

	while (pysm) {
		PyOS_snprintf(str2, sizeof(str2), "|%s%%x%d", pysm->name, i);
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

/* BPyMenu_GetEntry:
 * given a group and a position, return the entry in that position from
 * that group.
*/ 
BPyMenu *BPyMenu_GetEntry (short group, short pos)
{
	BPyMenu *pym = NULL;

	if ((group < 0) || (group >= PYMENU_TOTAL)) return NULL;

	pym = BPyMenuTable[group];

	while (pos--) {
		if (pym) pym = pym->next;
		else break;
	}

	return pym; /* found entry or NULL */
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
static BPyMenu *bpymenu_AddEntry (short group, short version, char *name,
	char *fname, int whichdir, char *tooltip)
{
	BPyMenu *menu, *next = NULL, **iter;
	int nameclash = 0;

	if ((group < 0) || (group >= PYMENU_TOTAL)) return NULL;
	if (!name || !fname) return NULL;

	menu = bpymenu_FindEntry (group, name); /* already exists? */

	/* if a menu with this name already exists in the same group:
	 * - if one script is in the default dir and the other in U.pythondir,
	 *   accept and let the new one override the other.
	 * - otherwise, report the error and return NULL. */
	if (menu) {
		if (menu->dir < whichdir) { /* new one is in U.pythondir */
			nameclash = 1;
			if (menu->name) MEM_freeN(menu->name);
			if (menu->filename) MEM_freeN(menu->filename);
			if (menu->tooltip) MEM_freeN(menu->tooltip);
			if (menu->submenus) bpymenu_RemoveAllSubEntries(menu->submenus);
			next = menu->next;
		}
		else { /* they are in the same dir */
			if (DEBUG) {
				printf("\nWarning: script %s's menu name is already in use.\n", fname);
				printf ("Edit the script and change its Name: '%s' field, please.\n"
							"Note: if you really want two scripts in the same menu with\n"
							"the same name, keep one in the default dir and the other in\n"
							"the user defined dir, where it will take precedence.\n", name);
			}
			return NULL;
		}
	}
	else menu = MEM_mallocN(sizeof(BPyMenu), "pymenu");

	if (!menu) return NULL;

	menu->name = BLI_strdup(name);
	menu->version = version;
	menu->filename = BLI_strdup(fname);
	menu->tooltip = NULL;
	if (tooltip) menu->tooltip = BLI_strdup(tooltip);
	menu->dir = whichdir;
	menu->submenus = NULL;
	menu->next = next; /* non-NULL if menu already existed */

	if (nameclash) return menu; /* no need to place it, it's already at the list*/

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
static int bpymenu_CreateFromFile (void)
{
	FILE *fp;
	char line[255], w1[255], w2[255], tooltip[255], *tip;
	int parsing, version, whichdir;
	short group;
	BPyMenu *pymenu = NULL;

	/* init global bpymenu table (it is a list of pointers to struct BPyMenus
	 * for each available cathegory: import, export, etc.) */
	for (group = 0; group < PYMENU_TOTAL; group++)
		BPyMenuTable[group] = NULL;

	/* let's try to open the file with bpymenu data */
	BLI_make_file_string ("/", line, bpymenu_gethome(), BPYMENU_DATAFILE);

	fp = fopen(line, "rb");

	if (!fp) {
		if (DEBUG) printf("BPyMenus error: couldn't open config file %s.\n", line);
		return -1;
	}

	fgets(line, 255, fp); /* header */

	/* check if the U.pythondir we saved at the file is different from the
	 * current one.  If so, return to force updating from dirs */
	w1[0] = '\0';
	fscanf(fp, "# User defined scripts dir: %[^\n]\n", w1);
	if (w1) {
		if (strcmp(w1, U.pythondir) != 0) return -1;
		w1[0] = '\0';
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
			if (group < 0 && DEBUG) { /* invalid type */
				printf("BPyMenus error parsing config file: wrong group: %s, "
					"will use 'Misc'.\n", w1);
			}
		}
		else continue;

		while (1) {
			tip = NULL; /* optional tooltip */
			fgets(line, 255, fp);
			if (line[0] == '}') break;
			else if (line[0] == '\n') continue;
			else if (line[0] == '\'') { /* menu entry */
				parsing = sscanf(line, "'%[^']' %d %s %d '%[^']'\n", w1, &version, w2, &whichdir, tooltip);

				if (parsing <= 0) { /* invalid line, get rid of it */
					fgets(line, 255, fp);
				}
				else if (parsing == 5) tip = tooltip; /* has tooltip */

				pymenu = bpymenu_AddEntry(group, (short)version, w1, w2, whichdir, tip);
				if (!pymenu) {
					puts("BPyMenus error: couldn't create bpymenu entry.\n");
					fclose(fp);
					return -1;
				}
			}
			else if (line[0] == '|' && line[1] == '_') { /* menu sub-entry */
				if (!pymenu) continue; /* no menu yet, skip this line */
				sscanf(line, "|_%[^:]: %s\n", w1, w2);
				bpymenu_AddSubEntry(pymenu, w1, w2);
			}
		}
	}

	fclose(fp);
	return 0;
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

	BLI_make_file_string("/", fname, bpymenu_gethome(), BPYMENU_DATAFILE);

	fp = fopen(fname, "w");
	if (!fp) {
		if (DEBUG) printf("BPyMenus error: couldn't write %s file.", fname);
		return;
	}

	fprintf(fp, "# Blender: registered menu entries for bpython scripts\n");

	if (U.pythondir[0] != '\0')
		fprintf(fp, "# User defined scripts dir: %s\n", U.pythondir);

	for (i = 0; i < PYMENU_TOTAL; i++) {
		pymenu = BPyMenuTable[i];
		if (!pymenu) continue;
		fprintf(fp, "\n%s {\n", BPyMenu_group_itoa(i));
		while (pymenu) {
			fprintf(fp,"'%s' %d %s %d", pymenu->name, pymenu->version, pymenu->filename, pymenu->dir);
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

	fclose(fp);
	return;
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
			printf("'%s' %d %s %d", pymenu->name, pymenu->version, pymenu->filename, pymenu->dir);
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

/* bpymenu_GetDataFromDir:
 * this function scans the scripts dir looking for .py files with the
 * right header and menu info, using that to fill the bpymenu structs.
 * whichdir defines if the script is in the default scripts dir or the
 * user defined one (U.pythondir: whichdir == 1).
 * Speed is important.
*/
static int bpymenu_CreateFromDir (char *dirname, int whichdir)
{
	DIR *dir;
	FILE *fp;
	struct stat st;
	struct dirent *dir_entry;
	BPyMenu *pymenu;
	char *s, *fname, str[FILE_MAXFILE+FILE_MAXDIR];
	char line[100], w[100];
	char name[100], submenu[100], subarg[100], tooltip[100];
	int res = 0, version = 0;

	dir = opendir(dirname);

	if (!dir) return -1;

/* we scan the dir for filenames ending with .py and starting with the
 * right 'magic number': '#!BPY'.  All others are ignored. */

	while ((dir_entry = readdir(dir)) != NULL) {
		fname = dir_entry->d_name;
		/* ignore anything starting with a dot */
		if (fname[0] == '.') continue; /* like . and .. */

		/* also skip filenames whose extension isn't '.py' */
		s = strstr(fname, ".py");
		if (!s || *(s+3) != '\0') continue;

		BLI_make_file_string("/", str, dirname, fname);

		/* paranoia: check if this is really a file and not a disguised dir */
		if ((stat(str, &st) == -1) || !S_ISREG(st.st_mode)) continue;

		fp = fopen(str, "rb");

		if (!fp) {
			if (DEBUG) printf("BPyMenus error: couldn't open %s.\n", str);
			continue;
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
		 * Blender: <short int> (minimal Blender version)
		 * Submenu: 'submenu name' related_1word_arg
		 * Tooltip: 'tooltip for the menu'
		 *
		 * notes:
		 * - there may be more than one submenu line, or none:
		 * submenus and the tooltip are optional;
		 * - the Blender version is the same number reported by
		 * Blender.Get('version') in BPython or G.version in C;
		 * - only the first letter of each token is checked, both lower
		 * and upper cases, so that's all that matters for recognition:
		 * n 'script name' is enough for the name line, for example. */ 

		/* first the name: */
		res = fscanf(fp, "%[^']'%[^'\r\n]'\n", w, name);
		if ((res != 2) || (w[0] != 'n' && w[0] != 'N')) {
			if (DEBUG) printf("BPyMenus error: wrong 'name' line in %s.\n", str);
			goto discard;
		}

		line[0] = '\0'; /* used as group for this part */

		/* minimal Blender version: */
		res = fscanf(fp, "%s %d\n", w, &version);
		if ((res != 2) || (w[0] != 'b' && w[0] != 'B')) {
			if (DEBUG) printf("BPyMenus error: wrong 'blender' line in %s.\n", str);
			goto discard;
		}

		/* the group: */
		res = fscanf(fp, "%[^']'%[^'\r\n]'\n", w, line);
		if ((res != 2) || (w[0] != 'g' && w[0] != 'G')) {
			if (DEBUG) printf("BPyMenus error: wrong 'group' line in %s.\n", str);
			goto discard;
		}

		res = bpymenu_group_atoi(line);
		if (res < 0) {
			if (DEBUG) printf("BPyMenus error: unknown 'group' %s in %s.\n", line, str);
			goto discard;
		}

		pymenu = bpymenu_AddEntry(res, (short)version, name, fname, whichdir, NULL);
		if (!pymenu) {
			if (DEBUG) printf("BPyMenus error: couldn't create entry for %s.\n", str);
			fclose(fp);
			closedir(dir);
			return -2;
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
	return 0;
}

static int bpymenu_GetStatMTime(char *name, int is_file, time_t* mtime)
{
	struct stat st;
	int result;

	result = stat(name, &st);

	if (result == -1) return -1;

	if (is_file) { if (!S_ISREG(st.st_mode)) return -2;	}
	else if (!S_ISDIR(st.st_mode)) return -2;

	*mtime = st.st_mtime;

	return 0;
}

/* BPyMenu_Init:
 * import the bpython menus data to Blender, either from:
 * - the BPYMENU_DATAFILE file (?/.blender/Bpymenus) or
 * - the scripts dir(s), case newer than the datafile (then update the file).
 * then fill the bpymenu table with this data.
 * if param usedir != 0, then the data is recreated from the dir(s) anyway.
*/
int BPyMenu_Init(int usedir)
{
	char fname[FILE_MAXDIR+FILE_MAXFILE];
	char dirname[FILE_MAXDIR];
	char *upydir = U.pythondir;
	time_t tdir1, tdir2, tfile;
	int res1, res2, resf = 0;

	DEBUG = G.f & G_DEBUG; /* is Blender in debug mode (started with -d) ? */

	/* init global bpymenu table (it is a list of pointers to struct BPyMenus
	 * for each available group: import, export, etc.) */
	for (res1 = 0; res1 < PYMENU_TOTAL; res1++)
		BPyMenuTable[res1] = NULL;

	if (U.pythondir[0] == '\0') upydir = NULL;

	BLI_make_file_string ("/", dirname, bpymenu_gethome(), "scripts/");

	res1 = bpymenu_GetStatMTime(dirname, 0, &tdir1);

	if (res1 < 0) {
		tdir1 = 0;
		if (DEBUG) {
			printf ("\nDefault scripts dir: %s:\n%s\n", dirname, strerror(errno));
			if (upydir)
				printf("Getting scripts menu data from user defined dir: %s.\n",upydir);
		}
	}
	else { syspath_append(dirname); }

	if (upydir) {
		res2 = bpymenu_GetStatMTime(U.pythondir, 0, &tdir2);

		if (res2 < 0) {
			tdir2 = 0;
			if (DEBUG) printf("\nUser defined scripts dir: %s:\n%s.\n", upydir, strerror(errno));
			if (res1 < 0) {
			if (DEBUG) printf ("To have scripts in menus, please add them to the"
							"default scripts dir: %s\n"
							"and/or go to 'Info window -> File Paths tab' and set a valid\n"
							"path for the user defined scripts dir.\n", dirname);
			return -1;
			}
		}
	}
	else res2 = -1;

	if ((res1 < 0) && (res2 < 0)) {
		if (DEBUG) {
			printf ("\nCannot register scripts in menus, no scripts dir"
							" available.\nExpected default dir in %s .\n", dirname);
		}
		return -1;
	}

	if (DEBUG) printf("\nRegistering scripts in Blender menus ...\n\n");

	if (!usedir) { /* if we're not forced to use the dir */
		BLI_make_file_string("/", fname, bpymenu_gethome(), BPYMENU_DATAFILE);
		resf = bpymenu_GetStatMTime(fname, 1, &tfile);
		if (resf < 0) tfile = 0;
	}

	/* comparing dates */

	if ((tfile > tdir1) && (tfile > tdir2) && !resf) { /* file is newer */
		resf = bpymenu_CreateFromFile(); /* -1 if an error occurred */
		if (!resf && DEBUG)
			printf("Getting menu data for scripts from file: %s\n\n", fname);
	}
	else resf = -1; /* -1 to use dirs: didn't use file or it was corrupted */

	if (resf == -1) { /* use dirs */
		if (DEBUG) {
			printf("Getting menu data for scripts from dir(s):\n%s\n", dirname);
			if (upydir) printf("%s\n", upydir);
		}
		if (res1 == 0) bpymenu_CreateFromDir(dirname, 0);
		if (res2 == 0) bpymenu_CreateFromDir(U.pythondir, 1);

		/* check if we got any data */
		for (res1 = 0; res1 < PYMENU_TOTAL; res1++)
			if (BPyMenuTable[res1]) break;

		/* if we got, recreate the file */
		if (res1 < PYMENU_TOTAL) bpymenu_WriteDataFile();
		else if (DEBUG) {
			printf ("\nWarning: Registering scripts in menus -- no info found.\n"
							"Either your scripts dirs have no .py scripts or the scripts\n"
						 	"don't have a header with registration data.\n"
							"Default scripts dir is: %s\n", dirname);
			if (upydir)
				printf("User defined scripts dir is: %s\n", upydir);
		}

		return 0;
	}

	return 0;
}
