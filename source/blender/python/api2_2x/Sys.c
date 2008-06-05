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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
*/

#include "Sys.h" /*This must come first*/

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BLI_blenlib.h"
#include "DNA_scene_types.h" /* G.scene-"r.cfra */
#include "PIL_time.h"
#include "gen_utils.h"

#ifdef WIN32
#define DIRSEP '\\'
#define DIRSEP_STR "\\"
#else
#define DIRSEP '/'
#define DIRSEP_STR "/"
#endif


/*****************************************************************************/
/* Python API function prototypes for the sys module.                        */
/*****************************************************************************/
static PyObject *M_sys_basename( PyObject * self, PyObject * value );
static PyObject *M_sys_dirname( PyObject * self, PyObject * value );
static PyObject *M_sys_join( PyObject * self, PyObject * args );
static PyObject *M_sys_splitext( PyObject * self, PyObject * value );
static PyObject *M_sys_makename( PyObject * self, PyObject * args,
	PyObject * kw );
static PyObject *M_sys_exists( PyObject * self, PyObject * value );
static PyObject *M_sys_time( PyObject * self );
static PyObject *M_sys_sleep( PyObject * self, PyObject * args );
static PyObject *M_sys_expandpath( PyObject *self, PyObject *value);
static PyObject *M_sys_cleanpath( PyObject *self, PyObject *value);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.sys.__doc__                                                       */
/*****************************************************************************/
static char M_sys_doc[] = "The Blender.sys submodule\n\
\n\
This is a minimal system module to supply simple functionality available\n\
in the default Python module os.";

static char M_sys_basename_doc[] =
	"(path) - Split 'path' in dir and filename.\n\
Return the filename.";

static char M_sys_dirname_doc[] =
	"(path) - Split 'path' in dir and filename.\n\
Return the dir.";

static char M_sys_join_doc[] =
	"(dir, file) - Join dir and file to form a full filename.\n\
Return the filename.";

static char M_sys_splitext_doc[] =
	"(path) - Split 'path' in root and extension:\n\
/this/that/file.ext -> ('/this/that/file','.ext').\n\
Return the pair (root, extension).";

static char M_sys_makename_doc[] =
	"(path = Blender.Get('filename'), ext = \"\", strip = 0) -\n\
Strip dir and extension from path, leaving only a name, then append 'ext'\n\
to it (if given) and return the resulting string.\n\n\
(path) - string: a pathname -- Blender.Get('filename') if 'path' isn't given;\n\
(ext = \"\") - string: the extension to append.\n\
(strip = 0) - int: strip dirname from 'path' if given and non-zero.\n\
Ex: makename('/path/to/file/myfile.foo','-01.abc') returns 'myfile-01.abc'\n\
Ex: makename(ext='.txt') returns 'untitled.txt' if Blender.Get('filename')\n\
returns a path to the file 'untitled.blend'";

static char M_sys_time_doc[] =
	"() - Return a float representing time elapsed in seconds.\n\
Each successive call is garanteed to return values greater than or\n\
equal to the previous call.";

static char M_sys_sleep_doc[] =
	"(milliseconds = 10) - Sleep for the specified time.\n\
(milliseconds = 10) - the amount of time in milliseconds to sleep.\n\
This function can be necessary in tight 'get event' loops.";

static char M_sys_exists_doc[] =
	"(path) - Check if the given pathname exists.\n\
The return value is as follows:\n\
\t 0: path doesn't exist;\n\
\t 1: path is an existing filename;\n\
\t 2: path is an existing dirname;\n\
\t-1: path exists but is neither a regular file nor a dir.";

static char M_sys_expandpath_doc[] =
"(path) - Expand this Blender internal path to a proper file system path.\n\
(path) - the string path to convert.\n\n\
Note: internally Blender paths can contain two special character sequences:\n\
- '//' (at start) for base path directory (the current .blend's dir path);\n\
- '#' characters in the filename will be replaced by the frame number.\n\n\
This function expands these to their actual content, returning a valid path.\n\
If the special chars are not found in the given path, it is simply returned.";

static char M_sys_cleanpath_doc[] =
"(path) - Removes parts of a path that are not needed paths such as '../foo/../bar/' and '//./././'";

/*****************************************************************************/
/* Python method structure definition for Blender.sys module:                */
/*****************************************************************************/
struct PyMethodDef M_sys_methods[] = {
	{"basename", M_sys_basename, METH_O, M_sys_basename_doc},
	{"dirname", M_sys_dirname, METH_O, M_sys_dirname_doc},
	{"join", M_sys_join, METH_VARARGS, M_sys_join_doc},
	{"splitext", M_sys_splitext, METH_O, M_sys_splitext_doc},
	{"makename", ( PyCFunction ) M_sys_makename,
	 METH_VARARGS | METH_KEYWORDS,
	 M_sys_makename_doc},
	{"exists", M_sys_exists, METH_O, M_sys_exists_doc},
	{"sleep", M_sys_sleep, METH_VARARGS, M_sys_sleep_doc},
	{"time", ( PyCFunction ) M_sys_time, METH_NOARGS, M_sys_time_doc},
	{"expandpath", M_sys_expandpath, METH_O, M_sys_expandpath_doc},
	{"cleanpath", M_sys_cleanpath, METH_O, M_sys_cleanpath_doc},
	{NULL, NULL, 0, NULL}
};

/* Module Functions */

static PyObject *g_sysmodule = NULL;	/* pointer to Blender.sys module */

PyObject *sys_Init( void )
{
	PyObject *submodule, *dict;

	submodule = Py_InitModule3( "Blender.sys", M_sys_methods, M_sys_doc );

	g_sysmodule = submodule;

	dict = PyModule_GetDict( submodule );
	
	EXPP_dict_set_item_str( dict, "dirsep", PyString_FromString(DIRSEP_STR) );
	EXPP_dict_set_item_str( dict, "sep", PyString_FromString(DIRSEP_STR) );

	return submodule;
}

static PyObject *M_sys_basename( PyObject * self, PyObject * value )
{
	char *name = PyString_AsString(value);
	char *p, basename[FILE_MAXDIR + FILE_MAXFILE];
	int n, len;

	if( !name )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument" );

	len = strlen( name );
	
#ifdef WIN32
	p = MAX2(strrchr( name, '/' ), strrchr( name, '\\' ));
#else
	p = strrchr( name, DIRSEP );
#endif

	if( p ) {
		n = name + len - p - 1;	/* - 1 because we don't want the sep */

		if( n > FILE_MAXDIR + FILE_MAXFILE )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						      "path too long" );

		BLI_strncpy( basename, p + 1, n + 1 );
		return PyString_FromString( basename );
	}

	return PyString_FromString( name );
}

static PyObject *M_sys_dirname( PyObject * self, PyObject * value )
{
	char *name = PyString_AsString(value);
	char *p, dirname[FILE_MAXDIR + FILE_MAXFILE];
	int n;

	if( !name )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument" );

#ifdef WIN32
	p = MAX2(strrchr( name, '/' ), strrchr( name, '\\' ));
#else
	p = strrchr( name, DIRSEP );
#endif

	if( p ) {
		n = p - name;

		if( n > FILE_MAXDIR + FILE_MAXFILE )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						      "path too long" );

		BLI_strncpy( dirname, name, n + 1 );
		return PyString_FromString( dirname );
	}

	return PyString_FromString( "." );
}

static PyObject *M_sys_join( PyObject * self, PyObject * args )
{
	char *name = NULL, *path = NULL;
	char filename[FILE_MAXDIR + FILE_MAXFILE];
	int pathlen = 0, namelen = 0;

	if( !PyArg_ParseTuple( args, "ss", &path, &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument" );

	pathlen = strlen( path ) + 1;
	namelen = strlen( name ) + 1;	/* + 1 to account for '\0' for BLI_strncpy */

	if( pathlen + namelen > FILE_MAXDIR + FILE_MAXFILE - 1 )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "filename is too long." );

	BLI_strncpy( filename, path, pathlen );

	if( filename[pathlen - 2] != DIRSEP ) {
		filename[pathlen - 1] = DIRSEP;
		pathlen += 1;
	}

	BLI_strncpy( filename + pathlen - 1, name, namelen );

	return PyString_FromString( filename );
}

static PyObject *M_sys_splitext( PyObject * self, PyObject * value )
{
	char *name = PyString_AsString(value);
	char *dot, *p, path[FILE_MAXDIR + FILE_MAXFILE], ext[FILE_MAXDIR + FILE_MAXFILE];
	int n, len;

	if( !name )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument" );

	len = strlen( name );
	dot = strrchr( name, '.' );

	if( !dot )
		return Py_BuildValue( "ss", name, "" );

	p = strrchr( name, DIRSEP );

	if( p ) {
		if( p > dot )
			return Py_BuildValue( "ss", name, "" );
	}

	n = name + len - dot;

	/* loong extensions are supported -- foolish, but Python's os.path.splitext
	 * supports them, so ... */
	 
	if( n >= FILE_MAXDIR + FILE_MAXFILE || ( len - n ) >= FILE_MAXDIR + FILE_MAXFILE )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError, "path too long" );

	BLI_strncpy( ext, dot, n + 1 );
	BLI_strncpy( path, name, dot - name + 1 );

	return Py_BuildValue( "ss", path, ext );
}

static PyObject *M_sys_makename( PyObject * self, PyObject * args,
				 PyObject * kw )
{
	char *path = G.sce, *ext = NULL;
	int strip = 0;
	static char *kwlist[] = { "path", "ext", "strip", NULL };
	char *dot = NULL, *p = NULL, basename[FILE_MAXDIR + FILE_MAXFILE];
	int n, len, lenext = 0;

	if( !PyArg_ParseTupleAndKeywords( args, kw, "|ssi", kwlist,
					  &path, &ext, &strip ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected one or two strings and an int (or nothing) as arguments" );

	len = strlen( path ) + 1;	/* + 1 to consider ending '\0' */
	if( ext )
		lenext = strlen( ext ) + 1;

	if( ( len + lenext ) > FILE_MAXDIR + FILE_MAXFILE )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "path too long" );

	p = strrchr( path, DIRSEP );

	if( p && strip ) {
		n = path + len - p;
		BLI_strncpy( basename, p + 1, n );	/* + 1 to skip the sep */
	} else
		BLI_strncpy( basename, path, len );

	dot = strrchr( basename, '.' );

	/* now the extension: always remove the one in basename */
	if( dot || ext ) {
		if( !ext )
			basename[dot - basename] = '\0';
		else {		/* if user gave an ext, append it */

			if( dot )
				n = dot - basename;
			else
				n = strlen( basename );

			BLI_strncpy( basename + n, ext, lenext );
		}
	}

	return PyString_FromString( basename );
}

static PyObject *M_sys_time( PyObject * self )
{	
	return PyFloat_FromDouble( PIL_check_seconds_timer(  ) );
}

static PyObject *M_sys_sleep( PyObject * self, PyObject * args )
{
	int millisecs = 10;

	if( !PyArg_ParseTuple( args, "|i", &millisecs ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected int argument" );

	PIL_sleep_ms( millisecs );

	return EXPP_incr_ret( Py_None );
}

static PyObject *M_sys_exists( PyObject * self, PyObject * value )
{
	char *fname = PyString_AsString(value);
	
	int mode = 0, i = -1;

	if( !fname )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string (pathname) argument" );

	mode = BLI_exist(fname);
	
	if( mode == 0 )
		i = 0;
	else if( S_ISREG( mode ) )
		i = 1;
	else if( S_ISDIR( mode ) )
		i = 2;
	/* i stays as -1 if path exists but is neither a regular file nor a dir */
	
	return PyInt_FromLong(i);
}

static PyObject *M_sys_expandpath( PyObject * self, PyObject * value )
{
	char *path = PyString_AsString(value);
	char expanded[FILE_MAXDIR + FILE_MAXFILE];

	if (!path)
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"expected string argument" );
	
	BLI_strncpy(expanded, path, FILE_MAXDIR + FILE_MAXFILE);
	BLI_convertstringcode(expanded, G.sce);
	BLI_convertstringframe(expanded, G.scene->r.cfra);

	return PyString_FromString(expanded);
}

static PyObject *M_sys_cleanpath( PyObject * self, PyObject * value )
{
	char *path = PyString_AsString(value);
	char cleaned[FILE_MAXDIR + FILE_MAXFILE];
	int trailing_slash = 0, last;
	if (!path)
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"expected string argument" );
	last = strlen(path)-1;
	if ((last >= 0) && ((path[last]=='/') || (path[last]=='\\'))) {
		trailing_slash = 1;
	}
	BLI_strncpy(cleaned, path, FILE_MAXDIR + FILE_MAXFILE);
	BLI_cleanup_file(NULL, cleaned);
	
	if (trailing_slash) {
		BLI_add_slash(cleaned);
	}
	
	return PyString_FromString(cleaned);
}
