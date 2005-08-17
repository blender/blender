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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Michel Selten, Willian P. Germano, Stephen Swaney,
 * Chris Keith, Chris Want
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <Python.h>

#include "compile.h"		/* for the PyCodeObject */
#include "eval.h"		/* for PyEval_EvalCode */
#include "BLI_blenlib.h"	/* for BLI_last_slash() */
#include "BIF_interface.h"	/* for pupmenu() */
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"
#include "BKE_library.h"
#include "BKE_object.h"		/* during_scriptlink() */
#include "BKE_text.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"	/* for U.pythondir */
#include "MEM_guardedalloc.h"
#include "BPY_extern.h"
#include "BPY_menus.h"
#include "BPI_script.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "api2_2x/EXPP_interface.h"
#include "api2_2x/constant.h"
#include "api2_2x/gen_utils.h"
#include "api2_2x/BGL.h" 
#include "api2_2x/Blender.h"
#include "api2_2x/Camera.h"
#include "api2_2x/Draw.h"
#include "api2_2x/Lamp.h"
#include "api2_2x/NMesh.h"
#include "api2_2x/Object.h"
#include "api2_2x/Registry.h"
#include "api2_2x/Scene.h"
#include "api2_2x/World.h"

/* bpy_registryDict is declared in api2_2x/Registry.h and defined
 * in api2_2x/Registry.c
 * This Python dictionary will be used to store data that scripts
 * choose to preserve after they are executed, so user changes can be
 * restored next time the script is used.  Check the Blender.Registry module. 
 */
//#include "api2_2x/Registry.h"

/*Declares the modules and their initialization functions
*These are TOP-LEVEL modules e.g. import `module` - there is no
*support for packages here e.g. import `package.module` */
static struct _inittab BPy_Inittab_Modules[] = {
	{"Blender", M_Blender_Init},
	{NULL}
};

/*************************************************************************
* Structure definitions	
**************************************************************************/
#define FILENAME_LENGTH 24
typedef struct _ScriptError {
	char filename[FILENAME_LENGTH];
	int lineno;
} ScriptError;

/****************************************************************************
* Global variables 
****************************************************************************/
ScriptError g_script_error;

/***************************************************************************
* Function prototypes 
***************************************************************************/
PyObject *RunPython( Text * text, PyObject * globaldict );
char *GetName( Text * text );
PyObject *CreateGlobalDictionary( void );
void ReleaseGlobalDictionary( PyObject * dict );
void DoAllScriptsFromList( ListBase * list, short event );
PyObject *importText( char *name );
void init_ourImport( void );
PyObject *blender_import( PyObject * self, PyObject * args );

int BPY_txt_do_python_Text( struct Text *text );
void BPY_Err_Handle( char *script_name );
PyObject *traceback_getFilename( PyObject * tb );

void BPY_free_screen_spacehandlers(struct bScreen *sc);

/****************************************************************************
* Description: This function will start the interpreter and load all modules
* as well as search for a python installation.
****************************************************************************/
void BPY_start_python( int argc, char **argv )
{
	static int argc_copy = 0;
	static char **argv_copy = NULL;
	int first_time = argc;

	/* we keep a copy of the values of argc and argv so that the game engine
	 * can call BPY_start_python(0, NULL) whenever a game ends, without having
	 * to know argc and argv there (in source/blender/src/space.c) */
	if( first_time ) {
		argc_copy = argc;
		argv_copy = argv;
	}

	//stuff for Registry module
	bpy_registryDict = PyDict_New(  );/* check comment at start of this file */
	if( !bpy_registryDict )
		printf( "Error: Couldn't create the Registry Python Dictionary!" );
	Py_SetProgramName( "blender" );

	/* Py_Initialize() will attempt to import the site module and
	 * print an error if not found.  See init_syspath() for the
	 * rest of our init msgs.
	 */
	// Py_GetVersion() returns a ptr to astatic string
	printf( "Using Python version %.3s\n", Py_GetVersion() );

	//Initialize the TOP-LEVEL modules
	PyImport_ExtendInittab(BPy_Inittab_Modules);
	
	//Start the interpreter
	Py_Initialize(  );
	PySys_SetArgv( argc_copy, argv_copy );

	//Overrides __import__
	init_ourImport(  );

	//init a global dictionary
	g_blenderdict = NULL;

	//Look for a python installation
	init_syspath( first_time ); /* not first_time: some msgs are suppressed */

	return;
}

/*****************************************************************************/
/* Description: This function will terminate the Python interpreter	     */
/*****************************************************************************/
void BPY_end_python( void )
{
	if( bpy_registryDict ) {
		Py_DECREF( bpy_registryDict );
		bpy_registryDict = NULL;
	}

	Py_Finalize(  );

	BPyMenu_RemoveAllEntries(  );	/* freeing bpymenu mem */

	/* a script might've opened a .blend file but didn't close it, so: */
	EXPP_Library_Close(  );

	return;
}

void syspath_append( char *dirname )
{
	PyObject *mod_sys, *dict, *path, *dir;

	PyErr_Clear(  );

	dir = Py_BuildValue( "s", dirname );

	mod_sys = PyImport_ImportModule( "sys" );	/* new ref */
	dict = PyModule_GetDict( mod_sys );	/* borrowed ref */
	path = PyDict_GetItemString( dict, "path" );	/* borrowed ref */

	if( !PyList_Check( path ) )
		return;

	PyList_Append( path, dir );

	if( PyErr_Occurred(  ) )
		Py_FatalError( "could not build sys.path" );

	Py_DECREF( mod_sys );
}

void init_syspath( int first_time )
{
	PyObject *path;
	PyObject *mod, *d;
	PyObject *p;
	char *c, *progname;
	char execdir[FILE_MAXDIR];	/*defines from DNA_space_types.h */

	int n;

	path = Py_BuildValue( "s", bprogname );

	mod = PyImport_ImportModule( "Blender.sys" );

	if( mod ) {
		d = PyModule_GetDict( mod );
		PyDict_SetItemString( d, "progname", path );
		Py_DECREF( mod );
	} else
		printf( "Warning: could not set Blender.sys.progname\n" );

	progname = BLI_last_slash( bprogname );	/* looks for the last dir separator */

	c = Py_GetPath(  );	/* get python system path */
	PySys_SetPath( c );	/* initialize */

	n = progname - bprogname;
	if( n > 0 ) {
		strncpy( execdir, bprogname, n );
		if( execdir[n - 1] == '.' )
			n--;	/*fix for when run as ./blender */
		execdir[n] = '\0';

		syspath_append( execdir );	/* append to module search path */

		/* set Blender.sys.progname */
	} else
		printf( "Warning: could not determine argv[0] path\n" );

	/* 
	 * bring in the site module so we can add 
	 * site-package dirs to sys.path 
	 */

	mod = PyImport_ImportModule( "site" );	/* new ref */

	if( mod ) {
		PyObject *item;
		int size = 0;
		int index;

		/* get the value of 'sitedirs' from the module */

		/* the ref man says GetDict() never fails!!! */
		d = PyModule_GetDict( mod );	/* borrowed ref */
		p = PyDict_GetItemString( d, "sitedirs" );	/* borrowed ref */

		if( p ) {	/* we got our string */
			/* append each item in sitedirs list to path */
			size = PyList_Size( p );

			for( index = 0; index < size; index++ ) {
				item = PySequence_GetItem( p, index );	/* new ref */
				if( item )
					syspath_append( PyString_AsString
							( item ) );
			}
		}
		Py_DECREF( mod );
	} else {		/* import 'site' failed */
		PyErr_Clear(  );
		if( first_time ) {
			printf( "No installed Python found.\n" );
			printf( "Only built-in modules are available.  Some scripts may not run.\n" );
			printf( "Continuing happily.\n" );
		}
	}

	/* 
	 * initialize the sys module
	 * set sys.executable to the Blender exe 
	 */

	mod = PyImport_ImportModule( "sys" );	/* new ref */

	if( mod ) {
		d = PyModule_GetDict( mod );	/* borrowed ref */
		PyDict_SetItemString( d, "executable",
				      Py_BuildValue( "s", bprogname ) );
		Py_DECREF( mod );
	}
}

/****************************************************************************
* Description: This function finishes Python initialization in Blender.	 

Because U.pythondir (user defined dir for scripts) isn't	 
initialized when BPY_start_Python needs to be executed, we	 
postpone adding U.pythondir to sys.path and also BPyMenus	  
(mechanism to register scripts in Blender menus) for when  
that dir info is available.   
****************************************************************************/
void BPY_post_start_python( void )
{
	char dirpath[FILE_MAXDIR];
	char *sdir = NULL;

	if(U.pythondir[0] != '\0' ) {
		char modpath[FILE_MAXDIR];
		int upyslen = strlen(U.pythondir);

		/* check if user pydir ends with a slash and, if so, remove the slash
		 * (for eventual implementations of c library's stat function that might
		 * not like it) */
		if (upyslen > 2) { /* avoids doing anything if dir == '//' */
			char ending = U.pythondir[upyslen - 1];

			if (ending == '/' || ending == '\\')
				U.pythondir[upyslen - 1] = '\0';
		}

		BLI_strncpy(dirpath, U.pythondir, FILE_MAXDIR);
		BLI_convertstringcode(dirpath, G.sce, 0);
		syspath_append(dirpath);	/* append to module search path */

		BLI_make_file_string("/", modpath, dirpath, "bpymodules");
		if (BLI_exists(modpath)) syspath_append(modpath);
	}

	sdir = bpy_gethome(1);
	if (sdir) {

		syspath_append(sdir);

		BLI_make_file_string("/", dirpath, sdir, "bpymodules");
		if (BLI_exists(dirpath)) syspath_append(dirpath);
	}

	BPyMenu_Init( 0 );	/* get dynamic menus (registered scripts) data */

	return;
}

/****************************************************************************
* Description: This function will return the linenumber on which an error  
*       	has occurred in the Python script.			
****************************************************************************/
int BPY_Err_getLinenumber( void )
{
	return g_script_error.lineno;
}

/*****************************************************************************/
/* Description: This function will return the filename of the python script. */
/*****************************************************************************/
const char *BPY_Err_getFilename( void )
{
	return g_script_error.filename;
}

/*****************************************************************************/
/* Description: Return PyString filename from a traceback object	    */
/*****************************************************************************/
PyObject *traceback_getFilename( PyObject * tb )
{
	PyObject *v = NULL;

/* co_filename is in f_code, which is in tb_frame, which is in tb */

	v = PyObject_GetAttrString( tb, "tb_frame" );
	if (v) {
		Py_DECREF( v );
		v = PyObject_GetAttrString( v, "f_code" );
		if (v) {
			Py_DECREF( v );
			v = PyObject_GetAttrString( v, "co_filename" );
		}
	}

	if (v) return v;
	else return PyString_FromString("unknown");
}

/****************************************************************************
* Description: Blender Python error handler. This catches the error and	
* stores filename and line number in a global  
*****************************************************************************/
void BPY_Err_Handle( char *script_name )
{
	PyObject *exception, *err, *tb, *v;

	if( !script_name ) {
		printf( "Error: script has NULL name\n" );
		return;
	}

	PyErr_Fetch( &exception, &err, &tb );

	if( !exception && !tb ) {
		printf( "FATAL: spurious exception\n" );
		return;
	}

	strcpy( g_script_error.filename, script_name );

	if( exception
	    && PyErr_GivenExceptionMatches( exception, PyExc_SyntaxError ) ) {
		/* no traceback available when SyntaxError */
		PyErr_Restore( exception, err, tb );	/* takes away reference! */
		PyErr_Print(  );
		v = PyObject_GetAttrString( err, "lineno" );
		if( v ) {
			g_script_error.lineno = PyInt_AsLong( v );
			Py_DECREF( v );
		} else {
			g_script_error.lineno = -1;
		}
		/* this avoids an abort in Python 2.3's garbage collecting: */
		PyErr_Clear(  );
		return;
	} else {
		PyErr_NormalizeException( &exception, &err, &tb );
		PyErr_Restore( exception, err, tb );	/* takes away reference! */
		PyErr_Print(  );
		tb = PySys_GetObject( "last_traceback" );

		if( !tb ) {
			printf( "\nCan't get traceback\n" );
			return;
		}

		Py_INCREF( tb );

/* From old bpython BPY_main.c:
 * 'check traceback objects and look for last traceback in the
 *	same text file. This is used to jump to the line of where the
 *	error occured. "If the error occured in another text file or module,
 *	the last frame in the current file is adressed."' 
 */

		for(;;) {
			v = PyObject_GetAttrString( tb, "tb_next" );

			if( !v || v == Py_None ||
				strcmp(PyString_AsString(traceback_getFilename(v)), script_name)) {
				break;
			}

			Py_DECREF( tb );
			tb = v;
		}

		v = PyObject_GetAttrString( tb, "tb_lineno" );
		if (v) {
			g_script_error.lineno = PyInt_AsLong(v);
			Py_DECREF(v);
		}
		v = traceback_getFilename( tb );
		if (v) {
			strncpy( g_script_error.filename, PyString_AsString( v ),
				FILENAME_LENGTH );
			Py_DECREF(v);
		}
		Py_DECREF( tb );
	}

	return;
}

/****************************************************************************
* Description: This function executes the script passed by st.		
* Notes:	It is called by blender/src/drawtext.c when a Blender user  
*		presses ALT+PKEY in the script's text window. 
*****************************************************************************/
int BPY_txt_do_python_Text( struct Text *text )
{
	PyObject *py_dict, *py_result;
	BPy_constant *info;
	char textname[24];
	Script *script = G.main->script.first;

	if( !text )
		return 0;

	/* check if this text is already running */
	while( script ) {
		if( !strcmp( script->id.name + 2, text->id.name + 2 ) ) {
			/* if this text is already a running script, 
			 * just move to it: */
			SpaceScript *sc;
			newspace( curarea, SPACE_SCRIPT );
			sc = curarea->spacedata.first;
			sc->script = script;
			return 1;
		}
		script = script->id.next;
	}

	/* Create a new script structure and initialize it: */
	script = alloc_libblock( &G.main->script, ID_SCRIPT, GetName( text ) );

	if( !script ) {
		printf( "couldn't allocate memory for Script struct!" );
		return 0;
	}

	/* if in the script Blender.Load(blendfile) is not the last command,
	 * an error after it will call BPY_Err_Handle below, but the text struct
	 * will have been deallocated already, so we need to copy its name here.
	 */
	BLI_strncpy( textname, GetName( text ),
		     strlen( GetName( text ) ) + 1 );

	script->id.us = 1;
	script->flags = SCRIPT_RUNNING;
	script->py_draw = NULL;
	script->py_event = NULL;
	script->py_button = NULL;
	script->py_browsercallback = NULL;

	py_dict = CreateGlobalDictionary(  );

	script->py_globaldict = py_dict;

	info = ( BPy_constant * ) PyConstant_New(  );
	if( info ) {
		PyConstant_Insert( info, "name",
				 PyString_FromString( script->id.name + 2 ) );
		Py_INCREF( Py_None );
		PyConstant_Insert( info, "arg", Py_None );
		PyDict_SetItemString( py_dict, "__script__",
				      ( PyObject * ) info );
	}

	py_result = RunPython( text, py_dict );	/* Run the script */

	if( !py_result ) {	/* Failed execution of the script */

		BPY_Err_Handle( textname );
		ReleaseGlobalDictionary( py_dict );
		script->py_globaldict = NULL;
		if( G.main->script.first )
			free_libblock( &G.main->script, script );

		return 0;
	} else {
		Py_DECREF( py_result );
		script->flags &= ~SCRIPT_RUNNING;
		if( !script->flags ) {
			ReleaseGlobalDictionary( py_dict );
			script->py_globaldict = NULL;
			free_libblock( &G.main->script, script );
		}
	}

	return 1;		/* normal return */
}

/****************************************************************************
* Description: Called from command line to run a Python script
* automatically. 
****************************************************************************/
void BPY_run_python_script( char *fn )
{
	Text *text = NULL;
	int is_blender_text = 0;

	if (!BLI_exists(fn)) {	/* if there's no such filename ... */
		text = G.main->text.first;	/* try an already existing Blender Text */

		while (text) {
			if (!strcmp(fn, text->id.name + 2)) break;
			text = text->id.next;
		}

		if (text == NULL) {
			printf("\nError: no such file or Blender text -- %s.\n", fn);
			return;
		}
		else is_blender_text = 1;	/* fn is already a Blender Text */
	}

	else {
		text = add_text(fn);

		if (text == NULL) {
			printf("\nError in BPY_run_python_script:\n"
				"couldn't create Blender text from %s\n", fn);
		/* Chris: On Windows if I continue I just get a segmentation
		 * violation.  To get a baseline file I exit here. */
		exit(2);
		/* return; */
		}
	}

	if (BPY_txt_do_python_Text(text) != 1) {
		printf("\nError executing Python script from command-line:\n"
			"%s (at line %d).\n", fn, BPY_Err_getLinenumber());
	}

	if (!is_blender_text) free_libblock(&G.main->text, text);
}

/****************************************************************************
* Description: This function executes the script chosen from a menu.
* Notes:	It is called by the ui code in src/header_???.c when a user  
*		clicks on a menu entry that refers to a script.
*		Scripts are searched in the BPyMenuTable, using the given
*		menutype and event values to know which one was chosen.	
*****************************************************************************/
int BPY_menu_do_python( short menutype, int event )
{
	PyObject *py_dict, *py_res, *pyarg = NULL;
	BPy_constant *info;
	BPyMenu *pym;
	BPySubMenu *pysm;
	FILE *fp = NULL;
	char *buffer, *s;
	char filestr[FILE_MAXDIR + FILE_MAXFILE];
	char scriptname[21];
	Script *script = NULL;
	int len;

	pym = BPyMenu_GetEntry( menutype, ( short ) event );

	if( !pym )
		return 0;

	if( pym->version > G.version )
		notice( "Version mismatch: script was written for Blender %d. "
			"It may fail with yours: %d.", pym->version,
			G.version );

/* if there are submenus, let the user choose one from a pupmenu that we
 * create here.*/
	pysm = pym->submenus;
	if( pysm ) {
		char *pupstr;
		int arg;

		pupstr = BPyMenu_CreatePupmenuStr( pym, menutype );

		if( pupstr ) {
			arg = pupmenu( pupstr );
			MEM_freeN( pupstr );

			if( arg >= 0 ) {
				while( arg-- )
					pysm = pysm->next;
				pyarg = PyString_FromString( pysm->arg );
			} else
				return 0;
		}
	}

	if( !pyarg ) { /* no submenus */
		Py_INCREF( Py_None );
		pyarg = Py_None;
	}

	if( pym->dir ) { /* script is in U.pythondir */
		char upythondir[FILE_MAXDIR];

		/* dirs in Blender can be "//", which has a special meaning */
		BLI_strncpy(upythondir, U.pythondir, FILE_MAXDIR);
		BLI_convertstringcode(upythondir, G.sce, 0); /* if so, this expands it */
		BLI_make_file_string( "/", filestr, upythondir, pym->filename );
	}
	else { /* script is in default scripts dir */
		char *scriptsdir = bpy_gethome(1);

		if (!scriptsdir) {
			printf("Error loading script: can't find default scripts dir!");
			return 0;
		}

		BLI_make_file_string( "/", filestr, scriptsdir, pym->filename );
	}

	fp = fopen( filestr, "rb" );
	if( !fp ) {
		printf( "Error loading script: couldn't open file %s\n",
			filestr );
		return 0;
	}

	BLI_strncpy(scriptname, pym->name, 21);
	len = strlen(scriptname) - 1;
	/* by convention, scripts that open the file browser or have submenus
	 * display '...'.  Here we remove them from the datablock name */
	while ((len > 0) && scriptname[len] == '.') {
		scriptname[len] = '\0';
		len--;
	}
	
	/* Create a new script structure and initialize it: */
	script = alloc_libblock( &G.main->script, ID_SCRIPT, scriptname );

	if( !script ) {
		printf( "couldn't allocate memory for Script struct!" );
		fclose( fp );
		return 0;
	}

	/* let's find a proper area for an eventual script gui:
	 * (still experimenting here, need definition on which win
	 * each group will be put to code this properly) */
	switch ( menutype ) {

	case PYMENU_IMPORT:	/* first 4 were handled in header_info.c */
	case PYMENU_EXPORT:
	case PYMENU_HELP:
	case PYMENU_RENDER:
	case PYMENU_WIZARDS:
		break;

	default:
		if( curarea->spacetype != SPACE_SCRIPT ) {
			ScrArea *sa = NULL;

			sa = find_biggest_area_of_type( SPACE_BUTS );
			if( sa ) {
				if( ( 1.5 * sa->winx ) < sa->winy )
					sa = NULL;	/* too narrow? */
			}

			if( !sa )
				sa = find_biggest_area_of_type( SPACE_SCRIPT );
			if( !sa )
				sa = find_biggest_area_of_type( SPACE_TEXT );
			if( !sa )
				sa = find_biggest_area_of_type( SPACE_IMAGE );	/* group UV */
			if( !sa )
				sa = find_biggest_area_of_type( SPACE_VIEW3D );

			if( !sa )
				sa = find_biggest_area(  );

			areawinset( sa->win );
		}
		break;
	}

	script->id.us = 1;
	script->flags = SCRIPT_RUNNING;
	script->py_draw = NULL;
	script->py_event = NULL;
	script->py_button = NULL;
	script->py_browsercallback = NULL;

	py_dict = CreateGlobalDictionary(  );

	script->py_globaldict = py_dict;

	info = ( BPy_constant * ) PyConstant_New(  );
	if( info ) {
		PyConstant_Insert( info, "name",
				 PyString_FromString( script->id.name + 2 ) );
		PyConstant_Insert( info, "arg", pyarg );
		PyDict_SetItemString( py_dict, "__script__",
				      ( PyObject * ) info );
	}

	/* Previously we used PyRun_File to run directly the code on a FILE 
	 * object, but as written in the Python/C API Ref Manual, chapter 2,
	 * 'FILE structs for different C libraries can be different and 
	 * incompatible'.
	 * So now we load the script file data to a buffer */

	fseek( fp, 0L, SEEK_END );
	len = ftell( fp );
	fseek( fp, 0L, SEEK_SET );

	buffer = MEM_mallocN( len + 2, "pyfilebuf" );	/* len+2 to add '\n\0' */
	len = fread( buffer, 1, len, fp );

	buffer[len] = '\n';	/* fix syntax error in files w/o eol */
	buffer[len + 1] = '\0';

	/* fast clean-up of dos cr/lf line endings: change '\r' to space */

	/* we also have to check for line splitters: '\\' */
	/* to avoid possible syntax errors on dos files on win */
	 /**/
		/* but first make sure we won't disturb memory below &buffer[0]: */
		if( *buffer == '\r' )
		*buffer = ' ';

	/* now handle the whole buffer */
	for( s = buffer + 1; *s != '\0'; s++ ) {
		if( *s == '\r' ) {
			if( *( s - 1 ) == '\\' ) {	/* special case: long lines split with '\': */
				*( s - 1 ) = ' ';	/* we write ' \', because '\ ' is a syntax error */
				*s = '\\';
			} else
				*s = ' ';	/* not a split line, just replace '\r' with ' ' */
		}
	}

	fclose( fp );

	/* run the string buffer */

	py_res = PyRun_String( buffer, Py_file_input, py_dict, py_dict );

	MEM_freeN( buffer );

	if( !py_res ) {		/* Failed execution of the script */

		BPY_Err_Handle( script->id.name + 2 );
		ReleaseGlobalDictionary( py_dict );
		script->py_globaldict = NULL;
		if( G.main->script.first )
			free_libblock( &G.main->script, script );
		error( "Python script error: check console" );

		return 0;
	} else {
		Py_DECREF( py_res );
		script->flags &= ~SCRIPT_RUNNING;

		if( !script->flags ) {
			ReleaseGlobalDictionary( py_dict );
			script->py_globaldict = NULL;
			free_libblock( &G.main->script, script );

			/* special case: called from the menu in the Scripts window
			 * we have to change sc->script pointer, since it'll be freed here.*/
			if( curarea->spacetype == SPACE_SCRIPT ) {
				SpaceScript *sc = curarea->spacedata.first;
				sc->script = G.main->script.first;	/* can be null, which is ok ... */
				/* ... meaning no other script is running right now. */
			}

		}
	}

	return 1;		/* normal return */
}

/*****************************************************************************
* Description:	
* Notes:
*****************************************************************************/
void BPY_free_compiled_text( struct Text *text )
{
	if( !text->compiled )
		return;
	Py_DECREF( ( PyObject * ) text->compiled );
	text->compiled = NULL;

	return;
}

/*****************************************************************************
* Description: This function frees a finished (flags == 0) script.
*****************************************************************************/
void BPY_free_finished_script( Script * script )
{
	if( !script )
		return;

	if( PyErr_Occurred(  ) ) {	/* if script ended after filesel */
		PyErr_Print(  );	/* eventual errors are handled now */
		error( "Python script error: check console" );
	}

	free_libblock( &G.main->script, script );
	return;
}

static void unlink_script( Script * script )
{	/* copied from unlink_text in drawtext.c */
	bScreen *scr;
	ScrArea *area;
	SpaceLink *sl;

	for( scr = G.main->screen.first; scr; scr = scr->id.next ) {
		for( area = scr->areabase.first; area; area = area->next ) {
			for( sl = area->spacedata.first; sl; sl = sl->next ) {
				if( sl->spacetype == SPACE_SCRIPT ) {
					SpaceScript *sc = ( SpaceScript * ) sl;

					if( sc->script == script ) {
						sc->script = NULL;

						if( sc ==
						    area->spacedata.first ) {
							scrarea_queue_redraw
								( area );
						}
					}
				}
			}
		}
	}
}

void BPY_clear_script( Script * script )
{
	PyObject *dict;

	if( !script )
		return;

	Py_XDECREF( ( PyObject * ) script->py_draw );
	Py_XDECREF( ( PyObject * ) script->py_event );
	Py_XDECREF( ( PyObject * ) script->py_button );
	Py_XDECREF( ( PyObject * ) script->py_browsercallback );

	dict = script->py_globaldict;

	if( dict ) {
		PyDict_Clear( dict );
		Py_DECREF( dict );	/* Release dictionary. */
		script->py_globaldict = NULL;
	}

	unlink_script( script );
}

/*****************************************************************************/
/* ScriptLinks                                                        */
/*****************************************************************************/

/*****************************************************************************/
/* Description:								 */
/* Notes:				Not implemented yet	 */
/*****************************************************************************/
void BPY_clear_bad_scriptlinks( struct Text *byebye )
{
/*
	BPY_clear_bad_scriptlist(getObjectList(), byebye);
	BPY_clear_bad_scriptlist(getLampList(), byebye);
	BPY_clear_bad_scriptlist(getCameraList(), byebye);
	BPY_clear_bad_scriptlist(getMaterialList(), byebye);
	BPY_clear_bad_scriptlist(getWorldList(),	byebye);
	BPY_clear_bad_scriptlink(&scene_getCurrent()->id, byebye);

	allqueue(REDRAWBUTSSCRIPT, 0);
*/
	return;
}

/*****************************************************************************
* Description: Loop through all scripts of a list of object types, and 
*	execute these scripts.	
*	For the scene, only the current active scene the scripts are 
*	executed (if any).
*****************************************************************************/
void BPY_do_all_scripts( short event )
{
	DoAllScriptsFromList( &( G.main->object ), event );
	DoAllScriptsFromList( &( G.main->lamp ), event );
	DoAllScriptsFromList( &( G.main->camera ), event );
	DoAllScriptsFromList( &( G.main->mat ), event );
	DoAllScriptsFromList( &( G.main->world ), event );

	BPY_do_pyscript( &( G.scene->id ), event );

	return;
}

/*****************************************************************************
* Description: Execute a Python script when an event occurs. The following  
*		events are possible: frame changed, load script and redraw.  
*		Only events happening to one of the following object types   
*		are handled: Object, Lamp, Camera, Material, World and	    
*		Scene.			
*****************************************************************************/

static ScriptLink *ID_getScriptlink( ID * id )
{
	switch ( MAKE_ID2( id->name[0], id->name[1] ) ) {
	case ID_OB:
		return &( ( Object * ) id )->scriptlink;
	case ID_LA:
		return &( ( Lamp * ) id )->scriptlink;
	case ID_CA:
		return &( ( Camera * ) id )->scriptlink;
	case ID_MA:
		return &( ( Material * ) id )->scriptlink;
	case ID_WO:
		return &( ( World * ) id )->scriptlink;
	case ID_SCE:
		return &( ( Scene * ) id )->scriptlink;
	default:
		return NULL;
	}
}

static PyObject *ID_asPyObject( ID * id )
{
	switch ( MAKE_ID2( id->name[0], id->name[1] ) ) {
	case ID_OB:
		return Object_CreatePyObject( ( Object * ) id );
	case ID_LA:
		return Lamp_CreatePyObject( ( Lamp * ) id );
	case ID_CA:
		return Camera_CreatePyObject( ( Camera * ) id );
	case ID_MA:
		return Material_CreatePyObject( ( Material * ) id );
	case ID_WO:
		return World_CreatePyObject( ( World * ) id );
	case ID_SCE:
		return Scene_CreatePyObject( ( Scene * ) id );
	default:
		Py_INCREF( Py_None );
		return Py_None;
	}
}

int BPY_has_onload_script( void )
{
	ScriptLink *slink = &G.scene->scriptlink;
	int i;

	if( !slink || !slink->totscript )
		return 0;

	for( i = 0; i < slink->totscript; i++ ) {
		if( ( slink->flag[i] == SCRIPT_ONLOAD )
		    && ( slink->scripts[i] != NULL ) )
			return 1;
	}

	return 0;
}

void BPY_do_pyscript( ID * id, short event )
{
	ScriptLink *scriptlink;

	if( !id ) return;

	scriptlink = ID_getScriptlink( id );

	if( scriptlink && scriptlink->totscript ) {
		PyObject *dict;
		PyObject *ret;
		int index, during_slink = during_scriptlink(  );

		/* invalid scriptlinks (new .blend was just loaded), return */
		if( during_slink < 0 )
			return;

		/* tell we're running a scriptlink.  The sum also tells if this script
		 * is running nested inside another.  Blender.Load needs this info to
		 * avoid trouble with invalid slink pointers. */
		during_slink++;
		disable_where_scriptlink( (short)during_slink );

		/* set globals in Blender module to identify scriptlink */
		PyDict_SetItemString( g_blenderdict, "bylink", EXPP_incr_ret_True() );
		PyDict_SetItemString( g_blenderdict, "link",
				      ID_asPyObject( id ) );
		PyDict_SetItemString( g_blenderdict, "event",
				      PyString_FromString( event_to_name
							   ( event ) ) );

		if (event == SCRIPT_POSTRENDER) event = SCRIPT_RENDER;

		for( index = 0; index < scriptlink->totscript; index++ ) {
			if( ( scriptlink->flag[index] == event ) &&
			    ( scriptlink->scripts[index] != NULL ) ) {
				dict = CreateGlobalDictionary(  );
				ret = RunPython( ( Text * ) scriptlink->
						 scripts[index], dict );
				ReleaseGlobalDictionary( dict );

				if( !ret ) {
					/* Failed execution of the script */
					BPY_Err_Handle( scriptlink->
							scripts[index]->name +
							2 );
					//BPY_end_python ();
					//BPY_start_python ();
				} else {
					Py_DECREF( ret );
				}
				/* If a scriptlink has just loaded a new .blend file, the
				 * scriptlink pointer became invalid (see api2_2x/Blender.c),
				 * so we stop here. */
				if( during_scriptlink(  ) == -1 ) {
					during_slink = 1;
					break;
				}
			}
		}

		disable_where_scriptlink( (short)(during_slink - 1) );

		/* cleanup bylink flag and clear link so PyObject
		 * can be released 
		 */
		PyDict_SetItemString( g_blenderdict, "bylink", EXPP_incr_ret_False() );
		Py_INCREF( Py_None );
		PyDict_SetItemString( g_blenderdict, "link", Py_None );
		PyDict_SetItemString( g_blenderdict, "event",
				      PyString_FromString( "" ) );
	}
}

/* SPACE HANDLERS */

/* These are special script links that can be assigned to ScrArea's to
 * (EVENT type) receive events sent to a given space (and use or ignore them) or
 * (DRAW type) be called after the space is drawn, to draw anything on top of
 * the space area. */

/* How to add space handlers to other spaces:
 * - add the space event defines to DNA_scriptlink_types.h, as done for
 *   3d view: SPACEHANDLER_VIEW3D_EVENT, for example;
 * - add the new defines to Blender.SpaceHandler dictionary in Blender.c;
 * - check space.c for how to call the event handlers;
 * - check drawview.c for how to call the draw handlers;
 * - check header_view3d.c for how to add the "Space Handler Scripts" menu.
 * Note: DRAW handlers should be called with 'event = 0', chech drawview.c */

int BPY_has_spacehandler(Text *text, ScrArea *sa)
{
	ScriptLink *slink;
	int index;

	if (!sa || !text) return 0;

	slink = &sa->scriptlink;

	for (index = 0; index < slink->totscript; index++) {
		if (slink->scripts[index] && (slink->scripts[index] == (ID *)text))
			return 1;
	}

	return 0;	
}

int BPY_is_spacehandler(Text *text, char spacetype)
{
	TextLine *tline = text->lines.first;
	unsigned short type = 0;

	if (tline && (tline->len > 10)) {
		char *line = tline->line;

		/* Expected format: # SPACEHANDLER.SPACE.TYPE
		 * Ex: # SPACEHANDLER.VIEW3D.DRAW
		 * The actual checks are forgiving, so slight variations also work. */
		if (line && line[0] == '#' && strstr(line, "HANDLER")) {
			line++; /* skip '#' */

			/* only done for 3D View right now, trivial to add for others: */
			switch (spacetype) {
				case SPACE_VIEW3D:
					if (strstr(line, "3D")) { /* VIEW3D, 3DVIEW */
						if (strstr(line, "DRAW")) type = SPACEHANDLER_VIEW3D_DRAW;
						else if (strstr(line, "EVENT")) type = SPACEHANDLER_VIEW3D_EVENT;
					}
					break;
			}
		}
	}
	return type; /* 0 if not a space handler */
}

int BPY_del_spacehandler(Text *text, ScrArea *sa)
{
	ScriptLink *slink;
	int i, j;

	if (!sa || !text) return -1;

	slink = &sa->scriptlink;
	if (slink->totscript < 1) return -1;

	for (i = 0; i < slink->totscript; i++) {
		if (text == (Text *)slink->scripts[i]) {

			for (j = i; j < slink->totscript - 1; j++) {
				slink->flag[j] = slink->flag[j+1];
				slink->scripts[j] = slink->scripts[j+1];
			}
			slink->totscript--;
			/* like done in buttons_script.c we just free memory
			 * if all slinks have been removed -- less fragmentation,
			 * these should be quite small arrays */
			if (slink->totscript == 0) {
				if (slink->scripts) MEM_freeN(slink->scripts);
				if (slink->flag) MEM_freeN(slink->flag);
				break;
			}
		}
	}
	return 0;
}

int BPY_add_spacehandler(Text *text, ScrArea *sa, char spacetype)
{
	unsigned short handlertype;

	if (!sa || !text) return -1;

	handlertype = (unsigned short)BPY_is_spacehandler(text, spacetype);

	if (handlertype) {
		ScriptLink *slink = &sa->scriptlink;
		void *stmp, *ftmp;
		unsigned short space_event = SPACEHANDLER_VIEW3D_EVENT;

		/* extend slink */

		stmp= slink->scripts;		
		slink->scripts= MEM_mallocN(sizeof(ID*)*(slink->totscript+1),
			"spacehandlerscripts");
	
		ftmp= slink->flag;		
		slink->flag= MEM_mallocN(sizeof(short*)*(slink->totscript+1),
			"spacehandlerflags");
	
		if (slink->totscript) {
			memcpy(slink->scripts, stmp, sizeof(ID*)*(slink->totscript));
			MEM_freeN(stmp);

			memcpy(slink->flag, ftmp, sizeof(short)*(slink->totscript));
			MEM_freeN(ftmp);
		}

		switch (spacetype) {
			case SPACE_VIEW3D:
				if (handlertype == 1) space_event = SPACEHANDLER_VIEW3D_EVENT;
				else space_event = SPACEHANDLER_VIEW3D_DRAW;
				break;
			default:
				break;
		}

		slink->scripts[slink->totscript] = (ID *)text;
		slink->flag[slink->totscript]= space_event;

		slink->totscript++;
		slink->actscript = slink->totscript;

	}
	return 0;
}

int BPY_do_spacehandlers( ScrArea *sa, unsigned short event,
	unsigned short space_event )
{
	ScriptLink *scriptlink;
	int retval = 0;

	if (!sa) return 0;

	scriptlink = &sa->scriptlink;

	if (scriptlink->totscript > 0) {
		PyObject *dict;
		PyObject *ret;
		int index, during_slink = during_scriptlink();

		/* invalid scriptlinks (new .blend was just loaded), return */
		if (during_slink < 0) return 0;

		/* tell we're running a scriptlink.  The sum also tells if this script
		 * is running nested inside another.  Blender.Load needs this info to
		 * avoid trouble with invalid slink pointers. */
		during_slink++;
		disable_where_scriptlink( (short)during_slink );

		/* set globals in Blender module to identify space handler scriptlink */
		PyDict_SetItemString(g_blenderdict, "bylink", EXPP_incr_ret_True());
		/* unlike normal scriptlinks, here Blender.link is int (space event type) */
		PyDict_SetItemString(g_blenderdict, "link", PyInt_FromLong(space_event));
		/* note: DRAW space_events set event to 0 */
		PyDict_SetItemString(g_blenderdict, "event", PyInt_FromLong(event));

		/* now run all assigned space handlers for this space and space_event */
		for( index = 0; index < scriptlink->totscript; index++ ) {

			/* for DRAW handlers: */
			if (event == 0) {
				glPushAttrib(GL_ALL_ATTRIB_BITS);
				glMatrixMode(GL_PROJECTION);
				glPushMatrix();
				glMatrixMode(GL_MODELVIEW);
				glPushMatrix();
			}

			if( ( scriptlink->flag[index] == space_event ) &&
			    ( scriptlink->scripts[index] != NULL ) ) {
				dict = CreateGlobalDictionary();
				ret = RunPython( ( Text * ) scriptlink->scripts[index], dict );
				ReleaseGlobalDictionary( dict );

				if (!ret) { /* Failed execution of the script */
					BPY_Err_Handle( scriptlink->scripts[index]->name+2 );
				} else {
					Py_DECREF(ret);

					/* an EVENT type (event != 0) script can either accept an event or
					 * ignore it:
					 * if the script sets Blender.event to None it accepted it;
					 * otherwise the space's event handling callback that called us
					 * can go on processing the event */
					if (event && (PyDict_GetItemString(g_blenderdict,"event") == Py_None))
						retval = 1; /* event was swallowed */
				}

				/* If a scriptlink has just loaded a new .blend file, the
				 * scriptlink pointer became invalid (see api2_2x/Blender.c),
				 * so we stop here. */
				if( during_scriptlink(  ) == -1 ) {
					during_slink = 1;
					if (event == 0) glPopAttrib();
					break;
				}
			}

			/* for DRAW handlers: */
			if (event == 0) {
				glMatrixMode(GL_PROJECTION);
				glPopMatrix();
				glMatrixMode(GL_MODELVIEW);
				glPopMatrix();
				glPopAttrib();
			}

		}

		disable_where_scriptlink( (short)(during_slink - 1) );

		PyDict_SetItemString(g_blenderdict, "bylink", EXPP_incr_ret_False());
		PyDict_SetItemString(g_blenderdict, "link", EXPP_incr_ret(Py_None));
		PyDict_SetItemString(g_blenderdict, "event", PyString_FromString(""));
	}
	
	/* retval:
	 * space_event is of type EVENT:
	 * 0 - event was returned,
	 * 1 - event was processed;
	 * space_event is of type DRAW:
	 * 0 always */

	return retval;
}

/*****************************************************************************
* Description:	
* Notes:
*****************************************************************************/
void BPY_free_scriptlink( struct ScriptLink *slink )
{
	if( slink->totscript ) {
		if( slink->flag )
			MEM_freeN( slink->flag );
		if( slink->scripts )
			MEM_freeN( slink->scripts );
	}

	return;
}

void BPY_free_screen_spacehandlers(struct bScreen *sc)
{
	ScrArea *sa;

	for (sa = sc->areabase.first; sa; sa = sa->next)
		BPY_free_scriptlink(&sa->scriptlink);
}

static int CheckAllSpaceHandlers(Text *text)
{
	bScreen *screen;
	ScrArea *sa;
	ScriptLink *slink;
	int fixed = 0;

	for (screen = G.main->screen.first; screen; screen = screen->id.next) {
		for (sa = screen->areabase.first; sa; sa = sa->next) {
			slink = &sa->scriptlink;
			if (!slink->totscript) continue;
			if (BPY_del_spacehandler(text, sa) == 0) fixed++;
		}
	}
	return fixed;
}

static int CheckAllScriptsFromList( ListBase * list, Text * text )
{
	ID *id;
	ScriptLink *scriptlink;
	int index;
	int fixed = 0;

	id = list->first;

	while( id != NULL ) {
		scriptlink = ID_getScriptlink( id );
		if( scriptlink && scriptlink->totscript ) {
			for( index = 0; index < scriptlink->totscript; index++) {
				if ((Text *)scriptlink->scripts[index] == text) {
					scriptlink->scripts[index] = NULL;
					fixed++;
				}
			}
		}
		id = id->next;
	}

	return fixed;
}

/* When a Text is deleted, we need to unlink it from eventual scriptlinks */
int BPY_check_all_scriptlinks( Text * text )
{
	int fixed = 0;
	fixed += CheckAllScriptsFromList( &( G.main->object ), text );
	fixed += CheckAllScriptsFromList( &( G.main->lamp ), text );
	fixed += CheckAllScriptsFromList( &( G.main->camera ), text );
	fixed += CheckAllScriptsFromList( &( G.main->mat ), text );
	fixed += CheckAllScriptsFromList( &( G.main->world ), text );
	fixed += CheckAllScriptsFromList( &( G.main->scene ), text );
	fixed += CheckAllSpaceHandlers(text);

	return fixed;
}

/*****************************************************************************
* Description: 
* Notes:
*****************************************************************************/
void BPY_copy_scriptlink( struct ScriptLink *scriptlink )
{
	void *tmp;

	if( scriptlink->totscript ) {

		tmp = scriptlink->scripts;
		scriptlink->scripts =
			MEM_mallocN( sizeof( ID * ) * scriptlink->totscript,
				     "scriptlistL" );
		memcpy( scriptlink->scripts, tmp,
			sizeof( ID * ) * scriptlink->totscript );

		tmp = scriptlink->flag;
		scriptlink->flag =
			MEM_mallocN( sizeof( short ) * scriptlink->totscript,
				     "scriptlistF" );
		memcpy( scriptlink->flag, tmp,
			sizeof( short ) * scriptlink->totscript );
	}

	return;
}

/****************************************************************************
* Description:
* Notes:		Not implemented yet
*****************************************************************************/
int BPY_call_importloader( char *name )
{			/* XXX Should this function go away from Blender? */
	printf( "In BPY_call_importloader(name=%s)\n", name );
	return ( 0 );
}

/*****************************************************************************
* Private functions
*****************************************************************************/

/*****************************************************************************
* Description: This function executes the python script passed by text.	
*		The Python dictionary containing global variables needs to
*		be passed in globaldict.
*****************************************************************************/
PyObject *RunPython( Text * text, PyObject * globaldict )
{
	char *buf = NULL;

/* The script text is compiled to Python bytecode and saved at text->compiled
 * to speed-up execution if the user executes the script multiple times */

	if( !text->compiled ) {	/* if it wasn't already compiled, do it now */
		buf = txt_to_buf( text );

		text->compiled =
			Py_CompileString( buf, GetName( text ),
					  Py_file_input );

		MEM_freeN( buf );

		if( PyErr_Occurred(  ) ) {
			BPY_free_compiled_text( text );
			return NULL;
		}

	}

	return PyEval_EvalCode( text->compiled, globaldict, globaldict );
}

/*****************************************************************************
* Description: This function returns the value of the name field of the	
*	given Text struct.
*****************************************************************************/
char *GetName( Text * text )
{
	return ( text->id.name + 2 );
}

/*****************************************************************************
* Description: This function creates a new Python dictionary object.
*****************************************************************************/
PyObject *CreateGlobalDictionary( void )
{
	PyObject *dict = PyDict_New(  );

	PyDict_SetItemString( dict, "__builtins__", PyEval_GetBuiltins(  ) );
	PyDict_SetItemString( dict, "__name__",
			      PyString_FromString( "__main__" ) );

	return dict;
}

/*****************************************************************************
* Description: This function deletes a given Python dictionary object.
*****************************************************************************/
void ReleaseGlobalDictionary( PyObject * dict )
{
	PyDict_Clear( dict );
	Py_DECREF( dict );	/* Release dictionary. */

	return;
}

/***************************************************************************
* Description: This function runs all scripts (if any) present in the
*		list argument. The event by which the function has been	
*		called, is passed in the event argument.
*****************************************************************************/
void DoAllScriptsFromList( ListBase * list, short event )
{
	ID *id;

	id = list->first;

	while( id != NULL ) {
		BPY_do_pyscript( id, event );
		id = id->next;
	}

	return;
}

PyObject *importText( char *name )
{
	Text *text;
	char *txtname;
	char *buf = NULL;
	int namelen = strlen( name );

	txtname = malloc( namelen + 3 + 1 );
	if( !txtname )
		return NULL;

	memcpy( txtname, name, namelen );
	memcpy( &txtname[namelen], ".py", 4 );

	text = ( Text * ) & ( G.main->text.first );

	while( text ) {
		if( !strcmp( txtname, GetName( text ) ) )
			break;
		text = text->id.next;
	}

	if( !text ) {
		free( txtname );
		return NULL;
	}

	if( !text->compiled ) {
		buf = txt_to_buf( text );
		text->compiled =
			Py_CompileString( buf, GetName( text ),
					  Py_file_input );
		MEM_freeN( buf );

		if( PyErr_Occurred(  ) ) {
			PyErr_Print(  );
			BPY_free_compiled_text( text );
			free( txtname );
			return NULL;
		}
	}

	free( txtname );
	return PyImport_ExecCodeModule( name, text->compiled );
}

static PyMethodDef bimport[] = {
	{"blimport", blender_import, METH_VARARGS, "our own import"}
};

PyObject *blender_import( PyObject * self, PyObject * args )
{
	PyObject *exception, *err, *tb;
	char *name;
	PyObject *globals = NULL, *locals = NULL, *fromlist = NULL;
	PyObject *m;

	if( !PyArg_ParseTuple( args, "s|OOO:bimport",
			       &name, &globals, &locals, &fromlist ) )
		return NULL;

	m = PyImport_ImportModuleEx( name, globals, locals, fromlist );

	if( m )
		return m;
	else
		PyErr_Fetch( &exception, &err, &tb );	/*restore for probable later use */

	m = importText( name );
	if( m ) {		/* found module, ignore above exception */
		PyErr_Clear(  );
		Py_XDECREF( exception );
		Py_XDECREF( err );
		Py_XDECREF( tb );
		printf( "imported from text buffer...\n" );
	} else {
		PyErr_Restore( exception, err, tb );
	}
	return m;
}

void init_ourImport( void )
{
	PyObject *m, *d;
	PyObject *import = PyCFunction_New( bimport, NULL );

	m = PyImport_AddModule( "__builtin__" );
	d = PyModule_GetDict( m );
	PyDict_SetItemString( d, "__import__", import );
}
