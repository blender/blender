/* 
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
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano, Michael Reimpell
 *
 * ***** END GPL LICENSE BLOCK *****
*/

/* 
 *This is the main file responsible for having bpython scripts accessible
 * from Blender menus.  To know more, please start with its header file.
 */

#include "BPY_menus.h"

#include <Python.h>
#ifndef WIN32
  #include <dirent.h>
#else
  #include "BLI_winstuff.h"
#endif
#include "BKE_global.h"
#include "BKE_utildefines.h"
#include "BLI_blenlib.h"
#include "MEM_guardedalloc.h"
#include "DNA_userdef_types.h"	/* for U.pythondir */
#include "api2_2x/EXPP_interface.h" /* for bpy_gethome() */

#define BPYMENU_DATAFILE "Bpymenus"
#define MAX_DIR_DEPTH 4 /* max depth for traversing scripts dirs */
#define MAX_DIR_NUMBER 30 /* max number of dirs in scripts dirs trees */

static int DEBUG;
static int Dir_Depth;
static int Dirs_Number;

/* BPyMenuTable holds all registered pymenus, as linked lists for each menu
 * where they can appear (see PYMENUHOOKS enum in BPY_menus.h).
*/
BPyMenu *BPyMenuTable[PYMENU_TOTAL];

static int bpymenu_group_atoi( char *str )
{
	if( !strcmp( str, "Export" ) )
		return PYMENU_EXPORT;
	else if( !strcmp( str, "Import" ) )
		return PYMENU_IMPORT;
	else if( !strcmp( str, "Help" ) )
		return PYMENU_HELP;
	else if( !strcmp( str, "HelpWebsites" ) )
		return PYMENU_HELPWEBSITES;
	else if( !strcmp( str, "HelpSystem" ) )
		return PYMENU_HELPSYSTEM;
	else if( !strcmp( str, "Render" ) )
		return PYMENU_RENDER;
	else if( !strcmp( str, "System" ) )
		return PYMENU_SYSTEM;
	else if( !strcmp( str, "Object" ) )
		return PYMENU_OBJECT;
	else if( !strcmp( str, "Mesh" ) )
		return PYMENU_MESH;
	else if( !strncmp( str, "Theme", 5 ) )
		return PYMENU_THEMES;
	else if( !strcmp( str, "Add" ) )
		return PYMENU_ADD;
	else if( !strcmp( str, "Wizards" ) )
		return PYMENU_WIZARDS;
	else if( !strcmp( str, "Animation" ) )
		return PYMENU_ANIMATION;
	else if( !strcmp( str, "Materials" ) )
		return PYMENU_MATERIALS;
	else if( !strcmp( str, "UV" ) )
		return PYMENU_UV;
	else if( !strcmp( str, "Image" ) )
		return PYMENU_IMAGE;
	else if( !strcmp( str, "FaceSelect" ) )
		return PYMENU_FACESELECT;
	else if( !strcmp( str, "WeightPaint" ) )
		return PYMENU_WEIGHTPAINT;
	else if( !strcmp( str, "VertexPaint" ) )
		return PYMENU_VERTEXPAINT;
	else if( !strcmp( str, "UVCalculation" ) )
		return PYMENU_UVCALCULATION;
	else if( !strcmp( str, "Armature" ) )
		return PYMENU_ARMATURE;
	else if( !strcmp( str, "ScriptTemplate" ) )
		return PYMENU_SCRIPTTEMPLATE;
	else if( !strcmp( str, "MeshFaceKey" ) )
		return PYMENU_MESHFACEKEY;
	else if( !strcmp( str, "AddMesh" ) )
		return PYMENU_ADDMESH;
	/* "Misc" or an inexistent group name: use misc */
	else
		return PYMENU_MISC;
}

char *BPyMenu_group_itoa( short menugroup )
{
	switch ( menugroup ) {
	case PYMENU_EXPORT:
		return "Export";
		break;
	case PYMENU_IMPORT:
		return "Import";
		break;
	case PYMENU_ADD:
		return "Add";
		break;
	case PYMENU_HELP:
		return "Help";
		break;
	case PYMENU_HELPWEBSITES:
		return "HelpWebsites";
		break;
	case PYMENU_HELPSYSTEM:
		return "HelpSystem";
		break;
	case PYMENU_RENDER:
		return "Render";
		break;
	case PYMENU_SYSTEM:
		return "System";
		break;
	case PYMENU_OBJECT:
		return "Object";
		break;
	case PYMENU_MESH:
		return "Mesh";
		break;
	case PYMENU_THEMES:
		return "Themes";
		break;
	case PYMENU_WIZARDS:
		return "Wizards";
		break;
	case PYMENU_ANIMATION:
		return "Animation";
		break;
	case PYMENU_MATERIALS:
		return "Materials";
		break;
	case PYMENU_UV:
		return "UV";
		break;
	case PYMENU_IMAGE:
		return "Image";
		break;
	case PYMENU_FACESELECT:
		return "FaceSelect";
		break;
	case PYMENU_WEIGHTPAINT:
		return "WeightPaint";
		break;
	case PYMENU_VERTEXPAINT:
		return "VertexPaint";
		break;
	case PYMENU_UVCALCULATION:
		return "UVCalculation";
		break;
	case PYMENU_ARMATURE:
		return "Armature";
		break;
	case PYMENU_SCRIPTTEMPLATE:
		return "ScriptTemplate";
		break;
	case PYMENU_MESHFACEKEY:
		return "MeshFaceKey";
		break;
	case PYMENU_ADDMESH:
		return "AddMesh";
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
char *BPyMenu_CreatePupmenuStr( BPyMenu * pym, short menugroup )
{
	BPySubMenu *pysm = pym->submenus;
	char str[1024], str2[100];
	int i = 0, rlen;

	if( !pym || !pysm )
		return NULL;

	str[0] = '\0';

	PyOS_snprintf( str2, sizeof( str2 ), "%s: %s%%t",
		       BPyMenu_group_itoa( menugroup ), pym->name );
	strcat( str, str2 );

	while( pysm ) {
		PyOS_snprintf( str2, sizeof( str2 ), "|%s%%x%d", pysm->name,
			       i );
		rlen = sizeof( str ) - strlen( str );
		strncat( str, str2, rlen );
		i++;
		pysm = pysm->next;
	}

	return BLI_strdup( str );
}

static void bpymenu_RemoveAllSubEntries( BPySubMenu * smenu )
{
	BPySubMenu *tmp;

	while( smenu ) {
		tmp = smenu->next;
		if( smenu->name )
			MEM_freeN( smenu->name );
		if( smenu->arg )
			MEM_freeN( smenu->arg );
		MEM_freeN( smenu );
		smenu = tmp;
	}
	return;
}

void BPyMenu_RemoveAllEntries( void )
{
	BPyMenu *tmp, *pymenu;
	int i;

	for( i = 0; i < PYMENU_TOTAL; i++ ) {
		pymenu = BPyMenuTable[i];
		while( pymenu ) {
			tmp = pymenu->next;
			if( pymenu->name )
				MEM_freeN( pymenu->name );
			if( pymenu->filename )
				MEM_freeN( pymenu->filename );
			if( pymenu->tooltip )
				MEM_freeN( pymenu->tooltip );
			if( pymenu->submenus )
				bpymenu_RemoveAllSubEntries( pymenu->
							     submenus );
			MEM_freeN( pymenu );
			pymenu = tmp;
		}
		BPyMenuTable[i] = NULL;
	}

	Dirs_Number = 0;
	Dir_Depth = 0;

	return;
}

static BPyMenu *bpymenu_FindEntry( short group, char *name )
{
	BPyMenu *pymenu;

	if( ( group < 0 ) || ( group >= PYMENU_TOTAL ) )
		return NULL;

	pymenu = BPyMenuTable[group];

	while( pymenu ) {
		if( !strcmp( pymenu->name, name ) )
			return pymenu;
		pymenu = pymenu->next;
	}

	return NULL;
}

/* BPyMenu_GetEntry:
 * given a group and a position, return the entry in that position from
 * that group.
*/
BPyMenu *BPyMenu_GetEntry( short group, short pos )
{
	BPyMenu *pym = NULL;

	if( ( group < 0 ) || ( group >= PYMENU_TOTAL ) )
		return NULL;

	pym = BPyMenuTable[group];

	while( pos-- ) {
		if( pym )
			pym = pym->next;
		else
			break;
	}

	return pym;		/* found entry or NULL */
}

static void bpymenu_set_tooltip( BPyMenu * pymenu, char *tip )
{
	if( !pymenu )
		return;

	if( pymenu->tooltip )
		MEM_freeN( pymenu->tooltip );
	pymenu->tooltip = BLI_strdup( tip );

	return;
}

/* bpymenu_AddEntry:
 * try to find an existing pymenu entry with the given type and name;
 * if found, update it with new info, otherwise create a new one and fill it.
 */
static BPyMenu *bpymenu_AddEntry( short group, short version, char *name,
	char *fname, int is_userdir, char *tooltip )
{
	BPyMenu *menu, *next = NULL, **iter;
	int nameclash = 0;

	if( ( group < 0 ) || ( group >= PYMENU_TOTAL ) )
		return NULL;
	if( !name || !fname )
		return NULL;

	menu = bpymenu_FindEntry( group, name );	/* already exists? */

	/* if a menu with this name already exists in the same group:
	 * - if one script is in the default dir and the other in U.pythondir,
	 *   accept and let the new one override the other.
	 * - otherwise, report the error and return NULL. */
	if( menu ) {
		if( menu->dir < is_userdir ) {	/* new one is in U.pythondir */
			nameclash = 1;
			if( menu->name )
				MEM_freeN( menu->name );
			if( menu->filename )
				MEM_freeN( menu->filename );
			if( menu->tooltip )
				MEM_freeN( menu->tooltip );
			if( menu->submenus )
				bpymenu_RemoveAllSubEntries( menu->submenus );
			next = menu->next;
		} else {	/* they are in the same dir */
			if (DEBUG) {
				fprintf(stderr, "\n\
Warning: script %s's menu name is already in use.\n\
Edit the script and change its \n\
Name: '%s'\n\
field, please.\n\
Note: if you really want to have two scripts for the same menu with\n\
the same name, keep one in the default dir and the other in\n\
the user defined dir (only the later will be registered).\n", fname, name);
			}
			return NULL;
		}
	} else
		menu = MEM_mallocN( sizeof( BPyMenu ), "pymenu" );

	if( !menu )
		return NULL;

	menu->name = BLI_strdup( name );
	menu->version = version;
	menu->filename = BLI_strdup( fname );
	menu->tooltip = NULL;
	if( tooltip )
		menu->tooltip = BLI_strdup( tooltip );
	menu->dir = is_userdir;
	menu->submenus = NULL;
	menu->next = next;	/* non-NULL if menu already existed */

	if( nameclash )
		return menu;	/* no need to place it, it's already at the list */
	else {	/* insert the new entry in its correct position at the table */
		BPyMenu *prev = NULL;
		char *s = NULL;

		iter = &BPyMenuTable[group];
		while( *iter ) {
			s = ( *iter )->name;
			if( s )
				if( strcmp( menu->name, s ) < 0 )
					break;	/* sort by names */
			prev = *iter;
			iter = &( ( *iter )->next );
		}

		if( *iter ) {	/* prepend */
			menu->next = *iter;
			if( prev )
				prev->next = menu;
			else
				BPyMenuTable[group] = menu;	/* is first entry */
		} else
			*iter = menu;	/* append */
	}

	return menu;
}

/* bpymenu_AddSubEntry:
 * add a submenu to an existing python menu.
 */
static int bpymenu_AddSubEntry( BPyMenu * mentry, char *name, char *arg )
{
	BPySubMenu *smenu, **iter;

	smenu = MEM_mallocN( sizeof( BPySubMenu ), "pysubmenu" );
	if( !smenu )
		return -1;

	smenu->name = BLI_strdup( name );
	smenu->arg = BLI_strdup( arg );
	smenu->next = NULL;

	if( !smenu->name || !smenu->arg )
		return -1;

	iter = &( mentry->submenus );
	while( *iter )
		iter = &( ( *iter )->next );

	*iter = smenu;

	return 0;
}

/* bpymenu_CreateFromFile:
 * parse the bpymenus data file where Python menu data is stored;
 * based on this data, create and fill the pymenu structs.
 */
static int bpymenu_CreateFromFile( void )
{
	FILE *fp;
	char line[255], w1[255], w2[255], tooltip[255], *tip;
	char upythondir[FILE_MAX];
	char *homedir = NULL;
	int parsing, version, is_userdir;
	short group;
	BPyMenu *pymenu = NULL;

	/* init global bpymenu table (it is a list of pointers to struct BPyMenus
	 * for each available cathegory: import, export, etc.) */
	for( group = 0; group < PYMENU_TOTAL; group++ )
		BPyMenuTable[group] = NULL;

	/* let's try to open the file with bpymenu data */
	homedir = bpy_gethome(0);
	if (!homedir) {
		if( DEBUG )
			fprintf(stderr,
				"BPyMenus error: couldn't open config file Bpymenus: no home dir.\n");
		return -1;
	}

	BLI_make_file_string( "/", line, homedir, BPYMENU_DATAFILE );

	fp = fopen( line, "rb" );

	if( !fp ) {
		if( DEBUG )
			fprintf(stderr, "BPyMenus error: couldn't open config file %s.\n", line );
		return -1;
	}

	fgets( line, 255, fp );	/* header */

	/* check if the U.pythondir we saved at the file is different from the
	 * current one.  If so, return to force updating from dirs */
	w1[0] = '\0';
	fscanf( fp, "# User defined scripts dir: %[^\n]\n", w1 );

		BLI_strncpy(upythondir, U.pythondir, FILE_MAX);
		BLI_convertstringcode(upythondir, G.sce);

		if( strcmp( w1, upythondir ) != 0 )
			return -1;

		w1[0] = '\0';

	while( fgets( line, 255, fp ) ) {	/* parsing file lines */

		switch ( line[0] ) {	/* check first char */
		case '#':	/* comment */
			continue;
			break;
		case '\n':
			continue;
			break;
		default:
			parsing = sscanf( line, "%s {\n", w1 );	/* menu group */
			break;
		}

		if( parsing == 1 ) {	/* got menu group string */
			group = (short)bpymenu_group_atoi( w1 );
			if( group < 0 && DEBUG ) {	/* invalid type */
				fprintf(stderr,
					"BPyMenus error parsing config file: wrong group: %s,\n\
will use 'Misc'.\n", w1 );
			}
		} else
			continue;

		for(;;) {
			tip = NULL;	/* optional tooltip */
			fgets( line, 255, fp );
			if( line[0] == '}' )
				break;
			else if( line[0] == '\n' )
				continue;
			else if( line[0] == '\'' ) {	/* menu entry */
				parsing =
					sscanf( line,
						"'%[^']' %d %s %d '%[^']'\n",
						w1, &version, w2, &is_userdir,
						tooltip );

				if( parsing <= 0 ) {	/* invalid line, get rid of it */
					fgets( line, 255, fp );
				} else if( parsing == 5 )
					tip = tooltip;	/* has tooltip */

				pymenu = bpymenu_AddEntry( group,
							   ( short ) version,
							   w1, w2, is_userdir,
							   tip );
				if( !pymenu ) {
					puts( "BPyMenus error: couldn't create bpymenu entry.\n" );
					fclose( fp );
					return -1;
				}
			} else if( line[0] == '|' && line[1] == '_' ) {	/* menu sub-entry */
				if( !pymenu )
					continue;	/* no menu yet, skip this line */
				sscanf( line, "|_%[^:]: %s\n", w1, w2 );
				bpymenu_AddSubEntry( pymenu, w1, w2 );
			}
		}
	}

	fclose( fp );
	return 0;
}

/* bpymenu_WriteDataFile:
 * writes the registered scripts info to the user's home dir, for faster
 * access when the scripts dir hasn't changed.
*/
static void bpymenu_WriteDataFile( void )
{
	BPyMenu *pymenu;
	BPySubMenu *smenu;
	FILE *fp;
	char fname[FILE_MAXDIR], *homedir;
	int i;

	homedir = bpy_gethome(0);

	if (!homedir) {
		if( DEBUG )
			fprintf(stderr,
				"BPyMenus error: couldn't write Bpymenus file: no home dir.\n\n");
		return;
	}

	BLI_make_file_string( "/", fname, homedir, BPYMENU_DATAFILE );

	fp = fopen( fname, "w" );
	if( !fp ) {
		if( DEBUG )
			fprintf(stderr, "BPyMenus error: couldn't write %s file.\n\n",
				fname );
		return;
	}

	fprintf( fp,
		 "# Blender: registered menu entries for bpython scripts\n" );

	if (U.pythondir[0] != '\0' &&
			strcmp(U.pythondir, "/") != 0 && strcmp(U.pythondir, "//") != 0)
	{
		char upythondir[FILE_MAX];

		BLI_strncpy(upythondir, U.pythondir, FILE_MAX);
		BLI_convertstringcode(upythondir, G.sce);
		fprintf( fp, "# User defined scripts dir: %s\n", upythondir );
	}

	for( i = 0; i < PYMENU_TOTAL; i++ ) {
		pymenu = BPyMenuTable[i];
		if( !pymenu )
			continue;
		fprintf( fp, "\n%s {\n", BPyMenu_group_itoa( (short)i ) );
		while( pymenu ) {
			fprintf( fp, "'%s' %d %s %d", pymenu->name,
				 pymenu->version, pymenu->filename,
				 pymenu->dir );
			if( pymenu->tooltip )
				fprintf( fp, " '%s'\n", pymenu->tooltip );
			else
				fprintf( fp, "\n" );
			smenu = pymenu->submenus;
			while( smenu ) {
				fprintf( fp, "|_%s: %s\n", smenu->name,
					 smenu->arg );
				smenu = smenu->next;
			}
			pymenu = pymenu->next;
		}
		fprintf( fp, "}\n" );
	}

	fclose( fp );
	return;
}

/* BPyMenu_PrintAllEntries:
 * useful for debugging.
 */
void BPyMenu_PrintAllEntries( void )
{
	BPyMenu *pymenu;
	BPySubMenu *smenu;
	int i;

	printf( "# Blender: registered menu entries for bpython scripts\n" );

	for( i = 0; i < PYMENU_TOTAL; i++ ) {
		pymenu = BPyMenuTable[i];
		printf( "\n%s {\n", BPyMenu_group_itoa( (short)i ) );
		while( pymenu ) {
			printf( "'%s' %d %s %d", pymenu->name, pymenu->version,
				pymenu->filename, pymenu->dir );
			if( pymenu->tooltip )
				printf( " '%s'\n", pymenu->tooltip );
			else
				printf( "\n" );
			smenu = pymenu->submenus;
			while( smenu ) {
				printf( "|_%s: %s\n", smenu->name,
					smenu->arg );
				smenu = smenu->next;
			}
			pymenu = pymenu->next;
		}
		printf( "}\n" );
	}
}

/* bpymenu_ParseFile:
 * recursively scans folders looking for scripts to register.
 *
 * This function scans the scripts directory looking for .py files with the
 * right header and menu info, using that to fill the bpymenu structs.
 * is_userdir defines if the script is in the default scripts dir or the
 * user defined one (U.pythondir: is_userdir == 1).
 * Speed is important.
 *
 * The first line of the script must be '#!BPY'.
 * The header registration lines must appear between the first pair of
 * '\"\"\"' and follow this order (the single-quotes are part of
 * the format):
 *
 * # \"\"\"<br>
 * # Name: 'script name for the menu'
 * # Blender: <code>short int</code> (minimal Blender version)
 * # Group: 'group name' (defines menu)
 * # Submenu: 'submenu name' related_1word_arg
 * # Tooltip: 'tooltip for the menu'
 * # \"\"\"
 *
 * Notes:
 *
 * - Commenting out header lines with "#" is optional, but recommended.
 * - There may be more than one submenu line, or none:
 *   submenus and the tooltip are optional;
 * - The Blender version is the same number reported by 
 *   Blender.Get('version') in BPython or G.version in C;
 * - Line length must be less than 99.
 */
static int bpymenu_ParseFile(FILE *file, char *fname, int is_userdir)
{
	char line[100];
	char head[100];
	char middle[100];
	char tail[100];
	int matches;
	int parser_state;

	char script_name[100];
	int script_version = 1;
	int script_group;

	BPyMenu *scriptMenu = NULL;

	if (file != NULL) {
		parser_state = 1; /* state of parser, 0 to terminate */

		while ((parser_state != 0) && (fgets(line, 100, file) != NULL)) {

			switch (parser_state) {

				case 1: /* !BPY */
					if (strncmp(line, "#!BPY", 5) == 0) {
						parser_state++;
					} else {
						parser_state = 0;
					}
					break;

				case 2: /* \"\"\" */
					if ((strstr(line, "\"\"\""))) {
						parser_state++;
					}
					break;

				case 3: /* Name: 'script name for the menu' */
					matches = sscanf(line, "%[^']'%[^']'%c", head, script_name, tail);
					if ((matches == 3) && (strstr(head, "Name:") != NULL)) {
						parser_state++;
					} else {
						if (DEBUG)
							fprintf(stderr, "BPyMenus error: Wrong 'Name' line: %s\n", fname);
						parser_state = 0;
					}
					break;

				case 4: /* Blender: <short int> */
					matches = sscanf(line, "%[^1234567890]%i%c", head, &script_version,
						tail);
					if (matches == 3) {
						parser_state++;
					} else {
						if (DEBUG)
							fprintf(stderr,"BPyMenus error: Wrong 'Blender' line: %s\n",fname);
						parser_state = 0;
					}
					break;

				case 5: /* Group: 'group name' */
					matches = sscanf(line, "%[^']'%[^']'%c", head, middle, tail);
					if ((matches == 3) && (strstr(head, "Group:") != NULL)) {
						script_group = bpymenu_group_atoi(middle);
						if (script_group < 0) {
							if (DEBUG)
								fprintf(stderr, "BPyMenus error: Unknown group \"%s\": %s\n",
									middle, fname);
							parser_state = 0;
						}
						
						else { /* register script */
							scriptMenu = bpymenu_AddEntry((short)script_group,
								(short int)script_version, script_name, fname, is_userdir,NULL);
							if (scriptMenu == NULL) {
								if (DEBUG)
									fprintf(stderr,
										"BPyMenus error: Couldn't create entry for: %s\n", fname);
								parser_state = 0;
							} else {
								parser_state++;
							}
						}

					} else {
						if (DEBUG)
							fprintf(stderr, "BPyMenus error: Wrong 'Group' line: %s\n",fname);
						parser_state = 0;
					}
					break;

				case 6: /* optional elements */
					/* Submenu: 'submenu name' related_1word_arg */
					matches = sscanf(line, "%[^']'%[^']'%s\n", head, middle, tail);
					if ((matches == 3) && (strstr(head, "Submenu:") != NULL)) {
						bpymenu_AddSubEntry(scriptMenu, middle, tail);
					} else {
						/* Tooltip: 'tooltip for the menu */
						matches = sscanf(line, "%[^']'%[^']'%c", head, middle, tail);
						if ((matches == 3) && ((strstr(head, "Tooltip:") != NULL) ||
							(strstr(head, "Tip:") != NULL))) {
							bpymenu_set_tooltip(scriptMenu, middle);
						}
						parser_state = 0;
					}
					break;

				default:
					parser_state = 0;
					break;
			}
		}
	}

	else { /* shouldn't happen, it's checked in bpymenus_ParseDir */
		if (DEBUG)
			fprintf(stderr, "BPyMenus error: Couldn't open %s.\n", fname);
		return -1;
	}

	return 0;
}

/* bpymenu_ParseDir:
 * recursively scans folders looking for scripts to register.
 *
 * This function scans the scripts directory looking for .py files with the
 * right header and menu info.
 * - is_userdir defines if the script is in the default scripts dir or the
 * user defined one (U.pythondir: is_userdir == 1);
 * - parentdir is the parent dir name to store as part of the script filename,
 * if we're down a subdir.
 * Speed is important.
 */
static int bpymenu_ParseDir(char *dirname, char *parentdir, int is_userdir )
{
	DIR *dir; 
	FILE *file = NULL;
	struct dirent *de;
	struct stat status;
	char *file_extension;
	char path[FILE_MAX];
	char subdir[FILE_MAX];
	char *s = NULL;
	
	dir = opendir(dirname);

	if (dir != NULL) {
		while ((de = readdir(dir)) != NULL) {

			/* skip files and dirs starting with '.' or 'bpy' */
			if ((de->d_name[0] == '.') || !strncmp(de->d_name, "bpy", 3)) {
				continue;
			}
			
			BLI_make_file_string("/", path, dirname, de->d_name);
			
			if (stat(path, &status) != 0) {
				if (DEBUG)
					fprintf(stderr, "stat %s failed: %s\n", path, strerror(errno));
			}

			if (S_ISREG(status.st_mode)) { /* is file */

				file_extension = strstr(de->d_name, ".py");

				if (file_extension && *(file_extension + 3) == '\0') {
					file = fopen(path, "rb");

					if (file) {
						s = de->d_name;
						if (parentdir) {
							/* Join parentdir and de->d_name */
							BLI_join_dirfile(subdir, parentdir, de->d_name);

							s = subdir;
						}
						bpymenu_ParseFile(file, s, is_userdir);
						fclose(file);
					}

					else {
						if (DEBUG)
							fprintf(stderr, "BPyMenus error: Couldn't open %s.\n", path);
					}
				}
			}

			else if (S_ISDIR(status.st_mode)) { /* is subdir */
				Dirs_Number++;
				Dir_Depth++;
				if (Dirs_Number > MAX_DIR_NUMBER) {
					if (DEBUG) {
						fprintf(stderr, "BPyMenus error: too many subdirs.\n");
					}
					closedir(dir);
					return -1;
				}
				else if (Dir_Depth > MAX_DIR_DEPTH) {
					if (DEBUG)
						fprintf(stderr,
							"BPyMenus error: max depth reached traversing dir tree.\n");
					closedir(dir);
					return -1;
				}
				s = de->d_name;
				if (parentdir) {
					/* Join parentdir and de->d_name */
					BLI_join_dirfile(subdir, parentdir, de->d_name);					
					s = subdir;
				}
				if (bpymenu_ParseDir(path, s, is_userdir) == -1) {
					closedir(dir);
					return -1;
				}
				Dir_Depth--;
			}

		}
		closedir(dir);
	}

	else { /* open directory stream failed */
		if (DEBUG)
			fprintf(stderr, "opendir %s failed: %s\n", dirname, strerror(errno));
		return -1;
	}

	return 0;
}

static int bpymenu_GetStatMTime( const char *name, int is_file, time_t * mtime )
{
	struct stat st;
	int result;

#ifdef WIN32
	if (is_file) {
	result = stat( name, &st );
	} else {
		/* needed for win32 only, remove trailing slash */
		char name_stat[FILE_MAX];
		BLI_strncpy(name_stat, name, FILE_MAX);
		BLI_del_slash(name_stat);
		result = stat( name_stat, &st );
	}
#else
	result = stat( name, &st );
#endif
	
	if( result == -1 )
		return -1;

	if( is_file ) {
		if( !S_ISREG( st.st_mode ) )
			return -2;
	} else if( !S_ISDIR( st.st_mode ) )
		return -2;

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
int BPyMenu_Init( int usedir )
{
	char fname[FILE_MAXDIR];
	char dirname[FILE_MAX];
	char upythondir[FILE_MAX];
	char *upydir = U.pythondir, *sdir = NULL;
	time_t time_dir1 = 0, time_dir2 = 0, time_file = 0;
	int stat_dir1 = 0, stat_dir2 = 0, stat_file = 0;
	int i;

	DEBUG = G.f & G_DEBUG;	/* is Blender in debug mode (started with -d) ? */

	/* init global bpymenu table (it is a list of pointers to struct BPyMenus
	 * for each available group: import, export, etc.) */
	for( i = 0; i < PYMENU_TOTAL; i++ )
		BPyMenuTable[i] = NULL;

	if( DEBUG )
		fprintf(stdout, "\nRegistering scripts in Blender menus ...\n\n" );

	if( U.pythondir[0] == '\0') {
		upydir = NULL;
	}
	else if (strcmp(U.pythondir, "/") == 0 || strcmp(U.pythondir, "//") == 0) {
		/* these are not accepted to prevent possible slight slowdowns on startup;
		 * they should not be used as user defined scripts dir, anyway, also from
		 * speed considerations, since they'd not be dedicated scripts dirs */
		if (DEBUG) fprintf(stderr,
			"BPyMenus: invalid user defined Python scripts dir: \"/\" or \"//\".\n");
		upydir = NULL;
	}
	else {
		BLI_strncpy(upythondir, upydir, FILE_MAX);
		BLI_convertstringcode(upythondir, G.sce);
	}

	sdir = bpy_gethome(1);

	if (sdir) {
		BLI_strncpy(dirname, sdir, FILE_MAX);
		stat_dir1 = bpymenu_GetStatMTime( dirname, 0, &time_dir1 );

		if( stat_dir1 < 0 ) {
			time_dir1 = 0;
			if( DEBUG ) {
				fprintf(stderr,
					"\nDefault scripts dir: %s:\n%s\n", dirname, strerror(errno));
				if( upydir )
					fprintf(stdout,
						"Getting scripts menu data from user defined dir: %s.\n",
						upythondir );
			}
		}
	}
	else stat_dir1 = -1;

	if( upydir ) {
		stat_dir2 = bpymenu_GetStatMTime( upythondir, 0, &time_dir2 );

		if( stat_dir2 < 0 ) {
			time_dir2 = 0;
			upydir = NULL;
			if( DEBUG )
				fprintf(stderr, "\nUser defined scripts dir: %s:\n%s.\n",
					upythondir, strerror( errno ) );
			if( stat_dir1 < 0 ) {
				if( DEBUG )
					fprintf(stderr, "\
To have scripts in menus, please add them to the default scripts dir:\n\
%s\n\
and / or go to 'Info window -> File Paths tab' and set a valid path for\n\
the user defined Python scripts dir.\n", dirname );
				return -1;
			}
		}
	}
	else stat_dir2 = -1;

	if( ( stat_dir1 < 0 ) && ( stat_dir2 < 0 ) ) {
		if( DEBUG ) {
			fprintf(stderr, "\nCannot register scripts in menus, no scripts dir"
							" available.\nExpected default dir at: %s \n", dirname );
		}
		return -1;
	}

	if (usedir) stat_file = -1;
	else { /* if we're not forced to use the dir */
		char *homedir = bpy_gethome(0);

		if (homedir) {
			BLI_make_file_string( "/", fname, homedir, BPYMENU_DATAFILE );
			stat_file = bpymenu_GetStatMTime( fname, 1, &time_file );
			if( stat_file < 0 )
				time_file = 0;

		/* comparing dates */

			if((stat_file == 0)
				&& (time_file > time_dir1) && (time_file > time_dir2))
			{	/* file is newer */
				stat_file = bpymenu_CreateFromFile(  );	/* -1 if an error occurred */
				if( !stat_file && DEBUG )
					fprintf(stdout,
						"Getting menu data for scripts from file:\n%s\n\n", fname );
			}
			else stat_file = -1;
		}
		else stat_file = -1;	/* -1 to use dirs: didn't use file or it was corrupted */
	}

	if( stat_file == -1 ) {	/* use dirs */
		if( DEBUG ) {
			fprintf(stdout,
				"Getting menu data for scripts from dir(s):\ndefault: %s\n", dirname );
			if( upydir )
				fprintf(stdout, "user defined: %s\n", upythondir );
			fprintf(stdout, "\n");
		}
		if( stat_dir1 == 0 ) {
			i = bpymenu_ParseDir( dirname, NULL, 0 );
			if (i == -1 && DEBUG)
				fprintf(stderr, "Default scripts dir does not seem valid.\n\n");
		}
		if( stat_dir2 == 0 ) {
			BLI_strncpy(dirname, U.pythondir, FILE_MAX);
			BLI_convertstringcode(dirname, G.sce);
			i = bpymenu_ParseDir( dirname, NULL, 1 );
			if (i == -1 && DEBUG)
				fprintf(stderr, "User defined scripts dir does not seem valid.\n\n");
		}

		/* check if we got any data */
		for( i = 0; i < PYMENU_TOTAL; i++ )
			if( BPyMenuTable[i] )
				break;

		/* if we got, recreate the file */
		if( i < PYMENU_TOTAL )
			bpymenu_WriteDataFile(  );
		else if( DEBUG ) {
			fprintf(stderr, "\n\
Warning: Registering scripts in menus -- no info found.\n\
Either your scripts dirs have no .py scripts or the scripts\n\
don't have a header with registration data.\n\
Default scripts dir is:\n\
%s\n", dirname );
			if( upydir )
				fprintf(stderr, "User defined scripts dir is: %s\n",
					upythondir );
		}
	}

	return 0;
}
